# Animal Crossing Android V6.0 RC4 — candidata ARM64 + ARM32

Código 71a9b2d7b3d2aa048fa3f0d59ac61e8e5c0b9d6d; build Actions 39555811821. Base V5/PC 0.9.3. master intacta.

Corrige el cierre ARM64 registrado en RC3: el cálculo fijo de 32 bits hacía fallar la creación del heap del sistema y después se accedía al objeto nulo. El Build ID del informe del A15 coincide con los símbolos CI. Se reproduce el exceso de reserva de 144 bytes en AArch64; ARM32 pasa. RC4 usa la capacidad real después de crear el heap raíz. No cambia el cálculo ARM32 ni elimina módulos.

Instalar sobre RC1/RC2/RC3 sin desinstalar ni borrar datos (misma firma V6, versionCode=9). No reemplaza directamente una V5 de otro certificado. Certificado SHA-256: 265e4f32ba1e44b346e8361941a3e0f02f3549ea5e6d9bbecf29f6ccebdaeccc.

El ZIP incluye 18 pruebas nativas por plataforma, 98 contratos de guardado, simbolización del cierre RC3 y reproducciones antes/después. Los inputs CI se firman sin modificar el payload; la universal contiene las mismas bibliotecas que las individuales. No hay claves privadas ni contraseñas. Los símbolos DWARF con Build IDs verificados se conservan en Actions.

Candidata de pruebas, no estable. ARM32 RC2 fue confirmada por el usuario; RC4 requiere prueba física de ambas ABI, partida, pueblos/packs reales, controles, Mali y rendimiento. No se ha jugado en Android en este entorno. QEMU sólo ejecuta componentes Linux. La etiqueta v6.0-rc4-apks fija estas descargas.

Si vuelve a cerrarse, reabrir y usar Copiar informe. La recepción del informe RC3 confirma esa ruta en el A15. Se lee el historial de la propia app y se guarda localmente; no hay envío automático por red.
