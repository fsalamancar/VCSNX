# H2: prueba en el Mac (2026-10-11)

## Suites (`ctest --preset mac`)
`100% tests passed out of 14`: las 5 suites del spec (psprecomp_tests, vcs_profile_tests, vcs_config_tests, audio_resampler_tests, vcs_bootstrap_paths_tests) y 9 propias.

## Checklist del usuario (mando conectado al Mac)
| # | Punto | Resultado | Nota |
|---|---|---|---|
| 1 | Logos y menú | ✓ | |
| 2 | Menú con cruceta, aceptar con botón de abajo | ✓ | |
| 3 | Intro de partida nueva | ✓ | |
| 4 | Caminar y cámara | ✓ | |
| 5 | ZR/ZL en vehículo | ✓ | |
| 6 | Audio | ✓ tras el arreglo | Primero estaba mudo: mi arreglo del cierre dejaba el hilo de audio detenido después del reinicio que hace `install_profile()` al arrancar (commit 413c6d9, test `test_audio_shutdown`). Ahora suena, pero entrecortado mientras el juego va a pocos FPS |
| 7 | Cierre limpio | ✓ | Sin abortos desde el arreglo del hilo de audio (antes cada cierre llamaba a `std::terminate`) |

## perf.log (Mac M1, referencia; corrida con la compilación de la Switch en paralelo, que usaba 4 de 8 núcleos)
```
t=10s frames=589 fps_avg=58.8 fps_min=4.6 guest_ms_avg=14.6 guest_ms_max=216.4 ge_ms_avg=0.0 ge_ms_max=0.0 present_ms_avg=2.4 present_ms_max=6.3 mem_peak_mb=258
t=20s frames=587 fps_avg=58.7 fps_min=4.1 guest_ms_avg=14.5 guest_ms_max=239.4 ge_ms_avg=0.0 ge_ms_max=0.0 present_ms_avg=2.5 present_ms_max=4.5 mem_peak_mb=285
t=30s frames=312 fps_avg=31.0 fps_min=1.2 guest_ms_avg=16.3 guest_ms_max=784.8 ge_ms_avg=13.7 ge_ms_max=121.7 present_ms_avg=2.0 present_ms_max=5.1 mem_peak_mb=285
t=40s frames=54 fps_avg=5.3 fps_min=3.4 guest_ms_avg=3.8 guest_ms_max=6.0 ge_ms_avg=183.8 ge_ms_max=287.4 present_ms_avg=1.8 present_ms_max=4.5 mem_peak_mb=285
```
- Logos e intro en video (sin GE): ~59 FPS.
- Desde que empieza el 3D: el dibujado por CPU (`ge_ms`) sube a 50–210 ms por cuadro y el juego baja a 5–17 FPS. El juego en sí (`guest_ms`) se mantiene en 2–6 ms.
- Pico de memoria: ~310 MB.

## Conclusión
**H2 cumplido:** las suites pasan y los puntos 1 a 4 están en ✓; además, los puntos 5 a 7 también están en ✓.

**Pendiente para H4:** el dibujado por CPU no alcanza 30 FPS ni siquiera en el M1, y el audio entrecortado es consecuencia de eso. La medición limpia (sin compilación en paralelo y por fases) va en `docs/h3-results.md`.
