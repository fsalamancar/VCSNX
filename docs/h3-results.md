# H3: primera imagen en la Switch (2026-10-11)

## Build
- **`.nro`:** 95,8 MB. El código generado se compila a -O2 y la compilación completa con GCC 16.1 tarda unos 20 minutos (las recompilaciones parciales, ~1 minuto).
- **Cambios en `vendor/` que hizo falta para la Switch** (detalle en `UPSTREAM.md`):
  - `Runtime::set_game_root` y `normalized_native_path`: newlib no puede canonicalizar `sdmc:/`.
  - `vcs_media_decoder`: FFmpeg leía `sdmc:` como protocolo de red.
- **Cambios de VCSNX:**
  - La ventana se crea en el hilo principal: `main.cpp` llama a `display_window_start()` ahí, antes de crear el hilo del juego.
  - Log de arranque `vcsnx_boot.log`.
  - Trazas de hilos y de la ventana.
  - Latido `[vcsnx-status]`.

## Transferencia a la SD
- **MTP desde el Mac:** el Mac no detectó la Switch por USB (cable o hub).
- **FTP de DBI:** funciona para descargar y para subir archivos sueltos (unos 0,5 MB/s). Falla al crear subcarpetas.
- **Copia inicial:** `PSP_DATA` + `.ini` + `.nro` pasaron por pendrive → PC → MTP de DBI (Windows).
- **nxlink:** no funcionó en esta red. Mis comprobaciones del puerto con `nc` dejaron el Netloader en error, y la búsqueda por UDP nunca recibió respuesta.

## Arranque
| Punto | Resultado | Nota |
|---|---|---|
| Logos y menú | ✓ (logos e intro) | Tres errores corregidos en este orden: `canonical` de `sdmc:`, la espera de 5 s de la ventana y el protocolo `sdmc:` en FFmpeg |
| Joy-Con en el menú | ✓ | Confirmado por el usuario |
| Cerrar con HOME | sin confirmar | Los cierres con HOME de las corridas de diagnóstico no dejaron informes de fallo de Atmosphère |
| Mensaje en modo Álbum | no probado | Pendiente para el Plan 2 |

## Rendimiento (portátil, reloj normal, dibujado por CPU; corrida con diagnósticos de E/S, eventos, hilos y audio activos)
```
t=283s frames=14 fps_avg=1.3 fps_min=1.3 guest_ms_avg=55.5 guest_ms_max=74.5 ge_ms_avg=680.9 ge_ms_max=689.4 present_ms_avg=20.8 present_ms_max=20.8 mem_peak_mb=3185
t=294s frames=14 fps_avg=1.3 fps_min=1.3 guest_ms_avg=52.7 guest_ms_max=75.3 ge_ms_avg=685.4 ge_ms_max=690.8 present_ms_avg=20.8 present_ms_max=20.9 mem_peak_mb=3185
t=304s frames=14 fps_avg=1.3 fps_min=1.2 guest_ms_avg=55.4 guest_ms_max=95.7 ge_ms_avg=692.2 ge_ms_max=696.4 present_ms_avg=20.8 present_ms_max=20.8 mem_peak_mb=3185
t=322s frames=13 fps_avg=0.7 fps_min=0.1 guest_ms_avg=827.0 guest_ms_max=10118.0 ge_ms_avg=502.5 ge_ms_max=691.4 present_ms_avg=20.8 present_ms_max=20.8 mem_peak_mb=3185
t=332s frames=26 fps_avg=2.5 fps_min=0.5 guest_ms_avg=253.7 guest_ms_max=1972.2 ge_ms_avg=125.8 ge_ms_max=132.0 present_ms_avg=20.8 present_ms_max=20.9 mem_peak_mb=3185
t=343s frames=26 fps_avg=2.5 fps_min=1.1 guest_ms_avg=148.8 guest_ms_max=762.7 ge_ms_avg=222.6 ge_ms_max=385.3 present_ms_avg=20.8 present_ms_max=20.8 mem_peak_mb=3185
t=353s frames=23 fps_avg=2.2 fps_min=2.1 guest_ms_avg=51.9 guest_ms_max=74.8 ge_ms_avg=378.6 ge_ms_max=384.0 present_ms_avg=20.8 present_ms_max=22.4 mem_peak_mb=3185
t=363s frames=27 fps_avg=2.6 fps_min=0.4 guest_ms_avg=179.5 guest_ms_max=2694.3 ge_ms_avg=184.7 ge_ms_max=406.6 present_ms_avg=20.9 present_ms_max=22.3 mem_peak_mb=3185
t=373s frames=15 fps_avg=1.5 fps_min=0.6 guest_ms_avg=249.4 guest_ms_max=1159.0 ge_ms_avg=401.9 ge_ms_max=414.1 present_ms_avg=21.2 present_ms_max=22.2 mem_peak_mb=3185
t=384s frames=17 fps_avg=1.7 fps_min=0.6 guest_ms_avg=177.1 guest_ms_max=1202.6 ge_ms_avg=403.3 ge_ms_max=416.2 present_ms_avg=20.9 present_ms_max=22.4 mem_peak_mb=3185
t=394s frames=21 fps_avg=2.1 fps_min=1.9 guest_ms_avg=60.0 guest_ms_max=90.2 ge_ms_avg=403.2 ge_ms_max=416.4 present_ms_avg=20.8 present_ms_max=22.4 mem_peak_mb=3185
t=404s frames=19 fps_avg=1.8 fps_min=0.8 guest_ms_avg=106.5 guest_ms_max=760.0 ge_ms_avg=413.0 ge_ms_max=423.8 present_ms_avg=21.2 present_ms_max=22.3 mem_peak_mb=3185
```
| Medida | Switch | Mac M1 (sin carga, `docs/h2-results.md`) |
|---|---|---|
| FPS en 3D | 1–3 | 14–30 |
| Dibujado por CPU (`ge_ms`) | 130–690 ms/cuadro | ~40 ms/cuadro (90 % en el bucle de píxeles) |
| Juego (`guest_ms`) | ~50–80 ms/cuadro (incluye el costo de escribir los diagnósticos) | 3–14 ms/cuadro |
| Presentación | ~21 ms (vsync) | 1–3 ms |

- **Escenas:** solo se midieron los logos y la intro. A 1–3 FPS no es práctico llegar a manejar por la ciudad.
- **Memoria:** el valor de `mem_peak_mb=3185` no sirve. En modo aplicación, libnx reserva todo el heap al arrancar y `InfoType_UsedMemorySize` lo cuenta entero. Hay que medir con `mallinfo` o similar en el Plan 2.
- **Comparación de imagen Mac/Switch (spec §9):** no se hizo. La decisión de H4 no depende de ella.

## Para la decisión de H4 (spec §7)
- **¿Las 3 escenas llegan a ≥ 30 FPS de promedio?** No. Están en 1–3 FPS, entre 10 y 30 veces por debajo de la meta.
- **Dónde se va el tiempo:** sobre todo en el dibujado por CPU (hasta 690 ms por cuadro). El juego en sí también pesa: unos 50 ms con diagnósticos, a medir otra vez sin ellos.

## Decisión H4 (usuario, 2026-10-11)
- **Se adelanta la etapa 2 con Vulkan.** El spec suponía que la Switch homebrew no tenía Vulkan (§2.2), pero `mesa-switch` (https://github.com/danfromtico/mesa-switch, Mesa 26.2.4) trae NVK para el Tegra X1 como bibliotecas estáticas para libnx (`libvulkan.a`, CMake `Vulkan::Vulkan`, sin loader). Autorun la usa.
- **Plan 2:** compilar el backend Vulkan del fork (`ge_gpu_backend_vulkan.cpp`) para la Switch con `mesa-switch`, en lugar de escribir un backend OpenGL. Presentación: la imagen se lee de la GPU hacia la CPU (como `platform/generic` del fork) y la muestra nuestra ventana SDL; la presentación directa va después.
- **Primera medición del Plan 2:** el `guest_ms` sin diagnósticos, para saber si la CPU limita aunque la GPU dibuje.
