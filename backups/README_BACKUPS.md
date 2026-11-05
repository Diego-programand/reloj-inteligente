# Registro de Copias de Seguridad (Backups de Memoria Flash)

En esta carpeta se almacenan los volcados binarios (`.bin`) extraídos de la memoria Flash de 32 MB del reloj.
> *Nota:* El respaldo maestro `factory_backup_32MB.bin` (32 MB) está incluido en el repositorio Git (`!backups/factory_backup_32MB.bin` en `.gitignore`) para que esté disponible automáticamente al clonar el proyecto en cualquier PC.

---

## Tabla de Backups Registrados

| Fecha | Nombre de Archivo | Tamaño | Offset | Hash SHA256 | Notas |
| :--- | :--- | :--- | :--- | :--- | :--- |
| 2026-11-05 | `factory_backup_32MB.bin` | `33,554,432` bytes (32 MB) | `0x00000000` | `3883DB5C407AD1ABD9B60942B42ABF30636206DDA3402ACA5D38960B40214D30` | Imagen original de fábrica (`ESP-Brookesia phone demo`) |

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

