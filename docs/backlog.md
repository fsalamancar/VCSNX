# Backlog después de v0.1.0

Lista del autor (2026-10-12) para los próximos releases. No se toca antes del launch 0.1.

## Bugs

1. ~~A veces se queda en "loading" (pasa de vez en cuando).~~ **Arreglado (2026-10-12, 984266f)**, junto con el 3: ver abajo.
2. Crash al entrar a una misión.
3. ~~Al ir al ícono de guardar, la pantalla se quedó en negro y nunca cargó.~~ **Arreglado (2026-10-12, 984266f)**.
4. Al cargar una partida, la imagen titila.
5. Subtítulos grandes en las cinemáticas de las misiones, a veces mezclados.
6. Los colores de algunos carros se ven extraños y titilan.
7. Al menú de pausa le falta relleno: se ve como si una parte estuviera en otra dimensión.

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
