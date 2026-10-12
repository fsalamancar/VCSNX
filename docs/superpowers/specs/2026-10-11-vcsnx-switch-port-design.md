# VCSNX: port de GTA Vice City Stories (PSPRecomp) a Nintendo Switch. Diseño

- **Fecha:** 2026-10-11
- **Estado:** partes 1–3 aprobadas en conversación; falta la revisión de este documento
- **Siguiente paso:** escribir el Plan 1 (§12)

## 1. Objetivo y criterio de éxito

Hacer que GTA: Vice City Stories (PSP, ULUS-10160) se pueda jugar en una Nintendo Switch con Atmosphère, como homebrew `.nro`. La base es el código de **PSPRecomp**: una recompilación estática que traduce el ejecutable de PSP a C++ nativo. No hay emulador.

- **Cómo se abre:**
  - El `.nro` va en `sdmc:/switch/` y se abre desde el menú de homebrew, ya sea hbmenu o Sphaira.
  - El usuario creará con Sphaira un forwarder: un ícono en el menú HOME que lo abre como aplicación, con toda la memoria.
  - Nosotros no fabricamos el forwarder ni ningún NSP.
- **Éxito final:** el juego se completa de principio a fin en la Switch.
- **Meta de la etapa 1 (dibujado por CPU):** al menos 30 FPS estables en modo portátil, con el reloj normal de la consola.
- **Roles:**
  - Claude escribe el código, el sistema de build y las revisiones.
  - El usuario prueba en la Switch y en su PC, compara con la versión de PC y toma las decisiones.

## 2. Contexto y hallazgos (2026-10-11, solo lectura)

### 2.1 Proyectos fuente

| Proyecto | Commit | Qué aporta |
|---|---|---|
| `jessicanataliagta/PSPRecomp` (original) | `f6e7d41` (2026-08-13) | Framework con licencia MIT y perfil `profiles/vcs`. Solo Windows: DX12, waveOut y Win32/XInput/RawInput, compilado con MSVC + AVX2 |
| `OverkillLabs3/vice-city-stories-recomp` (fork) | `b5a861d` (2026-10-05), 3 commits del autor "OverkillLabs2" | Añade `profiles/vcs/platform/{windows,macos,generic}`, una ventana SDL2 (`display_window_sdl.cpp`, 712 líneas), audio con SDL2, rutas NEON para ARM y un backend Vulkan (`ge_gpu_backend_vulkan.cpp`, 7.166 líneas). Ya compila con clang en macOS ARM |

La autoría del fork **no está verificada**: hay tres cuentas OverkillLabs casi idénticas y `OverkillLabs/PSPRecomp-Vulkan` da 404. Por eso el fork se revisa antes de usarlo (§8, H0).

### 2.2 Lo que muestra el código

- **Código traducido** (`profiles/vcs/generated/`): 237 archivos en total, 104 MB. Son 234 unidades, más el registro, una cabecera y un informe. Es C++ estándar, sin intrinsics ni extensiones de MSVC. Las unidades más grandes pesan unos 650 KB.
  - El fork tiene 236 archivos porque le falta `auto_codegen_report.json`.
  - Además, el fork editó a mano 2 unidades. En la 0096 cambió el texto de un mensaje de error y en la 0217 dejó un `fprintf` de diagnóstico. Ninguna de las dos ediciones cambia el comportamiento del juego.
  - Los scripts `optimize_generated_*.py` solo existen en el original.
- **Memoria de la PSP:** un `std::vector` de 32 MB con comprobación de límites. No usa trucos de memoria virtual.
- **Hilos de la PSP:** el runtime tiene su propio planificador. No usa fibers ni hilos del sistema por cada hilo de la PSP.
- **Dibujado:**
  - `RenderingBackend::Software` es un rasterizador por CPU bit-exact y es el respaldo permanente.
  - El backend de GPU es opcional y está detrás de unas 35 funciones libres `ge_gpu_backend_*` (`ge_gpu_backend.hpp`).
  - El rasterizador usa hilos de ayuda (`std::thread`).
- **Videos:** FFmpeg (`avformat`) más el decodificador ATRAC3P, detrás del HLE de `sceMpeg`.
- **Gráficos en la Switch:** al escribir el spec se creía que no había Vulkan para homebrew (las opciones eran OpenGL 4.3 core con switch-mesa, o deko3d). **Corrección del 2026-10-11 (H4):** `mesa-switch` (Mesa 26.2.4) trae NVK, Vulkan para el Tegra X1, como bibliotecas estáticas para libnx. Ver `docs/h3-results.md`.

### 2.3 Copia del usuario

Ruta: `~/VCSNative`. Ocupa 1,3 GB en 322 archivos y vive fuera del repo.

- `PARAM.SFO`: DISC_ID **ULUS10160**, DISC_VERSION 1.03.
- `PSP_DATA/EBOOT.BIN` es un ELF descifrado. Su SHA-256 coincide con `expected_sha256` de `vcs_ulus10160.toml`, que es el ELF del que se generó el código publicado.
- `PSP_GAME/SYSDIR/BOOT.BIN` también es un ELF. Su hash coincide con el de `prepare_game.ps1`.
- `VCSNative.exe` (2026-09-05) es una build DX12 del original, que importa d3d12, dxgi y WINMM. Sirve **solo como referencia** en el PC.
- Las 5 DLL de FFmpeg son idénticas byte a byte a las del repo original. No hay otros ejecutables.

## 3. Alcance

**Dentro:**
- Etapa 1, dibujado por CPU:
  - imagen escalada;
  - controles con distribución PSP;
  - audio, videos y partidas guardadas;
  - el Mac como banco de pruebas.
- Etapa 2: backend de GPU con **Vulkan (`mesa-switch`/NVK)**, reutilizando `ge_gpu_backend_vulkan.cpp` del fork (decisión H4, 2026-10-11; antes era OpenGL 4.3).

**Fuera (YAGNI):**
- Íconos de botones de Switch.
- Menú para reasignar controles dentro del juego.
- Texturas HD.
- Efectos extra del fork (nubes, bloom, HDR) en la etapa 1.
- Control por movimiento de los Joy-Con.
- deko3d, salvo que Vulkan con `mesa-switch` no alcance (§13).
- Builds de Windows o Mac pulidas para jugar.
- Crear forwarders o NSP: eso lo hace Sphaira.
- Soportar el modo applet (abrir desde el Álbum) si no alcanza la memoria.
- Publicar el port.

## 4. Arquitectura

```
VCSNX/
  vendor/vcs-recomp/   código del fork en b5a861d, sin generated/ ni binarios precompilados,
                       más los optimize_generated_*.py del original (f6e7d41)
  platform/switch/     código nuevo: ventana + presentación OpenGL, controles, rutas, registro de rendimiento
  cmake/               presets "mac" y "switch"
  tools/               regenerar el código traducido, preparar la SD
  docs/                spec, planes, revisión del fork
  UPSTREAM.md          origen, licencias y lista de cambios locales en vendor/
```

**Reglas:**
1. `vendor/` se toca lo mínimo. Cada cambio queda anotado en `UPSTREAM.md` para poder traer mejoras de upstream o de otros forks.
2. `platform/switch/` también compila en el Mac, excepto los archivos `nx_*.cpp`, que son exclusivos de la consola (rutas de la SD, applet, stdio por nxlink).
3. Flujo del código traducido:
   - Se parte del `EBOOT.BIN` del usuario.
   - El traductor corre en el Mac y deja el resultado en `generated/`, ignorado por git.
   - Ese código se compila dentro del `.nro`.
   - El código traducido **nunca** entra al repo.
4. El `.nro` contiene código derivado del juego. Es para uso personal y nunca se distribuye.
5. Se fija la plataforma a un commit concreto. Actualizar `vendor/` es una decisión explícita, nunca automática.

## 5. Plataforma Switch

| Pieza | Diseño |
|---|---|
| Imagen | `Backend=Software` produce un cuadro de 480×272. Se sube como textura OpenGL y se escala a 1280×720 en modo portátil o 1920×1080 en el dock, manteniendo la proporción. Son unas 200 líneas nuevas |
| Ventana | SDL2 (switch-sdl2) con contexto OpenGL (switch-mesa) |
| Audio | La ruta SDL2 que ya trae el fork: 44,1 kHz estéreo |
| Videos | switch-ffmpeg, si incluye H.264 y ATRAC3+ (se comprueba en H0). Si no, se compila un FFmpeg propio con esos decodificadores |
| Archivos | Datos del juego en `sdmc:/switch/vcsnx/PSP_DATA/`. Configuración, partidas y `perf.log` en `sdmc:/switch/vcsnx/` |
| Núcleos | La aplicación tiene 3 núcleos: el juego usa 1 y el rasterizador usa 2 hilos de ayuda |
| Memoria | Siempre en modo aplicación: mediante el forwarder de Sphaira, o desde hbmenu/Sphaira abierto manteniendo R al iniciar un juego. Si arranca en modo applet (Álbum), muestra un aviso que explica cómo abrirlo bien y se cierra. En H3 se mide el pico de memoria; si resulta que cabe en modo applet, se quita el aviso |
| Metadatos del `.nro` | Nombre "GTA Vice City Stories", autor y versión en el NACP. Son los datos que usa Sphaira para el forwarder. El ícono se genera **en el Mac del usuario** a partir de su `PSP_GAME/ICON0.PNG`, adaptado a 256×256, y nunca entra al repo. Si ese archivo no está, se usa un ícono genérico propio |

**Controles por defecto** (distribución PSP por posición):

| PSP | Switch |
|---|---|
| Stick analógico | Stick izquierdo |
| Cruceta | Cruceta |
| × | B |
| ○ | A |
| □ | Y |
| △ | X |
| L / R | L / R. ZL y ZR: a pie repiten L y R; en vehículo, ZR acelera y ZL frena (comportamiento del fork) |
| Start / Select | + / − |
| Cámara (`CameraStick` del fork) | Stick derecho |

La reasignación se hace con `[Controls]` en `VCSNative.ini`, sin menú dentro del juego.

## 6. Herramientas y flujo de trabajo

- **Mac (M1):**
  - Homebrew: `cmake`, `ninja`, `sdl2`.
  - devkitPro con pacman: `devkitA64`, `libnx`, `switch-sdl2`, `switch-mesa`, `switch-ffmpeg`, `switch-tools` (incluye `elf2nro` y `nxlink`).
- **Ciclo de trabajo:**
  1. Compilar en el Mac.
  2. Enviar el `.nro` por wifi con `nxlink`. Con Sphaira basta tenerla abierta, porque trae nxlink integrado y siempre activo. Con hbmenu hay que entrar al netloader con el botón Y.
  3. Leer la salida estándar por nxlink y `perf.log` en la SD.
- **Datos en la Switch:** se copia `PSP_DATA/` desde la copia del usuario a `sdmc:/switch/vcsnx/PSP_DATA/`. Lo automatiza un script en `tools/`.
- **Referencia:** `VCSNative.exe` en el PC del usuario, con Windows Defender activo, para comparar comportamiento.

## 7. Medición de rendimiento

- **Contador en pantalla:** el overlay de FPS que ya trae el fork.
- **Registro:** cada 10 s se añade una línea a `perf.log`. Lleva el promedio y el peor valor de los FPS (el mínimo), del tiempo de CPU del juego (ms), del rasterizador (ms) y de la presentación (ms); para los tiempos, el peor valor es el máximo. También registra el pico de memoria usada (MB), que sirve para la decisión sobre el modo applet de §5.
- **Escenas fijas:**
  1. el menú principal;
  2. la intro;
  3. un recorrido en vehículo por la ciudad, de unos 2 minutos.
- **Regla de decisión en H4:**
  - Si las tres escenas dan ≥30 FPS en modo portátil a reloj normal, se sigue con H5.
  - Si no, se mide con overclock (sys-clk) para saber cuánto falta, y el usuario decide entre optimizar las zonas más pesadas o adelantar H6.

## 8. Hitos

| # | Hito | Terminado cuando |
|---|---|---|
| H0 | Preparación | El fork está importado en `vendor/` sin `generated/` ni binarios. `docs/fork-review.md` documenta las diferencias contra el original y deja una decisión de base: fork u original. Las herramientas están instaladas. Se sabe qué decodificadores trae switch-ffmpeg |
| H1 | Regenerar el código traducido | El traductor y los scripts `optimize_generated_*.py` (tomados del original) compilan y corren en el Mac. A partir del `EBOOT.BIN` del usuario producen los 237 archivos de `generated/` del **original**, con hashes idénticos. Las 2 ediciones a mano del fork no se reproducen porque son solo diagnóstico. Si los archivos no salen idénticos, se aplica §13 |
| H2 | Prueba en el Mac | En el M1, con `platform/switch` y `Backend=Software`, el juego llega al menú y la intro se juega con mando. Las 5 suites de `ctest` pasan |
| H3 | Primera imagen en la Switch | El `.nro` muestra el menú, responde a los Joy-Con y `perf.log` tiene las 3 escenas de §7 |
| H4 | Decisión de rendimiento | Se aplicó la regla de §7 y la decisión quedó escrita en `docs/` |
| H5 | Jugable (etapa 1) | Funcionan los videos, el audio y guardar/cargar partida. Las 3 primeras misiones se juegan sin cierres |
| H6 | GPU con Vulkan (etapa 2, adelantada en H4) | El backend Vulkan del fork (`ge_gpu_backend_vulkan.cpp`) compila y corre en la Switch con `mesa-switch`; presentación por lectura de la imagen hacia la CPU (como `platform/generic`). El juego corre a 720p/1080p y los FPS son al menos los de la etapa 1. La GPU no es bit-exact, así que la imagen se valida comparando capturas lado a lado contra el dibujado por CPU: no debe haber diferencias visibles |
| H7 | La GPU transforma los vértices (decidido por el usuario el 2026-10-11, tras H6) | En H6 el cuello de botella resultó ser la CPU: unos 60.000 vértices por frame, transformados y recortados en un núcleo (70–150 ms por frame a 1020 MHz), lo que pone el juego en cámara lenta y deja el audio sin datos. Terminado cuando: (1) el camino `HardwareTransform` del fork dibuja con Vulkan; (2) un comparador por draw dentro de la misma corrida (`VCSNX_HW_VERIFY`) no encuentra diferencias de posición, profundidad, UV ni color frente al camino actual, salvo los triángulos que la CPU recorta; (3) en el Mac no hay diferencias visibles al jugar; (4) en la Switch está encendido por defecto y `docs/h7-results.md` registra los FPS frente a H6 |
| Final | Completo | El juego se completa de principio a fin. Existen instrucciones para preparar la SD. El `.nro` tiene nombre, autor e ícono, y su forwarder de Sphaira abre el juego en modo aplicación |

## 9. Pruebas

- **Del proyecto:** las 5 suites de `ctest` que ya trae deben pasar en el Mac en cada hito a partir de H2.
- **Regeneración:** el hash de cada archivo de `generated/` se compara contra el publicado (H1).
- **Igualdad de imagen entre equipos:** el rasterizador por CPU es determinista. Se capturan los mismos cuadros (con `framebuffer_capture` del host) en el Mac y en la Switch y deben ser idénticos. Si difieren, la causa está en el compilador (GCC frente a clang) o en las opciones de compilación.
- **Código nuevo:** pruebas automáticas en el Mac para el mapeo de controles, las rutas y el formato de `perf.log`.
- **Pruebas de juego:** el usuario sigue en la Switch una lista corta de comprobaciones por hito, que viene en cada plan.

## 10. Riesgos

| Riesgo | Señal | Respuesta |
|---|---|---|
| La CPU de la Switch no alcanza | Menos de 30 FPS en H3 | Medir con overclock, optimizar las zonas más pesadas o adelantar H6 |
| GCC (devkitA64) sufre con unidades de 650 KB o cambia el comportamiento | Compilación muy lenta, falta de memoria o capturas distintas | Bajar optimización o inlining en las unidades frías (como ya hace el CMake del perfil), compilar en paralelo, comparar capturas |
| switch-ffmpeg no trae H.264 o ATRAC3+ | Videos sin imagen o sin sonido | Compilar un FFmpeg propio con esos decodificadores |
| El juego se abre en modo applet | Falta de memoria al arrancar | Detectarlo al iniciar y mostrar el aviso de §5. La solución para el usuario es el forwarder de Sphaira |
| La revisión del fork encuentra algo sospechoso | Código no explicado en el diff | Usar el original y portar nosotros las piezas de ARM y SDL |
| La regeneración no sale idéntica | Hashes distintos en H1 | §13 |
| Take-Two | — | §11 |

## 11. Higiene del repo y aspectos legales

- El repo es local y privado, sin remoto. Si se crea uno, debe ser privado.
- Nunca se suben archivos del juego, código traducido, `.nro`, `.exe` ni logs. Lo garantiza el `.gitignore`, comprobado con `git check-ignore`.
- `vendor/` conserva las licencias y avisos de origen (MIT del framework y los avisos de terceros del perfil). `UPSTREAM.md` registra de dónde viene cada cosa.
- FFmpeg (LGPL) queda enlazado estáticamente en el `.nro`. Como no se distribuye, no hay obligaciones de redistribución.
- Nada del proyecto se publica.

## 12. División en planes

- **Plan 1:** H0 a H3, hasta la primera imagen en la Switch con números de rendimiento.
- **Plan 2:** lo que se decida en H4: H5, o una fase de optimización.
- **Plan 3:** H6, la GPU con Vulkan (`mesa-switch`). **H4 decidió adelantarlo:** pasa a ser el Plan 2, y H5 va después.
- **Plan 3:** H7, la transformación de vértices en la GPU (`docs/superpowers/plans/2026-10-11-vcsnx-plan-3-gpu-transform.md`).
- **Plan final:** pulido e instrucciones.

## 13. Decisiones abiertas

1. **Base de `vendor/`:** por defecto el fork. Se confirma o se cambia al original según `docs/fork-review.md` (H0).
2. **Regeneración no idéntica:**
   - Primero se revisa si el orden o las opciones de los scripts `optimize_generated_*.py` explican la diferencia.
   - Si no coincide, se usa localmente el `generated/` publicado del original, igual fuera del repo.
   - En ambos casos la diferencia se documenta en `docs/`.
3. **Apuntar y disparar:** si L/R resultan incómodos en la prueba de juego, se ajusta `[Controls]` después de H3.
4. **deko3d:** solo si en H6 Vulkan con `mesa-switch` no alcanza los FPS esperados.
5. **Conversor para el usuario (idea futura, con su propio spec y fuera de los planes 1–3):**
   - Sería un programa, al estilo de otros ports de PC, que tome la copia del juego de cada usuario y produzca el `.nro` en su propio equipo.
   - Así nunca se distribuye nada derivado del juego.
   - Tendría que incluir el traductor, el toolchain de devkitA64 (o equivalente) y la capa `platform/switch` precompilada.
   - Los scripts de `tools/` de H1 y H3 se escriben de forma que luego se puedan reutilizar ahí.

## Apéndice A: hashes (SHA-256)

| Archivo | SHA-256 |
|---|---|
| `PSP_DATA/EBOOT.BIN` (ELF descifrado) | `85c5e172fa0207086b72a37649e9a10d47845f0866b0dee179c48d930e43d17f` |
| `PSP_GAME/SYSDIR/BOOT.BIN` | `fee2e86c7fe457ab6da463d9fdc16f156a0900adcdaa2fd260206c11907c5d65` |
| `PSP_GAME/SYSDIR/EBOOT.BIN` (cifrado) | `6f5a369c334a35811792c82976e44c9648b7bff00fd958e492d518b5b3464dd6` |
| `PSP_GAME/PARAM.SFO` | `2bb27a13cc037f4c640516b82ff9ce1d2fe5fbe12204339a5d230ca9545ae835` |
| `VCSNative.exe` (referencia) | `0b46dd8cf4c6f67e00580771fb6cc7616705ffa68b326b8e5bb300c2f97d106d` |

## Apéndice B: referencias

- PSPRecomp: https://github.com/jessicanataliagta/PSPRecomp
- Perfil VCS: https://github.com/jessicanataliagta/PSPRecomp/tree/main/profiles/vcs
- Fork Vulkan/macOS: https://github.com/OverkillLabs3/vice-city-stories-recomp
- deko3d: https://github.com/devkitPro/deko3d
- mesa-switch (NVK Vulkan + GL para libnx): https://github.com/danfromtico/mesa-switch
- Sphaira (reemplazo de hbmenu, forwarders, nxlink integrado): https://github.com/ITotalJustice/sphaira
- Mesa/OpenGL para Switch (devkitPro): https://devkitpro.org/viewtopic.php?p=16123
