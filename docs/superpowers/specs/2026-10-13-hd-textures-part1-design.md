# Texturas HD, parte 1: el Builder amplía las texturas de carros, personajes, armas y pantallas de carga

Spec aprobado en conversación el 2026-10-13. Desarrolla el punto 18 de `docs/backlog.md` (texturas HD) para la
primera de tres partes que eligió el usuario: (1) carros, personajes, armas y pantallas de carga; (2) la ciudad;
(3) un paquete comprimido que la consola carga sin decodificar PNG.

> **Cambio del autor (2026-10-13):** el Builder ya no crea las texturas HD. El paso 4 de la ventana ("Texturas HD
> (opcional)", `--hd-folder` en la línea de comandos) pide una carpeta que el jugador ya tiene (su `Textures`, o una
> copia de la SD con `switch\vcsnx\Textures`) y copia sus `<clave>.png` a `switch\vcsnx\Textures` con el juego; sin
> carpeta no toca `Textures` (`builder/Builder.Core/HdFolder.cs`). Los videos HD van en la misma carpeta (decisión
> del autor): `LOGO.mp4` y `TITLES.mp4` junto a las texturas o en su `HD`, copiados a `switch\vcsnx\HD`. El ampliador de abajo quedó en `builder/HdPack`,
> herramienta del autor que no va en el zip del Builder, y el Builder no baja Real-ESRGAN.

## 1. Objetivo

- Cada jugador crea en su PC, con su copia del juego, versiones HD de las texturas de carros, personajes, armas y
  pantallas de carga y legales, y las copia a `sdmc:/switch/vcsnx/Textures/`.
- La consola ya las usa sin cambios: el sistema de reemplazo existente (`[Textures] Replacement=true`) carga
  `Textures/<código>.png` cuando el PNG mide exactamente 2, 4 u 8 veces la textura original.
- Nada del juego entra al repo ni al release: ni texturas, ni nombres de texturas, ni códigos de texturas.

### Lo que pidió el usuario
- "quiero las texturas en hd. de todo" (2026-10-13).
- Ruta B: el Builder saca todas las texturas de los archivos del juego, sin tener que jugar.
- Estilo A, **fiel**: el mismo dibujo, más nítido; la IA no inventa detalle.
- Por partes: primero carros, personajes y armas; después la ciudad.
- Dentro del Builder: nada que instalar para el jugador.

### Supuestos (confirmados en la conversación)
- El HUD, los menús y las letras quedan fuera: la IA deforma el texto y el HUD tiene su propia versión HD.
- El paso es opcional: necesita tiempo y una tarjeta gráfica con Vulkan.

### Fuera de la parte 1
- La ciudad (`MAINLA`, `BEACH`, `MALL`: `.LVZ` + `.IMG`, otro formato sin descifrar todavía).
- Formatos comprimidos de GPU, mipmaps hechos en el PC y carga desde RAM (parte 3).
- Cualquier cambio al dibujo o a los colores (estilo B, "remaster").

## 2. Cómo identifica la consola cada textura

`vendor/vcs-recomp/profiles/vcs/host/vcs_texture_replace.cpp`:
- El código es FNV-1a de 64 bits: se mezclan el ancho y el alto como enteros de 64 bits (`hash ^= valor;
  hash *= 0x100000001B3`) y después cada byte de los `ancho × alto × 4` bytes RGBA8 del nivel 0 decodificado,
  partiendo de `0xCBF29CE484222325` (`texture_content_hash`). El archivo se llama `%016x.png`.
- Ancho y alto son los del registro de tamaño del GE (potencias de dos).
- Texturas de menos de 4×4 no se reemplazan.
- Un archivo que ya existe con ese nombre gana: los reemplazos hechos a mano se respetan.

La decodificación de la consola está en `ge_renderer.cpp` (`decode_texture_rgba_into`, `swizzled_offset`,
`unpack16`, `unpack32`, `read_clut_fast`). El Builder la reimplementa en C# y se valida contra la consola (§6).

### Prueba de factibilidad (2026-10-13, código desechable)
El banco de pruebas del Mac con `DumpOriginals=true` guardó 22 texturas del arranque. Un lector escrito aparte
reprodujo **exactamente** 3 códigos, los de dos pantallas de carga y una legal, leyendo los archivos sin
ejecutar el juego. Las demás vienen de archivos donde la paleta y los mipmaps están ordenados de otra forma; esa
disposición se determina en la tarea 1 (§9).

## 3. El formato que se lee

Los archivos usan bloques reubicables con firma `xet\0` (textura):
- Cabecera de 0x20 bytes: firma, 0, tamaño del archivo, tamaño de los datos, tabla de reubicación, número de
  entradas, y dos campos más. Los punteros internos son desplazamientos desde el inicio del bloque.
- Un diccionario con una lista enlazada de texturas (cabeza en 0x28). Cada nodo apunta a su raster y lleva
  el nombre de la textura.
- El raster lleva el puntero a los texels, `log2` del ancho y del alto, la profundidad (4 u 8 bits) y un byte
  que en los archivos de modelos coincide con `min(log2 ancho, log2 alto)` (probablemente los mipmaps).
- Texels "swizzled" del PSP (bloques de 16 bytes × 8 filas, fila = `max(ancho × bits / 8, 16)` bytes).
- Paleta de 16 o 256 entradas de 32 bits (R, G, B, A). En las pantallas de carga sigue a los texels del nivel 0.

Dónde están estos bloques:
- Los `.XTX` de `RUNDATA` (pantallas de carga y legales): un bloque por archivo.
- `GTA3PSP.IMG`: los bloques empiezan en múltiplos de 2048 bytes. Se encuentran por la firma y se aceptan solo si
  la cabecera es coherente: el tamaño cabe en el archivo, la tabla de reubicación cae dentro del bloque y la lista
  termina. No hace falta el directorio del archivo.

Texturas que se saltan: profundidades o formatos desconocidos (se cuentan en el log del Builder), y las que
el filtro 2D de §4.6 reconoce como HUD, radar, iconos o letras.

## 4. El paso nuevo del Builder

### 4.1 Arquitectura (todo en `Builder.Core`, C#, probado en el Mac con `dotnet test`)
- `TexChunkReader`: encuentra y valida los bloques, y recorre la lista de texturas.
- `PspRaster`: decodifica el nivel 0 a RGBA8 igual que la consola (swizzle, paleta, 4/8 bits).
- `TextureKey`: el código de §2.
- `PngCodec`: escribe PNG RGBA8 y lee los PNG RGB/RGBA de 8 bits sin entrelazar que produce Real-ESRGAN. Se
  escribe desde cero (zlib de .NET y CRC32), sin librerías de imagen.
- `Resample`: reducción por promedio de cajas (para el control de fidelidad) y Lanczos-3 (ampliación clásica).
- `HdUpscaler`: prepara cada textura, llama a Real-ESRGAN a través del `IProcessRunner` existente (los tests usan
  `FakeRunner`), aplica el control de fidelidad y elige entre IA y ampliación clásica.
- `HdTextures`: orquesta el paso con progreso (`BuildProgress`, `Eta`) y cancelación.

### 4.2 Ampliación
- Escala: 4x si el lado mayor mide hasta 64 px; 2x hasta 512 px (la regla de `auto_scale_shift` del juego, más
  las pantallas de 512 px). Real-ESRGAN amplía 4x; para 2x se reduce a la mitad con promedio de cajas.
- Bordes unidos: antes de ampliar, la textura se rodea con una franja de su propio lado opuesto (repetición
  toroidal, 8 px a tamaño original o un cuarto del lado si es menor). Después se recorta el centro. Así las
  texturas que se repiten no muestran costuras.
- Transparencia: el RGB de los texels con alfa 0 se rellena con el color opaco más cercano antes de ampliar,
  para que la IA no arrastre colores escondidos. El alfa se amplía con Lanczos-3. Si el original solo tiene alfa
  0 y lleno (recortes como rejas o pelo), el resultado se vuelve a cortar en el valor medio para que siga nítido.
- Modelo: la tarea 3 compara `realesrgan-x4plus` y `realesr-animevideov3` sobre una muestra de la copia del autor
  con el control de fidelidad y se queda con el que más texturas aprueba. El modelo elegido queda fijo en el
  código.

### 4.3 Control de fidelidad (estilo A)
- El resultado se reduce de vuelta al tamaño original (promedio de cajas) y se compara con el original:
  PSNR sobre RGBA ≥ 32 dB, el mismo umbral que el ampliador del juego (`round_trip_psnr`).
- Si no llega, esa textura se amplía con Lanczos-3. Las dos salidas se cuentan en el log del paso.

### 4.4 Real-ESRGAN
- `realesrgan-ncnn-vulkan` oficial (Real-ESRGAN, BSD-3; ncnn, BSD-3), versión para Windows con sus modelos.
- El Builder lo baja la primera vez desde una URL fija y comprueba su SHA-256 antes de usarlo, igual que el
  código traducido (`Corpus.DownloadAsync`, `HashList`). No va dentro del zip del Builder.
- Se ejecuta por carpetas (entrada y salida en `trabajo\hd\`), no una vez por textura.
- Sin tarjeta gráfica con Vulkan, o si el programa falla, todas las texturas usan Lanczos-3 y la ventana lo dice.
- Las licencias van a `builder/licenses/` y a `THIRD_PARTY.md`.

### 4.5 La ventana y la salida
- Un botón nuevo, "Crear texturas HD", que solo necesita la carpeta `PSP_DATA` (y la SD si se eligió). Sirve
  también a quien ya tiene `vcs_game.so`.
- Progreso "Texturas HD: n/N" con tiempo restante, y Cancelar.
- Salida: `<SD>/switch/vcsnx/Textures/<código>.png`, o `salida\Textures\` si no se eligió SD. Un archivo que ya
  existe no se sobrescribe.
- `salida\texturas_hd.log`: cuántas texturas se leyeron, se saltaron (y por qué), se ampliaron con IA o con
  Lanczos, y el tiempo. Queda en el PC del jugador.
- `Builder.Cli` recibe la misma opción, para correr el paso en el Mac del autor.

### 4.6 Filtro 2D
- Se saltan las texturas cuyo nombre contiene una palabra de una lista corta y genérica (por ejemplo `font`,
  `hud`, `radar`, `icon`, `button`), sin distinguir mayúsculas. La lista es de palabras comunes, no de nombres del
  juego.
- La tarea 1 la ajusta mirando los nombres de la copia del autor. Los nombres no se copian al repo.
- Las pantallas de carga y legales (`.XTX`) siempre entran.

## 5. Errores
- Un bloque o raster incoherente se salta y se cuenta; nunca detiene el paso.
- Falla al bajar Real-ESRGAN o su SHA-256 no coincide: el paso sigue solo con Lanczos y lo avisa.
- Falla de Real-ESRGAN con una carpeta: esas texturas pasan a Lanczos.
- Disco o SD llenos: el paso se detiene con el mensaje del sistema. Lo ya escrito queda y sirve.
- Cancelar deja escritas las texturas terminadas. Volver a correr el paso salta las que ya existen.

## 6. Pruebas
- Unitarias (C#, datos sintéticos hechos en el test, nada del juego):
  - Bloque `xet\0` armado a mano: cabecera, lista y raster.
  - Swizzle contra vectores de `swizzled_offset`.
  - Paleta de 4 y 8 bits.
  - Código: un vector fijo (una imagen 4×4 conocida) calculado por `texture_content_hash` en un test de C++
    y por `TextureKey` en C#; los dos tests comparan con la misma constante.
  - PNG: escribir y leer de vuelta, y leer un PNG RGB de 8 bits.
  - Franja toroidal, relleno de transparencia y corte del alfa.
  - Control de fidelidad: una ampliación fiel pasa, una que cambia colores no pasa.
  - `HdUpscaler` con `FakeRunner`: elige IA o Lanczos según el umbral y cuando Real-ESRGAN falla.
- Validación contra la consola (local, en el Mac del autor): una sesión en la Switch con `DumpOriginals=true`
  da la lista de códigos que la consola ve. Se baja solo la lista de nombres. El lector tiene que producir el
  100 % de los códigos de esa lista que vienen de `GTA3PSP.IMG` y de los `.XTX`.
- En la Switch, con las texturas HD:
  - Memoria: alloc-diag y el presupuesto de reemplazos (597 MB).
  - Tirones al aparecer objetos (`[ge-slow]`, perf.log).
  - Revisión visual del autor.

## 7. Lo que puede necesitar la consola
Ningún cambio obligatorio. Riesgos que se miden en §6 y se corrigen solo si aparecen:
- La consola decodifica cada PNG (stb) en el hilo de reemplazos y hace un `exists()` en la SD por cada textura
  nueva. Si ese hilo le quita tiempo al hilo principal, se le baja la prioridad o se mueve de núcleo.
- La textura aparece primero en baja calidad y cambia a HD un instante después. Es el comportamiento actual;
  la parte 3 lo elimina.

## 8. Licencias, aspectos legales y publicación
- Las texturas HD salen de la copia de cada jugador, en su PC. No se reparten.
- El repo no lleva nombres ni códigos de texturas del juego. Los tests usan datos sintéticos.
- Real-ESRGAN y ncnn (BSD-3) se bajan, no se redistribuyen dentro del zip. Sus licencias van en
  `builder/licenses/`.
- Hace falta un Builder nuevo (0.2.x): los jugadores lo bajan una vez más para tener el botón. `vcs_game.so`
  no cambia.

## 9. División en tareas (para el plan)
1. Disposición de paleta y mipmaps en `GTA3PSP.IMG`: el banco del Mac anota las direcciones de texels y paleta
   de cada textura guardada, se cruzan con las lecturas de archivo, y se fija la regla en el lector de prueba
   hasta reproducir los códigos.
2. `TexChunkReader`, `PspRaster`, `TextureKey` con sus tests, más el test de C++ del vector compartido.
3. `PngCodec`, `Resample`, `HdUpscaler`: franja, transparencia, control de fidelidad, elección de modelo.
4. Descarga y verificación de Real-ESRGAN, y licencias.
5. Botón y progreso en la ventana; opción en `Builder.Cli`.
6. Validación contra la consola y sesión de juego con las texturas HD.
7. Builder 0.2.x en el release, con notas para los jugadores.

## 10. Riesgos
- Algunas texturas pueden decodificarse distinto de lo previsto. Por ejemplo, el juego podría usar otra
  paleta o parte de una paleta al dibujar (`CMODE` con desplazamiento): esas no coincidirían. La validación de §6
  dice cuántas.
- Memoria: 2.776 texturas a 2x-4x en RGBA8 con mipmaps pueden pasar del presupuesto si muchas están a la vez en
  pantalla. Pasado el presupuesto, la consola deja las originales sin fallar.
- Tiempo del paso en PCs lentos: no está medido. Se mide en la tarea 3 en el Mac y se informa en las notas.
- Real-ESRGAN puede cambiar el dibujo en texturas pequeñas de pocos colores: el control de fidelidad las manda a
  Lanczos.
