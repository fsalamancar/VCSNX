# VCSNX 0.1.1

Fixes the **black screen when saving** and the **"loading" that never ends** (for example when entering a
building) that appeared after a while of play, around mission 2. Threads that end with
`sceKernelExitDeleteThread` kept their stack; after enough sound banks the game could no longer create the
thread that loads them, and waited forever.

**Updating from 0.1.0:** download **`vcsnx.nro`** and replace `sdmc:/switch/vcsnx/vcsnx.nro` on your SD card.
Your `vcs_game.so` keeps working: you do not need to run the Builder again. (If you made `vcsnx.nro` with your
own icon, the new one has the default icon; to get yours back, open the new Builder with your image: it only
takes seconds.)

**New players:** download the Builder once, [`VCSNX-Builder-0.1.0.zip`](https://github.com/fsalamancar/VCSNX/releases/download/v0.1.0/VCSNX-Builder-0.1.0.zip) (it does not change with updates),
and the newest runtime, [`vcsnx.nro`](https://github.com/fsalamancar/VCSNX/releases/latest/download/vcsnx.nro); unzip the Builder and copy `vcsnx.nro` into its `kit` folder,
replacing the one there, then follow the README. You need the `PSP_DATA` folder of the VCS PC port (VCSNative), set up with your own copy
(ULUS-10160, US). No game files are included.

**If something goes wrong:** the logs of the session that failed are kept as `vcsnx_boot.prev.log` and
`vcsnx_stderr.prev.log` in `sdmc:/switch/vcsnx/`, and a load that never ends writes `vcsnx_stall.log`. Attach
them when you report a problem.

---

Arregla la **pantalla negra al guardar** y el **"loading" que nunca termina** (por ejemplo al entrar a un
edificio), que aparecían tras un rato de juego, hacia la misión 2. Los hilos que terminan con
`sceKernelExitDeleteThread` se quedaban con su pila; tras suficientes bancos de sonido el juego ya no podía crear el
hilo que los carga y esperaba para siempre.

**Si tienes la 0.1.0:** baja **`vcsnx.nro`** y reemplaza `sdmc:/switch/vcsnx/vcsnx.nro` en tu SD. Tu `vcs_game.so`
sigue sirviendo: no hace falta volver a usar el Builder. (Si hiciste tu `vcsnx.nro` con tu propio ícono, el nuevo
trae el de por defecto; para recuperar el tuyo, abre el Builder nuevo con tu imagen: tarda segundos.)

**Si es la primera vez:** baja el Builder una sola vez, [`VCSNX-Builder-0.1.0.zip`](https://github.com/fsalamancar/VCSNX/releases/download/v0.1.0/VCSNX-Builder-0.1.0.zip) (no cambia con las
actualizaciones), y el runtime más nuevo, [`vcsnx.nro`](https://github.com/fsalamancar/VCSNX/releases/latest/download/vcsnx.nro); descomprime el Builder y copia `vcsnx.nro` dentro de
su carpeta `kit`, reemplazando el que está, y sigue el README. Necesitas la carpeta `PSP_DATA` del port de PC de VCS (VCSNative), armada con tu propia copia
(ULUS-10160, EE. UU.). No incluye archivos del juego.

**Si algo falla:** los logs de la sesión que falló quedan como `vcsnx_boot.prev.log` y `vcsnx_stderr.prev.log` en
`sdmc:/switch/vcsnx/`, y una carga que nunca termina escribe `vcsnx_stall.log`. Adjúntalos al reportar un problema.
