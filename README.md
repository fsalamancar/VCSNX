# VCSNX — GTA: Vice City Stories on Nintendo Switch

A native Nintendo Switch port of **Grand Theft Auto: Vice City Stories** (PSP, ULUS-10160). It is not
an emulator: the game's code is statically recompiled to C++ by
[PSPRecomp](https://github.com/jessicanataliagta/PSPRecomp) and the
[vice-city-stories-recomp](https://github.com/OverkillLabs3/vice-city-stories-recomp) profile, and this
project builds it for the Switch with devkitPro, SDL2 and a Vulkan renderer (Mesa NVK).

> **No game files are included, and none are ever downloaded for you.** You need your own legally
> obtained copy of the PSP game (UMD or PSN, US version ULUS-10160). This project is not affiliated
> with or endorsed by Rockstar Games or Take-Two Interactive.

Para leerlo en español, ve a [En español](#en-español).

## What you get

- The full game rendered at 1280×720 (scaled to the TV when docked), with sound and the PSP's movies.
- Switch controls: A/B/X/Y in place of the PSP's buttons, ZR/ZL to accelerate and brake, the right
  stick for the camera, and on-screen prompts that name the Switch buttons.
- A smaller, sharper HUD (`[Display] HudScale` in `VCSNative.ini`).

## Requirements

**To play:** a Switch that runs homebrew (Atmosphère) and your copy of the game, decrypted (see
step 3).

**To build** (tested on macOS with Apple Silicon):
- [devkitPro](https://devkitpro.org/wiki/Getting_Started) with `switch-dev`, `switch-sdl2`,
  `switch-mesa`, `switch-ffmpeg`, `switch-libexpat` and `switch-libzstd`
- Homebrew: `cmake ninja pkg-config python3 git rsync`
- Docker Desktop (it builds the Switch's Vulkan driver once)

`tools/check_toolchain.sh` checks all of this and says what is missing.

## Build and install

1. **Vulkan driver for the Switch** (once, about 10 minutes in Docker):
   ```sh
   tools/build_mesa_switch.sh
   ```
2. **Your game files.** Copy the disc's contents into a folder named `PSP_DATA`, so that it holds
   `PSP_GAME/USRDIR/...`, and add the **decrypted** executable as
   `PSP_DATA/PSP_GAME/SYSDIR/EBOOT_DECRYPTED.ELF`. PPSSPP can write it for you: enable
   *Settings → Tools → Developer tools → Dump decrypted EBOOT.BIN on game boot* and start the game once.
3. **The recompiled code**, after a check that your executable is ULUS-10160:
   ```sh
   tools/restore_generated.sh PSP_DATA/PSP_GAME/SYSDIR/EBOOT_DECRYPTED.ELF
   ```
4. **Build** the `.nro`:
   ```sh
   cmake --preset switch
   cmake --build --preset switch      # → build/switch/vcsnx.nro
   ```
5. **Copy to the SD card** (mounted on your computer):
   ```sh
   tools/prepare_sd.sh PSP_DATA /Volumes/SWITCH
   ```
   This leaves `sdmc:/switch/vcsnx/` with `vcsnx.nro`, `VCSNative.ini` and `PSP_DATA/`.
6. **Start it in application mode**: hold **R** while opening any game from the HOME menu to get
   hbmenu (or Sphaira) with the full console memory, then open VCSNX. From the album applet it does
   not have enough memory and says so.

## Controls

Each Switch button does what its PSP counterpart does in the original game:

| Switch | PSP | Notes |
|---|---|---|
| A / B / Y / X | ✕ / ○ / □ / △ | A confirms and B goes back in menus |
| Left stick | Analog stick | |
| Right stick | — | Free camera |
| D-pad | D-pad | |
| ZR / ZL | R / L on foot | Accelerate / brake in a vehicle |
| L / R | L / R | In a vehicle: look left / look right |
| R3, or L + R | L + R | In a vehicle: look behind |
| + / − | START / SELECT | |

The game's on-screen prompts name the Switch buttons. Inverted aiming can be turned off in the
game's *Pause → Controls* menu.

## Optional extras

These are made from your own files, so they are not included:
- **Pro Controller pictures in the Controls menu:** `python3 -I tools/make_switch_textures.py <folder>`, then
  copy the PNGs to `sdmc:/switch/vcsnx/Textures/` and set `[Textures] Replacement=true`.
- **Sharper HUD fonts:** dump the originals with `DumpOriginals=true`, then run `tools/make_hd_hud.py`.
- **HOME menu icon:** `tools/make_icon.py` cuts it from a cover image you provide (`local/icon.jpg`).

## Credits

- [PSPRecomp](https://github.com/jessicanataliagta/PSPRecomp) and
  [vice-city-stories-recomp](https://github.com/OverkillLabs3/vice-city-stories-recomp) (MIT): the
  recompiler and the game profile in `vendor/` (see `UPSTREAM.md`).
- [mesa-switch](https://github.com/danfromtico/mesa-switch) (the Vulkan driver), devkitPro, SDL2,
  FFmpeg (LGPL 2.1) and stb.
- Switch port, splash and sound: **studios bAlo**.

## License

The code of this project is MIT ([LICENSE](LICENSE)). `vendor/` keeps its own licenses, and the
"studios bAlo" logo and splash sound in `assets/splash/` are not MIT: they ship with this project
and its builds, but may not be used elsewhere. Grand Theft Auto and Vice City Stories are trademarks
of Take-Two Interactive; no part of the game is distributed here.

---

## En español

VCSNX es un port nativo de **GTA: Vice City Stories** (PSP, ULUS-10160) para Nintendo Switch. No es un
emulador: el código del juego se recompila a C++ y se compila para la Switch.

**No incluye archivos del juego.** Necesitas tu propia copia legal del juego de PSP (versión de EE. UU.,
ULUS-10160).

**Para jugar:** una Switch con homebrew (Atmosphère) y tu copia del juego.

**Pasos** (los detalles y comandos están arriba, en inglés):
1. `tools/build_mesa_switch.sh`: el driver Vulkan de la Switch (una sola vez, con Docker).
2. Copia el contenido del disco en una carpeta `PSP_DATA` y agrega el ejecutable **desencriptado** como
   `PSP_DATA/PSP_GAME/SYSDIR/EBOOT_DECRYPTED.ELF`. PPSSPP lo genera con *Ajustes → Herramientas →
   Herramientas de desarrollo → Volcar EBOOT.BIN desencriptado al iniciar*.
3. `tools/restore_generated.sh` con tu ejecutable: comprueba que sea ULUS-10160 y prepara el código.
4. `cmake --preset switch` y `cmake --build --preset switch`: crea `build/switch/vcsnx.nro`.
5. `tools/prepare_sd.sh PSP_DATA <tu SD>`: deja todo en `sdmc:/switch/vcsnx/`.
6. Ábrelo **manteniendo R** al abrir cualquier juego desde el menú HOME, para tener toda la memoria.

**Licencia:** el código es MIT; tu copia del juego y el logo "studios bAlo" no lo son.

**Controles:** A/B/Y/X hacen lo de ✕/○/□/△, ZR/ZL son R/L a pie y aceleran y frenan en un vehículo,
el stick derecho mueve la cámara, y en un vehículo L/R miran a los lados y R3 (o L + R) mira atrás.
Los textos del juego muestran los botones de la Switch.
