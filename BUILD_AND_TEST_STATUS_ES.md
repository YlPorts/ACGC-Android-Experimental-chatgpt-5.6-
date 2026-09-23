# Estado de compilación y validación

Fecha de revisión: 4 de agosto de 2026.

## Validado en este paquete

- El lanzador Android tiene una sola acción: **Seleccionar carpeta**.
- El permiso de la carpeta se conserva mediante el selector de carpetas de Android.
- Se crean `roms/`, `roms/nes/`, `textures/`, `saves/`, `config/` y `cache/`.
- La ROM principal no se copia: se mantiene abierta mediante un descriptor de archivo y se entrega al lector nativo con `--rom`.
- Se valida la edición `GAFE01_00` y los formatos ISO, GCM y CISO.
- Las texturas DDS se sincronizan manteniendo subcarpetas y eliminando la caché cuando cambian.
- Las ROMs NES se sincronizan desde `roms/nes/`.
- Las partidas y configuraciones se sincronizan con la carpeta seleccionada.
- Los controles táctiles originales permanecen sin rediseñar.
- El menú móvil oculta resolución, modo de ventana y editor de teclado de PC.
- El ajuste de imagen ofrece 4:3, 16:9 y 21:9 sin estirar la superficie.
- Los archivos Java modificados superaron una comprobación de sintaxis con Java 17 y stubs de API Android.
- `pc_settings.c`, `pc_settings_menu.c` y `pc_disc.c` superaron comprobaciones de sintaxis C para Android y escritorio.
- La matemática de ajuste de aspecto fue probada con superficies 16:9, 21:9 y resoluciones intermedias.

## Pendiente de una compilación Android real

Este entorno no contiene Android SDK, NDK ni `android.jar`, y el ZIP original dejó vacío el submódulo `android/SDL`. Por eso aquí no se generó ni se instaló un APK y no se afirma que haya sido probado físicamente en un teléfono.

El proyecto incluye dos formas de completar esa validación:

1. El workflow `.github/workflows/android-32.yml`, que recupera SDL2 si falta, compila `armeabi-v7a`, verifica que `libmain.so` sea ELF32 ARM y publica el APK.
2. Los scripts `android/prepare-sdl.ps1` y `android/prepare-sdl.sh` para preparar SDL2 antes de abrir `android/` en Android Studio.

## Pruebas recomendadas en dispositivo

- Primer inicio y persistencia del permiso de carpeta.
- Lectura directa de ISO, GCM y CISO desde distintos proveedores de archivos.
- Crear una partida, cerrar completamente la app y comprobar `saves/card_a/`.
- Añadir y reemplazar un DDS con el mismo nombre; después usar **Recargar ROMs y texturas**.
- Añadir ROMs NES y comprobar su aparición dentro del juego.
- Cambiar entre 4:3, 16:9 y 21:9 en una pantalla panorámica.
- Probar Normal, Alta 2x y Ultra 4x en una GPU que no admita todas las configuraciones MSAA.
- Probar controles táctiles, mando físico, pausa/reanudación y cambio de carpeta.
