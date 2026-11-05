<#
.SYNOPSIS
    Lee la salida del puerto serie del reloj ESP32-S3 durante los segundos indicados.
.DESCRIPTION
    Permite inspeccionar logs de arranque o mensajes de depuración sin bloquear el puerto permanentemente.
#>

param(
    [string]$Port = "COM5",
    [int]$Baud = 115200,
    [int]$DurationSeconds = 5,
    [switch]$ResetDevice
)

try {
    $serial = New-Object System.IO.Ports.SerialPort $Port, $Baud, None, 8, one
    $serial.ReadTimeout = 1000
    $serial.Open()

    if ($ResetDevice) {
        Write-Host "[INFO] Reiniciando placa via RTS..." -ForegroundColor Cyan
        $serial.RtsEnable = $true
        Start-Sleep -Milliseconds 100
        $serial.RtsEnable = $false
        $serial.DtrEnable = $false
    }

    Write-Host "[INFO] Escuchando $Port a $Baud baudios durante $DurationSeconds segundos..." -ForegroundColor Green
    Write-Host "[INFO] Solicitud 'i' enviada al reloj (presiona ahora los botones PWR o BOOT del reloj)..." -ForegroundColor Yellow
    Write-Host "------------------------------------------------------------"
    $serial.WriteLine("i")
    
    $endTime = (Get-Date).AddSeconds($DurationSeconds)
    while ((Get-Date) -lt $endTime) {
        if ($serial.BytesToRead -gt 0) {
            Write-Host -NoNewline $serial.ReadExisting()
        }
        Start-Sleep -Milliseconds 50
    }
    Write-Host "`n------------------------------------------------------------"
    $serial.Close()
} catch {
    Write-Host "[ERROR] No se pudo abrir $Port : $($_.Exception.Message)" -ForegroundColor Red
}

