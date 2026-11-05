<#
.SYNOPSIS
    Restaura una copia de seguridad (.bin) de 32 MB en la memoria Flash del reloj ESP32-S3.
.DESCRIPTION
    Verifica la existencia del archivo .bin y lo escribe en la dirección 0x0 en modo QIO 80MHz.
#>

param(
    [string]$Port = "COM5",
    [int]$Baud = 921600,
    [string]$BackupFile = (Join-Path $PSScriptRoot "..\backups\factory_backup_32MB.bin")
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $BackupFile)) {
    Write-Error "No se encontro el archivo de respaldo en: $BackupFile"
    exit 1
}

$Hash = (Get-FileHash -Path $BackupFile -Algorithm SHA256).Hash
Write-Host "============================================================" -ForegroundColor Yellow
Write-Host " RESTAURANDO IMAGEN COMPLETA A LA MEMORIA FLASH" -ForegroundColor Yellow
Write-Host " Puerto  : $Port" -ForegroundColor Yellow
Write-Host " Archivo : $BackupFile" -ForegroundColor Yellow
Write-Host " SHA256  : $Hash" -ForegroundColor Yellow
Write-Host "============================================================" -ForegroundColor Yellow

python -m esptool --chip esp32s3 --port $Port --baud $Baud write-flash --flash-mode qio --flash-freq 80m --flash-size 32MB 0x0 "$BackupFile"

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n[OK] Restauracion finalizada exitosamente!" -ForegroundColor Green
} else {
    Write-Error "Hubo un error durante la restauracion. Si el puerto COM desaparecio, entra en modo BOOT (Mantener BOOT + presionar PWR)."
}

