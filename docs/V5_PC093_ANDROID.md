# Animal Crossing Android V5.0 — PC upstream v0.9.3

## Base y seguridad
- Base oficial: flyngmt/ACGC-PC-Port `v0.9.3-playtest` (`4099d246c927e75b4fd342ca13f4ac4395c55af5`).
- Fusionada con Android V4 R3 SafeBoot (`d7040a4f8b9498ca1a454069899954a684371822`) mediante commit de dos padres.
- Preservados Android 32-bit armeabi-v7a, SDL2/OpenGL ES 3, controles multitáctiles, selector de carpeta SAF, guardado y texturas DDS.
- Preservado soporte Multi5: idiomas en `languages/<code>/` (`es`, `fr`, `de`, `it`). Los packs no se distribuyen en este repositorio.
- Audio regional PAL opt-in, inactivo por defecto. No activar `enable_pal_audio.flag` en uso normal.

## Cambios incorporados de PC 0.9.3
- Mejoras de cámara exterior y accesos rápidos de herramientas.
- Arreglos de temporización, pesca, objetos y animaciones.
- Corregidos bugs de audio, eventos, inventario y diálogos.
- Opción de filtrado de texturas integrada también en la pestaña Vídeo de Android.

## Pendiente de prueba
- Verificar GitHub Actions APK `armeabi-v7a` y firma.
- Verificar arranque en dispositivo con Animal Crossing USA Rev 0 (GAFE01_00).
- Verificar que el menú de idiomas, textos PAL, guardados, controles virtuales y texturas personalizados funcionan.
- No afirmar estabilidad final hasta haber completado pruebas reales en dispositivo.
