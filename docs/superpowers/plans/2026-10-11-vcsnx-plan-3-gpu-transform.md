# VCSNX Plan 3 (H7): que la GPU transforme los vértices (backend Vulkan)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Encender en la Switch el camino de transformación por hardware del fork (`HardwareTransform`, sombreadores `psp_ge_hw*.vert`), de modo que la GPU haga la transformación, el recorte y la preparación de triángulos que hoy cuestan 70–150 ms de CPU por frame, sin cambiar la imagen.

**Architecture:**
- **El camino de referencia es el actual** (*screen-space*): la CPU transforma y recorta, y el usuario lo validó en la consola ("gráficos perfectos").
- **El camino de hardware ya existe en el fork,** pero nadie lo probó con Vulkan. En el Mac se cuelga con hilos auxiliares y, con un solo hilo, dibuja triángulos gigantes sin textura (`docs/h6-results.md`).
- **Comparador por draw.** Como el modo demo no repite la misma imagen entre corridas, la comparación se hace dentro de la misma corrida: un comparador calcula para cada draw elegible el vértice en pantalla por los dos caminos y cuenta las diferencias.
  - Por el camino actual, con el decodificador de la CPU.
  - Por el de hardware, emulando en C++ el sombreador `psp_ge_hw.vert`, al estilo de `model_to_d3d()` del backend DX12.
- **Cada causa** que muestre el comparador se corrige con una prueba que primero falla, y el comparador vuelve a correr.

**Tech Stack:** lo del Plan 2. Las pruebas usan `render_ge_primitive()` con memoria falsa, como `vcs_profile_tests.cpp`, y funciones puras en `platform/switch/`.

**Spec:** `docs/superpowers/specs/2026-10-11-vcsnx-switch-port-design.md` (hito H7, agregado con este plan). Datos de partida: `docs/h6-results.md`.

## Global Constraints

- **El camino actual no cambia** mientras `HardwareTransform` esté apagado: con la opción apagada, los parches no alteran la imagen ni el costo.
- **Opción:** `PSPRECOMP_GE_GPU_HW_TRANSFORM=1/0` (la del vendor), por `vcsnx_env.txt` en la Switch.
  - **Valor por defecto:** sigue en 0 hasta que pasen los criterios de la Tarea 5.
- **El comparador** se enciende con `VCSNX_HW_VERIFY=1` y solo escribe resúmenes, una línea cada 10 s. Nada por vértice va a la SD.
- **Tolerancias del comparador:**
  - posición en pantalla: ≤ 0,5 px en x/y;
  - profundidad: ≤ 1/65535 × 2;
  - UV: ≤ 1e-4;
  - color: igual en RGBA8.
  - **Los triángulos que la CPU recorta** (`clipped`) se cuentan aparte: el recorte de la GPU es distinto por diseño.
- **Lo de siempre:** nada del juego en git, cambios en `vendor/` anotados en `UPSTREAM.md`, archivos con CRLF respetados.
- **En la Switch:** reloj de sistema (1020 MHz) para medir; `PSPRECOMP_RASTER_THREADS=1`.

## Review Focus

1. **Draws que el camino de hardware no acepta**, porque el backend devuelve "not accepted". Deben volver al camino actual en el mismo frame, sin huecos en la imagen. El comparador cuenta `hw_rejected`.
2. **Escenas con mezcla de draws de hardware y de pantalla:** los dos tipos tienen que respetar el orden y la profundidad entre sí.
3. **Iluminación por vértice** (`vertex_color_affine`) y **niebla**: colores y niebla iguales a los del camino actual dentro de la tolerancia.
4. **Recorte cerca de la cámara:** triángulos que cruzan el plano cercano no deben estirarse (el problema clásico de w ≈ 0).
5. **Decodificación en paralelo**, con más de un hilo en el pool: no debe colgarse ni escribir fuera del vector.

---

### Task 1: El crash de la decodificación en paralelo

**Files:**
- Modify: `vendor/vcs-recomp/profiles/vcs/host/ge_renderer.cpp` (camino de hardware en `render_ge_primitive`, junto a `decode_pool.run`; CRLF).
- Test: `tests/platform/test_hw_parallel_decode.cpp` (nuevo).

**Interfaces:**
- Consumes: `vcs::render_ge_primitive(...)` (`ge_renderer.hpp:94`).
- Produces: el camino de hardware que decodifica en paralelo sin escribir en el `thread_local` de otro hilo.

**Causa (verificada leyendo el código):**
- `decode_one` escribe en `decoded_vertices[i]` y lee `unique_indices[i]`, que son `static thread_local`.
- Dentro de la lambda que corre en un hilo del pool, esos nombres apuntan a la copia vacía de *ese* hilo, no a la del hilo que llamó.
- Con un solo hilo el crash desaparece (corrida `hw1` en el Mac, 150 s sin errores).

- [ ] **Step 1:** Escribir la prueba con memoria falsa, armando comandos y memoria como `vcs_profile_tests.cpp:60-120`:
  - un draw de triángulos con 512 vértices no indexados, layout `0x0115` y sin iluminación;
  - `PSPRECOMP_RASTER_THREADS=4`;
  - `PSPRECOMP_GE_GPU_HW_TRANSFORM=1`;
  - y lo necesario para que el draw sea elegible para el camino de hardware (`ge_gpu_backend_graphics_ready()` y `ge_gpu_backend_active()` en true).
  - La prueba es un ejecutable propio que enlaza `ge_renderer.cpp`, sus dependencias del vendor y un backend falso, `tests/platform/fake_gpu_backend.cpp`.
    - El backend falso es una copia de `platform/switch/ge_gpu_backend_null.cpp` que dice estar listo y cuyos `ge_gpu_backend_accumulate_hardware_triangles` y `ge_gpu_backend_accumulate_hardware_packed_0115` aceptan el draw y guardan los vértices.
    - No puede enlazarse junto al backend Vulkan, porque los dos definen las mismas funciones.
  - **Esperado:** sin crash, y los 512 vértices llegan al backend en orden.
- [ ] **Step 2:** Correrla. **Esperado:** falla (crash o vértices vacíos).
- [ ] **Step 3:** Antes de la lambda, tomar referencias locales `auto &out_vertices = decoded_vertices; const auto &indices = unique_indices;` y usarlas dentro de `decode_one`. Revisar el resto de las lambdas del pool en ese camino buscando el mismo patrón.
- [ ] **Step 4:** Correr la prueba y la suite completa. **Esperado:** todo pasa. Anotar el parche en `UPSTREAM.md`.
- [ ] **Step 5:** Commit `fix(vendor): hardware vertex decode wrote into the worker's own thread_local buffers`.

### Task 2: El comparador por draw (`VCSNX_HW_VERIFY`)

**Files:**
- Create: `platform/switch/hw_verify.hpp`, `platform/switch/hw_verify.cpp`.
- Modify: `vendor/.../ge_renderer.cpp`: una llamada con `VCSNX` en el camino de hardware, al lado de `accumulate_hardware_*`.
- Test: `tests/platform/test_hw_verify.cpp`.

**Interfaces:**
- Produces:
  - `vcsnx::HwVertexResult vcsnx::emulate_hw_vertex(const vcs::GeGpuVertex &model, const vcs::GeGpuHardwareTransform &t, float target_width, float target_height)`: port en C++ de `psp_ge_hw.vert` más la división por w y el paso a píxeles.
  - `vcsnx::HwCompare vcsnx::compare_vertex(const HwVertexResult &hw, const ScreenVertex &sw)`.
  - Un acumulador por ventana de 10 s que escribe `[hw-verify] draws=… compared=… pos_bad=… depth_bad=… uv_bad=… color_bad=… clipped=… hw_rejected=… max_pos_err_px=…`.

- [ ] **Step 1:** Pruebas de `emulate_hw_vertex` con transformaciones conocidas:
  1. **Identidad con viewport PSP** (centro 2048, escala 240/-136, offset 2048-240/2048-136): un vértice en el origen cae en el centro de la pantalla.
  2. **Una proyección simple:** el vértice en `z=-near` da profundidad 0.
  3. **UV escala/offset.**
  4. **Niebla:** `fog_end`/`fog_slope` dan el factor esperado.

  Los valores esperados se calculan a mano en la prueba. Correr y ver que fallan (no existe la función).
- [ ] **Step 2:** Implementar `emulate_hw_vertex`, copiando la matemática de `psp_ge_hw.vert` línea por línea, y `compare_vertex` con las tolerancias de Global Constraints. Correr. **Esperado:** pasan.
- [ ] **Step 3:** Conectar el comparador en `ge_renderer.cpp` (solo con `VCSNX` y con `VCSNX_HW_VERIFY=1`). Para cada draw elegible:
  1. decodificar los vértices también por el camino actual (`decode_vertex_optimized`, que ya da pantalla);
  2. emular el de hardware con la misma `GeGpuHardwareTransform` y el mismo tamaño de destino que usa el backend (`frame_extent()`);
  3. comparar vértice por vértice;
  4. acumular.
  
  El costo extra solo existe con la opción encendida.
- [ ] **Step 4:** Correr en el Mac 150 s con `PSPRECOMP_GE_GPU_HW_TRANSFORM=1 VCSNX_HW_VERIFY=1`. **Esperado:** líneas `[hw-verify]` con números distintos de cero, que son el punto de partida. Guardar la salida en el ledger.
- [ ] **Step 5:** Commit `feat(platform): per-draw comparison of the GPU vertex transform against the CPU path`.

### Task 3: Corregir posiciones y profundidad hasta `pos_bad=0`

**Files:** depende de la causa (`ge_gpu_backend_vulkan.cpp`, `ge_renderer.cpp` o `psp_ge_hw*.vert`). Test: `tests/platform/test_hw_verify.cpp`, con un caso nuevo por causa.

Bucle, una causa por vuelta (systematic-debugging):

- [ ] **Step 1:** Leer la última salida de `[hw-verify]` y el primer draw que falla. Con `VCSNX_HW_VERIFY=2` se escribe el detalle de los primeros 8 vértices de los primeros 4 draws malos de la sesión. Formular una sola hipótesis.
  - **Sospecha inicial, a confirmar o descartar con datos:** DX12 escala el viewport a la referencia de la PSP (`kReferenceWidth`, 480), y Vulkan a `frame_extent()`.
- [ ] **Step 2:** Escribir el caso de prueba que reproduce esa causa con números reales del draw. Verlo fallar.
- [ ] **Step 3:** Corregir solo esa causa. Verlo pasar y correr la suite.
- [ ] **Step 4:** Volver a correr el Mac 150 s. **Esperado:** `pos_bad` y `depth_bad` bajan. Commit `fix(vendor): <causa>`.
- [ ] **Salida del bucle:** `pos_bad=0` y `depth_bad=0` en dos corridas seguidas de 150 s.
- [ ] **Si después de 3 causas corregidas el número no baja:** parar y llevar el resultado al usuario (regla de las 3 correcciones).

### Task 4: Texturas, color y niebla hasta `uv_bad=0` y `color_bad=0`

El mismo bucle de la Tarea 3, pero con UV, color, niebla y la selección de textura. Esta última se agrega al comparador como `texture_bad`: el descriptor de textura del draw de hardware contra el del camino actual. Salida del bucle: los cuatro contadores en 0 en dos corridas seguidas.

### Task 5: Validación visual y por defecto

- [ ] **Step 1:** En el Mac, guardar frames con `PSPRECOMP_GE_GPU_DUMP_VBLANK` en las dos configuraciones, encendido y apagado, en varias escenas de la demo. Mirarlos y ponerlos lado a lado en `docs/h7-results.md`, sin subir imágenes del juego a git (solo la descripción).
- [ ] **Step 2 (USUARIO):** en el Mac, con mando, jugar 5 minutos con `HardwareTransform` encendido: ciudad de día y de noche, interiores y vehículos. Anotar cualquier diferencia.
- [ ] **Step 3:** Si no hay diferencias visibles, encender `PSPRECOMP_GE_GPU_HW_TRANSFORM` por defecto en la Switch (`nx_startup.cpp`, `setenv(..., 0)`). Commit.

### Task 6: Medir en la Switch (H7)

- [ ] **Step 1:** Subir el `.nro` por FTP. `vcsnx_env.txt` solo con `PSPRECOMP_RASTER_THREADS=1`.
- [ ] **Step 2 (USUARIO):** 3 minutos en la ciudad. Reportar FPS, sonido e imagen.
- [ ] **Step 3:** Bajar los logs y escribir en `docs/h7-results.md` la tabla contra H6 (FPS, `guest_ms`, `ge_ms`, heap). Probar también `PSPRECOMP_GE_ASYNC=1` contra 0 y dejar el mejor por defecto. Commit.

### Fuera de este plan
- HUD más chico (reutilizar el port del WidescreenFix de `ge_renderer.cpp`).
- El destello verde de la pantalla de carga (prueba A/B pendiente con `PSPRECOMP_GE_ASYNC=0`).
- Optimizar el código de vértices para el A57 (opción 2), si después de H7 sigue haciendo falta.
