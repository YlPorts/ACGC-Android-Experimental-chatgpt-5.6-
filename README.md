# Animal Crossing Android V6.0 RC1 — APK candidatas

Archivos firmados de la compilación Actions 39543289217, código fuente c479cd58d4a4df59ef7fdcd40b1939f322219c8f.

Candidata experimental; no se ha jugado en Android, Mali ni con pueblos reales V4/V5. No es una versión estable. Ver el informe y las pruebas pendientes en la rama android-v6-arm64.

Las tres APK usan una clave persistente V6 nueva autorizada por el propietario. No actualizan directamente una V5 de otra firma: verificar copias externas del pueblo/configuración y preferencias privadas antes de cambiar de instalación.

Certificado público SHA-256: 265e4f32ba1e44b346e8361941a3e0f02f3549ea5e6d9bbecf29f6ccebdaeccc.

SHA256SUMS permite verificar los APK. El ZIP contiene resultados de 13 tests de componentes en host/ARM32/AArch64, 98 contratos de guardado, firma y comparación byte a byte con CI. No contiene claves privadas ni contraseñas. Los inputs de Actions se han firmado sin modificar los archivos de la aplicación.

Esta rama separada sólo entrega archivos de prueba; no modifica master ni incorpora los binarios al historial de la rama de código fuente. La etiqueta v6.0-rc1-apks fija estas descargas.
