# Toolchain (verificado 2026-10-11 con tools/check_toolchain.sh)

## Salida
```
== Mac host
ok      cmake
ok      ninja
ok      pkg-config
ok      python3
ok      git
ok      rsync
info    cmake version 4.4.4
ok      sdl2 (pkg-config)
ok      ffmpeg libs (pkg-config)
info    host libavcodec 61.19.101
== devkitPro
ok      aarch64-none-elf-g++
ok      Switch.cmake
ok      elf2nro
ok      nxlink
ok      libnx
ok      switch pkg-config
ok      switch-sdl2
ok      switch-mesa (libEGL)
ok      switch-ffmpeg (libavcodec)
ok      cmake helper nx_create_nro
ok      cmake helper nx_generate_nacp
ok      cmake system name NintendoSwitch
info    aarch64-none-elf-gcc (devkitA64) 16.1.0
info    switch libavcodec major 61 (vendor code targets 61 = FFmpeg 7)
== switch-ffmpeg pieces used by vcs_media_decoder.cpp (needed from H5)
ok      ff_h264_decoder
ok      ff_atrac3p_decoder
ok      ff_mpegps_demuxer
exit=0
```

## Notas
- FFmpeg del Mac: ffmpeg@7 7.1.5 de Homebrew (keg-only; pkg-config libavcodec 61.19.101). El preset `mac` lo encuentra con `PKG_CONFIG_PATH=/opt/homebrew/opt/ffmpeg@7/lib/pkgconfig`.
- devkitA64: aarch64-none-elf-gcc 16.1.0.
- switch-ffmpeg: libavcodec major 61 (FFmpeg 7, la misma que el código del vendor). Decodificadores para H5: h264 ok, atrac3p ok, demuxer mpegps ok. El riesgo del spec §10 ("compilar FFmpeg propio") no aplica.

## Plan 2: Vulkan (verificado 2026-10-11)

Salida completa de `tools/check_toolchain.sh` con las comprobaciones de Vulkan:
```
== Mac host
ok      cmake
ok      ninja
ok      pkg-config
ok      python3
ok      git
ok      rsync
info    cmake version 4.4.4
ok      sdl2 (pkg-config)
ok      ffmpeg libs (pkg-config)
info    host libavcodec 61.19.101
== devkitPro
ok      aarch64-none-elf-g++
ok      Switch.cmake
ok      elf2nro
ok      nxlink
ok      libnx
ok      switch pkg-config
ok      switch-sdl2
ok      switch-mesa (libEGL)
ok      switch-ffmpeg (libavcodec)
ok      cmake helper nx_create_nro
ok      cmake helper nx_generate_nacp
ok      cmake system name NintendoSwitch
info    aarch64-none-elf-gcc (devkitA64) 16.1.0
info    switch libavcodec major 61 (vendor code targets 61 = FFmpeg 7)
== switch-ffmpeg pieces used by vcs_media_decoder.cpp (needed from H5)
ok      ff_h264_decoder
ok      ff_atrac3p_decoder
ok      ff_mpegps_demuxer
== Vulkan (Plan 2: fork GE backend, MoltenVK on Mac, mesa-switch on Switch)
ok      glslc (brew shaderc)
ok      docker daemon (builds mesa-switch)
ok      Vulkan loader (brew vulkan-loader)
ok      MoltenVK ICD (brew molten-vk)
ok      Vulkan headers (brew vulkan-headers)
ok      switch-libexpat
ok      switch-libzstd
ok      mesa-switch SDK staged (tools/build_mesa_switch.sh)
exit=0
```

- Mac (Homebrew): shaderc 2026.4 (`glslc`), molten-vk 1.4.2, vulkan-loader 1.4.363.0 y vulkan-headers 1.4.363.0.
- Docker Desktop: servidor 28.3.0, que compila `mesa-switch` en la imagen ARM64 de devkitPro.
- Portlibs que pide Mesa: switch-libexpat 2.2.5-3 y switch-libzstd 1.5.7-1 (ya estaban instaladas, así que no hizo falta el paso del usuario con `dkp-pacman`).
- SDK de `mesa-switch` (`tools/build_mesa_switch.sh`): commit `606441c`, compilado en Docker en 9 min 52 s. Deja 19 bibliotecas estáticas y los encabezados de Khronos en `.cache/mesa-switch-sdk` (183 MB). Mesa no pidió `libelf` ni otras portlibs fuera de las de devkitPro.
- Choque entre las dos Mesa al enlazar el juego: SDL2 dibuja en la Switch con el EGL de `switch-mesa` (Mesa 20.1), y su `libEGL.a` trae copias internas de Mesa (`util_format_*`, `ralloc`, NIR…) con los mismos nombres que `mesa-switch` (Mesa 26.2). Hubo 333 definiciones duplicadas. `tools/build_mesa_switch.sh` deja en `.cache/mesa-switch-sdk/legacy/libEGL.a` una copia con prefijo `vcsnx_mesa20_` en los 556 símbolos que definen las dos, y CMake enlaza esa copia en lugar de `-lEGL`. Además, el EGL viejo usa 109 funciones que no define y que `mesa-switch` sí define. Las 14 de hilos de C11 (`call_once`, `mtx_*`, `cnd_*`, `thrd_*`) usan los mismos tipos de pthread pero otros códigos de retorno: newlib devuelve `thrd_success = 4` y Mesa 26, 0. Por eso se redirigen a `vcsnx_legacy_c11_*` (`platform/switch/legacy_c11_shim.c`, con los códigos de newlib). Las 95 de NIR/GLSL se renombran a nombres que nadie define, de modo que si un enlace las llega a necesitar, falla con error. El script comprueba al final que no quede ninguna referencia cruzada.
