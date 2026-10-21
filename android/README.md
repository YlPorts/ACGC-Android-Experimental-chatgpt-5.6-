# Animal Crossing Android V6.0 RC5

Candidata de desarrollo del port nativo SDL2/GLES, basada en Android V5 y PC
0.9.3. Compila el motor real para ARM64 y ARM32. No incluye una imagen de disco.
**Candidata: RC4 ya carga en el teléfono, pero tiene fallos gráficos y no suena la música. RC5 corrige un fallo reproducido del layout de colores ARM64; imagen y música requieren validación en el dispositivo.**

Instalar sobre RC1–RC4 sin desinstalar ni borrar datos: RC5 mantiene la firma V6 y sube a `versionCode=10`. Para investigar la música, usar **Atrás → Opciones → Sistema → Copiar diagnostico** después de entrar al juego.

## Productos y herramientas

| Variante | ABI |
| --- | --- |
| Universal | `armeabi-v7a` y `arm64-v8a` |
| ARM64 | `arm64-v8a` |
| ARM32 | `armeabi-v7a` |

Se conserva `com.acgc.port`, OpenGL ES 3.0 y minSdk 23 (Android 6.0).
JDK 17, SDK 35, build-tools 35.0.0, NDK r28c `28.2.13676358`, CMake 3.22.1,
Gradle 8.11.1 y SDL2 2.30.11 están fijados en la configuración.

```sh
git clone --recurse-submodules https://github.com/YlPorts/ACGC-Android-Experimental-chatgpt-5.6-.git
cd ACGC-Android-Experimental-chatgpt-5.6-
git switch android-v6-arm64
cd android
./gradlew --no-daemon --max-workers=3 assembleUniversalDebug assembleArm64Debug assembleArm32Debug
```

Los APK aparecen en `app/build/outputs/apk/{universal,arm64,arm32}/debug/`.
Android Studio puede abrir esta carpeta y seleccionar cualquiera de esas
variantes. El motor se compila con las opciones de optimización originales del
port; estas candidatas mantienen la depuración Android para investigar fallos.

## Datos y controles existentes

El selector SAF concede acceso persistente a la carpeta de juego. Las imágenes
USA Rev 0 (`GAFE01_00`) ISO/GCM/CISO se abren mediante descriptor de archivo; no se
copian enteras al almacenamiento privado. La estructura externa sigue siendo:

```text
roms/             # imagen principal
roms/nes/         # ROMs adicionales
textures/         # DDS
languages/        # es, fr, de, it y formatos Multi5 anteriores
saves/            # partidas sincronizadas
config/           # settings.ini y keybindings.ini
cache/
```

Los recursos pequeños se reflejan en el directorio privado del motor. Las
partidas y configuración se sincronizan con la carpeta externa. Las preferencias
Android y las posiciones de controles también usan `shared_prefs` privado; una
copia de `config/` no respalda por sí sola esas preferencias.

Permanecen los controles multitáctiles, stick/C-stick/cruceta, A/B/X/Y, L/R/Z,
START, edición de posiciones, tamaño/opacidad/ocultación y soporte SDL de mandos.
Se conservan los menús existentes, DDS, modos de aspecto y opciones de FPS.
Los packs Multi5 mantienen sus formatos y SafeBoot no activa audio PAL por la
mera presencia de archivos. Conservar estas rutas de código no acredita aún su
funcionamiento completo en ARM64: requieren la aceptación de dispositivo.

## Firma y actualización

El propietario autorizó una **clave persistente nueva** para las tres candidatas
V6. Su certificado SHA-256 es:

```text
265e4f32ba1e44b346e8361941a3e0f02f3549ea5e6d9bbecf29f6ccebdaeccc
```

Android no permite una actualización directa de una V5 firmada con otra clave.
Verificar copias externas del pueblo/configuración y de las preferencias
privadas antes de cambiar de instalación. No desinstalar con la única copia del
pueblo dentro de la app. Consultar el [protocolo de migración y pruebas](../docs/V6_DEVICE_TESTS_ES.md).

Gradle usa `ACGC_KEYSTORE_PATH` y `ACGC_KEYSTORE_PASSWORD`, con PKCS12 y alias
`acgc-public`. Sin esas variables, el build local ordinario lleva firma de
desarrollo; no es intercambiable con la candidata firmada. `ACGC_UNSIGNED_BUILD=1`
produce explícitamente un input sin firma.

## GitHub Actions y alcance de las comprobaciones

El workflow `Animal Crossing Android V6.0 candidates` prueba componentes, layouts
persistentes y serialización cruzada, y compila las tres variantes. Comprueba
todas las bibliotecas empaquetadas, ABI/ELF, dependencias, los 54 métodos JNI de
las clases compiladas, alineación ELF64 de 16 KB, zipalign y estado de firma.
Los workflows manuales de 32/64 bits reutilizan esa validación.

Con los secretos `ACGC_SIGNING_KEY` (PKCS12 en base64) y
`ACGC_SIGNING_PASSWORD`, los artifacts firmados contienen:

- `Animal-Crossing-Android-V6.0-Universal.apk`
- `Animal-Crossing-Android-V6.0-ARM64.apk`
- `Animal-Crossing-Android-V6.0-ARM32.apk`

La integración de esta sesión no puede configurar secretos GitHub (HTTP 403).
Sin ambos secretos CI publica únicamente inputs `-unsigned.apk` en artifacts
`V6-unsigned-…-build`, con `signature_verified=false`. No son instalables ni se
confunden con las candidatas firmadas en el entorno local. Una configuración de
secretos incompleta falla. No se generan claves temporales por variante ni se
publica material privado de firma.

Para una clave mantenida fuera de GitHub, el workflow manual
`Verify externally signed V6 candidates` compara las APK de una prerelease
`v6.0-rcN` con los inputs de una ejecución V6 correcta del mismo commit. Verifica
que sólo se añadieron firmas, exige el certificado V6 y sube las tres candidatas
como artifacts. No recibe claves privadas ni permite cambiar el código compilado.
Puede iniciarse desde el workflow principal con los inputs manuales
`signed_build_run_id` y `signed_release_tag`. Dejarlos vacíos compila normalmente.
Si la integración rechaza la subida de assets, los archivos firmados pueden
publicarse desde una rama separada con etiqueta `v6.0-rcN-apks`; la prerelease
enlaza las descargas y Actions exige la misma firma y contenido CI exacto.

Las pruebas QEMU son ejecutables Linux de componentes; los APK usan código NDK
nativo de cada ABI. No se dispone de teléfono, GPU Mali, imagen de juego, pueblos
V4/V5 ni packs externos completos para verificar una partida o medir rendimiento.
El [informe técnico](../docs/V6_ARM64_REPORT_ES.md) distingue resultados comprobados
y aceptación pendiente.
