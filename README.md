Animal Crossing Android V6.0 RC5 — candidata, no estable. Base Android V5 / PC 0.9.3. `com.acgc.port`; versionCode 10. Se conserva la firma V6: instalar sobre RC1–RC4 sin desinstalar ni borrar datos. No sustituye directamente V5 de otra firma.

RC4 ARM64 ya carga en el Galaxy A15 / Android 16 del usuario, pero los modelos aparecen negros, los menús no se ven y falta la música. RC4 ARM32 no se ha probado. RC5 corrige un fallo reproducido: Gsetcolor usaba unsigned long y en LP64 leía el color desde el offset 8 en vez del word del offset 4. Con la cabecera RC4 el píxel esperado (180,120,60) es negro; con RC5 es correcto. Se conserva la iluminación y el motor real. También se corrigen los inmediatos GBI estáticos y el contrato GXColorS10.

La música real sigue pendiente de diagnóstico en el teléfono; no se declara corregida. Entrar al juego, esperar 15–30 segundos y usar **Atrás → Opciones → Sistema → Copiar diagnostico**. Pegar ese texto permite revisar las secuencias, bancos y picos del sintetizador/mezcla. El informe es local, limitado a 64 KiB; no hay envío automático. SafeBoot no se modifica ni se activa audio PAL por la presencia de archivos.

Comprobaciones: 20 pruebas de componentes en host ASan/UBSan, ARM32 y AArch64; 98 contratos de guardado V5 en ambas ABI; nueve comprobaciones gráficas en cada modo de shader mediante Mesa EGL/GLES, GX y Emu64 reales; síntesis S8/ADPCM con bucles y 15360 bytes PCM idénticos entre ARM32 y AArch64. Las pruebas Linux/QEMU y Mesa no sustituyen una partida Android ni la prueba física de Mali, datos y packs reales.

Código: c5572284bde60ef178ed95aa3a33a10982e0d540 en android-v6-arm64. master conserva d717e971bb684a36549ac9b2d7d3879a3a296fc5. Las APK firmadas conservan byte a byte el payload validado por Actions. El ZIP público incluye informes, hashes, certificado público y reproducción del fallo de color; nunca incluye claves privadas ni contraseñas.

Descargas fijadas por la etiqueta v6.0-rc5-apks:

- [ARM64](https://raw.githubusercontent.com/YlPorts/ACGC-Android-Experimental-chatgpt-5.6-/v6.0-rc5-apks/Animal-Crossing-Android-V6.0-ARM64.apk)
- [Universal](https://raw.githubusercontent.com/YlPorts/ACGC-Android-Experimental-chatgpt-5.6-/v6.0-rc5-apks/Animal-Crossing-Android-V6.0-Universal.apk)
- [ARM32](https://raw.githubusercontent.com/YlPorts/ACGC-Android-Experimental-chatgpt-5.6-/v6.0-rc5-apks/Animal-Crossing-Android-V6.0-ARM32.apk)
- [Validación](https://raw.githubusercontent.com/YlPorts/ACGC-Android-Experimental-chatgpt-5.6-/v6.0-rc5-apks/Animal-Crossing-Android-V6.0-Validation.zip)

[Informe técnico](https://github.com/YlPorts/ACGC-Android-Experimental-chatgpt-5.6-/blob/c5572284bde60ef178ed95aa3a33a10982e0d540/docs/V6_ARM64_REPORT_ES.md) · [Pruebas físicas pendientes](https://github.com/YlPorts/ACGC-Android-Experimental-chatgpt-5.6-/blob/c5572284bde60ef178ed95aa3a33a10982e0d540/docs/V6_DEVICE_TESTS_ES.md)

[Compilación y pruebas Actions](https://github.com/YlPorts/ACGC-Android-Experimental-chatgpt-5.6-/actions/runs/39620348697) satisfactorias. La verificación independiente de las firmas se registra en Actions tras publicar estos archivos.
