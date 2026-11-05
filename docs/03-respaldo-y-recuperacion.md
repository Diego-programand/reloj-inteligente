# 03. Protocolo de Respaldo, Restauración y Desbrickeo

Este documento detalla cómo hacer copias de seguridad completas de la memoria Flash de 32 MB, cómo restaurar el reloj exactamente a como vino de fábrica y cómo recuperar la placa si un error de programación bloquea el arranque o hace desaparecer el puerto `COM5`.

---

## 1. Cómo Hacer el Respaldo Completo de 32 MB (Backup)

### Opción A: Usando nuestro Script Automatizado (Recomendado)
Desde una terminal de PowerShell en `G:\ITM\reloj-inteligente`, ejecuta:

```powershell
.\scripts\backup_flash_32mb.ps1
```

Este script:
1. Verifica que el reloj esté conectado en `COM5`.
2. Extrae los **32 MB completos** (`0x00000000` a `0x02000000`) a `921600` baudios usando `esptool`.
3. Guarda el archivo en `backups\factory_backup_32MB_<fecha>.bin`.
4. Calcula automáticamente el hash **SHA256** del archivo para garantizar su integridad.

### Opción B: Comando Manual con `esptool`
Si prefieres ejecutarlo manualmente:

```powershell
python -m esptool --chip esp32s3 --port COM5 --baud 921600 read-flash 0x0 0x2000000 "G:\ITM\reloj-inteligente\backups\factory_backup_32MB.bin"
```

> [!TIP]
> **Respaldo rápido de emergencia (solo primeros 28 MB útiles):**
> Como vimos en la tabla de particiones ([`docs/02-estado-fabrica-y-particiones.md`](file:///G:/ITM/reloj-inteligente/docs/02-estado-fabrica-y-particiones.md)), las particiones terminan en `0x01C00000` (28 MB), pero hacer el volcado completo de `0x2000000` (32 MB) a `921600` baudios tarda apenas unos **5 a 6 minutos** y garantiza una copia 1:1 bit a bit.

---

## 2. Cómo Restaurar el Reloj a su Estado Original de Fábrica

Si en cualquier momento deseas borrar todo nuestro código y dejar el reloj idéntico a como salió de la caja:

### Opción A: Usando nuestro Script Automatizado
```powershell
.\scripts\restore_flash_32mb.ps1 -BackupFile "G:\ITM\reloj-inteligente\backups\factory_backup_32MB.bin"
```

### Opción B: Comando Manual con `esptool`
```powershell
python -m esptool --chip esp32s3 --port COM5 --baud 921600 write-flash --flash-mode qio --flash-freq 80m --flash-size 32MB 0x0 "G:\ITM\reloj-inteligente\backups\factory_backup_32MB.bin"
```

---

## 3. Guía Anti-Brick: ¿Qué hacer si el reloj no enciende o desaparece `COM5`?

El chip **ESP32-S3** tiene su bootloader ROM grabado en silicio de fábrica (en una memoria ROM interna de solo lectura que **es físicamente imposible de borrar**). Por lo tanto, **es imposible "brickear" permanentemente esta placa por software**.

### Síntoma común:
Subiste un programa nuevo y de repente:
* La pantalla se queda totalmente negra, **o**
* El reloj entra en un bucle de reinicio infinito (*bootloop*), **o**
* Windows deja de mostrar el puerto `COM5` o dice *"No se reconoce el dispositivo USB"*.

### Por qué ocurre:
1. El código nuevo tiene un fallo (por ejemplo, accedió a un puntero nulo o tocó `GPIO 35`) y el chip se reinicia cada pocos milisegundos, derribando la conexión USB nativa antes de que Windows pueda abrir el puerto.
2. O compilaste el programa con la opción `USB CDC On Boot: Disabled`.

### Solución Paso a Paso (Modo Descarga Forzado / Bootloader ROM):

1. Conecta el reloj por USB al PC.
2. **Mantén presionado el botón `BOOT`** (el botón lateral conectado a `GPIO 0`).
3. Sin soltar `BOOT`, **presiona y suelta brevemente el botón `PWR` / Reset** (o desconecta y vuelve a conectar el cable USB mientras mantienes presionado `BOOT`).
4. **Suelta el botón `BOOT`.**
5. En este estado, el ESP32-S3 no ejecuta ningún programa de la memoria Flash: se queda quieto en modo `DOWNLOAD_BOOT` esperando órdenes por USB.
   * *Nota:* En este estado la pantalla estará apagada (es normal, porque el AXP2101 no ha sido inicializado por software), pero en el Administrador de Dispositivos de Windows verás que el puerto **`COM5`** (o un nuevo puerto COM de `USB JTAG/serial debug unit`) permanece estable y sin desconectarse.
6. Sube un firmware funcional desde el IDE o ejecuta el comando de restauración o borrado:
   ```powershell
   python -m esptool --chip esp32s3 --port COM5 erase-flash
   ```
7. Una vez flasheado el código corregido, presiona el botón `PWR` o reconecta el USB para arrancar normalmente.
