# H7: la GPU transforma los vértices (Plan 3)

- **Fecha:** 2026-10-11.
- **Qué cambió:** el camino `HardwareTransform` del fork (sombreadores `psp_ge_hw*.vert`) ya funciona con Vulkan. Con `PSPRECOMP_GE_GPU_HW_TRANSFORM=1`, la GPU hace la transformación, el recorte y la preparación de triángulos de los modelos 3D; el 2D (HUD, textos) y los casos especiales siguen por el camino de pantalla.
- **Cómo se validó:**
  1. **Comparador por draw** (`VCSNX_HW_VERIFY`): para cada vértice compara lo que calculará la GPU con el camino de CPU, en la misma corrida.
  2. **Frames guardados cada N vblanks** (`VCSNX_GPU_DUMP_EVERY`): la cinemática de la intro es igual en cada corrida, así que se comparan las mismas tomas.
  3. **Bisección por formato de vértice** (`VCSNX_HW_EXCLUDE_VTYPES`).

## Los fallos del camino de hardware (todos del fork, corregidos)

| Síntoma | Causa | Commit |
|---|---|---|
| Crash al empezar el 3D (SIGSEGV) | La decodificación en paralelo escribía en los `thread_local` de cada hilo del pool | ed2801c |
| Triángulos gigantes sin forma | Los pipelines eran siempre `TRIANGLE_LIST` y las tiras de la PSP se dibujaban como triángulos sueltos | 4a9aafe |
| Todo de un color plano, sin texturas | Los sombreadores pasaban UV normalizadas y `psp_ge.frag` las espera en texels | 79a6220 |
| Rectángulos negros detrás de las palmeras | El replay dibujaba todos los lotes de pantalla y después todos los de hardware: se perdía el orden del juego | 3d7fa15 |
| Sombras y follaje opacos (manchas negras) | El shader de fragmentos de hardware no recibía el formato del framebuffer; tomado como 5650, forzaba alfa 1 | 8284883 |
| Texturas proyectadas (UV con `q`) | El shader forzaba `q = 1` | 3834b68 |

Además, dos fallos que también afectaban al camino de pantalla:
- **Destello verde antes de la pantalla de carga:** el juego borra el framebuffer en verde y dibuja la pantalla un vblank después; el backend presentaba ese borrado solo. Ahora un bucket que solo tiene borrados no se presenta (ff6797c).
- **Coste de presentar:** cada frame se reordenaba píxel a píxel (RGBA → RGB888) en el hilo del juego; ahora se copia tal cual a una textura ABGR8888 (72056a1).

El comparador queda limpio en UV, color y niebla; las únicas diferencias de posición son de redondeo float en vértices muy fuera de pantalla.

## Rendimiento en la Switch (1020 MHz, 1 hilo de rasterizado)

"FPS" = vueltas del bucle de vblank por segundo (ver `docs/h6-results.md`): 60 es velocidad real.

| Corrida | FPS en 3D | `guest_ms` | `ge_ms` | `present_ms` | Heap en uso |
|---|---|---|---|---|---|
| H6, corrida 4 (transformación en CPU) | 5,5–15 (tramos largos a 5,6) | 13–39 | 55–163 | 11–14 | 426–486 MB |
| **H7 (transformación en GPU, GE en el hilo del juego)** | **11–25** (típico 13–18) | 25–37 | **9–37** | 12–16 | 466–489 MB |
| **8: + GE asíncrono (núcleo 2) y presentar sin reordenar** | **20–27** | 19–37 | 10–26 | 10–16 | 430–476 MB |
| **9: + 30 FPS y lectura del frame con caché** | **36–51** | 12,8–15,5 | 3–10 | 5–12 | 433–471 MB |

- Con la transformación en GPU, el GE bajó unas 4 veces y quedó parecido al juego (~28 ms). Por eso el GE asíncrono ahora sí ayuda: el juego y el GE se solapan (en H6 no ayudaba porque el GE pesaba 40–160 ms). Va por defecto desde cdae002.
- **Corrida 9** (b7ead49): agrega `[Timing] FrameRate=30` (el juego conserva el limitador de la PSP y dibuja un frame cada dos vblanks), la lectura del frame desde memoria con caché (2b7839a) y el arreglo del parpadeo de créditos y carga (88d20fe). Según el usuario, "el resto está perfecto": imagen, fluidez y pantallas de carga. El sonido solo mejoró un poco. El log confirma `[frame-rate] target=30` y `frame readback memory: cached=1 coherent=0` (se invalida antes de leer).
- **Corrida 10** (b9c87f9, la 9 más el arreglo del audio de abajo): el usuario dice "va perfecto. jugable 100 porciento", con el reloj de serie.
- El reloj de la CPU sigue opcional (`VCSNX_CPU_MHZ`); el usuario prefiere el de serie.

## Audio: el limitador tiraba el audio del tiempo recuperado (b9c87f9)

- **Síntoma:** el sonido salía muy cortado en todas las corridas, aunque el juego ya iba a velocidad casi normal.
- **Causa:** cuando una vuelta del bucle tarda más que un vblank, `limit_frame_rate()` adelanta el reloj del juego para no quedar en cámara lenta. En ese salto también corría hacia adelante el horario de cada canal de audio. El fork lo hace para la sincronía de labios tras un tirón aislado en un PC rápido. En la Switch casi todas las vueltas van atrasadas, así que el ajuste se aplicaba en cada una: el audio del tiempo recuperado nunca se generaba y el dispositivo tocaba silencio. Con `PSPRECOMP_AUDIO_DIAG`, el canal se veía "264 ms adelantado" según el juego, mientras la cola real estaba vacía.
- **Arreglo:** en VCSNX, el limitador ya no mueve el horario del audio.
- **Medición en el Mac** (`tools/audio_bench_mac.sh`; `VCSNX_DEBUG_VBLANK_SLEEP_US=35000` hace cada vuelta tan lenta como en la Switch, ~22 por segundo; el reloj del juego sigue al 99,7 % del tiempo real):

| Corrida en el Mac | Bloques de salida en silencio | Underrun acumulado |
|---|---|---|
| Velocidad normal | 4–5 % (pausas de la escena) | 0–512 frames |
| Lenta, antes del arreglo | **84,8 %** | 1.376.140 frames |
| Lenta, con el arreglo | **1,4 %** (0,0 % al repetir) | 908 frames |

- **Límite:** el salto del limitador es de hasta 4 vblanks por vuelta. Por debajo de ~12 vueltas por segundo el reloj del juego ya no alcanza el tiempo real, y el juego y el audio irían algo más lentos.

## Pendiente
- Cifras de audio de la corrida 10 (`PSPRECOMP_AUDIO_SUMMARY=1` y `PSPRECOMP_REALTIME_SPEED_DIAG=1` están activos en su `vcsnx_env.txt`; opcional, el usuario ya la dio por buena).
- Fuera de este plan: HUD más chico y nítido; subir la CPU solo durante las cargas; feedback de framebuffer en la GPU para las composiciones.
