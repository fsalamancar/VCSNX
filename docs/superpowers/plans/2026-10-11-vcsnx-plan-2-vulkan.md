# VCSNX Plan 2 (H6 adelantado): dibujado por GPU con Vulkan (`mesa-switch`)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Que VCSNX dibuje con la GPU de la Switch: el backend Vulkan del fork (`ge_gpu_backend_vulkan.cpp`) corriendo sobre NVK (`mesa-switch`), con resolución interna de 720p y FPS muy por encima de los 1–3 de la etapa 1.

**Architecture:**
- **Vulkan en la Switch:** `mesa-switch`, fijado en un commit, se compila en Docker y deja bibliotecas estáticas en `.cache/mesa-switch-sdk`, ignorada por git. VCSNX las enlaza en el `.nro`.
- **Backend:** el del fork tal cual. Presenta por lectura de la imagen hacia la CPU, con la superficie `platform/generic` del fork, y nuestra ventana SDL ya muestra esa imagen (`display_window_present_rgba`). No hace falta WSI ni swapchain.
- **Mac:** el mismo backend corre sobre MoltenVK, como banco de pruebas.
- **Respaldo:** si Vulkan falla al iniciar, el backend vuelve solo al dibujado por CPU (línea 4960 del backend).

**Tech Stack:**
- Todo lo del Plan 1.
- `mesa-switch` 26.2.4 (NVK) y Docker Desktop (imagen ARM64 de devkitPro).
- Homebrew: `shaderc` (para `glslc`), `molten-vk`, `vulkan-loader`, `vulkan-headers`.

**Spec:** `docs/superpowers/specs/2026-10-11-vcsnx-switch-port-design.md` (H6 actualizado en cd40bc9). Resultado de la etapa 1: `docs/h3-results.md`.

## Global Constraints

- **`mesa-switch`:** `https://github.com/danfromtico/mesa-switch`, commit `606441c90e459028a50fca352daebfc20211f890`. Nunca se sube al repo: se clona y compila en `.cache/`.
- **Bibliotecas extra a enlazar en la Switch** (de `VulkanConfig.cmake`):
  - `expat`, `zstd`, `z`, `nx`, `stdc++`, `m`; `elf` solo si el build lo pide (es opcional en Mesa);
  - todo en grupo `--start-group/--end-group`;
  - más `-Wl,-u,vk_icdGetInstanceProcAddr -Wl,-u,vk_icdNegotiateLoaderICDInterfaceVersion -pthread`.
- **API Vulkan:** el backend pide Vulkan 1.1 (`VK_API_VERSION_1_1`) y usa `samplerAnisotropy` solo si existe.
- **Presentación:** lectura hacia la CPU. `swapchain_default_enabled() == false`, mediante `platform/generic/vulkan_platform_stub.cpp`.
- **Resolución interna:** `InternalResolutionMode=Custom`, 1280×720. Los efectos extra siguen apagados, como en la etapa 1.
- **GE asíncrono:** apagado al empezar (`PSPRECOMP_GE_ASYNC=0`) para cambiar una sola variable a la vez. Se evalúa en la Tarea 8.
- **Lo de siempre:** nada del juego ni de Mesa en git, y los cambios en `vendor/` se anotan en `UPSTREAM.md`.
- **Pruebas en la Switch:** el `.nro` se sube por FTP de DBI (`switch/vcsnx/vcsnx.nro`, archivo suelto) y los logs se bajan por FTP. nxlink no funciona en esta red.

## Review Focus

1. **NVK no inicia en la consola** (driver, memoria o versión). El juego debe seguir corriendo con el dibujado por CPU y dejar en el log por qué falló Vulkan. Se ve en la Tarea 3 (sonda) y en la Tarea 7 (log `ge backend active=`).
2. **La GPU termina pero la lectura hacia la CPU llega vacía o desfasada** (sincronización en la Switch). La sonda comprueba píxeles concretos después de esperar el fence (Tarea 3).
3. **Memoria:** el caché de texturas del backend (`TextureCacheMB=512`) más Mesa pueden no caber junto al juego. El pico se mide con `mallinfo` (Tarea 6), porque `InfoType_UsedMemorySize` cuenta todo el heap reservado.
4. **Las bibliotecas estáticas de Mesa chocan con las de VCSNX** (símbolos duplicados de zlib o FFmpeg, o `stdc++`). La Tarea 4 enlaza todo y su build tiene que dar 0 errores.
5. **Compilar los shaders en un Mac sin `glslc`:** CMake tiene que fallar con un mensaje claro, no con un error críptico (Tarea 4).

---

### Task 1: Herramientas para Vulkan (Mac, Docker y portlibs)

**Files:** Modify `tools/check_toolchain.sh` and `docs/toolchain.md`.

- [ ] **Step 1:** El agente instala con Homebrew: `brew install shaderc molten-vk vulkan-loader vulkan-headers`.
- [ ] **Step 2 (USUARIO):** `sudo dkp-pacman -S --needed switch-libexpat switch-libzstd`
- [ ] **Step 3:** Añadir a `tools/check_toolchain.sh` estas comprobaciones: `glslc`, `docker info`, `vulkaninfo` o `/opt/homebrew/lib/libvulkan.dylib`, y `libexpat.a` y `libzstd.a` en portlibs. Correrlo. Expected: sin `MISSING`.
- [ ] **Step 4:** Actualizar `docs/toolchain.md` con la salida y hacer commit `chore: add Vulkan toolchain checks`.

### Task 2: SDK de `mesa-switch` reproducible

**Files:** Create `tools/build_mesa_switch.sh`. Modify `tools/lib/pinned.sh` and `docs/toolchain.md`.

- [ ] **Step 1:** Añadir a `tools/lib/pinned.sh`:
  - `MESA_SWITCH_URL=https://github.com/danfromtico/mesa-switch.git`
  - `MESA_SWITCH_COMMIT=606441c90e459028a50fca352daebfc20211f890`
- [ ] **Step 2:** Escribir `tools/build_mesa_switch.sh` con estos pasos:
  1. `fetch_pinned` a `.cache/mesa-switch`, saltándolo si ya está en el commit.
  2. Correr su `build-switch.sh` (Docker).
  3. Copiar a `.cache/mesa-switch-sdk/lib/` todas las bibliotecas `.a` que lista el `ninja` de su script (19 archivos).
  4. Copiar a `.cache/mesa-switch-sdk/include/` los encabezados de Khronos (`include/vulkan`, `include/vk_video`).
  5. Comprobar que existen `libvulkan.a` y `vulkan/vulkan.h`, e imprimir el tamaño total.
- [ ] **Step 3:** Correrlo; la primera vez tarda mucho porque arma la imagen. Expected: `mesa-switch SDK ready: .cache/mesa-switch-sdk (N archives)`.
- [ ] **Step 4:** Si el build de Mesa pide `libelf` o portlibs que el Mac no tiene: copiarlos desde el contenedor (`docker cp mesa-switch-build:/opt/devkitpro/portlibs/switch/lib/<lib>.a`) hacia `.cache/mesa-switch-sdk/lib`, y anotarlo en `docs/toolchain.md`.
- [ ] **Step 5:** Commit `feat(tools): build the pinned mesa-switch Vulkan SDK in Docker`.

### Task 3: Sonda Vulkan en la Switch (`vkprobe.nro`)

**Files:** Create `tools/vkprobe/vkprobe.cpp` and `cmake/vcsnx_vkprobe.cmake`. Modify `CMakeLists.txt`.

`vkprobe` es un `.nro` aparte, de unos pocos cientos de líneas, que valida NVK de punta a punta antes de tocar el juego. Escribe en la consola de libnx y en `sdmc:/switch/vcsnx/vkprobe.log`:

1. `vkCreateInstance` con API 1.1, y `vkEnumeratePhysicalDevices`. Imprime `deviceName`, `apiVersion`, `driverVersion`, `maxImageDimension2D`, los heaps de memoria y las familias de colas.
2. `vkCreateDevice` con una cola gráfica y un command pool.
3. Crea una imagen `R8G8B8A8_UNORM` de 256×256 (tiling óptimo), la limpia con `vkCmdClearColorImage` a (0.25, 0.5, 0.75, 1.0), la copia a un buffer visible por el host, la envía con fence y espera.
4. Lee el buffer y comprueba los píxeles (0,0) y (255,255) contra (64,128,191,255) ±1. Imprime `PASS` o `FAIL` con los valores leídos.
5. Espera a que el usuario pulse + para salir.

- [ ] **Step 1:** Escribir `tools/vkprobe/vkprobe.cpp`, que hace exactamente lo anterior: libnx `consoleInit`, `PadState` y `appletMainLoop`, y Vulkan C. Cada `VkResult` distinto de `VK_SUCCESS` se imprime con su nombre y aborta la secuencia, dejando el mensaje en pantalla.
- [ ] **Step 2:** En `cmake/vcsnx_vkprobe.cmake` (solo Switch):
  - `add_executable(vkprobe tools/vkprobe/vkprobe.cpp)`;
  - enlazar las bibliotecas de `.cache/mesa-switch-sdk/lib` en `LINK_GROUP:RESCAN`, más `expat zstd z nx stdc++ m`;
  - `target_link_options(-Wl,-u,vk_icdGetInstanceProcAddr -Wl,-u,vk_icdNegotiateLoaderICDInterfaceVersion -pthread)`;
  - `nx_generate_nacp` y `nx_create_nro` con el nombre "VCSNX Vulkan probe".
- [ ] **Step 3:** Compilar `cmake --build --preset switch --target vkprobe_nro`. Expected: `vkprobe.nro` generado. Cada símbolo sin definir se resuelve agregando la portlib que falta (Task 2, Step 4).
- [ ] **Step 4 (USUARIO):**
  1. El agente sube `vkprobe.nro` por FTP a `switch/vcsnx/vkprobe.nro`.
  2. El usuario lo abre desde hbmenu **con R + juego**, lee la pantalla o manda una foto, y sale con +.
  3. El agente baja `vkprobe.log`.

  Expected: `PASS` y un `deviceName` de NVK.
- [ ] **Step 5:** Guardar el log en `docs/vkprobe-results.md` y hacer commit `feat(tools): Vulkan probe for NVK on the Switch`.
- [ ] **Si sale `FAIL` o se cuelga:** parar y llevar el resultado al usuario. El plan B es OpenGL (NVC0 de la misma `mesa-switch`) o deko3d, y es decisión suya (spec §13.4).

### Task 4: Backend Vulkan en el build de VCSNX

**Files:** Create `cmake/vcsnx_vulkan.cmake`. Modify `cmake/vcsnx_game.cmake` and `CMakeLists.txt`. Test: `tests/platform/test_gpu_backend_selection.cpp`, if a pure part arises.

- [ ] **Step 1:** Opción `VCSNX_GPU_BACKEND` (`null` o `vulkan`; valor por defecto `vulkan`).
  - **Con `vulkan`:** se compilan `host/ge_gpu_backend_vulkan.cpp` y `platform/generic/vulkan_platform_stub.cpp` del vendor en lugar de `ge_gpu_backend_null.cpp`.
  - **Con `null`:** todo queda como en el Plan 1. Esto evita romper el banco de pruebas mientras tanto.
- [ ] **Step 2: Shaders.** Copiar del CMake del fork (`profiles/vcs/CMakeLists.txt`) la lista `VCS_SHADER_STEMS/EXTS/SYMBOLS` y la regla `glslc -O -mfmt=c --target-env=vulkan1.1`, que genera `<stem>_<ext>_spv.h` en `${CMAKE_BINARY_DIR}/vcs_shaders`.
  - `find_program(VCSNX_GLSLC glslc HINTS /opt/homebrew/bin)`.
  - Si no está: `message(FATAL_ERROR "glslc not found: brew install shaderc (needed by VCSNX_GPU_BACKEND=vulkan)")` (Review Focus 5).
- [ ] **Step 3: Enlace.**
  - **Mac:** `find_package(Vulkan REQUIRED)` (vulkan-loader de Homebrew + MoltenVK).
  - **Switch:** el mismo grupo de `.cache/mesa-switch-sdk` que la Tarea 3, factorizado en una función `vcsnx_link_mesa_vulkan(target)` en `cmake/vcsnx_vulkan.cmake`, que también usa `vkprobe`.
- [ ] **Step 4:** Compilar los dos presets y correr `ctest --preset mac`. Expected:
  - las 16 pruebas pasan, salvo `test_gpu_backend_null`, que se compila solo con `VCSNX_GPU_BACKEND=null`;
  - el `.nro` enlaza con 0 errores (Review Focus 4).
- [ ] **Step 5:** Commit `feat(build): select the fork's Vulkan GE backend (MoltenVK on Mac, mesa-switch on Switch)`.

### Task 5: Configuración y prueba en el Mac (MoltenVK)

**Files:** Modify `config/VCSNative.ini`, `tools/run_mac.sh` and `platform/switch/nx_startup.cpp`.

- [ ] **Step 1:** En `config/VCSNative.ini`:
  - `[Rendering] Backend=Vulkan`;
  - `InternalResolutionMode=Custom`, `InternalWidth=1280`, `InternalHeight=720`.
- [ ] **Step 2:** Poner `PSPRECOMP_GE_ASYNC=0` por defecto: `setenv(..., 0)` en `nx_startup.cpp` y `export` en `run_mac.sh` si no viene definido.
- [ ] **Step 3:** Correr en el Mac 60 s. Expected:
  - `VCSNative.log` contiene `ge backend requested=Vulkan active=Vulkan`;
  - se ve la imagen a 1280×720;
  - `perf.log` muestra FPS mucho mayores que con CPU en las escenas 3D.
- [ ] **Step 4:** Commit `feat(config): Vulkan backend at 720p internal resolution`.

### Task 6: Memoria real en `perf.log` (`mallinfo`)

**Files:** Modify `platform/switch/memory_usage.cpp` and `.hpp`. Test: `tests/platform/test_memory_usage.cpp`.

- [ ] **Step 1 (RED):** Test nuevo: después de reservar y tocar 64 MiB, `memory_in_use_mb()` sube al menos 60. En el Mac se usa `mstats()` o `malloc_zone_statistics`; en la Switch, `mallinfo().uordblks`.
- [ ] **Step 2 (GREEN):** Implementar `memory_in_use_mb()` y hacer que `perf.log` la use en lugar de `memory_used_mb()`. Esta última deja de servir en la Switch porque libnx reserva todo el heap al arrancar.
- [ ] **Step 3:** Commit `fix(platform): report heap in use, not the pre-reserved heap`.

### Task 7: Primera corrida Vulkan en la Switch (H6)

- [ ] **Step 1:** Compilar el `.nro`, borrar `vcsnx_env.txt` de la SD si existe y subir el `.nro` por FTP.
- [ ] **Step 2 (USUARIO):** abrir con R + juego. Dejarlo en el menú 30 s, ver la intro y, si llega, manejar 1 minuto. Contar qué se ve, cómo suena y si hay fallos gráficos. Cerrar con HOME.
- [ ] **Step 3:** Bajar `VCSNative.log`, `perf.log` y `vcsnx_boot.log`.
  - Si dice `active=Software`, hubo respaldo: leer el motivo en el log y corregir.
- [ ] **Step 4:** Escribir `docs/h6-results.md` con la tabla de FPS, `ge_ms`, `guest_ms` y memoria real frente a la etapa 1 y al Mac, la checklist y los problemas visuales con capturas si las hay. Commit.

### Task 8: GE asíncrono y fijación de núcleos (solo si la Tarea 7 lo justifica)

- [ ] Si `guest_ms + ge_ms` en serie supera 33 ms, y la suma de cada uno por separado cabría en paralelo, activar `PSPRECOMP_GE_ASYNC=1`.
- [ ] Fijar el hilo `ge_async` del vendor al núcleo 2. Es un parche mínimo en `vcs_profile.cpp`, donde arranca el worker (`pin_current_thread(ThreadRole::RasterWorker, 1)` o un rol nuevo `GeWorker` con test en `test_threads`), anotado en `UPSTREAM.md`.
- [ ] Medir otra vez en la Switch y anotar en `docs/h6-results.md`.

### Fuera de este plan
- Presentación directa con WSI/swapchain sobre NWindow, sin lectura hacia la CPU. Se considera solo si la lectura pesa en `present_ms`.
- 1080p en el dock, texturas HD y efectos (nubes, bloom).
- Las tareas pendientes de H3: el mensaje del Álbum y la comparación de imagen Mac/Switch.
