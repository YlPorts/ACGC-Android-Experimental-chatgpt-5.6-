# Animal Crossing Android V6.0 RC1 — estado técnico

Esta rama contiene una implementación nativa ARM64 y conserva la compilación
ARM32 sobre V5 / PC 0.9.3. Es una **candidata de desarrollo**, no una versión
estable ni una afirmación de que se haya jugado en Android.

Base: `d717e971bb684a36549ac9b2d7d3879a3a296fc5` de `master`.
SDL2: 2.30.11, submódulo `fa24d868ac2f8fd558e4e914c9863411245db8fd`.
La inspección anterior a las modificaciones está en [la auditoría inicial](V6_ARM64_AUDIT_ES.md).

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
- JSystem usa aritmética de direcciones nativa. Los bloques del heap mantienen
  alineación de 16 bytes. Las entradas RARC conservan exactamente 20 bytes y su
  campo reservado de cuatro bytes, incluso dentro del proceso de 64 bits.
- ARQ transporta MRAM sin truncamiento, conserva offsets ARAM y completa los
  campos de la solicitud antes de invocar su callback.
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
con la migración. ELF ARM64 y las bibliotecas sin comprimir del APK se comprueban
para páginas de 16 KB.

## Comprobaciones ejecutadas

| Comprobación | Resultado local | Alcance |
| --- | --- | --- |
| Motor + SDL2 con NDK, ARM32 | Compila y enlaza | Biblioteca real `libmain.so` |
| Motor + SDL2 con NDK, ARM64 | Compila y enlaza | ELF64 AArch64 auténtico |
| Tres variantes Gradle | Generadas | Universal, ARM64 y ARM32 |
| Paquete completo | Verificado | Todas las `.so`, ABI, dependencias, JNI, firma y zipalign |
| Layout persistente V5 | 98/98 en ambas ABI | Comparación con baseline ARM32 intacta |
| Serializador de partidas | Ida/vuelta cruzada ARM32 ↔ AArch64 | Payload válido sintético de 148128 bytes |
| Pruebas de componentes | 12 casos en host/ARM32/AArch64 | ASan/UBSan en host; ejecutables ARM bajo QEMU |
| Workflow | actionlint correcto | Además de compilación/configuración Gradle local |

Los doce casos cubren referencias concurrentes con direcciones superiores a 4 GB,
GBI estático/dinámico/binario, ARAM y callbacks, comandos de escenas, Multi5 y
SafeBoot con packs sintéticos, bancos NEOS, IBNK/WSYS originales incluidos en el
código, traducción de direcciones DOL, colas y copia DSP, NES, serializador y
entradas RARC/bloques del heap. La prueba NES ejecuta el **wrapper de producción**
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

## Compilación y firma

```sh
git clone --recurse-submodules https://github.com/YlPorts/ACGC-Android-Experimental-chatgpt-5.6-.git
cd ACGC-Android-Experimental-chatgpt-5.6-
git switch android-v6-arm64
cd android
./gradlew --no-daemon assembleUniversalDebug assembleArm64Debug assembleArm32Debug
```

En CI se usan los mismos secretos `ACGC_SIGNING_KEY` y
`ACGC_SIGNING_PASSWORD`, el mismo PKCS12 y el mismo alias `acgc-public` que V5.
El validador compara el certificado del APK con ese keystore. Una compilación de
distribución sin esos secretos falla; no publica un sustituto con otra firma.
Los PR sin secretos pueden compilar para revisión y sólo publican el informe.

Una compilación local sin las variables de firma usa el certificado de desarrollo
de Android. **No debe confundirse con una actualización de la APK ya instalada**.
Para actualizar conservando el pueblo se requiere el mismo certificado del APK
instalado. No se debe desinstalar la aplicación para sortear una discrepancia.

Products/artifacts de la candidata firmada por CI:

- `Animal-Crossing-Android-V6.0-Universal.apk`
- `Animal-Crossing-Android-V6.0-ARM64.apk`
- `Animal-Crossing-Android-V6.0-ARM32.apk`

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
