# Protocolo de aceptación V6

Estado inicial de los casos completos de esta página: **pendiente**. No son resultados
obtenidos. Registrar APK, SHA-256, certificado, ABI de proceso, dispositivo, versión
Android, GPU, versión de cada pack y logs. Objetivo principal: Galaxy A15 SM-A155M,
Android 16/Mali; repetir los casos básicos en otro dispositivo y en ARM32.

**Regresión conocida de RC1:** el usuario notificó cierre al iniciar el juego en
las tres APK. RC1 está retirada. RC2 corrige la escritura sobre una solicitud
ARAM nula y la copia parcial de una tarea DVD en ARM64. La nueva prueba de esta
ruta usa fuentes de producción con disco sintético; la prueba de arranque en
el teléfono con RC2 sigue pendiente y debe registrarse expresamente.

**Resultado comunicado por el usuario:** ARM32 RC2 funciona; ARM64 y universal
RC2 se cierran al iniciar. No considerar esas dos variantes aprobadas. RC3
corrige los contratos nativos OS/DVD reproducidos en las pruebas; falta confirmar
el arranque corregido y el resto del protocolo en el dispositivo.

Al abrir RC3 después de un cierre, Android 11+ puede mostrar «El juego se cerró
en el intento anterior». Elegir **Copiar informe** y pegar el texto para revisar
señal, pila y Build ID. El botón **Reintentar juego** permite continuar después.
No se necesita un PC ni borrar datos. Si vuelve a cerrarse, reabrir la app y
copiar el informe nuevo. Una traza que Android ya haya descartado no puede
recuperarse; en ese caso obtener `adb logcat -b crash -d`. La lectura del informe
y su interacción todavía no se han ejecutado físicamente en este entorno.

Para actualizar RC1/RC2 a RC3, instalar la APK sobre la anterior sin desinstalar,
borrar datos ni cambiar la carpeta de juego. Ambas usan el mismo certificado
V6; RC3 tiene `versionCode=8`. Comprobarlo con `apksigner verify --print-certs`
y `aapt dump badging` antes de la prueba. Un error de certificado no se resuelve
borrando el pueblo. Si persiste un cierre, conservar `adb logcat -b crash -d`,
`adb logcat -d` y `save/startup-stage.txt` del almacenamiento privado. Estas APK
son depurables: se puede extraer esa etapa con
`adb exec-out run-as com.acgc.port cat files/save/startup-stage.txt`.

1. Conservar y verificar una copia de la carpeta de juego y del pueblo V4/V5.
   Comparar certificados: usar `adb install -r <apk>` sólo si coinciden. Las
   candidatas V6 de esta sesión tienen una firma nueva autorizada por el
   propietario y **no actualizan directamente una V5 de otra firma**. Seguir las
   precauciones de migración que figuran abajo antes de cambiar de instalación.
   Comprobar
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

## Migración de firma V5 → V6

Una firma nueva no cambia el formato de guardado, pero Android impide reemplazar
la instalación anterior. No desinstalar V5 hasta comprobar las copias siguientes:

- Salir del juego normalmente, dejar que finalice la sincronización y verificar
  los archivos de `saves/` y `config/` en la carpeta externa seleccionada.
  Copiar la carpeta completa, incluidos idiomas, texturas y ROMs adicionales,
  fuera del almacenamiento privado de la app y de `Android/data/com.acgc.port`.
- Las preferencias Android y posiciones de controles viven en `shared_prefs`;
  no se presupone que se exporten con `settings.ini`. Para una instalación V5
  depurable, se pueden respaldar junto con los archivos privados mediante
  `adb exec-out run-as com.acgc.port tar -cf - files shared_prefs > acgc-v5-private.tar`.
  Verificar que el comando termina correctamente y que el archivo se puede leer
  con `tar -tf acgc-v5-private.tar`. Si `run-as` no está permitido, no considerar
  respaldadas esas preferencias ni continuar con la única copia del pueblo.
- La restauración del archivo privado, con la app detenida y sobre una V6
  depurable, puede hacerse mediante
  `adb shell am force-stop com.acgc.port` y
  `adb shell run-as com.acgc.port tar -xf - < acgc-v5-private.tar`.
  No se ha ejecutado esta migración en un dispositivo de esta sesión. Probarla
  primero sobre copias, conservar la APK V5 y no eliminar los respaldos.
- La concesión SAF de Android no se restaura copiando preferencias: volver a
  seleccionar la misma carpeta y otorgar acceso. Verificar pueblo, configuración
  y todos los controles antes de jugar sobre los datos principales.

Las siguientes actualizaciones V6 sólo podrán usar instalación directa si se
conserva la nueva clave persistente. Las APK de desarrollo con otra firma tampoco
son intercambiables por `adb install -r`.
