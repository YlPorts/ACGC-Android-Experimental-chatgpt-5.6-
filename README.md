# Animal Crossing Android V6.0 RC2 — APK candidatas

Corrige el cierre de RC1 al iniciar la carga del audio en ARM32 y ARM64, y una tarea DVD de tamaño incorrecto en ARM64. No desactiva audio ni módulos.

Código: 35de8220fa5e85d55df0570506c17957d8be96e6. Build Actions: 39547534898. Base PC 0.9.3/V5. master permanece intacta.

Las tres APK conservan la firma de RC1 y usan versionCode=7, versionName=6.0-rc2-pc0.9.3. Se instalan sobre RC1 sin desinstalar ni borrar datos. No actualizan directamente una V5 de otro certificado.

Certificado público SHA-256: 265e4f32ba1e44b346e8361941a3e0f02f3549ea5e6d9bbecf29f6ccebdaeccc. No hay claves privadas ni contraseñas en esta rama.

Los inputs exactos de Actions se firman fuera de GitHub. Firma, ABI, dependencias, 54 métodos JNI, alineación y payload completo se verifican. SHA256SUMS identifica los bytes distribuidos. El ZIP contiene los informes de 14 pruebas de componentes por plataforma, 98 contratos persistentes, pruebas sintéticas de guardado cruzado y la reproducción de la regresión antes de corregirla.

No se ha probado RC2 físicamente en Android/Mali ni con pueblos o packs reales. Es una candidata, no una versión estable. La prueba del cargador de audio usa el código de producción con un backend de disco sintético; no es una partida.

Esta rama separada sólo entrega archivos. La etiqueta v6.0-rc2-apks fija los APK. RC1 está retirada; sus etiquetas se conservan para diagnóstico.
