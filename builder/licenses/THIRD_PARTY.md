# Third-party software in VCSNX 0.1.0

## In VCSNX-Builder-0.1.0.zip

| Component | Version | License | Source |
|---|---|---|---|
| devkitA64 GCC (toolchain/devkitA64) | 16.1.0-1 | GPL-3.0 with the GCC Runtime Library Exception (`GPL-3.0.txt`, `GCC-RUNTIME-LIBRARY-EXCEPTION-3.1.txt`, `LGPL-3.0.txt`) | `SOURCES.md` |
| devkitA64 binutils | 2.46.0-1 | GPL-3.0 (`GPL-3.0.txt`; parts GPL-2.0, `GPL-2.0.txt`) | `SOURCES.md` |
| newlib (devkita64-newlib) | 4.6.0.20260123-4 | BSD-style (`newlib-COPYING.NEWLIB.txt`, `newlib-COPYING.LIBGLOSS.txt`) | `SOURCES.md` |
| PSPRecomp headers (kit/include/psprecomp) | f6e7d41 | MIT (PSPRecomp-LICENSE.txt) | https://github.com/jessicanataliagta/PSPRecomp |
| vice-city-stories-recomp headers (kit/include/host) | b5a861d | MIT | https://github.com/OverkillLabs3/vice-city-stories-recomp |
| SkiaSharp (inside VCSNX-Builder.exe) | 4.153.1 | MIT | https://github.com/mono/SkiaSharp |
| .NET runtime and Windows Forms (inside VCSNX-Builder.exe) | 10 | MIT | https://github.com/dotnet/runtime, https://github.com/dotnet/winforms |

## In vcsnx.nro

| Component | Version | License | Source |
|---|---|---|---|
| Game-module loader (platform/switch/elf_module, game_module) | adapted from so_util.c | MIT, (C) 2021 Andy Nguyen, fgsfds (`so_util-LICENSE.txt`) | this repository |
| PSPRecomp runtime and the vice-city-stories-recomp profile | f6e7d41 / b5a861d | MIT | see above |
| mesa-switch (Vulkan) | 606441c | MIT (`mesa-license.rst`, `mesa-MIT.txt`) | https://github.com/danfromtico/mesa-switch |
| switch-mesa EGL, renamed copy | 20.1.0 | MIT | https://github.com/devkitPro/pacman-packages |
| libnx | 4.12.0 | ISC (`libnx-LICENSE.md`) | https://github.com/switchbrew/libnx |
| SDL2 | 2.28.5 | zlib (`SDL2-LICENSE.txt`) | https://github.com/libsdl-org/SDL |
| FFmpeg | 7.1 | LGPL-2.1 (`FFmpeg-COPYING.LGPLv2.1`) | `SOURCES.md` |
| dav1d, bzip2, zlib, expat, zstd | devkitPro portlibs (1.5.0, 1.0.8, 1.3.1, 2.2.5, -) | BSD-2 / bzip2 / zlib / MIT / BSD-3 (`dav1d-COPYING.txt`, `bzip2-LICENSE.txt`, `zlib-LICENSE.txt`, `expat-COPYING.txt`, `zstd-LICENSE.txt`) | https://github.com/devkitPro/pacman-packages |
| stb_image | 2.x | MIT / public domain | https://github.com/nothings/stb |

The toolchain packages are devkitPro's own Windows builds, unmodified; `SOURCES.md` says where their source is. VCSNX's code is MIT
(VCSNX-LICENSE.txt); the "studios bAlo" logo and the splash sound are not covered by it.
No part of Grand Theft Auto: Vice City Stories is included: vcs_game.so is made on each player's PC from
their own copy.
