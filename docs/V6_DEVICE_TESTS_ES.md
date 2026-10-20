# Protocolo de aceptación V6

Estado inicial de todos los casos de esta página: **pendiente**. No son resultados
obtenidos. Registrar APK, SHA-256, certificado, ABI de proceso, dispositivo, versión
Android, GPU, versión de cada pack y logs. Objetivo principal: Galaxy A15 SM-A155M,
Android 16/Mali; repetir los casos básicos en otro dispositivo y en ARM32.

1. Conservar una copia de la carpeta de juego y del pueblo V4/V5. Verificar el
   certificado y actualizar con `adb install -r <apk>` sin desinstalar. Comprobar
   que siguen la carpeta SAF, permisos persistentes, preferencias, idiomas, DDS,
   ROMs adicionales y todos los perfiles/posiciones de controles.
2. Instalar ARM64 y universal en un dispositivo compatible. Confirmar
   `primaryCpuAbi` con `adb shell dumpsys package com.acgc.port` y revisar
   `adb logcat` para fallos del linker, JNI y señales nativas. Repetir con ARM32 en
   un dispositivo que realmente admita procesos de 32 bits.
3. Arrancar desde cada formato de disco admitido (ISO/GCM/CISO), crear un pueblo,
   entrar/salir de una casa, hablar con vecinos, abrir inventario/mapa/menús,
   usar herramientas, pescar/cazar insectos, comprar, guardar y reiniciar.
4. Con copias de prueba, cargar un pueblo real V4/V5, guardar en ARM64, abrir en
   ARM32, guardar de nuevo y abrir en ARM64. Comprobar nombre, fecha/hora, cartas,
   inventario, dinero, vecinos, museo, mobiliario y copias/sincronización GCI.
   Comparar datos lógicos; no exigir igualdad de bytes si avanza el reloj.
5. Probar inglés y cada pack real es/fr/de/it sin regenerarlo. Revisar diálogos,
   artículos/gramática, nombres de objetos/vecinos, menús y gráficos localizados.
   Retirar un miembro de un par de recursos y comprobar el fallback. Restaurarlo.
6. Verificar que archivos PAL sin flag mantienen audio USA. Sólo en una copia de
   prueba activar explícitamente PAL con un pack completo; revisar música, efectos,
   voz y cambios de escena. Probar pareja incompleta/inválida y recuperación USA.
7. Revisar todos los botones, stick, C-stick, cruceta, L/R/START, posición, tamaño,
   opacidad, ocultación y ocultación automática con mando. Conectar/desconectar
   mandos Bluetooth y USB, pausar y volver al juego.
8. Probar 4:3, 16:9 y 21:9; DDS y texturas originales; transparencia, partículas,
   interiores/exteriores, transiciones, framebuffer y reinicio del contexto GL.
   Registrar compilación/enlace de shaders y errores GLES en Mali y otra GPU.
9. Entrar a NES desde un mueble, jugar, probar entrada/sonido/WRAM, salir y regresar
   al pueblo. Probar ROMs adicionales, guardado y parches de metadatos usados por
   los packs reales; registrar cualquier dirección GameCube sin traducir.
10. Cambiar al segundo plano, volver, bloquear/desbloquear, girar el dispositivo y
    restaurar tras cierre de actividad. Verificar audio, imagen, controles y SAF.
11. Medir sesiones iguales a 30/60/90/120 donde proceda: tiempo de simulación,
    CPU/GPU, memoria, percentiles de fotograma y temperatura/energía. No anunciar
    optimización ARM64 sin comparación repetible con ARM32 y sin revisar el reloj
    y la sincronía de audio. Incluir viajes, isla y eventos de calendario.
12. Repetir arranque/carga/guardado, controles y escenas en ARM32. Un fallo relevante
    impide declarar estable a la universal; guardar el log, corregir y repetir el
    caso afectado en ambas arquitecturas.

No convertir un caso pendiente en aprobado a partir de una compilación, una
captura del lanzador o un test de componentes. La firma, archivos de usuario y
copias de seguridad se revisan antes de probar una actualización sobre el pueblo
principal.
