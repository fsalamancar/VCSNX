# Revisión del fork OverkillLabs3 (b5a861d) frente a PSPRecomp (f6e7d41)

- **Fecha:** 2026-10-11.
- **Método:** Tarea 3 del Plan 1, con estos pasos:
  - `diff -rq` del original (`.cache/upstream`) contra `vendor/vcs-recomp`, sin `generated/`;
  - búsqueda de llamadas a procesos o red, URLs y blobs en los 95 archivos añadidos o cambiados;
  - lectura de los scripts y launchers;
  - diff completo del núcleo (`src/`, `include/`) y del traductor.
- **No se ejecutó ningún script del fork.**

## Resumen
- **Entradas en el diff (sin `generated/`):** 74.
  - Solo en el fork: 26.
  - Modificados: 37.
  - Solo en el original: 11, que el fork eliminó. Entre ellos están `host/display_window.cpp` (reemplazado por `platform/*`), los binarios de FFmpeg para Windows, `tools/ghidra` y varios scripts de verificación.
- **El tamaño del diff engaña.** `vcs_profile.cpp` y `ge_renderer.cpp` parecen reescritos (21.436 y 10.265 líneas distintas), pero casi todo es el cambio de fin de línea a CRLF. Sin los `\r`, los cambios reales son 730 líneas en `vcs_profile.cpp`, 255 en `ge_renderer.cpp`, 554 en `audio_output.cpp` y 157 en `main.cpp`.
- **`generated/`:** el fork tiene 236 archivos. Le falta `auto_codegen_report.json` y editó a mano las unidades 0096 (texto de un mensaje de error) y 0217 (un `fprintf` de diagnóstico). VCSNX no usa esos archivos: H1 regenera contra el original.

## Hallazgos

| Categoría | Archivo:línea | Qué hace | ¿Entra en vcsnx? | Riesgo |
|---|---|---|---|---|
| A procesos/red | `launcher/VCSLauncher.swift:269` | `Process()` que ejecuta `/usr/bin/ditto -x -k` para descomprimir un pack de texturas que elige el usuario | no (launcher de macOS) | bajo |
| A procesos/red | `launcher/VCSLauncher.swift:1082,1095` | Abre el ejecutable del juego dentro de la propia app | no | bajo |
| A procesos/red | `launcher/windows/VCSLauncher.cs:924,953` | `Process.Start` de `VCSNative.exe` | no | bajo |
| A procesos/red | `launcher/VCSLauncher.swift:406,477,507,963` | Falsos positivos (`.font(.system(...))`) | no | ninguno |
| B URLs | `scripts/build_ffmpeg_macos.sh:23`, `scripts/macos_bundle.py:173` | `curl` del tarball oficial `https://ffmpeg.org/releases/ffmpeg-<versión>.tar.xz` para el build de Mac | no (no se ejecutan) | bajo |
| B URLs | `third_party/vcruntime/README.txt:6` | Enlace a `vc_redist.x64.exe` de Microsoft (solo texto; las DLL se excluyeron al importar) | no | ninguno |
| B URLs | `README.md`, `BUILDING.md`, `CMakeLists.txt`, `SMAA.h`, `stb_image*.h`, manifests y `Info.plist` | Referencias a documentación y esquemas XML | no | ninguno |
| C blobs | (sin resultados) | No hay cadenas base64 largas en el código | — | — |
| D scripts | `scripts/macos_bundle.py` | Empaqueta la app de macOS: `subprocess` para `xattr -cr` y `codesign --sign -` (firma ad-hoc local) | no | bajo |
| D scripts | `scripts/INSTALL_TEXTURE_PACK.bat`, `UNINSTALL_TEXTURE_PACK.bat` | PowerShell que descomprime o borra un pack de texturas dentro de la carpeta `Textures` del juego | no | bajo |
| D scripts | `scripts/package_release.bat` | Arma el zip del release; borra solo su propia carpeta de paquete | no | bajo |
| D scripts | `scripts/build_macos.sh`, `launcher/windows/build_launcher.*`, `build_fast.bat`, `build_release.bat` | Compilan el juego y los launchers | no | bajo |
| Escritura de archivos | `host/vcs_key_prompts.cpp:323-326` | Escribe un `ENGLISH.GXT` modificado (textos con nombres de teclas) en `temp_directory_path()/VCSNative_prompts` | sí, pero solo con `KeyboardPrompts=true`; VCSNX usa `false`. Si falla, devuelve `nullopt` sin romper nada | bajo |
| Escritura de archivos | `framebuffer_capture.cpp`, `audio_output.cpp`, `vcs_runtime_log.cpp`, `vcs_project2dfx.cpp`, `vcs_profile.cpp` | Volcados y logs de diagnóstico | sí (en la carpeta de datos o en la que indique el usuario) | bajo |
| Núcleo `include/` + `src/decoder.cpp` | `decoder.hpp:23`, `decoder.cpp:70-79` | Nuevo `OpcodeKind::Break`: decodifica `break` (SPECIAL funct 0x0D), que antes caía en "special?" | sí (núcleo) | bajo |
| Traductor | `tools/codegen_main.cpp:92-99`, `profiles/vcs/tools/vcs_codegen_main.cpp:93-100` | Traduce `break` como no-op (`// break (unhandled trap; treated as a no-op)`) en vez de detener el runtime. **El `generated/` publicado por el fork no se regeneró con este cambio**, así que su release no lo probó | solo afecta al código regenerado (ver la decisión de H1 abajo) | **medio** |
| Núcleo `src/runtime.cpp` | `runtime.cpp:1169-1183`, `1201-1209` | Antes del hook post-import, adelanta `ctx.pc` a la dirección de retorno si el import no lo hizo. Evita que una llamada se ejecute dos veces cuando el hook desvía el hilo a un callback del juego | sí | bajo (corrige un bug y está acotado) |

## Decisión
- **Base:** fork.
- **Razón:** no hay ningún hallazgo de riesgo alto en código que entre en vcsnx (`host/`, `platform/`, `src/`, `include/`). Lo riesgoso son scripts y launchers, que no se ejecutan ni se compilan.
- **Regla aplicada:** se usa el fork salvo que exista un hallazgo de riesgo alto en código que entra en vcsnx.
- **Consecuencia para H1 (Tarea 5):**
  - El traductor del fork genera código distinto al publicado en cada unidad que contenga `break`.
  - Para cumplir el criterio del spec (código idéntico al `generated/` del original) y usar el comportamiento probado en los releases, H1 regenera también con el traductor del original (`f6e7d41`).
  - El `generated/` que entra en el build es el que coincide con el original.
  - La traducción de `break` como no-op queda como opción documentada en `docs/regen-report.md`; no se adopta en el Plan 1.
