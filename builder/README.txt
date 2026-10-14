VCSNX Builder 0.3.0 — GTA: Vice City Stories para Nintendo Switch / for Nintendo Switch

ESPAÑOL
VCSNX son dos archivos en sdmc:/switch/vcsnx/: vcsnx.nro (del release, ya está en kit\) y vcs_game.so,
el código del juego, que este programa crea en tu PC con TU copia del juego (ULUS-10160, EE. UU.).
Requisito: la carpeta PSP_DATA del port de PC de VCS (VCSNative), armada con tu copia, con el ejecutable
desencriptado (PSP_GAME\SYSDIR\EBOOT_DECRYPTED.ELF) y PSP_GAME\USRDIR.
1. Descomprime esta carpeta en una ruta sin acentos, por ejemplo C:\VCSNX-Builder.
2. Abre VCSNX-Builder.exe (la ventana "GTA VCS Compiler"). Si Windows dice "Windows protegió su PC":
   "Más información" → "Ejecutar de todas formas".
3. Elige esa carpeta PSP_DATA.
4. Opcional: tu imagen para el ícono y la SD de la Switch.
5. Opcional: tu carpeta de texturas HD (archivos como 0123456789abcdef.png), con los videos HD LOGO.mp4 y
   TITLES.mp4 si los tienes. Van a switch\vcsnx\Textures y switch\vcsnx\HD. Sin carpeta, el juego queda sin HD.
6. Pulsa "Crear vcs_game.so". La primera vez baja el código traducido (necesita internet) y compila ~20–60 min.
7. Si no elegiste la SD, copia salida\vcsnx.nro, salida\vcs_game.so, salida\Textures (los controles de la Switch
   en Ajustes > Controles, y tus texturas HD) y kit\VCSNative.ini a sdmc:/switch/vcsnx/, tu carpeta PSP_DATA a
   sdmc:/switch/vcsnx/PSP_DATA/ y, si elegiste videos HD, salida\HD a sdmc:/switch/vcsnx/.
8. En la Switch, abre VCSNX desde hbmenu manteniendo R al abrir un juego.
Actualizaciones: baja el vcsnx.nro nuevo del release y reemplázalo en la SD; vcs_game.so sigue sirviendo salvo
que la consola diga que hay que crearlo de nuevo: entonces baja el VCSNX-Builder-<versión>.zip de ese release,
descomprímelo en esta carpeta (reemplazando los archivos) y pulsa "Crear" otra vez.
Si tu antivirus bloquea el compilador (toolchain\devkitA64\...), permite la carpeta del programa y vuelve a
pulsar "Crear": lo ya compilado se conserva.

ENGLISH
VCSNX is two files in sdmc:/switch/vcsnx/: vcsnx.nro (from the release, also in kit\) and vcs_game.so, the
game's code, which this program makes on your PC from YOUR copy of the game (ULUS-10160, US).
Requirement: the PSP_DATA folder of the VCS PC port (VCSNative), set up with your copy, with the decrypted
executable (PSP_GAME\SYSDIR\EBOOT_DECRYPTED.ELF) and PSP_GAME\USRDIR.
1. Unzip this folder to a path without accents, for example C:\VCSNX-Builder.
2. Open VCSNX-Builder.exe (the "GTA VCS Compiler" window). If Windows says "Windows protected your PC":
   "More info" → "Run anyway".
3. Pick that PSP_DATA folder.
4. Optional: your image for the icon, and the Switch's SD card.
5. Optional: your HD textures folder (files like 0123456789abcdef.png), with the HD movies LOGO.mp4 and TITLES.mp4
   if you have them. They go to switch\vcsnx\Textures and switch\vcsnx\HD. Without a folder there is no HD.
6. Press "Create vcs_game.so". The first time it downloads the translated code (needs internet) and compiles for ~20–60 min.
7. If you did not pick the SD card, copy salida\vcsnx.nro, salida\vcs_game.so, salida\Textures (the Switch's
   controls in Settings > Controls, and your HD textures) and kit\VCSNative.ini to sdmc:/switch/vcsnx/, your
   PSP_DATA folder to sdmc:/switch/vcsnx/PSP_DATA/ and, if you picked HD movies, salida\HD to sdmc:/switch/vcsnx/.
8. On the Switch, open VCSNX from hbmenu while holding R as you start a game.
Updates: download the new vcsnx.nro from the release and replace it on the SD card; vcs_game.so keeps working
unless the console says it must be made again: then download that release's VCSNX-Builder-<version>.zip, unzip
it into this folder (replacing the files) and press "Create" again.
If your antivirus blocks the compiler (toolchain\devkitA64\...), allow the program's folder and press "Create"
again: what was already compiled is kept.

Licenses: LICENSES\THIRD_PARTY.md. Problems: salida\builder.log.
