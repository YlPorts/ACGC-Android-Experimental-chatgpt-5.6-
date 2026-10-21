# Animal Crossing Android V6.0 RC2 — estado técnico

Esta rama contiene una implementación nativa ARM64 y conserva la compilación
ARM32 sobre V5 / PC 0.9.3. Es una **candidata de desarrollo**, no una versión
estable ni una afirmación de que se haya jugado en Android.

Base: `d717e971bb684a36549ac9b2d7d3879a3a296fc5` de `master`.
SDL2: 2.30.11, submódulo `fa24d868ac2f8fd558e4e914c9863411245db8fd`.
La inspección anterior a las modificaciones está en [la auditoría inicial](V6_ARM64_AUDIT_ES.md).

## Corrección del cierre al iniciar en RC1

El usuario confirmó que RC1 se cierra al iniciar el juego en los tres productos.
Se ha retirado esa candidata y se ha reproducido una regresión común a ambas ABI:
`pc_neos_init_sync()` carga `/audiorom.img` mediante `DVDT_LoadtoARAM_Main()`,
que llama a `ARQPostRequest()` sin un objeto de solicitud. RC1 escribía los campos
de ese objeto nulo antes de copiar el audio. UBSan identifica la escritura nula
en `pc/src/pc_aram.c`, con la cola DVD original en la traza.

RC2 sólo rellena esos campos cuando existe una solicitud y sigue ejecutando la
transferencia completa y su callback. No desactiva el audio ni SafeBoot. También
corrige `DVDT_CheckPass()`: una tarea de proceso copiaba 88 bytes fijos y perdía
sus punteros finales en ARM64; ahora copia `sizeof(DVDCall)`. En ARM32 ese tamaño
sigue siendo 88 bytes. El callback PC usa además la firma correcta de la cola.

La nueva prueba `dvd_audio` compila la cola DVD, el cargador de audio, la copia
de tareas, ARQ y DMA **de producción**. Sólo sustituye el backend del disco por
bytes sintéticos, sin incluir archivos del juego. Verifica una carga con dos
bloques y cola diferida, la cola final alineada a 32 bytes, ambas direcciones
MRAM/ARAM, estados/callbacks, la barrera de finalización y archivo ausente.
Antes de la corrección falla en la escritura nula; después comprueba los bytes
transferidos. Esto no equivale a haber arrancado una partida Android.

La candidata usa `versionCode=7`, `versionName=6.0-rc2-pc0.9.3` y el mismo
certificado V6. Puede reemplazar RC1 firmada con ese certificado sin desinstalar
ni borrar datos. Las etiquetas RC1 permanecen inmutables para diagnóstico.

## Cambios del motor

- `u32` y `s32` siguen midiendo cuatro bytes. En LP64 dejan de depender de
  `long`; la identidad original de los typedef de ARM32 se conserva.
- Direcciones del proceso, rangos ELF, arenas, alineación, callbacks y DMA usan
  `uintptr_t`/`intptr_t`. `pc_image_base`/`pc_image_end` analizan ELF32 o ELF64
  según el proceso real.
- `pc_address` mantiene referencias opacas de 32 bits para campos transitorios
  que necesitan conservar su tamaño. La dirección completa se almacena aparte;
  los identificadores no se reciclan al llenar una tabla pequeña. No se escriben
  en partidas, archivos ROM ni packs. Los offsets de ARAM, disco y GameCube
  siguen siendo enteros de 32 bits.
- GBI conserva comandos compilados de ocho bytes en ARM32 y usa comandos nativos
  de 16 bytes en ARM64. Vértices y matrices siguen midiendo 16 y 64 bytes.
  Las tres listas gráficas binarias del calendario/diseñador se convierten desde
  sus comandos BE de ocho bytes; no se reinterpretan con otro stride. También
  se corrigen las vistas de punteros en los comandos de escenas compilados.
- GX conserva sus objetos compactos y recupera las direcciones de imágenes/TLUT
  mediante referencias. No se eliminan shaders, rutas GLES, DDS ni modos de
  aspecto. No se han probado en una GPU Mali.
- JSystem usa aritmética de direcciones nativa. En ARM64, los bloques y la granularidad de asignación del heap usan 16 bytes;
  ARM32 conserva sus encabezados y granularidad originales de cuatro bytes. Las entradas RARC conservan exactamente 20 bytes y su
  campo reservado de cuatro bytes, incluso dentro del proceso de 64 bits.
- ARQ transporta MRAM sin truncamiento, conserva offsets ARAM, admite las
  transferencias síncronas sin solicitud del cargador original y completa los
  campos de las solicitudes existentes antes de invocar su callback.
- Los bancos de audio NEOS y los recursos IBNK/WSYS se decodifican desde bytes BE
  inmutables a objetos nativos. Se conservan referencias compartidas y se validan
  offsets, límites de muestras, bucles y la capacidad real de los libros ADPCM.
  Una carga inválida no se marca como un banco listo para usar.
- Los paquetes DSP siguen midiendo ocho bytes; sus operandos de memoria usan
  referencias. Las colas de comandos, callbacks y la salida AI transportan
  punteros completos. Se corrige también el búfer de audio del NES integrado.
- El backend fixNES, la lógica de actores, muebles, insectos, escenas, audio y
  las interfaces existentes permanecen en la biblioteca. No se introduce un
  emulador GameCube, WebView ni un motor alternativo.

Las rutas antiguas de acceso directo a direcciones GameCube se separan de las
nativas. Para parches de recursos DOL se utiliza una traducción con límites y
orden de bytes. Las direcciones que no corresponden a un recurso registrado se
notifican. Esto **no acredita soporte de parches arbitrarios de instrucciones
PowerPC** de packs NES externos; esa compatibilidad sigue pendiente de revisar
con ejemplos reales.

## Datos, traducciones y Android

No se cambia `com.acgc.port`, la organización de carpetas, las preferencias Java,
la interfaz táctil ni los formatos de packs Multi5. Se conservan
`languages/{es,fr,de,it}/{aram,items,graphics}` y `ui_strings.ini`.

SafeBoot sigue exigiendo `audio/enable_pal_audio.flag` y una pareja válida de
`audiorom.img`/`audio_headers.bin`. Encontrar archivos regionales no activa el
sonido PAL. Los snapshots/restauraciones USA consideran el tamaño nativo de los
encabezados; el formato externo de cada entrada permanece en 16 bytes.

Los guardados se protegen con 98 contratos obtenidos de la V5 ARM32 intacta:
tamaños y offsets de `Save_t`, `Save`, `CARDDir` y sus componentes. Esos contratos
se compilan dentro del motor y se comprueban con el NDK para ambas ABI. No se
introduce una conversión de pueblo entre arquitecturas.

Toolchain: JDK 17, Gradle 8.11.1 con checksum, AGP 8.9.2, NDK r28c
`28.2.13676358`, CMake 3.22.1, compileSdk 35, minSdk 23 y targetSdk 34. Se mantiene
el target anterior para no introducir un cambio de permisos/ciclo de vida junto
con la migración. Los segmentos ELF ARM64 se comprueban para páginas de 16 KB y
todos los APK pasan `zipalign -c -P 16`. Se conserva `extractNativeLibs=true`: en
los APK comprobados las bibliotecas están comprimidas y Android las extrae al
instalar; no se afirma que se carguen directamente desde el ZIP.

## Comprobaciones ejecutadas

| Comprobación | Resultado local | Alcance |
| --- | --- | --- |
| Motor + SDL2 con NDK, ARM32 | Compila y enlaza | Biblioteca real `libmain.so` |
| Motor + SDL2 con NDK, ARM64 | Compila y enlaza | ELF64 AArch64 auténtico |
| Tres variantes Gradle | Generadas | Universal, ARM64 y ARM32 |
| Paquete completo | Verificado | Todas las `.so`, ABI, dependencias, JNI, firma y zipalign |
| Layout persistente V5 | 98/98 en ambas ABI | Comparación con baseline ARM32 intacta |
| Serializador de partidas | Ida/vuelta cruzada ARM32 ↔ AArch64 | Payload válido sintético de 148128 bytes |
| Pruebas de componentes | 14 casos en host/ARM32/AArch64 | ASan/UBSan en host; ejecutables ARM bajo QEMU |
| Workflow | actionlint correcto | Además de compilación/configuración Gradle local |

Los catorce casos cubren referencias concurrentes con direcciones superiores a 4 GB,
GBI estático/dinámico/binario, ARAM y callbacks, la ruta DVD de carga del audio de arranque, comandos de escenas, Multi5 y
SafeBoot con packs sintéticos, bancos NEOS, IBNK/WSYS originales incluidos en el
código, traducción de direcciones DOL, colas y copia DSP, NES, serializador y
entradas RARC y asignación/redimensionado/liberación real del heap con tamaños y alineaciones variados. La prueba NES ejecuta el **wrapper de producción**
y su CPU/PPU/APU con una ROM NROM escrita para la prueba: comprueba cálculo,
controlador, WRAM, generación de framebuffer y entrega de audio con su puntero
completo. El test no invoca el render GLES; proporciona un adaptador para la restauración
del estado gráfico al salir.

QEMU ejecuta únicamente estos tests Linux de componentes. No se usa para correr
el motor ARM32 dentro de la aplicación. Los APK contienen código NDK nativo de
cada arquitectura.

Los resultados estructurados se generan con los scripts de `tools/validation/`
y se adjuntan como artifacts de Actions. `check_apk.py` comprueba también
`SDL_main`, los dos JNI propios, los JNI de inicialización SDL y la resolución de
importaciones contra las bibliotecas empaquetadas y los stubs NDK de API 23.
Además lee las clases Java realmente compiladas mediante `javap` y exige
exportaciones para sus **54 métodos nativos**, incluyendo entrada, audio, mandos
e HID. Esta comprobación pasa en las tres candidatas locales; no equivale a
ejecutar esos dispositivos.

## Compilación y firma

```sh
git clone --recurse-submodules https://github.com/YlPorts/ACGC-Android-Experimental-chatgpt-5.6-.git
cd ACGC-Android-Experimental-chatgpt-5.6-
git switch android-v6-arm64
cd android
./gradlew --no-daemon assembleUniversalDebug assembleArm64Debug assembleArm32Debug
```

El propietario autorizó crear **una clave persistente nueva para V6** al no estar
disponible la anterior. Las tres candidatas locales usan el mismo PKCS12 y alias
`acgc-public`. Certificado público SHA-256:

```text
265e4f32ba1e44b346e8361941a3e0f02f3549ea5e6d9bbecf29f6ccebdaeccc
```

**Esta firma no permite actualizar directamente la V5 de otra firma.** Antes de
cambiar de instalación deben verificarse copias externas del pueblo, configuración
y preferencias privadas. El [protocolo de dispositivo](V6_DEVICE_TESTS_ES.md)
explica la diferencia entre actualización con la misma firma y migración de
firma. No desinstalar la V5 con la única copia de esos datos dentro de la app.

El workflow acepta `ACGC_SIGNING_KEY` (PKCS12 en base64) y
`ACGC_SIGNING_PASSWORD`. No genera claves efímeras por variante ni sube claves,
contraseñas o keystores a artifacts, cachés o al repositorio. El validador compara
el certificado del APK con el keystore configurado.

La integración GitHub de esta sesión recibe HTTP 403 al solicitar la clave pública
de la API de secretos; no puede instalar esos dos secretos. La clave nueva se
conserva fuera del repositorio, con permisos privados, para copia segura y
configuración por el propietario. No basta con que un build tenga firma válida:
para futuras actualizaciones debe mantenerse exactamente este certificado.

Sin ambos secretos, CI compila y valida **inputs sin firmar**, publicados sólo como
`V6-unsigned-{Universal,ARM64,ARM32}-build`, con sufijo `-unsigned.apk`,
`release_status=unsigned-build` y `signature_verified=false`. No son instalables
ni se consideran candidatas distribuidas. Un secreto incompleto hace fallar el
workflow. Con ambos secretos, CI firma y verifica los tres productos normales.
Una compilación local ordinaria sin variables usa el certificado de desarrollo;
`ACGC_UNSIGNED_BUILD=1` solicita explícitamente el modo sin firma.

Cuando no se pueden configurar secretos, el workflow manual
`Verify externally signed V6 candidates` permite verificar una firma efectuada
fuera de Actions sin transferir la clave privada. Requiere una ejecución V6
correcta y una prerelease `v6.0-rcN` cuyo tag apunte al mismo commit. Descarga los
inputs de esa ejecución y las APK firmadas, compara todos los archivos de la
aplicación byte a byte (sólo permite añadir metadatos de firma), exige el
certificado V6 anterior y comprueba firma/zipalign. Sólo entonces sube los tres
artifacts con sus nombres normales. Este paso no convierte la candidata en
estable ni sustituye las pruebas de dispositivo. La comparación se ha probado
localmente con un input real de Actions y rechaza cambios de `classes.dex` o un
certificado distinto.
Se puede iniciar desde el workflow principal ya registrado, pasando los inputs
manuales `signed_build_run_id` y `signed_release_tag`; sin esos inputs se ejecuta
la compilación normal.

La subida directa a `uploads.github.com` también devuelve HTTP 401 en esta
integración. Para entregar los archivos se admite una rama separada de artifacts,
con etiqueta fija `v6.0-rcN-apks` y enlaces desde la prerelease. El verificador
elige esa etiqueta sólo cuando el APK no figura como asset de la release; una
descarga anunciada que falla no se ignora. La comparación completa con CI y el
certificado exigido son iguales en ambos casos. No se guardan APK binarios en la
rama del código fuente ni se publica material privado de firma.

Productos de la candidata firmada:

- `Animal-Crossing-Android-V6.0-Universal.apk`
- `Animal-Crossing-Android-V6.0-ARM64.apk`
- `Animal-Crossing-Android-V6.0-ARM32.apk`

El wrapper de la aplicación es el oficial de Gradle 8.11.1, verificado contra su
SHA-256 publicado. El wrapper no ejecutado que acompaña a SDL se verifica contra
el hash del commit upstream fijado; la validación completa sólo permite esa
excepción explícita para la dependencia, no wrappers desconocidos nuevos.

El workflow no modifica `master` ni publica una release estable automáticamente.
Los workflows manuales de 32 y 64 bits llaman a la misma validación con su ABI
real; el antiguo workflow cuyo nombre decía 64 bits pero seleccionaba ARM32 se
corrige.

## Aceptación pendiente antes de una versión estable

No hay una imagen ISO/GCM/CISO, partidas reales V4/V5, packs externos completos,
dispositivo Android, mando ni GPU Mali disponibles en este entorno. Por tanto:

- No se ha arrancado el juego hasta entrar al pueblo ni se ha jugado en Android.
- No se han abierto/escrito GCI reales de versiones anteriores. La prueba cruzada
  usa el serializador real con datos sintéticos válidos; no equivale a una tarjeta
  de memoria ni a un pueblo real.
- No se han verificado diálogos completos, inventario, NPC, calendario/eventos,
  isla, tiendas, transiciones, gráficos DDS/HD ni todos los juegos/mappers NES.
- Los tests Multi5 verifican rutas, direcciones virtuales y fallbacks; no validan
  todas las conversaciones y recursos de packs PAL V4/V5 reales.
- Controles táctiles/Bluetooth/USB, SAF y permisos persistentes, pausa/reanudación,
  bloqueo de pantalla, rotación, Android 16 y páginas físicas de 16 KB requieren
  pruebas de dispositivo. Conservar código Java no demuestra que hayan pasado.
- No hay mediciones de FPS, tiempos de fotograma, consumo, batería ni sincronía
  audiovisual de una partida. Se conservan las opciones existentes; no se anuncia
  una mejora de rendimiento no medida.

La candidata debe superar el protocolo de [pruebas de dispositivo](V6_DEVICE_TESTS_ES.md)
en ARM64 y ARM32 antes de sustituir a V5 o presentar la universal como estable.
