# Animal Crossing Android V6.0 RC3 — candidata ARM64 + ARM32

Código 7389bda585ceeeba49f5c713152cf5e890053d8d; build Actions 39551447727. Base V5/PC 0.9.3. master intacta.

Corrige contratos OS/DVD y desbordamientos al recibir mensajes. La prueba del usuario confirmó ARM32 RC2; ARM64/universal RC2 se cerraban. RC3 conserva audio y módulos, pero su aceptación física sigue pendiente.

Instalar sobre RC1/RC2 sin desinstalar ni borrar datos (misma firma V6, versionCode=8). No reemplaza directamente una V5 de otro certificado. Certificado SHA-256: 265e4f32ba1e44b346e8361941a3e0f02f3549ea5e6d9bbecf29f6ccebdaeccc.

Si sigue cerrándose, reabrir y usar Copiar informe. Sólo se lee el historial de la propia app y se guarda localmente; no hay envío automático por red. Android puede haber descartado la traza.

El ZIP incluye 17 pruebas nativas por plataforma, 98 contratos de guardado y reproducciones de los defectos. Los inputs CI se firman sin modificar el payload. No hay claves privadas ni contraseñas. Símbolos DWARF con Build IDs verificados se conservan en el build Actions.

Candidata de pruebas, no versión estable. No se ha jugado en Android en este entorno ni validado pueblos/packs reales, controles, Mali o rendimiento. QEMU sólo ejecuta componentes Linux. La etiqueta v6.0-rc3-apks fija estas descargas.
