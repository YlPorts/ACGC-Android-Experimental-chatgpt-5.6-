# Auditoría inicial V6 ARM64

Base inspeccionada: `master` / `d717e97`, PC v0.9.3-playtest
`4099d246c927e75b4fd342ca13f4ac4395c55af5`, SDL2 2.30.11
`fa24d868ac2f8fd558e4e914c9863411245db8fd`. Clon completo con submódulos.
Trabajo aislado en `android-v6-arm64`; no se modifica master.

## Obstáculos comprobados antes de cambiar el motor

- `pc/CMakeLists.txt` rechaza sizeof(void*) == 8. `pc_platform.h`, GBI y
  `emu64_utility.c` también exigen punteros de cuatro bytes.
- Gradle sólo selecciona armeabi-v7a. El workflow android-64 no cambia ese filtro
  y sólo se ejecuta en arm64-port: su nombre no acredita una compilación AArch64.
- Cuatro encabezados definen u32/s32 con long. En Android LP64 miden ocho bytes:
  hay que preservar los datos de cuatro bytes sin ensanchar archivos ni partidas.
- GBI mezcla referencias segmentadas originales, punteros estáticos y punteros
  dinámicos etiquetados. La tabla de punteros impares tiene sólo 8192 entradas y
  reutiliza identificadores. No permite representar punteros AArch64 generales.
- pc_image_base/end truncan direcciones y pc_main interpreta ELF como Elf32.
- JSystem, libc64/__osMalloc, GXTexObj/GXTlutObj, las transferencias ARAM y DMA de
  audio transportan direcciones nativas mediante u32. No basta con quitar asserts.
- Los bancos de audio contienen offsets/punteros serializados de cuatro bytes.
  Las estructuras smzwavetable, wtstr, voicetable y perctable tienen punteros
  nativos y se reinterpretan sobre esos bytes: necesitan una conversión explícita.
- Los tamaños y offsets de Save_t, Save, CARDDir y bloques ARAM persistentes
  deben contrastarse entre ABI antes de permitir la escritura de partidas.

## Funciones que deben conservarse

La biblioteca main incluye motor original, JSystem, jaudio_NES, emu64 (traductor
gráfico del port), fixNES, disco ISO/GCM/CISO, tarjetas GCI, DDS, ajustes y Multi5.
SDL2 y libc++ son dependencias compartidas; GLESv3, OpenSLES, android, log, dl y m
son bibliotecas de plataforma. JNI propio: nativeTakePendingActions y
nativeGetAndroidSetting; SDLActivity usa SDL_main y los JNI de SDL2.

El lanzador, SAF, sincronización, controles y ciclo de vida están en GameActivity,
GameFolderManager, LauncherActivity y VirtualControllerView. com.acgc.port y las
rutas actuales son parte del contrato de actualización.

Multi5 reside en pc_language.c/pc_language_graphics.c y languages/{es,fr,de,it}.
Sus direcciones ARAM virtuales son identificadores de 32 bits, no punteros
nativos. SafeBoot requiere audio/enable_pal_audio.flag y valida la pareja de
audiorom.img/audio_headers.bin; la mera presencia de archivos no activa PAL.

## Entorno y evidencia

El entorno inicial no contiene SDK/NDK, dispositivo Android, imagen de juego,
guardados ni packs de idiomas. Se preparan herramientas reales para compilar.
La configuración nativa de 64 bits original se detiene en el rechazo de CMake;
log inicial: /workspace/baseline-host64.log. Se registrarán por separado las
compilaciones Android y las pruebas ejecutadas. Ninguna prueba de compilación
acredita entrar al pueblo, audio, Mali, controles físicos o partidas reales.
