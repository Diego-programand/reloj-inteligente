/**
 * @file hal_display.cpp
 * @brief Implementación del driver QSPI para la pantalla AMOLED CO5300 / SH8601 (410x502).
 */

#include "hal_display.h"
#include <driver/spi_master.h>
#include <esp_heap_caps.h>

// Desplazamiento horizontal descubierto en la tabla de fábrica (0x0016 = 22 píxeles)
#define LCD_COL_OFFSET              0x16
#define LCD_ROW_OFFSET              0x00

// Códigos de operación QSPI del protocolo SH8601 / CO5300
#define LCD_OPCODE_WRITE_CMD        0x02
#define LCD_OPCODE_WRITE_COLOR      0x32

// Tamaño del bloque intermedio en memoria DMA (30 líneas: 410 * 30 * 2 = 24,600 bytes < límite DMA de 32 KB)
#define DMA_CHUNK_LINES             30
#define DMA_CHUNK_PIXELS            (WATCH_LCD_WIDTH * DMA_CHUNK_LINES)

static spi_device_handle_t lcdSpiHandle = nullptr;
static uint16_t *dmaSwapBuffer = nullptr;
static uint8_t currentBrightness = 220;
static bool displayPowerOn = true;

/**
 * @brief Envía un comando de control al chip de la pantalla (por 1 línea de datos).
 */
static void sendLcdCmd(uint8_t cmd, const uint8_t *data, size_t len) {
    if (!lcdSpiHandle) return;

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.cmd       = LCD_OPCODE_WRITE_CMD;
    t.addr      = ((uint32_t)cmd) << 8;
    t.length    = len * 8;
    t.tx_buffer = (len > 0) ? data : nullptr;

    spi_device_polling_transmit(lcdSpiHandle, &t);
}

bool hal_display_init() {
    // 1. Configurar el pin TE/EN (GPIO 13) y el pin de Reset físico de la pantalla (GPIO 8)
    pinMode(WATCH_LCD_TE_EN_PIN, INPUT);
    pinMode(WATCH_LCD_RST_PIN, OUTPUT);

    // Pulso de reinicio físico: apaga la imagen congelada anterior y despierta el chip limpio
    digitalWrite(WATCH_LCD_RST_PIN, HIGH);
    delay(20);
    digitalWrite(WATCH_LCD_RST_PIN, LOW);
    delay(50);
    digitalWrite(WATCH_LCD_RST_PIN, HIGH);
    delay(150);

    // 2. Configurar el bus QSPI de 4 carriles en el periférico SPI2 del ESP32-S3
    spi_bus_config_t buscfg = {};
    buscfg.sclk_io_num     = WATCH_LCD_SCK_PIN;  // GPIO 11 (Reloj)
    buscfg.data0_io_num    = WATCH_LCD_D0_PIN;   // GPIO 4  (Carril 0)
    buscfg.data1_io_num    = WATCH_LCD_D1_PIN;   // GPIO 5  (Carril 1)
    buscfg.data2_io_num    = WATCH_LCD_D2_PIN;   // GPIO 6  (Carril 2)
    buscfg.data3_io_num    = WATCH_LCD_D3_PIN;   // GPIO 7  (Carril 3)
    buscfg.data4_io_num    = -1;
    buscfg.data5_io_num    = -1;
    buscfg.data6_io_num    = -1;
    buscfg.data7_io_num    = -1;
    buscfg.max_transfer_sz = DMA_CHUNK_PIXELS * sizeof(uint16_t) + 64;
    buscfg.flags           = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS;

    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return false;
    }

    // 3. Registrar el dispositivo de pantalla en el bus QSPI a 80 MHz
    spi_device_interface_config_t devcfg = {};
    devcfg.command_bits   = 8;
    devcfg.address_bits   = 24;
    devcfg.mode           = 0;
    devcfg.clock_speed_hz = 80 * 1000 * 1000; // 80 MHz en 4 carriles = 320 Mbit/s (máxima fluidez)
    devcfg.spics_io_num   = WATCH_LCD_CS_PIN; // GPIO 12 (Chip Select)
    devcfg.queue_size     = 10;
    devcfg.flags          = SPI_DEVICE_HALFDUPLEX;

    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &lcdSpiHandle);
    if (ret != ESP_OK) {
        return false;
    }

    // 4. Reservar un bloque pequeño en memoria RAM rápida (DMA) para invertir bytes de color
    if (!dmaSwapBuffer) {
        dmaSwapBuffer = (uint16_t *)heap_caps_malloc(DMA_CHUNK_PIXELS * sizeof(uint16_t), MALLOC_CAP_DMA);
    }

    // 5. Enviar la secuencia exacta de inicialización (extraída del firmware original en 0x2720C4)
    sendLcdCmd(0x11, nullptr, 0); // Sleep Out (Despertar pantalla)
    delay(120);

    const uint8_t c4_data[] = {0x80};
    sendLcdCmd(0xC4, c4_data, 1); // Habilitar escritura QSPI en memoria GRAM

    const uint8_t madctl[] = {0x00};
    sendLcdCmd(0x36, madctl, 1);  // Orientación normal RGB

    const uint8_t colmod[] = {0x55};
    sendLcdCmd(0x3A, colmod, 1);  // Formato de color: 16 bits por píxel (RGB565)

    const uint8_t tear_scan[] = {0x01, 0xD1};
    sendLcdCmd(0x44, tear_scan, 2);

    const uint8_t te_on[] = {0x00};
    sendLcdCmd(0x35, te_on, 1);   // Tearing Effect ON (sincronización vertical)

    const uint8_t ctrl_disp[] = {0x20};
    sendLcdCmd(0x53, ctrl_disp, 1); // Control de brillo digital activado
    delay(10);

    const uint8_t hbm_bright[] = {0xFF};
    sendLcdCmd(0x63, hbm_bright, 1);
    delay(10);

    // Limpiar la pantalla en negro antes de encender el brillo para evitar parpadeos
    uint8_t initialBrightness = currentBrightness;
    hal_display_set_brightness(0);
    sendLcdCmd(0x29, nullptr, 0); // Display ON (Encender panel)
    delay(10);

    hal_display_fill_screen(COLOR_BLACK);
    hal_display_set_brightness(initialBrightness);

    return true;
}

void hal_display_set_brightness(uint8_t brightness) {
    currentBrightness = brightness;
    sendLcdCmd(0x51, &currentBrightness, 1);
}

uint8_t hal_display_get_brightness() {
    return currentBrightness;
}

void hal_display_set_power(bool on) {
    if (on) {
        sendLcdCmd(0x11, nullptr, 0); // Sleep OUT
        delay(60);
        sendLcdCmd(0x29, nullptr, 0); // Display ON
        delay(10);
        uint8_t restoreBright = (currentBrightness > 0) ? currentBrightness : 220;
        sendLcdCmd(0x51, &restoreBright, 1);
        displayPowerOn = true;
    } else {
        uint8_t zero = 0;
        sendLcdCmd(0x51, &zero, 1);   // Brillo a 0 inmediatamente
        sendLcdCmd(0x28, nullptr, 0); // Display OFF
        delay(10);
        sendLcdCmd(0x10, nullptr, 0); // Sleep IN (bajo consumo en CO5300)
        displayPowerOn = false;
    }
}

bool hal_display_is_on() {
    return displayPowerOn;
}

void hal_display_push_colors(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, const uint16_t *colorData) {
    if (!lcdSpiHandle || !colorData || !dmaSwapBuffer) return;

    uint16_t w = x2 - x1 + 1;
    uint16_t h = y2 - y1 + 1;

    // 1. Definir ventana horizontal sumando el offset físico de 22 columnas (0x16)
    uint16_t xs = x1 + LCD_COL_OFFSET;
    uint16_t xe = x2 + LCD_COL_OFFSET;
    uint8_t caset[4] = {
        (uint8_t)(xs >> 8), (uint8_t)(xs & 0xFF),
        (uint8_t)(xe >> 8), (uint8_t)(xe & 0xFF)
    };
    sendLcdCmd(0x2A, caset, 4);

    // 2. Cuántas filas caben en cada bloque DMA sin superar DMA_CHUNK_PIXELS (siempre múltiplo par de 2)
    uint16_t maxRowsPerChunk = (DMA_CHUNK_PIXELS / w) & ~0x01;
    if (maxRowsPerChunk == 0) maxRowsPerChunk = 2;

    // 3. Enviar franja por franja actualizando la ventana vertical (0x2B) y usando 0x2C (RAMWR)
    //    Nota técnica: En el chip CO5300, al levantar CS al final de cada transacción QSPI,
    //    cada nuevo bloque debe indicar su fila inicial en 0x2B y escribir con 0x002C00.
    uint16_t rowsSent = 0;
    while (rowsSent < h) {
        uint16_t chunkRows = h - rowsSent;
        if (chunkRows > maxRowsPerChunk) {
            chunkRows = maxRowsPerChunk;
        }

        uint16_t ys = y1 + rowsSent + LCD_ROW_OFFSET;
        uint16_t ye = ys + chunkRows - 1;
        uint8_t raset[4] = {
            (uint8_t)(ys >> 8), (uint8_t)(ys & 0xFF),
            (uint8_t)(ye >> 8), (uint8_t)(ye & 0xFF)
        };
        sendLcdCmd(0x2B, raset, 4);

        size_t chunkPixels = (size_t)w * (size_t)chunkRows;
        size_t offsetPixels = (size_t)rowsSent * (size_t)w;

        for (size_t i = 0; i < chunkPixels; i++) {
            uint16_t c = colorData[offsetPixels + i];
            dmaSwapBuffer[i] = (c >> 8) | (c << 8);
        }

        spi_transaction_t t;
        memset(&t, 0, sizeof(t));
        t.flags     = SPI_TRANS_MODE_QIO;
        t.cmd       = LCD_OPCODE_WRITE_COLOR;
        t.addr      = 0x002C00;
        t.length    = chunkPixels * 16;
        t.tx_buffer = dmaSwapBuffer;

        spi_device_polling_transmit(lcdSpiHandle, &t);

        rowsSent += chunkRows;
    }
}

void hal_display_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t colorRgb565) {
    if (!lcdSpiHandle || !dmaSwapBuffer || w == 0 || h == 0) return;
    if (x >= WATCH_LCD_WIDTH || y >= WATCH_LCD_HEIGHT) return;
    if (x + w > WATCH_LCD_WIDTH)  w = WATCH_LCD_WIDTH - x;
    if (y + h > WATCH_LCD_HEIGHT) h = WATCH_LCD_HEIGHT - y;

    uint16_t x2 = x + w - 1;

    uint16_t xs = x + LCD_COL_OFFSET;
    uint16_t xe = x2 + LCD_COL_OFFSET;
    uint8_t caset[4] = {(uint8_t)(xs >> 8), (uint8_t)(xs & 0xFF), (uint8_t)(xe >> 8), (uint8_t)(xe & 0xFF)};
    sendLcdCmd(0x2A, caset, 4);

    uint16_t maxRowsPerChunk = DMA_CHUNK_PIXELS / w;
    if (maxRowsPerChunk == 0) maxRowsPerChunk = 1;

    uint16_t swapped = (colorRgb565 >> 8) | (colorRgb565 << 8);
    size_t maxChunkPixels = (size_t)w * (size_t)((h < maxRowsPerChunk) ? h : maxRowsPerChunk);
    for (size_t i = 0; i < maxChunkPixels; i++) {
        dmaSwapBuffer[i] = swapped;
    }

    uint16_t rowsSent = 0;
    while (rowsSent < h) {
        uint16_t chunkRows = h - rowsSent;
        if (chunkRows > maxRowsPerChunk) chunkRows = maxRowsPerChunk;

        uint16_t ys = y + rowsSent + LCD_ROW_OFFSET;
        uint16_t ye = ys + chunkRows - 1;
        uint8_t raset[4] = {(uint8_t)(ys >> 8), (uint8_t)(ys & 0xFF), (uint8_t)(ye >> 8), (uint8_t)(ye & 0xFF)};
        sendLcdCmd(0x2B, raset, 4);

        size_t chunkPixels = (size_t)w * (size_t)chunkRows;

        spi_transaction_t t;
        memset(&t, 0, sizeof(t));
        t.flags     = SPI_TRANS_MODE_QIO;
        t.cmd       = LCD_OPCODE_WRITE_COLOR;
        t.addr      = 0x002C00;
        t.length    = chunkPixels * 16;
        t.tx_buffer = dmaSwapBuffer;

        spi_device_polling_transmit(lcdSpiHandle, &t);

        rowsSent += chunkRows;
    }
}

void hal_display_fill_screen(uint16_t colorRgb565) {
    hal_display_fill_rect(0, 0, WATCH_LCD_WIDTH, WATCH_LCD_HEIGHT, colorRgb565);
}
