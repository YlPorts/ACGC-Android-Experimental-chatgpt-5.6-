# Edición Android 32-bit con carpeta principal

## Flujo del usuario

1. En el primer inicio aparece un solo botón: **Seleccionar carpeta**.
2. La app crea `roms/`, `roms/nes/`, `textures/`, `saves/`, `config/` y `cache/`.
3. La ROM principal se deja dentro de `roms/` y se abre directamente desde allí.
4. La ubicación queda recordada y el selector no vuelve a mostrarse en arranques normales.
5. Las texturas DDS y las ROMs NES se preparan antes de iniciar el motor.
6. Las partidas y los ajustes se sincronizan de vuelta a la carpeta elegida.

## Ajustes que conserva Android

- Relación de imagen: 4:3, 16:9 y 21:9.
- VSync.
- Límite de 30, 60, 90 o 120 FPS.
- Calidad de imagen: Normal, Alta 2x y Ultra 4x.
- Texturas HD: bajo demanda, precarga o precarga con caché.
- Volumen principal.
- Zona muerta del stick y C-stick.
- Resetti.
- Requisito de visitante para mejorar la tienda.
- Acres continuos.
- Relación de imagen para juegos NES.

## Ajustes eliminados del menú móvil

- Modo ventana, pantalla completa y ventana sin bordes.
- Resoluciones de escritorio.
- Editor de teclas físicas de PC.

Los controles táctiles existentes se mantienen y pueden ocultarse, moverse o restaurarse desde el botón de engranaje.

## Texturas personalizadas

El cargador actual admite paquetes compatibles con Dolphin en formato `.dds`. Las carpetas internas se conservan. Cuando cambia cualquier textura, la caché binaria se invalida para evitar que se siga mostrando una versión anterior.

## Traducciones

La investigación de textos se hará después de estabilizar esta base. Hay que localizar si cada texto procede del DOL, módulos REL, archivos del disco o tablas comprimidas. La solución ideal será una carpeta externa de traducción con reemplazo en tiempo de carga, evitando modificar la ROM original.
