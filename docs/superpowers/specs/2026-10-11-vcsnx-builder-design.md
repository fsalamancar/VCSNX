# VCSNX v0.1.0: `vcsnx.nro` publicable + VCSNX Builder (crea `vcs_game.so` en el PC de cada jugador)

Spec aprobado por partes el 2026-10-11 y revisado el 2026-10-12. El usuario eligió partir el juego en dos, al
estilo del port de Liberty City Stories (`NaGaa95/gtalcs_nx`): un `.nro` publicable sin nada del juego y un
módulo con el código del juego que cada jugador crea con su propia copia. Desarrolla el §13.5 del spec del port
(`2026-10-11-vcsnx-switch-port-design.md`).

## 1. Objetivo

- Publicar en GitHub un **`vcsnx.nro` listo para descargar**, que contiene solo el runtime de VCSNX (renderer,
  audio, controles, splash, Vulkan, SDL, FFmpeg) y **nada del juego**.
- Dar a los jugadores de Windows un programa de ventana, **VCSNX Builder**, que crea **`vcs_game.so`** (el
  código del juego, traducido por PSPRecomp y compilado para la Switch) **una sola vez**, en su PC, desde su
  propia copia de GTA: Vice City Stories (PSP, ULUS-10160).
- Las actualizaciones posteriores son bajar un `vcsnx.nro` nuevo, sin volver a compilar, mientras no cambie la
  interfaz del módulo (§2.3).

### Lo que pidió el usuario

- Un `.exe` para **Windows** que compile el juego: "ese sería el v1".
- Una **ventana simple** titulada **"GTA VCS Compiler"** (solo texto: el logo del juego es de Rockstar), con **"bAlo productions"** abajo como crédito.
- **Todo incluido**: un `.zip` que no pide instalar nada aparte.
- **Copiar todo a la SD** si el jugador la elige, y poner **la imagen (artwork) que el jugador quiera** como ícono.
- **Partir ya en la v1**, como el port de LCS, para que el `.nro` se pueda publicar.
- Probar Windows con **Wine** en el Mac.

### Supuestos

- Windows 10/11 de 64 bits, ~3 GB libres, internet una vez (para bajar el código traducido).
- Crear `vcs_game.so` tarda ~35–60 min en un PC de 4 núcleos (medido: los 235 archivos suman 134,5 min de
  CPU en un M1; ~17 min con 8 núcleos).
- Nada derivado del juego se distribuye nunca: ni código traducido, ni `vcs_game.so`, ni arte de Rockstar.

### Fuera de la v1

Versión del Builder para Mac, texturas del Pro Controller, fuentes HD, firma de código pagada, traductor propio
(en H1 se trababa después de la unidad 0000), cargar el módulo en el Mac (el Mac sigue enlazando todo junto).

## 2. El módulo y el cargador en la consola

### 2.1 La frontera medida (build de Switch del 2026-10-12)

- El código traducido (235 objetos, 73,9 MB de `.text`) necesita de afuera **56 símbolos**: 26 del runtime de
  PSPRecomp (`psprecomp::GuestMemory::aot_store8_slow`, ...), 16 del host (`vcs::vcs_configuration`,
  `vcs::vcs_camera_axis_x`, ...), 5 del runtime de C++ (`__gxx_personality_v0`, `_Unwind_Resume`, ...) y
  funciones de C (`memcpy`, `strlen`, `sinf`, `cosf`, `sqrtf`, `asinf`, `exp2f`, `log2f`).
- El runtime necesita del código traducido **un solo símbolo**: `psprecomp::register_generated_functions(Runtime&)`.
- No hay TLS ni constructores estáticos (`.init_array`) en el código traducido. Relocaciones: `CALL26`,
  `ADR_GOT`/`LD64_GOT`, `ADD_ABS`, `ADR_PREL`, `LDST*`, `PREL32`, `JUMP26`, `ABS64`.

### 2.2 `vcs_game.so`

- ELF compartido AArch64 (`ET_DYN`), enlazado con devkitA64 con `-shared -nostdlib` a partir de los 235 objetos
  traducidos y de `module_entry.o` (código nuestro, sin nada del juego), con `libgcc` estático.
- Exporta dos símbolos C: `vcsnx_register_generated_functions(psprecomp::Runtime*)`, que llama a
  `psprecomp::register_generated_functions`, y `vcsnx_module_abi`, una cadena con la huella de §2.3.
- Deja sin resolver los 56 símbolos de §2.1, que el cargador resuelve.
- Va en la SD en `switch/vcsnx/vcs_game.so`.

### 2.3 La huella de interfaz (ABI)

SHA-256 de los headers con que se compila el código traducido (`psprecomp/*.hpp` y los tres `vcs_*.hpp` que
incluye) y de las opciones de compilación (`compile.json`). La calculan igual el kit (`kit/abi.txt`), el módulo
(compilada en `module_entry.o`) y el runtime (CMake la pasa como `VCSNX_MODULE_ABI`). Si no coinciden, el
runtime no carga el módulo y lo dice (§5). Un update del runtime que no toca esos headers ni esas opciones
mantiene la huella, y el `vcs_game.so` del jugador sigue sirviendo.

### 2.4 El cargador (`platform/switch/`)

- Adaptado del `so_util.c` de `gtalcs_nx` (MIT, © 2021 Andy Nguyen y fgsfds), con su crédito en el archivo y en
  `THIRD_PARTY.md`.
- Parte pura, probada en el Mac (`elf_module.{hpp,cpp}`): lee los `PT_LOAD` y la sección dinámica, aplica las
  relocaciones (`R_AARCH64_RELATIVE`, `ABS64`, `GLOB_DAT`, `JUMP_SLOT`) en un buffer y resuelve los símbolos con
  una tabla `{nombre, dirección}`; un símbolo que falta es un error con su nombre.
- Parte de la Switch (`game_module.{hpp,cpp}`): reserva memoria de código (`virtmemFindCodeMemory`), la mapea
  con `svcMapProcessCodeMemory` y le da permisos por segmento con `svcSetProcessMemoryPermission`, como
  `gtalcs_nx`; registra el `.eh_frame` del módulo con `__register_frame` para que las excepciones lo atraviesen.
- La tabla de importaciones (`game_module_imports.S`, generada por `tools/make_module_imports.py` a partir de
  los símbolos sin resolver del módulo) va dentro del runtime: obliga al linker a incluir esas 56 funciones y
  da sus direcciones al cargador.
- `game_module_bridge.cpp` define `psprecomp::register_generated_functions(Runtime&)` en el runtime de la
  Switch: carga el módulo y llama a su `vcsnx_register_generated_functions`. `main.cpp` del vendor no cambia.
- Antes de `main`, `nx_startup` comprueba que `vcs_game.so` exista y muestra el mensaje de §5 si no.
- El build del Mac no cambia: sigue enlazando el código traducido dentro del ejecutable.

## 3. Contenido del release

En GitHub, release **v0.1.0**, dos archivos:

1. **`vcsnx.nro`** (~38 MB): el runtime, construido por el autor sin el código traducido.
2. **`VCSNX-Builder-0.1.0.zip`** (estimado ~150 MB):

```
VCSNX-Builder-0.1.0/
  VCSNX-Builder.exe   la ventana (.NET 10, WinForms, autocontenido, un solo archivo)
  toolchain/          paquetes oficiales de devkitPro para Windows, sin modificar, solo lo que hace falta para
                      compilar y enlazar el módulo (devkita64-gcc 16.1.0, devkita64-binutils 2.46.0 y
                      devkita64-newlib, cuyos headers de C usa <cmath>), con sus licencias
  kit/                compile.json, module_link.json, abi.txt, obj/module_entry.o, include/ (los headers que
                      necesita el código traducido), vcsnx.nro (el mismo del release, para ponerle el ícono del
                      jugador) y VCSNative.ini
  hashes/             SHA-256 válidos: los dos ejecutables ULUS-10160 conocidos y los 237 archivos de generated/
  LICENSES/  README.txt
```

**Nunca entra en el release:** el código traducido, `vcs_game.so`, archivos de `PSP_DATA` o de `local/`.

### Cómo se arma (en el Mac del autor)

1. CMake (preset `switch`, opción `VCSNX_GAME_MODULE=ON`) construye el runtime `vcsnx.nro` y, para las pruebas
   del autor, su `vcs_game.so`.
2. `tools/make_builder_kit.py` arma el kit desde ese build: opciones de compilación, línea de enlace del módulo,
   huella, `module_entry.o`, headers, `vcsnx.nro` y `VCSNative.ini`.
3. `tools/fetch_windows_toolchain.py` baja y verifica los paquetes de Windows de devkitPro.
4. `dotnet publish` compila el `.exe`.
5. `tools/package_builder.py` arma el `.zip` y lo audita (§7).

## 4. El Builder

### 4.1 Arquitectura

- **`Builder.Core`** (.NET 10, sin ventana, probado en el Mac): `GameCopy` (busca y verifica el ejecutable en
  `PSP_DATA` con la lista y el orden del juego; lee `PARAM.SFO`), `Corpus` (baja el ZIP del commit `f6e7d41`
  de PSPRecomp y verifica los 237 archivos), `Compiler` (235 `.cpp` en paralelo, con huella por objeto,
  reanudable), `Linker` (enlaza `vcs_game.so`), `NroIcon` (reescribe el ícono en la sección de assets del
  `vcsnx.nro`), `IconMaker` (recorta la imagen del jugador a 256×256 JPEG), `SdCard` (copia a `switch\vcsnx\`
  sin repetir archivos iguales y sin pisar el `VCSNative.ini` del jugador), `Pipeline`.
- **`Builder.Cli`**: consola, para las pruebas en el Mac y bajo Wine.
- **`Builder.App`** (WinForms): la ventana, sin lógica propia.
- Carpetas junto al `.exe`: `trabajo\` (caché) y `salida\` (`vcs_game.so`, `vcsnx.nro`, `builder.log`).

### 4.2 La ventana

```
┌──────────────────────────────────────────────────┐
│                 GTA VCS Compiler                 │
├──────────────────────────────────────────────────┤
│  1. Tu juego (carpeta PSP_DATA)                  │
│     [C:\Juegos\VCS\PSP_DATA            ] [Elegir]│
│     ✓ GTA Vice City Stories ULUS-10160 v1.03     │
│  2. Ícono (opcional)            ┌──────┐         │
│     [Elegir imagen…] [Quitar]   │ prev │ 256×256 │
│                                 └──────┘         │
│  3. SD de la Switch (opcional)  [E:\ (SWITCH) ▾] │
│              [  Crear vcs_game.so  ]             │
│  Compilando el juego 87/235  ██████░░░░  37 %    │
│  Tiempo restante: ~24 min           [Cancelar]   │
│  ▸ Ver detalles                                  │
│  bAlo productions                                │
└──────────────────────────────────────────────────┘
```

1. Al elegir la carpeta la revisa al instante: ✓, o un mensaje con la solución ("no encontré el ejecutable", "el
   EBOOT está cifrado: desencríptalo con PPSSPP", "no es ULUS-10160").
2. Ícono opcional, con vista previa.
3. SD opcional.
4. "Crear" comprueba espacio (~3 GB) y conexión, y hace: bajar el código traducido → compilar → enlazar
   `vcs_game.so` → (con ícono) `vcsnx.nro` con el ícono → copiar a la SD.
5. Al terminar: "Listo: `vcs_game.so` y `vcsnx.nro` están en `salida\` (y en tu SD). Cópialos a
   `switch\vcsnx\` y abre VCSNX manteniendo R al abrir un juego".

Repetir con otro ícono o SD tarda segundos (no recompila ni reenlaza si nada cambió). Idioma: español si
Windows está en español, si no inglés. "Ver detalles" muestra el log, que queda en `salida\builder.log`.

## 5. Errores

| Situación | Qué pasa |
|---|---|
| Ejecutable cifrado, ausente o de otra versión | El Builder no deja pulsar "Crear" y explica cómo arreglarlo |
| La carpeta del programa tiene espacios o acentos | Mensaje: moverla a, por ejemplo, `C:\VCSNX-Builder` (`DEVKITPRO` va sin comillas en la línea de enlace) |
| Sin internet o GitHub no responde | Mensaje; acepta también un ZIP de PSPRecomp ya bajado |
| Un archivo traducido no coincide con su hash | Lo vuelve a bajar una vez; si insiste, se detiene |
| Falta espacio | Dice cuánto falta antes de empezar |
| Falla la compilación o el enlace | Se detiene, muestra el error y deja todo en el log |
| Se cierra a mitad | Al volver, reanuda con los objetos ya hechos |
| Falla la copia a la SD | Mensaje; los archivos quedan en `salida\` |
| En la consola falta `vcs_game.so` | Mensaje antes de arrancar: "Falta vcs_game.so: créalo con VCSNX Builder" |
| En la consola la huella no coincide | "vcs_game.so es de otra versión del runtime: vuelve a crearlo con VCSNX Builder" |
| En la consola falta un símbolo al cargar | Mensaje con el nombre del símbolo (indica un runtime y un módulo que no corresponden) |

## 6. Pruebas

1. **El cargador, en el Mac**: tests de `elf_module` sobre un módulo pequeño de prueba compilado con devkitA64
   (relocaciones aplicadas, símbolos resueltos, símbolo que falta).
2. **El cargador, en la consola, primero de todo**: `modprobe.nro` carga un módulo mini que llama a una función
   del runtime, lanza y atrapa una excepción que cruza la frontera y devuelve un valor; muestra PASS o FAIL.
3. **El juego con el módulo, en la consola**: `vcsnx.nro` sin el código traducido + `vcs_game.so` se juega igual
   que la run 17. Y el `.nro` publicable no define ningún símbolo del código traducido.
4. **Unitarias de `Builder.Core`** en el Mac.
5. **El Builder completo en el Mac** (`Builder.Cli` con el devkitA64 del Mac): su `vcs_game.so` tiene el mismo
   código que el del build de CMake.
6. **El Builder bajo Wine** (`Builder.Cli` de Windows con el toolchain del `.zip`): el mismo `vcs_game.so`.
   La ventana bajo Wine, para que el usuario la mire.
7. **Final en el PC con Windows del autor**: `vcs_game.so` + el `vcsnx.nro` del release se juegan en la Switch.

## 7. Licencias, aspectos legales y publicación

- `LICENSES/` lleva las licencias de devkitPro (GCC, binutils), del cargador (MIT, Andy Nguyen y fgsfds) y del
  proyecto, con las fuentes y versiones exactas. `THIRD_PARTY.md` lista también lo que va dentro de `vcsnx.nro`
  (PSPRecomp, mesa-switch, SDL2, FFmpeg LGPL 2.1, libnx, stb).
- No hay política publicada de devkitPro sobre empaquetar su toolchain; se incluyen sus paquetes sin modificar,
  con sus licencias y la fuente de cada uno.
- `tools/package_builder.py` rechaza el `.zip` si encuentra archivos de `generated/`, `vcs_game.so`, `PSP_DATA`,
  `local/` o cualquier archivo cuyo hash coincida con uno de `hashes/`. Antes de publicar, otra comprobación
  confirma que el `vcsnx.elf` del runtime no define ningún símbolo del código traducido.
- Release **v0.1.0** en `fsalamancar/VCSNX` con `vcsnx.nro` y el `.zip`, notas en inglés y español. El
  commit pasa a `public` como hasta ahora; nunca se sube el historial de `main`. El README explica el aviso de
  SmartScreen y el del antivirus.

## 8. Riesgos

| Riesgo | Mitigación |
|---|---|
| El mapeo de memoria de código falla en la consola | Es la prueba 2, antes de todo; `gtalcs_nx` usa lo mismo en modo aplicación |
| Las excepciones no atraviesan el módulo | La prueba 2 lanza y atrapa una a través de la frontera; `.eh_frame` registrado con `__register_frame` |
| Un update del runtime cambia la interfaz sin querer | La huella lo detecta y el mensaje pide recrear el módulo |
| El compilador de devkitPro para Windows no corre fuera de MSYS2 | Prueba con Wine al principio; sus `.exe` solo importan KERNEL32, msvcrt y rpcrt4 |
| Antivirus que bloquea el compilador | Lo explica el README; el log muestra el error |
| Poca RAM | Límite de compilaciones en paralelo según la RAM |
| El repositorio de PSPRecomp desaparece | Se acepta un ZIP local de ese commit, verificado con los mismos hashes |
| El kit y el compilador no coinciden | Versiones fijadas; el kit comprueba que el GCC sea 16.1.0 |

## 9. División en tareas (para el plan)

- **B0** Toolchain de Windows y prueba con Wine.
- **B1** Cargador: parte pura con tests en el Mac y `modprobe.nro` en la consola.
- **B2** Partir el build de la Switch (runtime sin código traducido + `vcs_game.so`) y jugar con el módulo.
- **B3** Kit y `Builder.Core` con sus pruebas.
- **B4** `Builder.Cli`: pruebas completas en el Mac y bajo Wine.
- **B5** `Builder.App`, la ventana.
- **B6** Licencias, empaquetado, auditoría y README.
- **B7** Prueba del autor en Windows y release v0.1.0.
