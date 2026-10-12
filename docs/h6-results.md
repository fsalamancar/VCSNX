# H6: primera corrida Vulkan en la Switch (Plan 2, Tareas 7 y 8)

- **Fecha:** 2026-10-11.
- **Consola:** Switch con Atmosphère; juego abierto desde hbmenu con R + juego (modo aplicación), en portátil.
- **Configuración:** `Backend=DirectX12`, que en este build es el backend Vulkan del fork; resolución interna 1280×720 (`Custom`); efectos apagados.
- **Datos:** `perf.log` (una línea cada 10 s), `VCSNative.log`, `vcsnx_boot.log` y, en la corrida 2, las líneas `[ge-phase]` y `[gpu-timestamp]` del vendor.

## Resultado

- **NVK funciona en la consola.** La sonda da `PASS` (`docs/vkprobe-results.md`) y el juego arranca con `ge backend requested=vulkan active=vulkan` y `[vulkan] GPU: NVIDIA Tegra X1 (GM20B) (NVK GM20B)`.
- **Los dos drivers conviven.** SDL presenta con GLES2 (`switch-mesa` 20.1, `window and renderer ready (opengles2)`) y el GE dibuja con NVK (`mesa-switch` 26.2) en el mismo proceso, sin errores.
- **El 3D se ve bien y nítido a 720p**, según el usuario ("gráficos perfectos"). El HUD y los textos se ven grandes y algo irregulares: ver "Problemas visuales".
- **Todavía no se puede jugar:** el 3D va a 6–14 FPS, contra 1–3 FPS en la etapa 1. El audio se escucha muy mal.

## Rendimiento (3D en la ciudad, ventanas de 10 s)

**Qué significa "FPS" aquí:** es el `frames=` de `perf.log` dividido por 10 s, es decir, cuántas vueltas por segundo da el bucle de vblank. El vendor hace avanzar el tiempo del juego con esos vblanks y no recupera el tiempo perdido, así que a "6 FPS" el juego corre al ~10 % de su velocidad. Como VCS dibuja un frame cada dos vblanks, se ven aproximadamente la mitad de frames nuevos.

| Corrida | FPS | `guest_ms` | `ge_ms` | `present_ms` | Heap en uso |
|---|---|---|---|---|---|
| Etapa 1, CPU (`docs/h3-results.md`) | 1–3 | 50–80 | 130–690 | ~21 | no medible |
| 1: Vulkan, GE en el hilo del juego, 1020 MHz | 6–14 | 18–30 | 43–134 | 10–13 | 480 MB |
| 2: igual a la 1, con diagnósticos de fase | 4–8 | 49–57 | 54–197 | 20–24 | 450–476 MB |
| 3: GE asíncrono (núcleo 2) + CPU a 1785 MHz | 5–28 | 12–33 | 15–180 | 9–14 | 435–486 MB |
| 4: GE asíncrono, 1 hilo de rasterizado, 1020 MHz | 5,5–15 (tramos largos a 5,6) | 13–39 | 16–163 | 11–14 | 426–486 MB |
| Mac M1, Vulkan sobre MoltenVK (referencia) | 60 | 8–16 | 6–9 | 0,6–3 | 130–149 MB |

La corrida 2 va más lenta porque cada frame escribe dos líneas de diagnóstico en la SD sin buffer; sus cifras sirven para repartir el tiempo, no como FPS.

## Dónde se va el tiempo del GE (corrida 2, promedio de 952 frames con más de 30.000 vértices)

| Fase (`[ge-phase]`) | Switch | Mac M1 |
|---|---|---|
| Total del GE en CPU (`ge_us`) | 92,5 ms | 8,1 ms |
| Decodificar vértices (`vdecode_us`) | 43,7 ms (681 ns por vértice) | 3,9 ms (49 ns por vértice) |
| Preparar triángulos (`triprep_us`) | 33,5 ms (523 ns por vértice) | 2,3 ms (29 ns por vértice) |
| Acumular para la GPU (`accum_us`) | 8,1 ms | 0,8 ms |
| Lista, setup y texturas | ~7 ms | ~1 ms |
| Vértices y draws por frame | 64.148 y 681 | 78.777 y 880 |
| GPU ejecutando el frame (`[gpu-timestamp]`) | 13,8 ms (máx. 29,6) | 2,5 ms |
| CPU grabando comandos Vulkan | 0,7 ms | 0,07 ms |

- **Conclusión:** la GPU no es el límite. El límite es la CPU, que transforma y recorta unos 60.000 vértices por frame en un solo hilo (el modo `HardwareTransform` del fork, que lo haría la GPU, dibuja mal según el propio fork).
- Por vértice, la Switch es 14–18 veces más lenta que el M1. Por reloj y diseño (A57 a 1,02 GHz contra M1 a 3,2 GHz) se esperaban unas 8–9 veces. Este código, con muchas ramas, penaliza más al A57.
- **El rasterizador de CPU no corre:** con Vulkan activo, `software_raster_skipped()` salta todos los draws.

## Tarea 8: GE asíncrono, núcleos y reloj (corridas 3–5)

- **GE asíncrono:** no ayudó. El juego espera al GE en cada frame (`sceGeDrawSync`), así que solo se pueden solapar los ~28 ms del juego contra un GE de 40–160 ms. Las corridas 3 y 4 no fueron más rápidas que la 1, y la 3 parpadeaba.
  - El parpadeo se corrigió en 2cfed2b: el frame de GPU retenido ya no se suelta mientras el GE sigue trabajando.
  - **Por defecto en la Switch:** GE en el hilo del juego (`PSPRECOMP_GE_ASYNC=0`). El hilo propio del GE (4 MiB de pila, núcleo 2) queda disponible con `vcsnx_env.txt`.
- **Hilos auxiliares del rasterizador:** después de cada trabajo giran hasta 262.144 vueltas antes de dormirse. Con un ayudante fijado al núcleo 2 (el del GE asíncrono) y otro al 0 (ventana y audio), Horizon no reparte el núcleo entre hilos de igual prioridad, y esos giros frenaban a los demás.
  - **Por defecto con Vulkan:** 1 hilo (`PSPRECOMP_RASTER_THREADS=1`), porque el rasterizador de CPU no se usa.
- **Reloj de la CPU:** a 1785 MHz funciona (`[vcsnx-clock] cpu previous=1020 MHz target=1785 MHz`), pero es **opcional** con `VCSNX_CPU_MHZ`. El usuario prefiere el reloj de serie y usa sys-clk por su cuenta, y un boost reaplicado cada 2 s pelearía con esa herramienta.
- **Audio:** se oye mal porque el juego corre en cámara lenta (el audio sale al ritmo del juego). Se arregla cuando el GE sea más rápido: Plan 3, transformación de vértices en la GPU.
- **Destello verde** al llegar a la pantalla de carga, después del segundo video: sigue igual con el GE en el hilo del juego (corrida 5), así que no viene del GE asíncrono. Queda pendiente aparte.
- **Referencia del usuario:** el port de Liberty City Stories para Switch (`NaGaa95/gtalcs_nx`) no emula la PSP: corre el binario de Android con su propio OpenGL ES. VCS no tiene versión Android. Lo único reutilizable es subir la CPU solo durante las cargas (`appletSetCpuBoostMode`).

## Problemas visuales

- **HUD, cuadros de ayuda y nombres de zona** (ej.: "Escobar International"): ocupan la misma proporción de pantalla que en la PSP, así que en la Switch se ven grandes. El texto queda algo irregular porque el 2D de la PSP (480×272) se escala ×2,67 hasta 720p.
- **El minimapa** se ve bien, según el usuario.
- **Pendiente** (fuera de este plan): un HUD más chico, reutilizando el código del fork que encoge la interfaz en modo panorámico (port del WidescreenFix de ThirteenAG en `ge_renderer.cpp`), y texto más limpio.

## Checklist del usuario (corrida 1)

| Punto | Resultado |
|---|---|
| Arranca, logos e intro | sí |
| Imagen nítida a 720p | sí |
| Gráficos 3D correctos | sí ("perfectos") |
| Controles | sí |
| Fluidez | no: 6–14 FPS |
| Audio | muy mal (cortes) |
| HUD y texto | grandes y algo deformados |
