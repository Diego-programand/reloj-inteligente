<#
.SYNOPSIS
    Realiza una copia de seguridad completa (32 MB) de la memoria Flash del reloj ESP32-S3.
.DESCRIPTION
    Conecta al puerto COM5 a 921600 baudios usando esptool, extrae los 32 MB (0x0 a 0x2000000),
    guarda el archivo en G:\ITM\reloj-inteligente\backups\ y calcula su hash SHA256.
#>

param(
    [string]$Port = "COM5",
    [int]$Baud = 921600,
    [string]$OutputName = "factory_backup_32MB.bin"
)

$ErrorActionPreference = "Stop"
$BackupDir = Join-Path $PSScriptRoot "..\backups"
if (-not (Test-Path $BackupDir)) {
    New-Item -ItemType Directory -Path $BackupDir -Force | Out-Null
}

$OutFile = Join-Path $BackupDir $OutputName

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " INICIANDO RESPALDO COMPLETO DE FLASH (32 MB) - ESP32-S3" -ForegroundColor Cyan
Write-Host " Puerto: $Port | Velocidad: $Baud baudios" -ForegroundColor Cyan
Write-Host " Archivo destino: $OutFile" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

python -m esptool --chip esp32s3 --port $Port --baud $Baud read-flash 0x0 0x2000000 "$OutFile"

if ($LASTEXITCODE -eq 0 -and (Test-Path $OutFile)) {
    $FileInfo = Get-Item $OutFile
    $Hash = (Get-FileHash -Path $OutFile -Algorithm SHA256).Hash
    Write-Host "`n[OK] Respaldo completado con exito!" -ForegroundColor Green
    Write-Host "Tamaño : $($FileInfo.Length) bytes" -ForegroundColor Green
    Write-Host "SHA256 : $Hash" -ForegroundColor Yellow
} else {
    Write-Error "Fallo al realizar el respaldo de la memoria Flash."
}
