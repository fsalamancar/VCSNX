# Where the source code is

## The toolchain (toolchain/ in VCSNX-Builder-0.3.0.zip)

`toolchain/devkitA64` is devkitPro's Windows build of devkitA64, unmodified, from these packages:

| Package | Version | As distributed | Source |
|---|---|---|---|
| devkita64-gcc | 16.1.0-1 | https://pkg.devkitpro.org/packages/windows/x86_64/ | https://github.com/devkitPro/buildscripts (build scripts and patches), https://ftp.gnu.org/gnu/gcc/gcc-16.1.0/ |
| devkita64-binutils | 2.46.0-1 | https://pkg.devkitpro.org/packages/windows/x86_64/ | https://github.com/devkitPro/buildscripts, https://ftp.gnu.org/gnu/binutils/ |
| devkita64-newlib | 4.6.0.20260123-4 | https://pkg.devkitpro.org/packages/ | https://github.com/devkitPro/newlib |

GCC and binutils are GPL-3.0 (`GPL-3.0.txt`; parts of binutils are GPL-2.0, `GPL-2.0.txt`, and their
libraries LGPL, `LGPL-3.0.txt`); GCC's runtime libraries carry the GCC Runtime Library Exception
(`GCC-RUNTIME-LIBRARY-EXCEPTION-3.1.txt`); newlib is covered by `newlib-COPYING.NEWLIB.txt` and
`newlib-COPYING.LIBGLOSS.txt`.

If any of these sources is no longer available, open an issue at https://github.com/fsalamancar/VCSNX/issues
and we will provide the complete corresponding source of the versions shipped here.

## vcsnx.nro

- VCSNX itself, including the game-module loader adapted from so_util.c: https://github.com/fsalamancar/VCSNX
  (this repository; MIT).
- FFmpeg 7.1 (LGPL-2.1, `FFmpeg-COPYING.LGPLv2.1`), linked statically: https://ffmpeg.org/releases/ and
  devkitPro's build, https://github.com/devkitPro/pacman-packages. VCSNX's source above lets you rebuild
  vcsnx.nro against a modified FFmpeg.
- The other libraries and their licenses are listed in `THIRD_PARTY.md`.
