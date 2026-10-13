# Backlog después de v0.1.0

Lista del autor (2026-10-12) para los próximos releases. No se toca antes del launch 0.1.

## Bugs

1. ~~A veces se queda en "loading" (pasa de vez en cuando).~~ **Arreglado (2026-10-12, 984266f)**, junto con el 3: ver abajo.
2. Crash al entrar a una misión.
3. ~~Al ir al ícono de guardar, la pantalla se quedó en negro y nunca cargó.~~ **Arreglado (2026-10-12, 984266f)**.
4. Al cargar una partida, la imagen titila.
5. Subtítulos grandes en las cinemáticas de las misiones, a veces mezclados.
6. ~~Los colores de algunos carros se ven extraños y titilan.~~ **Arreglado (2026-10-13, 228b7ce)**: ver abajo.
7. Al menú de pausa le falta relleno: se ve como si una parte estuviera en otra dimensión.
14. Objetos que aparecen de golpe (pop-in): como es un juego de PSP, los objetos y los modelos detallados se
    dibujan muy cerca y "aparecen" mientras uno avanza (pedido del autor, 2026-10-13). Hay una opción
    `[DrawDistance]` en `VCSNative.ini` (`World`, `LOD`) que en la Switch está apagada; `LOD` es solo visual
    (cambia el modelo simple por el detallado más lejos), `World` cuesta GPU y GE. Medir el costo antes de
    activarla.
15. ~~Crash del hilo del GE leyendo la paleta de una textura (2026-10-13, dos veces en el día).~~ **Arreglo en
    prueba**: ver abajo.
16. Pantalla de controles: el dibujo del control (el del PSP, con los nombres de la Switch encima) no le gusta
    al autor. Usar la Switch en modo portátil de su imagen (`local/assets/switch_handheld_reference.png`; es del
    autor, con licencia libre: puede ir en `assets/` y en el release) y rotular cada botón con lo que hace en VCSNX (`pad_mapping.cpp`:
    A = cruz, B = círculo, Y = cuadrado, X = triángulo, L/R, ZL/ZR en vehículo, + = START, − = SELECT).
17. El sonido se escucha bien pero "sucio" (2026-10-13). Pista: en la sesión P1 el canal 8 descartó el 7,4 % de
    las muestras (`[audio-summary]` `trimmed=952600` de `pushed=12788736`) y tuvo huecos (`underrun=114060`):
    saltos que suenan como chasquidos. Medir de nuevo con los fps actuales (`PSPRECOMP_AUDIO_SUMMARY=1`).

Para 1, 2 y 3, cuando pase: no borrar `atmosphere/crash_reports/` ni `switch/vcsnx/vcsnx_stderr.log` de la SD.

**Causa de 1 y 3 (encontrada el 2026-10-12):** `sceKernelExitDeleteThread` del runtime solo terminaba el hilo: su
pila quedaba ocupada y su registro en la tabla. El juego crea un "sfx bank load thread" por cada banco de sonido;
hacia la misión 2 se acababa el espacio entre las pilas y la memoria del juego, `sceKernelCreateThread` devolvía
0x80020190 (sin memoria), el banco no cargaba y el juego esperaba para siempre su bandera `WorldStreamEventFlag`
(pantalla negra al guardar, o "loading" al entrar a una casa). Ahora el hilo se borra y su pila vuelve a la lista
libre (`tests/platform/test_thread_stacks.cpp`). Confirmado en la consola: misión 2, guardar, sin cuelgue.

**Bug 2 (crash), lo que se sabe:** los dos crash reports del 2026-10-11 son el mismo: el hilo del GE se queda sin
memoria de la Switch (`std::bad_alloc` dentro de `ge_gpu_backend_accumulate_hardware_triangles`, que es
`noexcept`), el programa sale y SDL aborta al leer el táctil con la HID ya cerrada (0x1159). La compilación de
diagnóstico anota en `vcsnx_boot.log` el tamaño pedido y el estado del heap si vuelve a pasar.

**Bug 15 (crash del GE, 2026-10-13):** `vcsnx_crash.log` (registro nuevo, 2b101de) mostró que la página que
leía el GE estaba prestada a una llamada IPC (`attr=IpcMapped`, `perm=0`, `ipc=1`). En Horizon, las páginas
destino de una lectura grande quedan prestadas al servicio FS mientras dura la llamada, y ningún otro hilo
puede tocarlas. `sceIoRead` leía los archivos del juego (el streaming del mundo) directo a su RAM, y el GE, en
otro núcleo, leía una paleta en esas mismas páginas. Ahora la lectura pasa por un buffer propio y se copia
(`platform/switch/guest_io.cpp`).

## Mejoras

8. Al inicio, una interfaz parecida a la de pausa: continuar, cargar partida, nueva partida y las ranuras de partidas.
9. Sonido y rendimiento: mejorar los dos sin overclock.
10. Videos iniciales a 1080p, reescalados con IA (decisión del autor). Son videos del juego: el reescalado
    se hace en el PC de cada jugador (como `vcs_game.so`), nunca va al repo ni al release. Antes de elegir
    la resolución, medir si la Switch decodifica un video de 1080p a velocidad normal.
    Medido (2026-10-12, vidprobe en la consola): 1080p H.264 a 44 fps con 1 núcleo, 89 fps con 3 núcleos
    + conversión a RGBA; la intro necesita 30 → **1080p**. IA: Real-ESRGAN `realesr-animevideov3` x4
    (oficial v0.2.5.0, ~4 cuadros/s en un M1). Plan para la **v0.2**: casilla opcional en el Builder
    "compilar los videos en HD (tarda más)", y el runtime muestra el video HD sincronizado con el original.
11. "bAlo productions" en vez de "studios bAlo" en el splash. El autor aprobó que Claude arme el logo nuevo
    a partir de `assets/splash/logo.png`.
12. Minimapa en HD.
13. Artworks de las pantallas de carga en HD (como los videos: reescalados con IA en el PC de cada jugador,
    nunca en el repo ni en el release). Para la siguiente actualización.
18. Texturas HD del mundo (pedido del autor, 2026-10-13). Como los videos y los artworks: reescaladas con IA en
    el PC de cada jugador, con el sistema de reemplazo que ya existe (`[Textures]`, `<hash>.png` en
    `switch/vcsnx/Textures`), nunca en el repo ni en el release. Costo a medir: memoria (presupuesto de
    texturas), carga de PNG y ancho de banda de la GPU.
19. Mejores shaders (pedido del autor, 2026-10-13): el runtime ya trae FXAA (`SMAA`), `Sharpen`, color grade y
    bloom (`[SimulateHDR]`), apagados en la Switch. Probar cuáles caben en el presupuesto de GPU.
20. Rendimiento: el autor apunta a 60 fps. Orden acordado: 30 fps estables primero; después medir si 60
    (`[Timing] FrameRate=60`, que duplica el trabajo del juego y del GE por segundo) es alcanzable.
    Resolución en dock: se queda en 1280x720 (decisión del autor, 2026-10-13).

## Reflejos de los carros (bug 6, 2026-10-13)

Cada frame el juego reduce el último frame del mundo (VRAM `0x04088000`) con 11 sprites a un mapa RGB565
de 64x64 al inicio del back buffer (`0x04000000` o `0x04044000`, stride 64), le mezcla 4 sprites de una
textura en RAM, y la pintura y los vidrios de cada carro llevan un pase con environment mapping sobre ese
mapa (mezcla fija 0x15 + 0xEA, ~8%). Con Vulkan el mundo solo existe en la GPU y el rasterizador de CPU
está apagado (`software_raster_skipped`), así que el mapa era basura de VRAM. Ahora esos sprites se
dibujan desde el frame que el backend lee cada vblank (`platform/switch/reflection_map.cpp`) y el target
pequeño se rasteriza en la CPU. Mipmaps y transformación por hardware no tenían que ver (pruebas B y C).
Diagnóstico usado: `local/notes/uvgen_diag_v2.patch` (`VCSNX_UVGEN_DIAG=<carpeta>`).

## Pendiente visual (2026-10-13)

- ~~Menú de pausa / mapa con espacio a la derecha y abajo~~ **Arreglado (2026-10-13, 90193cd y a840f70)**:
  mientras el menú está en pantalla, la ventana muestra solo su parte de 480x272 del frame.
- Quitar las ayudas "move / select / back" del menú.
- Subtítulos: que se achiquen parejos (el anclaje por tercios de `hud_scale.cpp` los parte).
- Artwork y barra de carga fijos (sin titileo).
- El nombre de la zona que aparece abajo a la derecha al entrar a un lugar (p. ej. "'101 Bayshore Avenue'")
  pegado a la esquina inferior derecha de la pantalla, como el HUD de arriba (pedido del autor, 2026-10-13;
  foto en `local/photos/2026-10-13-zone-name.jpg`). Hoy queda separado del borde derecho e inferior.

## Mods (después del backlog normal)

Pedidos del autor (2026-10-13). Se hacen solo cuando estén cumplidos los bugs, los pendientes visuales y las
mejoras de arriba. Son ideas que vio como mods de GTA Vice City para PC: aquí se escriben desde cero para VCSNX
(nada de código de esos mods), como funciones del runtime que leen y cambian el estado del juego.

- M1. Apuntado libre pudiendo caminar: mover la mira con el stick derecho mientras se camina, en vez de quedar
  quieto o con el apuntado automático.
- M2. Taller estilo "Los Santos Customs" en los Pay 'n' Spray: además de pintar, elegir colores y otras
  mejoras del carro.
- M3. GPS: la ruta más corta por las calles hasta el punto marcado en el mapa, y poder poner puntos propios.
