# Registro de Copias de Seguridad (Backups de Memoria Flash)

En esta carpeta se almacenan los volcados binarios (`.bin`) extraídos de la memoria Flash de 32 MB del reloj.
> *Nota:* Los archivos `.bin` pesados están excluidos de Git en `.gitignore` para mantener el repositorio ligero, pero se conservan localmente en este directorio (`G:\ITM\reloj-inteligente\backups`).

---

## Tabla de Backups Registrados

| Fecha | Nombre de Archivo | Tamaño | Offset | Hash SHA256 | Notas |
| :--- | :--- | :--- | :--- | :--- | :--- |
| *Pendiente* | `factory_backup_32MB.bin` | `33,554,432` bytes (32 MB) | `0x00000000` | *(Se registrará al ejecutar `scripts\backup_flash_32mb.ps1`)* | Imagen original de fábrica (`ESP-Brookesia phone demo`) |

---

## Cómo Generar o Restaurar un Backup

* **Crear un backup nuevo:**
  ```powershell
  G:\ITM\reloj-inteligente\scripts\backup_flash_32mb.ps1
  ```
* **Restaurar un backup existente:**
  ```powershell
  G:\ITM\reloj-inteligente\scripts\restore_flash_32mb.ps1 -BackupFile "G:\ITM\reloj-inteligente\backups\factory_backup_32MB.bin"
  ```
