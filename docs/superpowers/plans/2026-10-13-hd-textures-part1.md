# Texturas HD, parte 1 — plan de implementación

> **Cambio del autor (2026-10-13):** el Builder ya no crea las texturas HD. El paso 4 de la ventana ("Texturas HD
> (opcional)", `--hd-folder` en la línea de comandos) pide una carpeta que el jugador ya tiene (su `Textures`, o una
> copia de la SD con `switch\vcsnx\Textures`) y copia sus `<clave>.png` a `switch\vcsnx\Textures` con el juego; sin
> carpeta no toca `Textures` (`builder/Builder.Core/HdFolder.cs`). Los videos HD van en la misma carpeta (decisión
> del autor): `LOGO.mp4` y `TITLES.mp4` junto a las texturas o en su `HD`, copiados a `switch\vcsnx\HD`. El ampliador de abajo quedó en `builder/HdPack`,
> herramienta del autor que no va en el zip del Builder, y el Builder no baja Real-ESRGAN.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Un botón "Crear texturas HD" en el Builder que saca de la copia del jugador las texturas de carros,
personajes, armas y pantallas de carga, las amplía con Real-ESRGAN (o Lanczos-3 si la IA no es fiel) y las
escribe como `Textures/<código>.png`, que la consola ya carga.

**Architecture:** Todo en `Builder.Core` (C#, .NET 10), bajo `Vcsnx.Builder.Core.Hd`: un lector de bloques `xet\0`,
un decodificador del raster del PSP que reproduce el de la consola, el código FNV-1a de la consola, un PNG
propio, remuestreo, control de fidelidad y el ampliador que llama a Real-ESRGAN por `IProcessRunner`. El paso
corre desde la ventana y desde `Builder.Cli`. La consola no cambia.

**Tech Stack:** C# / .NET 10, xUnit, `System.IO.Compression.ZLibStream`; C++20 para un test del código de la
consola; Real-ESRGAN ncnn-vulkan v0.2.5.0; Python 3 (`-I`) para la comparación local con la consola.

**Spec:** `docs/superpowers/specs/2026-10-13-hd-textures-part1-design.md`

## Global Constraints

- Nada del juego en el repo: ni texturas, ni nombres de texturas, ni códigos. Los tests usan datos sintéticos.
  Lo que salga de la copia del autor vive solo en `local/` o en el scratchpad.
- Código de la consola: FNV-1a 64, semilla `0xCBF29CE484222325`, primo `0x100000001B3`, se mezclan ancho y alto
  como `u64` y luego cada byte RGBA8 del nivel 0; nombre `%016x.png`. Texturas de menos de 4×4 no cuentan.
- Escala: 4x si el lado mayor ≤ 64 px; 2x si ≤ 512 px; si no, se salta. Real-ESRGAN siempre 4x; para 2x, promedio de
  cajas a la mitad.
- Franja toroidal: `min(8, lado/4)` px por eje a tamaño original.
- Fidelidad: PSNR ≥ 32 dB del resultado reducido contra el original (RGB de texels visibles + alfa de todos).
- Un `Textures/<código>.png` que ya existe nunca se sobrescribe.
- Real-ESRGAN `realesrgan-ncnn-vulkan` v0.2.5.0, bajado y verificado por SHA-256:
  - Windows `realesrgan-ncnn-vulkan-20220424-windows.zip`
    `abc02804e17982a3be33675e4d471e91ea374e65b70167abc09e31acb412802d`
  - macOS `realesrgan-ncnn-vulkan-20220424-macos.zip`
    `e0ad05580abfeb25f8d8fb55aaf7bedf552c375b5b4d9bd3c8d59764d2cc333a`
- Filtro 2D: palabras `font`, `hud`, `radar`, `icon`, `button` (sin distinguir mayúsculas); los `.XTX` siempre entran.
- Rama `feat/hd-textures`; nunca en `main`. Conventional Commits con
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Python que lee archivos del juego: `python3 -I`. Real-ESRGAN para macOS: aprobado por el autor (2026-10-13) solo
  para pruebas locales.
- Sin menciones de otros ports en docs ni código.

## Review Focus

1. Copia del jugador con nombres en minúsculas (`gta3psp.img`, `loadsc0.xtx`): las fuentes se encuentran igual
   (test en la tarea 2).
2. `Textures/` con reemplazos hechos a mano (los PNG del menú de controles): nunca se pisan (test en la tarea 7).
3. Cancelar a mitad de un lote: no quedan `.tmp` ni PNG truncados, y lo escrito se lee (test en la tarea 7).
4. Un archivo modificado o corrupto (lista cíclica, punteros fuera del bloque): no revienta, se cuenta como
   saltado (test en la tarea 2).
5. PC sin GPU con Vulkan (Real-ESRGAN sale con error): todo con Lanczos, un aviso, y no se reintenta la IA en cada
   lote (test en la tarea 5).

---

### Task 1: El código de textura (C# y C++)

**Files:**
- Create: `builder/Builder.Core/Hd/TextureKey.cs`
- Create: `builder/Builder.Core.Tests/Hd/TextureKeyTests.cs`
- Create: `tests/platform/test_texture_key.cpp`
- Modify: `cmake/vcsnx_tests.cmake` (registrar `test_texture_key`)

**Interfaces:**
- Produces: `static ulong TextureKey.Of(ReadOnlySpan<byte> rgba, int width, int height)`,
  `static string TextureKey.FileName(ulong key)`.

- [ ] **Step 1: Test de C# que falla**

```csharp
// builder/Builder.Core.Tests/Hd/TextureKeyTests.cs
using Vcsnx.Builder.Core.Hd;

namespace Vcsnx.Builder.Core.Tests.Hd;

public class TextureKeyTests
{
    [Fact]
    public void A4x4ImageOfBytes0To63HasTheKeySharedWithTheConsoleTest()
    {
        var rgba = Enumerable.Range(0, 64).Select(i => (byte)i).ToArray();
        Assert.Equal(0x7b21ac69b86eb9d5UL, TextureKey.Of(rgba, 4, 4));
        Assert.Equal("7b21ac69b86eb9d5.png", TextureKey.FileName(0x7b21ac69b86eb9d5UL));
    }

    [Fact]
    public void WidthIsMixedBeforeHeight()
    {
        var rgba = Enumerable.Range(0, 128).Select(i => (byte)(i * 3)).ToArray();
        Assert.Equal(0x163a1f6ba6829169UL, TextureKey.Of(rgba, 8, 4));
        Assert.NotEqual(TextureKey.Of(rgba, 8, 4), TextureKey.Of(rgba, 4, 8));
    }
}
```

- [ ] **Step 2: Correrlo y verlo fallar**

Run: `dotnet test builder/Builder.Core.Tests --filter TextureKeyTests`
Expected: error de compilación `The type or namespace name 'Hd' does not exist`.

- [ ] **Step 3: Implementación**

```csharp
// builder/Builder.Core/Hd/TextureKey.cs
namespace Vcsnx.Builder.Core.Hd;

/// <summary>The name the console gives a decoded texture (vcs_texture_replace.cpp texture_content_hash): FNV-1a 64
/// over the width and the height as whole 64-bit values, then every RGBA8 byte of level 0.</summary>
public static class TextureKey
{
    const ulong Seed = 0xCBF29CE484222325UL;
    const ulong Prime = 0x100000001B3UL;

    public static ulong Of(ReadOnlySpan<byte> rgba, int width, int height)
    {
        var hash = Seed;
        hash = (hash ^ (ulong)width) * Prime;
        hash = (hash ^ (ulong)height) * Prime;
        foreach (var b in rgba) hash = (hash ^ b) * Prime;
        return hash;
    }

    public static string FileName(ulong key) => key.ToString("x16") + ".png";
}
```

- [ ] **Step 4: Correr y ver pasar**

Run: `dotnet test builder/Builder.Core.Tests --filter TextureKeyTests`
Expected: `Passed!  - Failed: 0, Passed: 2`.

- [ ] **Step 5: El mismo vector en el código de la consola**

Es un test de caracterización: la función ya existe, así que debe pasar de entrada. Su valor es atar la constante
compartida al código real de la consola.

```cpp
// tests/platform/test_texture_key.cpp
// The replacement key the console computes; Builder.Core/Hd/TextureKey.cs must give the same value.
#include "vcs_texture_replace.hpp"
#include "test_support.hpp"

#include <array>
#include <cstddef>

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"a 4x4 image of bytes 0..63 has the key shared with the Builder test", [] {
             std::array<std::byte, 64> rgba{};
             for (std::size_t i = 0; i < rgba.size(); ++i) rgba[i] = static_cast<std::byte>(i);
             require(vcs::texture_content_hash(rgba, 4u, 4u) == 0x7b21ac69b86eb9d5ull, "shared key");
         }},
    });
}
```

En `cmake/vcsnx_tests.cmake`, debajo de `test_range_allocator`:

```cmake
vcsnx_add_test(test_texture_key tests/platform/test_texture_key.cpp vendor/vcs-recomp/profiles/vcs/host/vcs_texture_replace.cpp)
```

Run: `cmake --build build/mac --target test_texture_key && ctest --test-dir build/mac -R test_texture_key --output-on-failure`
Expected: `100% tests passed out of 1`. Si el enlace pide más símbolos, se agregan las fuentes que falten a esa
línea y se anota un `Ruling:`.

- [ ] **Step 6: Commit**

```bash
git add builder/Builder.Core/Hd/TextureKey.cs builder/Builder.Core.Tests/Hd/TextureKeyTests.cs tests/platform/test_texture_key.cpp cmake/vcsnx_tests.cmake
git commit -m "feat(builder): the console's texture replacement key, checked against the console's own function"
```

---

### Task 2: Lector de bloques, raster del PSP y catálogo

**Files:**
- Create: `builder/Builder.Core/Hd/PspRaster.cs`
- Create: `builder/Builder.Core/Hd/TexChunkReader.cs`
- Create: `builder/Builder.Core/Hd/HdCatalog.cs`
- Create: `builder/Builder.Core.Tests/Hd/TexChunkBuilder.cs` (ayuda de tests)
- Create: `builder/Builder.Core.Tests/Hd/PspRasterTests.cs`, `TexChunkReaderTests.cs`, `HdCatalogTests.cs`

**Interfaces:**
- Consumes: `TextureKey.Of`.
- Produces:
  - `sealed record RasterInfo(int Texels, int Width, int Height, int Depth, int Levels, int RowBytes)`
  - `static RasterInfo? PspRaster.Read(ReadOnlySpan<byte> chunk, int offset)`
  - `static long PspRaster.PaletteOffset(RasterInfo r)`
  - `static int PspRaster.SwizzledOffset(int byteX, int y, int rowBytes)`
  - `static byte[]? PspRaster.DecodeLevel0(ReadOnlySpan<byte> chunk, RasterInfo r)`
  - `static bool TexChunkReader.IsValid(ReadOnlySpan<byte> data, int offset, out int size)`
  - `static IReadOnlyList<int> TexChunkReader.FindChunks(byte[] data, int alignment)`
  - `static IReadOnlyList<(string Name, int Raster)> TexChunkReader.Textures(ReadOnlySpan<byte> chunk)`
  - `sealed record HdSource(string Path, int Alignment)`
  - `sealed record HdTexture(ulong Key, int Width, int Height, byte[] Rgba, string Source, string Name)`
  - `sealed record HdScan(IReadOnlyList<HdTexture> Textures, int Skipped2D, int SkippedFormat, int SkippedSmall, int Duplicates)`
  - `static IReadOnlyList<HdSource> HdCatalog.SourcesIn(string gameFolder)`
  - `static HdScan HdCatalog.Scan(IEnumerable<HdSource> sources)`

La regla del raster sale de la copia del autor (`local/notes/perf/task_plan.md`):
- Nivel 0: `RowBytes` es el `u16` en +8 del raster. Los mipmaps usan `max(ancho_i × bits / 8, 16)`.
- Niveles: `byte +13 & 0x3F`. La paleta (16 o 256 × 4 bytes) sigue a todos los niveles, cada uno de
  `filas × alto_i` bytes.
- Los texels van siempre "swizzled".
- La tarea 3 valida esta regla contra la consola.

- [ ] **Step 1: Ayuda de tests que arma bloques `xet\0` sintéticos**

```csharp
// builder/Builder.Core.Tests/Hd/TexChunkBuilder.cs
using System.Buffers.Binary;
using System.Text;
using Vcsnx.Builder.Core.Hd;

namespace Vcsnx.Builder.Core.Tests.Hd;

/// <summary>Builds a synthetic 'tex' chunk: header, dictionary (head link at 0x28), one node and one raster per
/// texture, then each texture's texel chain followed by its palette. No game data.</summary>
internal static class TexChunkBuilder
{
    internal sealed record Tex(string Name, int Width, int Height, int Depth, int Levels, int RowBytes, byte[] Chain, byte[] Palette);

    public static byte[] Build(params Tex[] textures)
    {
        const int nodesAt = 0x40, nodeSize = 0x60;
        var rastersAt = nodesAt + nodeSize * textures.Length;
        var dataAt = Align(rastersAt + 16 * textures.Length);
        var size = dataAt + textures.Sum(t => Align(t.Chain.Length + t.Palette.Length)) + 16;
        var chunk = new byte[size];
        "xet\0"u8.CopyTo(chunk);
        W(chunk, 8, size); W(chunk, 12, size - 16); W(chunk, 16, size - 16); W(chunk, 20, 0);
        var data = dataAt;
        for (var i = 0; i < textures.Length; i++)
        {
            var t = textures[i];
            int node = nodesAt + nodeSize * i, raster = rastersAt + 16 * i;
            W(chunk, node, raster);
            W(chunk, node + 4, 0x20);
            W(chunk, node + 8, i + 1 < textures.Length ? nodesAt + nodeSize * (i + 1) + 8 : 0x28);
            W(chunk, node + 12, i == 0 ? 0x28 : nodesAt + nodeSize * (i - 1) + 8);
            Encoding.ASCII.GetBytes(t.Name).CopyTo(chunk, node + 0x10);
            W(chunk, raster + 4, data);
            BinaryPrimitives.WriteUInt16LittleEndian(chunk.AsSpan(raster + 8), (ushort)t.RowBytes);
            chunk[raster + 10] = (byte)int.Log2(t.Width);
            chunk[raster + 11] = (byte)int.Log2(t.Height);
            chunk[raster + 12] = (byte)t.Depth;
            chunk[raster + 13] = (byte)t.Levels;
            chunk[raster + 15] = 0x45;
            t.Chain.CopyTo(chunk, data);
            t.Palette.CopyTo(chunk, data + t.Chain.Length);
            data += Align(t.Chain.Length + t.Palette.Length);
        }
        W(chunk, 0x28, textures.Length > 0 ? nodesAt + 8 : 0x28);
        W(chunk, 0x2C, textures.Length > 0 ? nodesAt + nodeSize * (textures.Length - 1) + 8 : 0x28);
        return chunk;
    }

    /// <summary>A texture whose palette index at (x, y) is index(x, y), swizzled like the PSP stores it.</summary>
    public static Tex Indexed(string name, int width, int height, int depth, int levels, Func<int, int, int> index, byte[] palette)
    {
        var rowBytes = Math.Max(width * depth / 8, 16);
        var probe = new RasterInfo(0, width, height, depth, levels, rowBytes);
        var chain = new byte[PspRaster.PaletteOffset(probe)];
        for (var y = 0; y < height; y++)
        for (var x = 0; x < width; x++)
        {
            var offset = PspRaster.SwizzledOffset(x * depth / 8, y, rowBytes);
            var value = index(x, y);
            if (depth == 8) chain[offset] = (byte)value;
            else chain[offset] |= (byte)((x & 1) != 0 ? value << 4 : value);
        }
        return new Tex(name, width, height, depth, levels, rowBytes, chain, palette);
    }

    public static byte[] Palette(int colors) =>
        Enumerable.Range(0, colors).SelectMany(i => new[] { (byte)(i * 16), (byte)(255 - i), (byte)i, (byte)255 }).ToArray();

    static int Align(int value) => (value + 15) & ~15;
    static void W(byte[] chunk, int offset, int value) => BinaryPrimitives.WriteInt32LittleEndian(chunk.AsSpan(offset), value);
}
```

- [ ] **Step 2: Tests del raster que fallan**

```csharp
// builder/Builder.Core.Tests/Hd/PspRasterTests.cs
using Vcsnx.Builder.Core.Hd;

namespace Vcsnx.Builder.Core.Tests.Hd;

public class PspRasterTests
{
    [Theory] // values of ge_renderer.cpp swizzled_offset(byte_x, y, row_bytes)
    [InlineData(0, 0, 16, 0)]
    [InlineData(15, 7, 16, 127)]
    [InlineData(0, 8, 16, 128)]
    [InlineData(17, 9, 32, 401)]
    public void SwizzleMatchesTheConsole(int byteX, int y, int rowBytes, int expected) =>
        Assert.Equal(expected, PspRaster.SwizzledOffset(byteX, y, rowBytes));

    [Fact]
    public void ThePaletteFollowsEveryMipLevel()
    {
        // 32x64, 4 bits, 5 levels: 16x64 + 16x32 + 16x16 + 16x8 + 16x4 = 1984 bytes after the texels.
        Assert.Equal(0x90 + 1984, PspRaster.PaletteOffset(new RasterInfo(0x90, 32, 64, 4, 5, 16)));
        // One level: right after level 0.
        Assert.Equal(0x40 + 512 * 256, PspRaster.PaletteOffset(new RasterInfo(0x40, 512, 256, 8, 1, 512)));
    }

    [Fact]
    public void A4BitTextureDecodesLowNibbleFirstThroughItsPalette()
    {
        var palette = TexChunkBuilder.Palette(16);
        var chunk = TexChunkBuilder.Build(TexChunkBuilder.Indexed("t", 32, 8, 4, 2, (x, y) => (x + y) & 15, palette));
        var (_, raster) = Assert.Single(TexChunkReader.Textures(chunk));
        var info = PspRaster.Read(chunk, raster)!;
        var rgba = PspRaster.DecodeLevel0(chunk, info)!;
        Assert.Equal(32 * 8 * 4, rgba.Length);
        for (var y = 0; y < 8; y++)
        for (var x = 0; x < 32; x++)
            Assert.Equal(palette.AsSpan(((x + y) & 15) * 4, 4).ToArray(), rgba.AsSpan((y * 32 + x) * 4, 4).ToArray());
    }

    [Fact]
    public void An8BitTextureUsesA256ColorPalette()
    {
        var palette = TexChunkBuilder.Palette(256);
        var chunk = TexChunkBuilder.Build(TexChunkBuilder.Indexed("t", 16, 16, 8, 3, (x, y) => (x * 16 + y) & 255, palette));
        var (_, raster) = Assert.Single(TexChunkReader.Textures(chunk));
        var rgba = PspRaster.DecodeLevel0(chunk, PspRaster.Read(chunk, raster)!)!;
        Assert.Equal(palette.AsSpan(((5 * 16 + 3) & 255) * 4, 4).ToArray(), rgba.AsSpan((3 * 16 + 5) * 4, 4).ToArray());
    }

    [Fact]
    public void OtherDepthsAndRastersOutsideTheChunkAreRejected()
    {
        var chunk = TexChunkBuilder.Build(TexChunkBuilder.Indexed("t", 16, 16, 8, 1, (_, _) => 0, TexChunkBuilder.Palette(256)));
        var (_, raster) = Assert.Single(TexChunkReader.Textures(chunk));
        chunk[raster + 12] = 16;
        Assert.Null(PspRaster.Read(chunk, raster));
        Assert.Null(PspRaster.Read(chunk, chunk.Length - 8));
    }
}
```

Run: `dotnet test builder/Builder.Core.Tests --filter PspRasterTests`
Expected: falla la compilación (`PspRaster`, `TexChunkReader`, `RasterInfo` no existen).

- [ ] **Step 3: `PspRaster`**

```csharp
// builder/Builder.Core/Hd/PspRaster.cs
using System.Buffers.Binary;

namespace Vcsnx.Builder.Core.Hd;

/// <summary>The 16-byte raster of a 'tex' chunk: +4 texels, +8 level-0 row bytes, +10/+11 log2 of width and height,
/// +12 bits per texel, +13 (low 6 bits) mip levels.</summary>
public sealed record RasterInfo(int Texels, int Width, int Height, int Depth, int Levels, int RowBytes);

/// <summary>Decodes level 0 the way the console does (ge_renderer.cpp decode_texture_rgba_into): swizzled 4/8-bit
/// indices through a palette of 32-bit RGBA entries.</summary>
public static class PspRaster
{
    public static RasterInfo? Read(ReadOnlySpan<byte> chunk, int offset)
    {
        if (offset < 0 || offset + 16 > chunk.Length) return null;
        var texels = BinaryPrimitives.ReadInt32LittleEndian(chunk[(offset + 4)..]);
        int rowBytes = BinaryPrimitives.ReadUInt16LittleEndian(chunk[(offset + 8)..]);
        int lw = chunk[offset + 10] & 15, lh = chunk[offset + 11] & 15, depth = chunk[offset + 12], levels = chunk[offset + 13] & 0x3F;
        if (depth is not (4 or 8) || levels == 0 || texels < 0 || rowBytes == 0) return null;
        return new RasterInfo(texels, 1 << lw, 1 << lh, depth, levels, rowBytes);
    }

    public static long PaletteOffset(RasterInfo r)
    {
        long offset = r.Texels + (long)r.RowBytes * r.Height;
        for (var i = 1; i < r.Levels; i++)
            offset += (long)Math.Max(Math.Max(1, r.Width >> i) * r.Depth / 8, 16) * Math.Max(1, r.Height >> i);
        return offset;
    }

    public static int SwizzledOffset(int byteX, int y, int rowBytes)
    {
        var blocksPerRow = (rowBytes + 15) / 16;
        return ((y >> 3) * blocksPerRow + (byteX >> 4)) * 128 + (y & 7) * 16 + (byteX & 15);
    }

    public static byte[]? DecodeLevel0(ReadOnlySpan<byte> chunk, RasterInfo r)
    {
        var colors = r.Depth == 4 ? 16 : 256;
        var palette = PaletteOffset(r);
        if (palette + colors * 4L > chunk.Length) return null;
        var rgba = new byte[r.Width * r.Height * 4];
        for (var y = 0; y < r.Height; y++)
        for (var x = 0; x < r.Width; x++)
        {
            var at = (long)r.Texels + SwizzledOffset(x * r.Depth / 8, y, r.RowBytes);
            if (at >= chunk.Length) return null;
            var value = chunk[(int)at];
            var index = r.Depth == 4 ? ((x & 1) != 0 ? value >> 4 : value & 15) : value;
            chunk.Slice((int)(palette + index * 4), 4).CopyTo(rgba.AsSpan((y * r.Width + x) * 4, 4));
        }
        return rgba;
    }
}
```

- [ ] **Step 4: Tests del lector que fallan**

```csharp
// builder/Builder.Core.Tests/Hd/TexChunkReaderTests.cs
using System.Buffers.Binary;
using Vcsnx.Builder.Core.Hd;

namespace Vcsnx.Builder.Core.Tests.Hd;

public class TexChunkReaderTests
{
    static TexChunkBuilder.Tex Small(string name) =>
        TexChunkBuilder.Indexed(name, 16, 16, 4, 1, (_, _) => 1, TexChunkBuilder.Palette(16));

    [Fact]
    public void TheListGivesEveryTextureInOrder()
    {
        var chunk = TexChunkBuilder.Build(Small("a"), Small("b"), Small("c"));
        Assert.True(TexChunkReader.IsValid(chunk, 0, out var size));
        Assert.Equal(chunk.Length, size);
        Assert.Equal(new[] { "a", "b", "c" }, TexChunkReader.Textures(chunk).Select(t => t.Name));
    }

    [Fact]
    public void ChunksAreFoundOnlyAtTheAlignment()
    {
        var one = TexChunkBuilder.Build(Small("a"));
        var image = new byte[4096 + one.Length];
        one.CopyTo(image, 0);
        one.CopyTo(image, 4096);
        one.CopyTo(image, 1000); // not on a 2048 boundary: ignored
        Assert.Equal(new[] { 0, 4096 }, TexChunkReader.FindChunks(image, 2048));
    }

    [Fact]
    public void ACyclicListOrPointersOutsideTheChunkEndTheWalk()
    {
        var chunk = TexChunkBuilder.Build(Small("a"), Small("b"));
        var firstLink = BinaryPrimitives.ReadInt32LittleEndian(chunk.AsSpan(0x28));
        BinaryPrimitives.WriteInt32LittleEndian(chunk.AsSpan(firstLink + 0x60), firstLink); // b points back to a
        Assert.Equal(2, TexChunkReader.Textures(chunk).Count);
        BinaryPrimitives.WriteInt32LittleEndian(chunk.AsSpan(0x28), chunk.Length + 100);
        Assert.Empty(TexChunkReader.Textures(chunk));
    }

    [Fact]
    public void AHeaderThatDoesNotFitIsNotAChunk()
    {
        var chunk = TexChunkBuilder.Build(Small("a"));
        BinaryPrimitives.WriteInt32LittleEndian(chunk.AsSpan(8), chunk.Length + 1);
        Assert.False(TexChunkReader.IsValid(chunk, 0, out _));
    }
}
```

- [ ] **Step 5: `TexChunkReader`**

```csharp
// builder/Builder.Core/Hd/TexChunkReader.cs
using System.Buffers.Binary;
using System.Text;

namespace Vcsnx.Builder.Core.Hd;

/// <summary>Relocatable 'tex' chunks ("xet\0"): a 0x20-byte header (ident, 0, file size, data size, relocation
/// table, relocation count, ...), then a dictionary whose texture list starts at 0x28. Each node: +0 raster,
/// +4 dictionary, +8 next link, +12 previous link, +0x10 name (32 bytes). Links point at a node's +8.</summary>
public static class TexChunkReader
{
    const int Head = 0x28;
    const int MaxTextures = 10_000;

    public static bool IsValid(ReadOnlySpan<byte> data, int offset, out int size)
    {
        size = 0;
        if (offset < 0 || offset + 0x30 > data.Length || !data.Slice(offset, 4).SequenceEqual("xet\0"u8)) return false;
        var fileSize = BinaryPrimitives.ReadInt32LittleEndian(data[(offset + 8)..]);
        var relocTable = BinaryPrimitives.ReadInt32LittleEndian(data[(offset + 16)..]);
        var relocCount = BinaryPrimitives.ReadInt32LittleEndian(data[(offset + 20)..]);
        if (fileSize < 0x30 || offset + (long)fileSize > data.Length) return false;
        if (relocTable < 0x20 || relocCount < 0 || relocTable + 4L * relocCount > fileSize) return false;
        size = fileSize;
        return true;
    }

    public static IReadOnlyList<int> FindChunks(byte[] data, int alignment)
    {
        var found = new List<int>();
        for (var at = 0; at + 0x30 <= data.Length; at += alignment)
            if (IsValid(data, at, out _)) found.Add(at);
        return found;
    }

    public static IReadOnlyList<(string Name, int Raster)> Textures(ReadOnlySpan<byte> chunk)
    {
        var list = new List<(string, int)>();
        var seen = new HashSet<int>();
        var link = BinaryPrimitives.ReadInt32LittleEndian(chunk[Head..]);
        while (link != Head && link >= 8 && link + 4 <= chunk.Length && seen.Add(link) && list.Count < MaxTextures)
        {
            var node = link - 8;
            if (node + 0x30 > chunk.Length) break;
            var raster = BinaryPrimitives.ReadInt32LittleEndian(chunk[node..]);
            var name = chunk.Slice(node + 0x10, 32);
            var end = name.IndexOf((byte)0);
            list.Add((Encoding.ASCII.GetString(end < 0 ? name : name[..end]), raster));
            link = BinaryPrimitives.ReadInt32LittleEndian(chunk[link..]);
        }
        return list;
    }
}
```

Run: `dotnet test builder/Builder.Core.Tests --filter "PspRasterTests|TexChunkReaderTests"`
Expected: `Passed: 11`.

- [ ] **Step 6: Tests del catálogo que fallan**

```csharp
// builder/Builder.Core.Tests/Hd/HdCatalogTests.cs
using Vcsnx.Builder.Core.Hd;

namespace Vcsnx.Builder.Core.Tests.Hd;

public sealed class HdCatalogTests : IDisposable
{
    readonly string _game = Path.Combine(Path.GetTempPath(), "vcsnx-hd-" + Guid.NewGuid().ToString("N"));
    string RunData => Path.Combine(_game, "PSP_GAME", "USRDIR", "RUNDATA");

    public HdCatalogTests() => Directory.CreateDirectory(RunData);
    public void Dispose() => Directory.Delete(_game, recursive: true);

    static TexChunkBuilder.Tex Tex(string name, int size, int seed) =>
        TexChunkBuilder.Indexed(name, size, size, 4, 1, (x, y) => (x + y + seed) & 15, TexChunkBuilder.Palette(16));

    void WriteImage(string fileName, params byte[][] chunks)
    {
        var image = new byte[chunks.Length * 65536];
        for (var i = 0; i < chunks.Length; i++) chunks[i].CopyTo(image, i * 65536);
        File.WriteAllBytes(Path.Combine(RunData, fileName), image);
    }

    [Fact]
    public void ScreensAndModelTexturesAreReadAndKeyed()
    {
        File.WriteAllBytes(Path.Combine(RunData, "LOADSC0.XTX"), TexChunkBuilder.Build(Tex("screen", 64, 1)));
        WriteImage("GTA3PSP.IMG", TexChunkBuilder.Build(Tex("car_body", 32, 2)), TexChunkBuilder.Build(Tex("ped_face", 16, 3)));
        var scan = HdCatalog.Scan(HdCatalog.SourcesIn(_game));
        Assert.Equal(new[] { "screen", "car_body", "ped_face" }, scan.Textures.Select(t => t.Name));
        var face = scan.Textures[2];
        Assert.Equal(TextureKey.Of(face.Rgba, 16, 16), face.Key);
    }

    [Fact]
    public void LowercaseFileNamesAreFoundToo()
    {
        File.WriteAllBytes(Path.Combine(RunData, "loadsc0.xtx"), TexChunkBuilder.Build(Tex("screen", 64, 1)));
        WriteImage("gta3psp.img", TexChunkBuilder.Build(Tex("car_body", 32, 2)));
        Assert.Equal(2, HdCatalog.Scan(HdCatalog.SourcesIn(_game)).Textures.Count);
    }

    [Fact]
    public void TwoDTexturesTinyOnesAndDuplicatesAreSkippedAndCounted()
    {
        WriteImage("GTA3PSP.IMG",
            TexChunkBuilder.Build(Tex("HUD_money", 32, 1), Tex("car_body", 32, 2)),
            TexChunkBuilder.Build(Tex("car_copy", 32, 2), TexChunkBuilder.Indexed("dot", 2, 2, 4, 1, (_, _) => 0, TexChunkBuilder.Palette(16))));
        var scan = HdCatalog.Scan(HdCatalog.SourcesIn(_game));
        Assert.Equal(new[] { "car_body" }, scan.Textures.Select(t => t.Name));
        Assert.Equal(1, scan.Skipped2D);
        Assert.Equal(1, scan.Duplicates);
        Assert.Equal(1, scan.SkippedSmall);
    }

    [Fact]
    public void GarbageInAnArchiveIsSkippedNotFatal()
    {
        var bad = TexChunkBuilder.Build(Tex("car_body", 32, 2));
        bad[0x28] = 0xFF; bad[0x29] = 0xFF; // list head far outside the chunk
        WriteImage("GTA3PSP.IMG", bad, TexChunkBuilder.Build(Tex("ped_face", 16, 3)));
        Assert.Equal(new[] { "ped_face" }, HdCatalog.Scan(HdCatalog.SourcesIn(_game)).Textures.Select(t => t.Name));
    }
}
```

Run: `dotnet test builder/Builder.Core.Tests --filter HdCatalogTests`
Expected: falla la compilación (`HdCatalog` no existe).

- [ ] **Step 7: `HdCatalog`**

```csharp
// builder/Builder.Core/Hd/HdCatalog.cs
namespace Vcsnx.Builder.Core.Hd;

public sealed record HdSource(string Path, int Alignment);
public sealed record HdTexture(ulong Key, int Width, int Height, byte[] Rgba, string Source, string Name);
public sealed record HdScan(IReadOnlyList<HdTexture> Textures, int Skipped2D, int SkippedFormat, int SkippedSmall, int Duplicates);

/// <summary>Part 1's textures (spec §3): the loading and legal screens (every RUNDATA *.XTX, one chunk each) and the
/// models' textures in GTA3PSP.IMG (chunks at 2048-byte boundaries), minus 2D art, by key.</summary>
public static class HdCatalog
{
    public static readonly string[] TwoDWords = ["font", "hud", "radar", "icon", "button"];
    static readonly EnumerationOptions AnyCase = new() { MatchCasing = MatchCasing.CaseInsensitive };

    public static IReadOnlyList<HdSource> SourcesIn(string gameFolder)
    {
        var runData = Path.Combine(gameFolder, "PSP_GAME", "USRDIR", "RUNDATA");
        if (!Directory.Exists(runData)) return [];
        var sources = Directory.EnumerateFiles(runData, "*.xtx", AnyCase).Order(StringComparer.OrdinalIgnoreCase)
            .Select(path => new HdSource(path, 0)).ToList();
        sources.AddRange(Directory.EnumerateFiles(runData, "gta3psp.img", AnyCase).Select(path => new HdSource(path, 2048)));
        return sources;
    }

    public static bool IsTwoD(string name) => TwoDWords.Any(word => name.Contains(word, StringComparison.OrdinalIgnoreCase));

    public static HdScan Scan(IEnumerable<HdSource> sources)
    {
        var textures = new List<HdTexture>();
        var keys = new HashSet<ulong>();
        int twoD = 0, format = 0, small = 0, duplicates = 0;
        foreach (var source in sources)
        {
            var data = File.ReadAllBytes(source.Path);
            IReadOnlyList<int> offsets = source.Alignment == 0 ? (TexChunkReader.IsValid(data, 0, out _) ? [0] : []) : TexChunkReader.FindChunks(data, source.Alignment);
            foreach (var offset in offsets)
            {
                TexChunkReader.IsValid(data, offset, out var size);
                var chunk = data.AsSpan(offset, size);
                foreach (var (name, raster) in TexChunkReader.Textures(chunk))
                {
                    if (source.Alignment != 0 && IsTwoD(name)) { twoD++; continue; }
                    var info = PspRaster.Read(chunk, raster);
                    if (info is null) { format++; continue; }
                    if (info.Width < 4 || info.Height < 4) { small++; continue; }
                    var rgba = PspRaster.DecodeLevel0(chunk, info);
                    if (rgba is null) { format++; continue; }
                    var key = TextureKey.Of(rgba, info.Width, info.Height);
                    if (!keys.Add(key)) { duplicates++; continue; }
                    textures.Add(new HdTexture(key, info.Width, info.Height, rgba, Path.GetFileName(source.Path), name));
                }
            }
        }
        return new HdScan(textures, twoD, format, small, duplicates);
    }
}
```

Run: `dotnet test builder/Builder.Core.Tests`
Expected: toda la suite en verde, incluidos los 4 `HdCatalogTests`.

- [ ] **Step 8: Commit**

```bash
git add builder/Builder.Core/Hd builder/Builder.Core.Tests/Hd
git commit -m "feat(builder): read the game's texture chunks and decode them as the console does"
```

---

### Task 3: Validación contra la consola (compuerta)

**Files:**
- Modify: `builder/Builder.Cli/Program.cs` (opción `--hd-list <archivo>`)
- Create: `tools/compare_texture_keys.py`
- Create: `tests/tools/test_compare_texture_keys.py`

**Interfaces:**
- Consumes: `HdCatalog.SourcesIn`, `HdCatalog.Scan`, `TextureKey.FileName`.
- Produces: `Builder.Cli --game <PSP_DATA> --hd-list <archivo>`. Escribe una línea
  `<código> <ancho> <alto> <fuente> <nombre>` por textura y sale con 0.

- [ ] **Step 1: Test del script de comparación que falla**

```python
# tests/tools/test_compare_texture_keys.py
"""compare_texture_keys: which of the textures the console dumped the Builder's list also has."""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TOOL = Path(__file__).resolve().parents[2] / "tools" / "compare_texture_keys.py"


class CompareTest(unittest.TestCase):
    def test_counts_matches_per_source_and_lists_unmatched_sizes(self):
        with tempfile.TemporaryDirectory() as tmp:
            listing = Path(tmp, "list.txt")
            listing.write_text("00000000000000aa 64 64 GTA3PSP.IMG car\n00000000000000bb 32 32 LOADSC0.XTX s\n")
            dumped = Path(tmp, "dumped.txt")
            dumped.write_text("00000000000000aa.png\n00000000000000cc.png\n00000000000000aa.png.usage\n")
            out = subprocess.run([sys.executable, "-I", str(TOOL), str(listing), str(dumped)],
                                 capture_output=True, text=True, check=True).stdout
            self.assertIn("dumped 2", out)
            self.assertIn("matched 1 (GTA3PSP.IMG 1)", out)
            self.assertIn("unmatched: 00000000000000cc", out)


if __name__ == "__main__":
    unittest.main()
```

Run: `python3 -I tests/tools/test_compare_texture_keys.py`
Expected: FAIL (`compare_texture_keys.py` no existe).

- [ ] **Step 2: El script**

```python
# tools/compare_texture_keys.py
"""Compares the Builder's texture list (Builder.Cli --hd-list) with the names of the textures the console dumped
([Textures] DumpOriginals=true writes sdmc:/switch/vcsnx/Textures/originals/<key>.png). Prints how many dumped keys
the Builder also produces, per source, and the dumped keys it does not. Nothing of the game is written.
Usage: python3 -I tools/compare_texture_keys.py <list.txt> <dumped-names.txt>
"""
import sys
from collections import Counter
from pathlib import Path


def main(listing: Path, dumped: Path) -> int:
    known = {}
    for line in listing.read_text().splitlines():
        parts = line.split(maxsplit=4)
        if len(parts) >= 4:
            known[parts[0]] = parts[3]
    keys = sorted({name.strip()[:16] for name in dumped.read_text().splitlines() if name.strip().endswith(".png")})
    per_source = Counter(known[key] for key in keys if key in known)
    matched = sum(per_source.values())
    detail = ", ".join(f"{source} {count}" for source, count in sorted(per_source.items()))
    print(f"dumped {len(keys)}")
    print(f"matched {matched} ({detail})")
    print("unmatched: " + " ".join(key for key in keys if key not in known))
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    sys.exit(main(Path(sys.argv[1]), Path(sys.argv[2])))
```

Run: `python3 -I tests/tools/test_compare_texture_keys.py`
Expected: `OK`.

- [ ] **Step 3: `--hd-list` en `Builder.Cli`**

En `builder/Builder.Cli/Program.cs`, justo después de la línea
`if (!options.TryGetValue("game", out var gameFolder)) { ... }`:

```csharp
if (options.TryGetValue("hd-list", out var listPath))
{
    var scan = Vcsnx.Builder.Core.Hd.HdCatalog.Scan(Vcsnx.Builder.Core.Hd.HdCatalog.SourcesIn(gameFolder));
    File.WriteAllLines(listPath, scan.Textures.Select(t => $"{t.Key:x16} {t.Width} {t.Height} {t.Source} {t.Name}"));
    Console.WriteLine($"{scan.Textures.Count} textures, skipped 2D {scan.Skipped2D} format {scan.SkippedFormat} small {scan.SkippedSmall} duplicates {scan.Duplicates}");
    return 0;
}
```

Agregar `--hd-list <file>` al comentario de uso al inicio del archivo.

Run:

```bash
dotnet run --project builder/Builder.Cli -c Release -- --game <SD copy>/switch/vcsnx/PSP_DATA --hd-list local/notes/hd/texture_list.txt
```

Expected: una línea `N textures, …`, con N entre 2.000 y 2.776. `local/notes/hd/texture_list.txt` queda solo en
`local/`.

- [ ] **Step 4: Commit**

```bash
git add builder/Builder.Cli/Program.cs tools/compare_texture_keys.py tests/tools/test_compare_texture_keys.py
git commit -m "feat(builder): list the HD texture keys, and compare them with a console dump"
```

- [ ] **Step 5: Sesión en la consola (con el autor)**

Hace falta un "ftp listo" del autor para cada subida.

1. Bajar `sdmc:/switch/vcsnx/VCSNative.ini`, poner `DumpOriginals=true` en `[Textures]`, subirlo y verificarlo por
   sha256.
2. El autor juega unos 5 minutos: camina cerca de peatones, maneja varios carros y saca un arma.
3. Listar los nombres:

```bash
curl -s --list-only ftp://<switch>:5000/switch/vcsnx/Textures/originals/ > local/notes/hd/dumped.txt
python3 -I tools/compare_texture_keys.py local/notes/hd/texture_list.txt local/notes/hd/dumped.txt
```

4. Volver a poner `DumpOriginals=false`, subirlo y verificarlo. Preguntar al autor antes de borrar
   `Textures/originals/` de la SD.

**Compuerta:** se sigue a la tarea 4 si `matched` de `GTA3PSP.IMG` es de al menos 200 y hay al menos uno de un
`.XTX`. Si no:
- Bajar a `local/notes/hd/dump/` hasta 50 PNG sin coincidencia, de 16 a 128 px.
- Comparar sus píxeles con la textura del mismo tamaño mejor parecida en la lista, con un script en el scratchpad
  (`-I`). Si coinciden RGB pero no el alfa, el problema es la regla del alfa; si nada coincide, es la paleta o
  la disposición.
- Corregir `PspRaster` con un test sintético nuevo que fije la regla corregida, y anotar un `Ruling:`.
- Repetir el paso 3 de esta sesión.

---

### Task 4: PNG, remuestreo y fidelidad

**Files:**
- Create: `builder/Builder.Core/Hd/PngCodec.cs`, `Resample.cs`, `Fidelity.cs`
- Create: `builder/Builder.Core.Tests/Hd/PngCodecTests.cs`, `ResampleTests.cs`, `FidelityTests.cs`

**Interfaces:**
- Produces:
  - `static byte[] PngCodec.EncodeRgba(ReadOnlySpan<byte> rgba, int width, int height)`
  - `static (int Width, int Height, byte[] Rgba) PngCodec.Decode(byte[] png)`
  - `static byte[] Resample.BoxDown(ReadOnlySpan<byte> rgba, int width, int height, int factor)`
  - `static byte[] Resample.Lanczos3Wrap(ReadOnlySpan<byte> rgba, int width, int height, int scale)`
  - `static byte[] Resample.WrapPad(ReadOnlySpan<byte> rgba, int width, int height, int marginX, int marginY)`
  - `static byte[] Resample.Crop(ReadOnlySpan<byte> rgba, int width, int x, int y, int cropWidth, int cropHeight)`
  - `const double Fidelity.MinPsnr = 32.0`
  - `static double Fidelity.Psnr(ReadOnlySpan<byte> original, ReadOnlySpan<byte> candidate)`

- [ ] **Step 1: Tests que fallan**

```csharp
// builder/Builder.Core.Tests/Hd/PngCodecTests.cs
using Vcsnx.Builder.Core.Hd;

namespace Vcsnx.Builder.Core.Tests.Hd;

public class PngCodecTests
{
    // 3x4 RGB, rows filtered Sub, Up, Average, Paeth (made with Python's zlib for this test; no game data).
    const string FilteredRgb = "iVBORw0KGgoAAAANSUhEUgAAAAMAAAAECAIAAADETxJQAAAAJElEQVR4nGNkOMGg8YgRiJi4GIwgiJk/xVvyoxQQsYAFGIEIAKPsBvXlL9mqAAAAAElFTkSuQmCC";

    [Fact]
    public void RgbaSurvivesARoundTrip()
    {
        var rgba = Enumerable.Range(0, 3 * 2 * 4).Select(i => (byte)(i * 11)).ToArray();
        var (w, h, back) = PngCodec.Decode(PngCodec.EncodeRgba(rgba, 3, 2));
        Assert.Equal((3, 2), (w, h));
        Assert.Equal(rgba, back);
    }

    [Fact]
    public void AnRgbPngWithEveryFilterDecodesToOpaqueRgba()
    {
        var (w, h, rgba) = PngCodec.Decode(Convert.FromBase64String(FilteredRgb));
        Assert.Equal((3, 4), (w, h));
        int[][] expected = [[0, 200, 0], [40, 170, 1], [80, 140, 2], [10, 200, 50], [50, 170, 51], [90, 140, 52],
                            [20, 200, 100], [60, 170, 101], [100, 140, 102], [30, 200, 150], [70, 170, 151], [110, 140, 152]];
        for (var i = 0; i < expected.Length; i++)
            Assert.Equal(expected[i].Select(v => (byte)v).Append((byte)255).ToArray(), rgba.AsSpan(i * 4, 4).ToArray());
    }

    [Fact]
    public void SomethingElseIsRejected() =>
        Assert.Throws<InvalidDataException>(() => PngCodec.Decode([1, 2, 3, 4, 5, 6, 7, 8, 9]));
}
```

```csharp
// builder/Builder.Core.Tests/Hd/ResampleTests.cs
using Vcsnx.Builder.Core.Hd;

namespace Vcsnx.Builder.Core.Tests.Hd;

public class ResampleTests
{
    static byte[] Image(int w, int h, Func<int, int, int, byte> value) =>
        Enumerable.Range(0, w * h * 4).Select(i => value(i / 4 % w, i / 4 / w, i % 4)).ToArray();

    [Fact]
    public void BoxDownAveragesEachBlock()
    {
        var rgba = Image(2, 2, (x, y, c) => (byte)(c == 3 ? 255 : (x + 2 * y) * 10)); // 0, 10, 20, 30
        Assert.Equal(new byte[] { 15, 15, 15, 255 }, Resample.BoxDown(rgba, 2, 2, 2));
    }

    [Fact]
    public void LanczosKeepsAFlatImageFlatAndScalesTheSize()
    {
        var flat = Image(8, 4, (_, _, c) => (byte)(c == 3 ? 255 : 77));
        var up = Resample.Lanczos3Wrap(flat, 8, 4, 4);
        Assert.Equal(32 * 16 * 4, up.Length);
        Assert.All(up.Where((_, i) => i % 4 != 3), v => Assert.Equal(77, v));
    }

    [Fact]
    public void LanczosWrapsAroundSoATileStaysATile()
    {
        var tile = Image(8, 8, (x, y, c) => (byte)(c == 3 ? 255 : (x * 31 + y * 7 + c * 50) & 255));
        var rolled = Image(8, 8, (x, y, c) => tile[(y * 8 + (x + 7) % 8) * 4 + c]); // shifted right by one texel
        var up = Resample.Lanczos3Wrap(tile, 8, 8, 2);
        var upRolled = Resample.Lanczos3Wrap(rolled, 8, 8, 2);
        for (var y = 0; y < 16; y++)
        for (var x = 0; x < 16; x++)
        for (var c = 0; c < 4; c++)
            Assert.Equal(up[(y * 16 + (x + 14) % 16) * 4 + c], upRolled[(y * 16 + x) * 4 + c]);
    }

    [Fact]
    public void WrapPadCopiesTheOppositeSideAndCropUndoesIt()
    {
        var rgba = Image(4, 4, (x, y, c) => (byte)(x * 16 + y * 4 + c));
        var padded = Resample.WrapPad(rgba, 4, 4, 1, 2);
        Assert.Equal(6 * 8 * 4, padded.Length);
        Assert.Equal(rgba.AsSpan((2 * 4 + 3) * 4, 4).ToArray(), padded.AsSpan((4 * 6 + 0) * 4, 4).ToArray()); // left margin = last column
        Assert.Equal(rgba, Resample.Crop(padded, 6, 1, 2, 4, 4));
    }
}
```

```csharp
// builder/Builder.Core.Tests/Hd/FidelityTests.cs
using Vcsnx.Builder.Core.Hd;

namespace Vcsnx.Builder.Core.Tests.Hd;

public class FidelityTests
{
    [Fact]
    public void TheSameImageIsInfinitelyFaithful()
    {
        byte[] rgba = [10, 20, 30, 255, 40, 50, 60, 255];
        Assert.True(Fidelity.Psnr(rgba, rgba) >= Fidelity.MinPsnr);
    }

    [Fact]
    public void ColorsUnderFullyTransparentTexelsDoNotCount()
    {
        byte[] original = [10, 20, 30, 255, 0, 0, 0, 0];
        byte[] candidate = [10, 20, 30, 255, 200, 200, 200, 0];
        Assert.Equal(double.PositiveInfinity, Fidelity.Psnr(original, candidate));
    }

    [Fact]
    public void InvertedColorsFail()
    {
        byte[] original = [10, 20, 30, 255, 200, 100, 50, 255];
        var inverted = original.Select((v, i) => i % 4 == 3 ? v : (byte)(255 - v)).ToArray();
        Assert.True(Fidelity.Psnr(original, inverted) < Fidelity.MinPsnr);
    }
}
```

Run: `dotnet test builder/Builder.Core.Tests --filter "PngCodecTests|ResampleTests|FidelityTests"`
Expected: falla la compilación (las tres clases no existen).

- [ ] **Step 2: `PngCodec`**

```csharp
// builder/Builder.Core/Hd/PngCodec.cs
using System.Buffers.Binary;
using System.IO.Compression;

namespace Vcsnx.Builder.Core.Hd;

/// <summary>Writes RGBA8 PNGs and reads the 8-bit, non-interlaced RGB/RGBA PNGs Real-ESRGAN writes. No image library.</summary>
public static class PngCodec
{
    static readonly byte[] Signature = [0x89, (byte)'P', (byte)'N', (byte)'G', 0x0D, 0x0A, 0x1A, 0x0A];
    static readonly uint[] CrcTable = MakeCrcTable();

    public static byte[] EncodeRgba(ReadOnlySpan<byte> rgba, int width, int height)
    {
        using var raw = new MemoryStream();
        using (var z = new ZLibStream(raw, CompressionLevel.Optimal, leaveOpen: true))
            for (var y = 0; y < height; y++)
            {
                z.WriteByte(0);
                z.Write(rgba.Slice(y * width * 4, width * 4));
            }
        using var png = new MemoryStream();
        png.Write(Signature);
        var header = new byte[13];
        BinaryPrimitives.WriteInt32BigEndian(header, width);
        BinaryPrimitives.WriteInt32BigEndian(header.AsSpan(4), height);
        header[8] = 8; header[9] = 6;
        WriteChunk(png, "IHDR"u8, header);
        WriteChunk(png, "IDAT"u8, raw.ToArray());
        WriteChunk(png, "IEND"u8, []);
        return png.ToArray();
    }

    public static (int Width, int Height, byte[] Rgba) Decode(byte[] png)
    {
        if (png.Length < 8 || !png.AsSpan(0, 8).SequenceEqual(Signature)) throw new InvalidDataException("not a PNG");
        int width = 0, height = 0, channels = 0;
        using var idat = new MemoryStream();
        for (var at = 8; at + 12 <= png.Length;)
        {
            var length = BinaryPrimitives.ReadInt32BigEndian(png.AsSpan(at));
            if (length < 0 || at + 12L + length > png.Length) throw new InvalidDataException("truncated PNG");
            var type = png.AsSpan(at + 4, 4);
            var data = png.AsSpan(at + 8, length);
            if (type.SequenceEqual("IHDR"u8))
            {
                width = BinaryPrimitives.ReadInt32BigEndian(data);
                height = BinaryPrimitives.ReadInt32BigEndian(data[4..]);
                if (data[8] != 8 || data[12] != 0 || data[9] is not (2 or 6)) throw new InvalidDataException("unsupported PNG");
                channels = data[9] == 6 ? 4 : 3;
            }
            else if (type.SequenceEqual("IDAT"u8)) idat.Write(data);
            else if (type.SequenceEqual("IEND"u8)) break;
            at += 12 + length;
        }
        if (channels == 0 || width <= 0 || height <= 0) throw new InvalidDataException("PNG without a header");
        var stride = width * channels;
        var raw = new byte[(stride + 1) * height];
        idat.Position = 0;
        using (var z = new ZLibStream(idat, CompressionMode.Decompress)) z.ReadExactly(raw);
        var pixels = new byte[stride * height];
        for (var y = 0; y < height; y++)
        {
            var filter = raw[y * (stride + 1)];
            var line = raw.AsSpan(y * (stride + 1) + 1, stride);
            var row = pixels.AsSpan(y * stride, stride);
            ReadOnlySpan<byte> prior = y == 0 ? new byte[stride] : pixels.AsSpan((y - 1) * stride, stride);
            for (var i = 0; i < stride; i++)
            {
                int a = i >= channels ? row[i - channels] : 0, b = prior[i], c = i >= channels ? prior[i - channels] : 0;
                var predictor = filter switch
                {
                    0 => 0, 1 => a, 2 => b, 3 => (a + b) / 2, 4 => Paeth(a, b, c),
                    _ => throw new InvalidDataException("bad PNG filter"),
                };
                row[i] = (byte)(line[i] + predictor);
            }
        }
        if (channels == 4) return (width, height, pixels);
        var rgba = new byte[width * height * 4];
        for (var i = 0; i < width * height; i++)
        {
            pixels.AsSpan(i * 3, 3).CopyTo(rgba.AsSpan(i * 4));
            rgba[i * 4 + 3] = 255;
        }
        return (width, height, rgba);
    }

    static int Paeth(int a, int b, int c)
    {
        int p = a + b - c, pa = Math.Abs(p - a), pb = Math.Abs(p - b), pc = Math.Abs(p - c);
        return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
    }

    static void WriteChunk(Stream png, ReadOnlySpan<byte> type, ReadOnlySpan<byte> data)
    {
        Span<byte> word = stackalloc byte[4];
        BinaryPrimitives.WriteInt32BigEndian(word, data.Length);
        png.Write(word);
        png.Write(type);
        png.Write(data);
        var crc = 0xFFFFFFFFu;
        foreach (var b in type) crc = CrcTable[(crc ^ b) & 0xFF] ^ (crc >> 8);
        foreach (var b in data) crc = CrcTable[(crc ^ b) & 0xFF] ^ (crc >> 8);
        BinaryPrimitives.WriteUInt32BigEndian(word, crc ^ 0xFFFFFFFFu);
        png.Write(word);
    }

    static uint[] MakeCrcTable()
    {
        var table = new uint[256];
        for (uint n = 0; n < 256; n++)
        {
            var c = n;
            for (var k = 0; k < 8; k++) c = (c & 1) != 0 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[n] = c;
        }
        return table;
    }
}
```

- [ ] **Step 3: `Resample` y `Fidelity`**

```csharp
// builder/Builder.Core/Hd/Resample.cs
namespace Vcsnx.Builder.Core.Hd;

/// <summary>RGBA8 resampling for the HD step: box reduction (the fidelity check, and 4x to 2x), Lanczos-3 with
/// wrap-around (classic upscale; textures tile), and the toroidal margin the upscaler adds around each texture.</summary>
public static class Resample
{
    public static byte[] BoxDown(ReadOnlySpan<byte> rgba, int width, int height, int factor)
    {
        int w = width / factor, h = height / factor, n = factor * factor;
        var output = new byte[w * h * 4];
        for (var y = 0; y < h; y++)
        for (var x = 0; x < w; x++)
        for (var c = 0; c < 4; c++)
        {
            var sum = 0;
            for (var dy = 0; dy < factor; dy++)
            for (var dx = 0; dx < factor; dx++)
                sum += rgba[((y * factor + dy) * width + x * factor + dx) * 4 + c];
            output[(y * w + x) * 4 + c] = (byte)((sum + n / 2) / n);
        }
        return output;
    }

    public static byte[] Lanczos3Wrap(ReadOnlySpan<byte> rgba, int width, int height, int scale)
    {
        int w = width * scale, h = height * scale;
        var horizontal = new float[w * height * 4];
        for (var y = 0; y < height; y++)
        for (var x = 0; x < w; x++)
            Sample(x, scale, width, i => rgba.Slice((y * width + i) * 4, 4), horizontal.AsSpan((y * w + x) * 4, 4));
        var output = new byte[w * h * 4];
        Span<float> pixel = stackalloc float[4];
        for (var y = 0; y < h; y++)
        for (var x = 0; x < w; x++)
        {
            SampleColumn(y, scale, height, horizontal, w, x, pixel);
            for (var c = 0; c < 4; c++) output[(y * w + x) * 4 + c] = (byte)Math.Clamp((int)MathF.Round(pixel[c]), 0, 255);
        }
        return output;
    }

    delegate ReadOnlySpan<byte> Texel(int index);

    static void Sample(int outputIndex, int scale, int size, Texel texel, Span<float> result)
    {
        var center = (outputIndex + 0.5f) / scale - 0.5f;
        var first = (int)MathF.Floor(center) - 2;
        result.Clear();
        var total = 0f;
        for (var k = 0; k < 6; k++)
        {
            var weight = Kernel(center - (first + k));
            var source = texel(((first + k) % size + size) % size);
            for (var c = 0; c < 4; c++) result[c] += weight * source[c];
            total += weight;
        }
        for (var c = 0; c < 4; c++) result[c] /= total;
    }

    static void SampleColumn(int outputIndex, int scale, int size, float[] rows, int rowWidth, int x, Span<float> result)
    {
        var center = (outputIndex + 0.5f) / scale - 0.5f;
        var first = (int)MathF.Floor(center) - 2;
        result.Clear();
        var total = 0f;
        for (var k = 0; k < 6; k++)
        {
            var weight = Kernel(center - (first + k));
            var row = ((first + k) % size + size) % size;
            for (var c = 0; c < 4; c++) result[c] += weight * rows[(row * rowWidth + x) * 4 + c];
            total += weight;
        }
        for (var c = 0; c < 4; c++) result[c] /= total;
    }

    static float Kernel(float d)
    {
        d = MathF.Abs(d);
        if (d < 1e-6f) return 1f;
        if (d >= 3f) return 0f;
        var pd = MathF.PI * d;
        return 3f * MathF.Sin(pd) * MathF.Sin(pd / 3f) / (pd * pd);
    }

    public static byte[] WrapPad(ReadOnlySpan<byte> rgba, int width, int height, int marginX, int marginY)
    {
        int w = width + 2 * marginX, h = height + 2 * marginY;
        var output = new byte[w * h * 4];
        for (var y = 0; y < h; y++)
        for (var x = 0; x < w; x++)
        {
            int sx = ((x - marginX) % width + width) % width, sy = ((y - marginY) % height + height) % height;
            rgba.Slice((sy * width + sx) * 4, 4).CopyTo(output.AsSpan((y * w + x) * 4, 4));
        }
        return output;
    }

    public static byte[] Crop(ReadOnlySpan<byte> rgba, int width, int x, int y, int cropWidth, int cropHeight)
    {
        var output = new byte[cropWidth * cropHeight * 4];
        for (var row = 0; row < cropHeight; row++)
            rgba.Slice(((y + row) * width + x) * 4, cropWidth * 4).CopyTo(output.AsSpan(row * cropWidth * 4));
        return output;
    }
}
```

```csharp
// builder/Builder.Core/Hd/Fidelity.cs
namespace Vcsnx.Builder.Core.Hd;

/// <summary>Style A's check (spec §4.3): the upscale, reduced back, against the original. RGB counts only where the
/// original texel is visible (alpha above 0); alpha counts everywhere. Same 32 dB bar as the console's upscaler.</summary>
public static class Fidelity
{
    public const double MinPsnr = 32.0;

    public static double Psnr(ReadOnlySpan<byte> original, ReadOnlySpan<byte> candidate)
    {
        double squared = 0;
        long samples = 0;
        for (var i = 0; i + 3 < original.Length; i += 4)
        {
            if (original[i + 3] != 0)
                for (var c = 0; c < 3; c++)
                {
                    double d = original[i + c] - candidate[i + c];
                    squared += d * d;
                    samples++;
                }
            double da = original[i + 3] - candidate[i + 3];
            squared += da * da;
            samples++;
        }
        if (squared == 0) return double.PositiveInfinity;
        return 10 * Math.Log10(255.0 * 255.0 / (squared / samples));
    }
}
```

Run: `dotnet test builder/Builder.Core.Tests --filter "PngCodecTests|ResampleTests|FidelityTests"`
Expected: `Passed: 10`.

- [ ] **Step 4: Commit**

```bash
git add builder/Builder.Core/Hd builder/Builder.Core.Tests/Hd
git commit -m "feat(builder): PNG, wrap-around Lanczos and the fidelity check for HD textures"
```

---

### Task 5: El ampliador

**Files:**
- Create: `builder/Builder.Core/Hd/HdPrep.cs`, `HdUpscaler.cs`
- Create: `builder/Builder.Core.Tests/Hd/FakeEsrgan.cs`, `HdPrepTests.cs`, `HdUpscalerTests.cs`

**Interfaces:**
- Consumes: `PngCodec`, `Resample`, `Fidelity`, `HdTexture`, `IProcessRunner`.
- Produces:
  - `static int HdPrep.ScaleFor(int width, int height)`
  - `static (int X, int Y) HdPrep.MarginFor(int width, int height)`
  - `static byte[] HdPrep.FillHidden(ReadOnlySpan<byte> rgba, int width, int height)`
  - `static bool HdPrep.BinaryAlpha(ReadOnlySpan<byte> rgba)`
  - `sealed record UpscaleOutcome(byte[] Rgba, int Width, int Height, bool UsedAi)`
  - `sealed class HdUpscaler(IProcessRunner runner, string? esrgan, string model, string workDir)` con
    `Task<IReadOnlyList<UpscaleOutcome>> UpscaleAsync(IReadOnlyList<HdTexture> batch, CancellationToken ct)`,
    `bool AiAvailable` y `static UpscaleOutcome Classic(HdTexture texture)`.

- [ ] **Step 1: Real-ESRGAN falso para los tests**

```csharp
// builder/Builder.Core.Tests/Hd/FakeEsrgan.cs
using Vcsnx.Builder.Core.Hd;

namespace Vcsnx.Builder.Core.Tests.Hd;

/// <summary>Stands in for realesrgan-ncnn-vulkan: reads every PNG of -i and writes a 4x one to -o. Faithful
/// (nearest neighbour), Inverting (wrong colors) or Failing (exit code 1, no output).</summary>
internal sealed class FakeEsrgan(FakeEsrgan.Mode mode) : IProcessRunner
{
    public enum Mode { Faithful, Inverting, Failing }
    public List<IReadOnlyList<string>> Calls { get; } = [];

    public Task<ProcessResult> RunAsync(string file, IReadOnlyList<string> args, CancellationToken ct)
    {
        Calls.Add(args);
        if (mode == Mode.Failing) return Task.FromResult(new ProcessResult(1, "vkCreateInstance failed"));
        string input = args[args.ToList().IndexOf("-i") + 1], output = args[args.ToList().IndexOf("-o") + 1];
        Directory.CreateDirectory(output);
        foreach (var path in Directory.GetFiles(input, "*.png"))
        {
            var (w, h, rgba) = PngCodec.Decode(File.ReadAllBytes(path));
            var up = new byte[w * 4 * h * 4 * 4];
            for (var y = 0; y < h * 4; y++)
            for (var x = 0; x < w * 4; x++)
            for (var c = 0; c < 4; c++)
            {
                var v = rgba[((y / 4) * w + x / 4) * 4 + c];
                up[(y * w * 4 + x) * 4 + c] = mode == Mode.Inverting && c < 3 ? (byte)(255 - v) : v;
            }
            File.WriteAllBytes(Path.Combine(output, Path.GetFileName(path)), PngCodec.EncodeRgba(up, w * 4, h * 4));
        }
        return Task.FromResult(new ProcessResult(0, ""));
    }
}
```

- [ ] **Step 2: Tests que fallan**

```csharp
// builder/Builder.Core.Tests/Hd/HdPrepTests.cs
using Vcsnx.Builder.Core.Hd;

namespace Vcsnx.Builder.Core.Tests.Hd;

public class HdPrepTests
{
    [Theory]
    [InlineData(64, 64, 4)]
    [InlineData(32, 128, 2)]
    [InlineData(512, 256, 2)]
    [InlineData(1024, 512, 1)]
    public void TheScaleFollowsTheLargestSide(int w, int h, int scale) => Assert.Equal(scale, HdPrep.ScaleFor(w, h));

    [Fact]
    public void TheMarginIsEightTexelsOrAQuarterOfTheSide() => Assert.Equal((8, 2), HdPrep.MarginFor(64, 8));

    [Fact]
    public void HiddenTexelsTakeTheNearestVisibleColorAndStayHidden()
    {
        byte[] rgba = [255, 0, 0, 255, 9, 9, 9, 0, 1, 1, 1, 0, 9, 9, 9, 0, 0, 0, 255, 255]; // R . . . B
        var filled = HdPrep.FillHidden(rgba, 5, 1);
        Assert.Equal(new byte[] { 255, 0, 0, 0 }, filled.AsSpan(4, 4).ToArray());
        Assert.Equal(new byte[] { 0, 0, 255, 0 }, filled.AsSpan(12, 4).ToArray());
    }

    [Fact]
    public void BinaryAlphaMeansOnlyHiddenOrFull()
    {
        Assert.True(HdPrep.BinaryAlpha([1, 2, 3, 0, 4, 5, 6, 255]));
        Assert.False(HdPrep.BinaryAlpha([1, 2, 3, 128]));
    }
}
```

```csharp
// builder/Builder.Core.Tests/Hd/HdUpscalerTests.cs
using Vcsnx.Builder.Core.Hd;

namespace Vcsnx.Builder.Core.Tests.Hd;

public sealed class HdUpscalerTests : IDisposable
{
    readonly string _work = Path.Combine(Path.GetTempPath(), "vcsnx-up-" + Guid.NewGuid().ToString("N"));
    public void Dispose() { if (Directory.Exists(_work)) Directory.Delete(_work, recursive: true); }

    static HdTexture Texture(int size, bool cutout = false) => new(1, size, size,
        Enumerable.Range(0, size * size).SelectMany(i => new[] { (byte)(i * 7), (byte)(i * 3), (byte)i,
            (byte)(cutout && i % 3 == 0 ? 0 : 255) }).ToArray(), "test", "t");

    [Fact]
    public async Task AFaithfulAiResultIsKeptAtFourAndTwoTimes()
    {
        var runner = new FakeEsrgan(FakeEsrgan.Mode.Faithful);
        var outcomes = await new HdUpscaler(runner, "esrgan", "model-x", _work).UpscaleAsync([Texture(16), Texture(128)], CancellationToken.None);
        Assert.All(outcomes, o => Assert.True(o.UsedAi));
        Assert.Equal((64, 64), (outcomes[0].Width, outcomes[0].Height));
        Assert.Equal((256, 256), (outcomes[1].Width, outcomes[1].Height));
        var models = Path.Combine(Path.GetDirectoryName(Path.GetFullPath("esrgan"))!, "models");
        Assert.Equal(new[] { "-i", Path.Combine(_work, "in"), "-o", Path.Combine(_work, "out"), "-m", models, "-n", "model-x", "-s", "4", "-f", "png" }, runner.Calls.Single());
    }

    [Fact]
    public async Task AnUnfaithfulAiResultFallsBackToLanczos()
    {
        var outcomes = await new HdUpscaler(new FakeEsrgan(FakeEsrgan.Mode.Inverting), "esrgan", "m", _work).UpscaleAsync([Texture(16)], CancellationToken.None);
        Assert.False(outcomes.Single().UsedAi);
        Assert.Equal(64 * 64 * 4, outcomes.Single().Rgba.Length);
    }

    [Fact]
    public async Task WithoutAWorkingAiEverythingIsClassicAndItIsNotRetried()
    {
        var runner = new FakeEsrgan(FakeEsrgan.Mode.Failing);
        var upscaler = new HdUpscaler(runner, "esrgan", "m", _work);
        await upscaler.UpscaleAsync([Texture(16)], CancellationToken.None);
        var second = await upscaler.UpscaleAsync([Texture(32)], CancellationToken.None);
        Assert.False(upscaler.AiAvailable);
        Assert.False(second.Single().UsedAi);
        Assert.Single(runner.Calls);
    }

    [Fact]
    public async Task CutoutsStayCrisp()
    {
        var outcome = (await new HdUpscaler(new FakeEsrgan(FakeEsrgan.Mode.Faithful), "esrgan", "m", _work)
            .UpscaleAsync([Texture(16, cutout: true)], CancellationToken.None)).Single();
        Assert.All(outcome.Rgba.Where((_, i) => i % 4 == 3), a => Assert.True(a is 0 or 255));
    }
}
```

Run: `dotnet test builder/Builder.Core.Tests --filter "HdPrepTests|HdUpscalerTests"`
Expected: falla la compilación (`HdPrep`, `HdUpscaler` no existen).

- [ ] **Step 3: `HdPrep`**

```csharp
// builder/Builder.Core/Hd/HdPrep.cs
namespace Vcsnx.Builder.Core.Hd;

/// <summary>What the upscaler does to a texture before and after the AI (spec §4.2).</summary>
public static class HdPrep
{
    public static int ScaleFor(int width, int height) => Math.Max(width, height) switch { <= 64 => 4, <= 512 => 2, _ => 1 };

    public static (int X, int Y) MarginFor(int width, int height) => (Math.Min(8, width / 4), Math.Min(8, height / 4));

    /// <summary>Every texel with alpha 0 takes the RGB of the nearest visible texel (breadth-first, wrapping); its
    /// alpha stays 0. An image with nothing visible is returned unchanged.</summary>
    public static byte[] FillHidden(ReadOnlySpan<byte> rgba, int width, int height)
    {
        var output = rgba.ToArray();
        var queue = new Queue<int>();
        var done = new bool[width * height];
        for (var i = 0; i < width * height; i++)
            if (rgba[i * 4 + 3] != 0) { done[i] = true; queue.Enqueue(i); }
        while (queue.Count > 0)
        {
            var i = queue.Dequeue();
            int x = i % width, y = i / width;
            foreach (var (nx, ny) in new[] { (x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1) })
            {
                var n = ((ny + height) % height) * width + (nx + width) % width;
                if (done[n]) continue;
                done[n] = true;
                output.AsSpan(i * 4, 3).CopyTo(output.AsSpan(n * 4, 3));
                queue.Enqueue(n);
            }
        }
        return output;
    }

    public static bool BinaryAlpha(ReadOnlySpan<byte> rgba)
    {
        for (var i = 3; i < rgba.Length; i += 4)
            if (rgba[i] is not (0 or 255)) return false;
        return true;
    }
}
```

- [ ] **Step 4: `HdUpscaler`**

```csharp
// builder/Builder.Core/Hd/HdUpscaler.cs
namespace Vcsnx.Builder.Core.Hd;

public sealed record UpscaleOutcome(byte[] Rgba, int Width, int Height, bool UsedAi);

/// <summary>Upscales a batch (spec §4.2-4.4): hidden texels filled, a toroidal margin, Real-ESRGAN 4x on the opaque
/// RGB for the whole batch at once, the margin cut, 2x by box reduction, alpha by Lanczos-3 (cut back to 0/255 for
/// cutouts), then the fidelity check; whatever fails it, or everything when the AI does not run, uses Lanczos-3.</summary>
public sealed class HdUpscaler(IProcessRunner runner, string? esrgan, string model, string workDir)
{
    string? _esrgan = esrgan;
    public bool AiAvailable => _esrgan is not null;

    public static UpscaleOutcome Classic(HdTexture t)
    {
        var scale = HdPrep.ScaleFor(t.Width, t.Height);
        var up = Resample.Lanczos3Wrap(HdPrep.FillHidden(t.Rgba, t.Width, t.Height), t.Width, t.Height, scale);
        if (HdPrep.BinaryAlpha(t.Rgba)) CutAlpha(up);
        return new UpscaleOutcome(up, t.Width * scale, t.Height * scale, false);
    }

    public async Task<IReadOnlyList<UpscaleOutcome>> UpscaleAsync(IReadOnlyList<HdTexture> batch, CancellationToken ct)
    {
        if (_esrgan is null) return batch.Select(Classic).ToList();
        string input = Path.Combine(workDir, "in"), output = Path.Combine(workDir, "out");
        foreach (var dir in new[] { input, output })
        {
            if (Directory.Exists(dir)) Directory.Delete(dir, recursive: true);
            Directory.CreateDirectory(dir);
        }
        for (var i = 0; i < batch.Count; i++)
        {
            var t = batch[i];
            var (mx, my) = HdPrep.MarginFor(t.Width, t.Height);
            var opaque = HdPrep.FillHidden(t.Rgba, t.Width, t.Height);
            for (var a = 3; a < opaque.Length; a += 4) opaque[a] = 255;
            var padded = Resample.WrapPad(opaque, t.Width, t.Height, mx, my);
            await File.WriteAllBytesAsync(Path.Combine(input, $"{i:D5}.png"), PngCodec.EncodeRgba(padded, t.Width + 2 * mx, t.Height + 2 * my), ct);
        }
        // The models sit next to the program (both zips); -m makes that explicit whatever the working directory.
        var models = Path.Combine(Path.GetDirectoryName(Path.GetFullPath(_esrgan))!, "models");
        var result = await runner.RunAsync(_esrgan, ["-i", input, "-o", output, "-m", models, "-n", model, "-s", "4", "-f", "png"], ct);
        if (result.ExitCode != 0)
        {
            _esrgan = null; // no Vulkan GPU, or the program cannot run here: do not try again for every batch
            return batch.Select(Classic).ToList();
        }
        var outcomes = new List<UpscaleOutcome>(batch.Count);
        for (var i = 0; i < batch.Count; i++) outcomes.Add(FromAi(batch[i], Path.Combine(output, $"{i:D5}.png")));
        return outcomes;
    }

    static UpscaleOutcome FromAi(HdTexture t, string path)
    {
        var classic = Classic(t);
        if (!File.Exists(path)) return classic;
        int width, height;
        byte[] ai;
        try { (width, height, ai) = PngCodec.Decode(File.ReadAllBytes(path)); }
        catch (InvalidDataException) { return classic; }
        var (mx, my) = HdPrep.MarginFor(t.Width, t.Height);
        if (width != (t.Width + 2 * mx) * 4 || height != (t.Height + 2 * my) * 4) return classic;
        var rgb = Resample.Crop(ai, width, mx * 4, my * 4, t.Width * 4, t.Height * 4);
        var scale = HdPrep.ScaleFor(t.Width, t.Height);
        if (scale == 2) rgb = Resample.BoxDown(rgb, t.Width * 4, t.Height * 4, 2);
        for (var a = 3; a < rgb.Length; a += 4) rgb[a] = classic.Rgba[a]; // alpha from Lanczos-3 (already cut for cutouts)
        var back = Resample.BoxDown(rgb, t.Width * scale, t.Height * scale, scale);
        return Fidelity.Psnr(t.Rgba, back) >= Fidelity.MinPsnr ? new UpscaleOutcome(rgb, t.Width * scale, t.Height * scale, true) : classic;
    }

    static void CutAlpha(byte[] rgba)
    {
        for (var a = 3; a < rgba.Length; a += 4) rgba[a] = rgba[a] >= 128 ? (byte)255 : (byte)0;
    }
}
```

Run: `dotnet test builder/Builder.Core.Tests --filter "HdPrepTests|HdUpscalerTests"`
Expected: `Passed: 11`.

- [ ] **Step 5: Commit**

```bash
git add builder/Builder.Core/Hd builder/Builder.Core.Tests/Hd
git commit -m "feat(builder): upscale textures with Real-ESRGAN, keeping only faithful results"
```

- [ ] **Step 6: Elección del modelo (local, en el Mac)**

Se hace después de la tarea 7, que es la que agrega `--hd`. El Real-ESRGAN para macOS ya está verificado en
`.cache/realesrgan/`: el binario es idéntico al del zip oficial, `c1c35d92…`. Correr dos veces con 300 texturas:

```bash
dotnet run --project builder/Builder.Cli -c Release -- --game <SD copy>/switch/vcsnx/PSP_DATA \
  --hd 1 --hd-limit 300 --hd-model realesrgan-x4plus --esrgan .cache/realesrgan/realesrgan-ncnn-vulkan --out local/notes/hd/x4plus
dotnet run --project builder/Builder.Cli -c Release -- --game <SD copy>/switch/vcsnx/PSP_DATA \
  --hd 1 --hd-limit 300 --hd-model realesr-animevideov3 --esrgan .cache/realesrgan/realesrgan-ncnn-vulkan --out local/notes/hd/animev3
```

Se compara cuántas texturas aprobaron con IA en cada `texturas_hd.log` y se miran 10 de cada una en el Mac. Gana
la que más aprueba; si la diferencia es de menos de 5 %, gana la que el autor prefiera. El ganador queda en
`HdTextures.DefaultModel`, con un `Ruling:`. Las imágenes quedan solo en `local/`.

---

### Task 6: Bajar y verificar Real-ESRGAN, y licencias

**Files:**
- Create: `builder/Builder.Core/Hd/RealEsrgan.cs`
- Create: `builder/Builder.Core.Tests/Hd/RealEsrganTests.cs`
- Create: `builder/licenses/Real-ESRGAN-LICENSE.txt`, `builder/licenses/ncnn-LICENSE.txt`,
  `builder/licenses/Real-ESRGAN-ncnn-vulkan-LICENSE.txt`
- Modify: `builder/licenses/THIRD_PARTY.md`

**Interfaces:**
- Produces: `static Task<string?> RealEsrgan.EnsureAsync(string dir, HttpClient http, TextWriter log, CancellationToken ct, Uri? zip = null, string? sha256 = null)`.
  Devuelve la ruta del ejecutable, o `null` si no se pudo, con el motivo escrito en `log`.

- [ ] **Step 1: Tests que fallan**

```csharp
// builder/Builder.Core.Tests/Hd/RealEsrganTests.cs
using System.IO.Compression;
using System.Net;
using System.Security.Cryptography;
using Vcsnx.Builder.Core.Hd;

namespace Vcsnx.Builder.Core.Tests.Hd;

public sealed class RealEsrganTests : IDisposable
{
    readonly string _dir = Path.Combine(Path.GetTempPath(), "vcsnx-esr-" + Guid.NewGuid().ToString("N"));
    public void Dispose() { if (Directory.Exists(_dir)) Directory.Delete(_dir, recursive: true); }

    sealed class Serve(byte[] body) : HttpMessageHandler
    {
        public int Requests { get; private set; }
        protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken ct)
        {
            Requests++;
            return Task.FromResult(new HttpResponseMessage(HttpStatusCode.OK) { Content = new ByteArrayContent(body) });
        }
    }

    static byte[] Zip()
    {
        using var memory = new MemoryStream();
        using (var zip = new ZipArchive(memory, ZipArchiveMode.Create, leaveOpen: true))
        {
            using (var w = new StreamWriter(zip.CreateEntry(RealEsrgan.ExecutableName).Open())) w.Write("exe");
            using (var w = new StreamWriter(zip.CreateEntry("models/m.param").Open())) w.Write("model");
        }
        return memory.ToArray();
    }

    [Fact]
    public async Task AVerifiedDownloadIsUnpackedOnceAndReused()
    {
        var zip = Zip();
        var serve = new Serve(zip);
        var sha = Convert.ToHexStringLower(SHA256.HashData(zip));
        var uri = new Uri("https://example.invalid/r.zip");
        var exe = await RealEsrgan.EnsureAsync(_dir, new HttpClient(serve), TextWriter.Null, CancellationToken.None, uri, sha);
        Assert.Equal(Path.Combine(_dir, RealEsrgan.ExecutableName), exe);
        Assert.True(File.Exists(Path.Combine(_dir, "models", "m.param")));
        Assert.Equal(exe, await RealEsrgan.EnsureAsync(_dir, new HttpClient(serve), TextWriter.Null, CancellationToken.None, uri, sha));
        Assert.Equal(1, serve.Requests);
    }

    [Fact]
    public async Task AZipWithTheWrongHashIsNotUsed()
    {
        var log = new StringWriter();
        var exe = await RealEsrgan.EnsureAsync(_dir, new HttpClient(new Serve(Zip())), log, CancellationToken.None,
            new Uri("https://example.invalid/r.zip"), new string('0', 64));
        Assert.Null(exe);
        Assert.Contains("SHA-256", log.ToString());
        Assert.False(File.Exists(Path.Combine(_dir, RealEsrgan.ExecutableName)));
    }
}
```

Run: `dotnet test builder/Builder.Core.Tests --filter RealEsrganTests`
Expected: falla la compilación (`RealEsrgan` no existe).

- [ ] **Step 2: `RealEsrgan`**

```csharp
// builder/Builder.Core/Hd/RealEsrgan.cs
using System.IO.Compression;
using System.Runtime.InteropServices;

namespace Vcsnx.Builder.Core.Hd;

/// <summary>Real-ESRGAN ncnn-vulkan v0.2.5.0 (BSD-3; ncnn BSD-3), downloaded on first use and checked by SHA-256
/// like the translated code (spec §4.4); never inside the Builder's zip.</summary>
public static class RealEsrgan
{
    static readonly Uri WindowsZip = new("https://github.com/xinntao/Real-ESRGAN/releases/download/v0.2.5.0/realesrgan-ncnn-vulkan-20220424-windows.zip");
    const string WindowsSha256 = "abc02804e17982a3be33675e4d471e91ea374e65b70167abc09e31acb412802d";
    static readonly Uri MacZip = new("https://github.com/xinntao/Real-ESRGAN/releases/download/v0.2.5.0/realesrgan-ncnn-vulkan-20220424-macos.zip");
    const string MacSha256 = "e0ad05580abfeb25f8d8fb55aaf7bedf552c375b5b4d9bd3c8d59764d2cc333a";

    public static string ExecutableName => OperatingSystem.IsWindows() ? "realesrgan-ncnn-vulkan.exe" : "realesrgan-ncnn-vulkan";

    public static async Task<string?> EnsureAsync(string dir, HttpClient http, TextWriter log, CancellationToken ct, Uri? zip = null, string? sha256 = null)
    {
        zip ??= OperatingSystem.IsWindows() ? WindowsZip : MacZip;
        sha256 ??= OperatingSystem.IsWindows() ? WindowsSha256 : MacSha256;
        var exe = Path.Combine(dir, ExecutableName);
        var stamp = Path.Combine(dir, "verified.sha256");
        if (File.Exists(exe) && File.Exists(stamp) && File.ReadAllText(stamp).Trim() == sha256) return exe;
        Directory.CreateDirectory(dir);
        var download = Path.Combine(dir, "download.zip");
        try
        {
            await Corpus.DownloadAsync(http, zip, download, ct);
            if (HashList.Sha256Of(download) != sha256)
            {
                log.WriteLine($"Real-ESRGAN: SHA-256 of {zip} does not match; HD textures use Lanczos-3 only");
                return null;
            }
            ZipFile.ExtractToDirectory(download, dir, overwriteFiles: true);
            if (!OperatingSystem.IsWindows()) File.SetUnixFileMode(exe, File.GetUnixFileMode(exe) | UnixFileMode.UserExecute);
            await File.WriteAllTextAsync(stamp, sha256, ct);
            return exe;
        }
        catch (Exception e) when (e is HttpRequestException or IOException or InvalidDataException or UnauthorizedAccessException)
        {
            log.WriteLine($"Real-ESRGAN: {e.Message}; HD textures use Lanczos-3 only");
            return null;
        }
        finally
        {
            if (File.Exists(download)) File.Delete(download);
        }
    }
}
```

Run: `dotnet test builder/Builder.Core.Tests --filter RealEsrganTests`
Expected: `Passed: 2`.

- [ ] **Step 3: Licencias**

```bash
curl -sL https://raw.githubusercontent.com/xinntao/Real-ESRGAN/master/LICENSE -o builder/licenses/Real-ESRGAN-LICENSE.txt
curl -sL https://raw.githubusercontent.com/Tencent/ncnn/master/LICENSE.txt -o builder/licenses/ncnn-LICENSE.txt
curl -sL https://raw.githubusercontent.com/xinntao/Real-ESRGAN-ncnn-vulkan/master/LICENSE -o builder/licenses/Real-ESRGAN-ncnn-vulkan-LICENSE.txt
head -3 builder/licenses/Real-ESRGAN-LICENSE.txt builder/licenses/ncnn-LICENSE.txt builder/licenses/Real-ESRGAN-ncnn-vulkan-LICENSE.txt
```

Expected: tres textos de licencia (BSD 3-Clause, BSD 3-Clause y MIT); si alguno es otro, se anota un `Ruling:`.
Se agrega una fila a `builder/licenses/THIRD_PARTY.md` con el mismo formato que las demás. Debe decir que el
Builder baja Real-ESRGAN ncnn-vulkan v0.2.5.0 (con modelos y `vcomp140.dll`) para las texturas HD, que no va en
el zip, y nombrar los tres archivos de licencia.

- [ ] **Step 4: Commit**

```bash
git add builder/Builder.Core/Hd/RealEsrgan.cs builder/Builder.Core.Tests/Hd/RealEsrganTests.cs builder/licenses
git commit -m "feat(builder): download Real-ESRGAN on first use, verified by SHA-256"
```

---

### Task 7: El paso completo, la consola del Builder y la ventana

**Files:**
- Create: `builder/Builder.Core/Hd/HdTextures.cs`
- Create: `builder/Builder.Core.Tests/Hd/HdTexturesTests.cs`
- Modify: `builder/Builder.Core/Strings.cs` (claves `CreateHd`, `StageHd`, `DoneHd`, `HdNoAi`)
- Modify: `builder/Builder.Cli/Program.cs` (`--hd`, `--hd-limit`, `--hd-model`, `--esrgan`)
- Modify: `builder/Builder.App/MainForm.cs` (botón y progreso)

**Interfaces:**
- Consumes: `HdCatalog`, `HdUpscaler`, `RealEsrgan.EnsureAsync`, `PngCodec`, `TextureKey.FileName`, `Eta.Remaining`,
  `BuildPaths`.
- Produces:
  - `sealed record HdRequest(string GameFolder, string? SdRoot, int Limit = int.MaxValue, string Model = HdTextures.DefaultModel, int BatchSize = 64)`
  - `sealed record HdProgress(int Done, int Total, TimeSpan? Remaining)`
  - `sealed record HdReport(int Written, int Ai, int Classic, int Existing, int Skipped2D, int SkippedFormat, int SkippedSmall, int Duplicates, bool AiAvailable, string OutputDir)`
  - `sealed class HdTextures(BuildPaths paths, IProcessRunner runner, Func<TextWriter, CancellationToken, Task<string?>> findEsrgan)`
    con `Task<HdReport> RunAsync(HdRequest, IProgress<HdProgress>, TextWriter log, CancellationToken)` y
    `const string DefaultModel = "realesrgan-x4plus"`.

- [ ] **Step 1: Tests que fallan**

```csharp
// builder/Builder.Core.Tests/Hd/HdTexturesTests.cs
using Vcsnx.Builder.Core.Hd;

namespace Vcsnx.Builder.Core.Tests.Hd;

public sealed class HdTexturesTests : IDisposable
{
    readonly string _root = Path.Combine(Path.GetTempPath(), "vcsnx-hdt-" + Guid.NewGuid().ToString("N"));
    string Game => Path.Combine(_root, "PSP_DATA");
    string Sd => Path.Combine(_root, "sd");
    string Textures => Path.Combine(Sd, "switch", "vcsnx", "Textures");
    BuildPaths Paths => new(Path.Combine(_root, "tc"), Path.Combine(_root, "kit"), Path.Combine(_root, "h"), Path.Combine(_root, "work"), Path.Combine(_root, "out"));

    public HdTexturesTests()
    {
        var runData = Path.Combine(Game, "PSP_GAME", "USRDIR", "RUNDATA");
        Directory.CreateDirectory(runData);
        var textures = Enumerable.Range(0, 5).Select(i =>
            TexChunkBuilder.Indexed($"car_{i}", 16, 16, 4, 1, (x, y) => (x * (i + 1) + y) & 15, TexChunkBuilder.Palette(16))).ToArray();
        File.WriteAllBytes(Path.Combine(runData, "GTA3PSP.IMG"), TexChunkBuilder.Build([.. textures, TexChunkBuilder.Indexed("hud_x", 16, 16, 4, 1, (_, _) => 3, TexChunkBuilder.Palette(16))]));
    }

    public void Dispose() => Directory.Delete(_root, recursive: true);

    HdTextures Step(IProcessRunner runner) => new(Paths, runner, (_, _) => Task.FromResult<string?>("esrgan"));

    [Fact]
    public async Task EveryModelTextureLandsOnTheSdAsItsKey()
    {
        var report = await Step(new FakeEsrgan(FakeEsrgan.Mode.Faithful)).RunAsync(new HdRequest(Game, Sd), new Progress<HdProgress>(), TextWriter.Null, CancellationToken.None);
        Assert.Equal((5, 5, 0, 1), (report.Written, report.Ai, report.Classic, report.Skipped2D));
        var scan = HdCatalog.Scan(HdCatalog.SourcesIn(Game));
        foreach (var t in scan.Textures)
        {
            var (w, h, _) = PngCodec.Decode(File.ReadAllBytes(Path.Combine(Textures, TextureKey.FileName(t.Key))));
            Assert.Equal((64, 64), (w, h));
        }
        Assert.True(File.Exists(Path.Combine(Paths.Output, "texturas_hd.log")));
    }

    [Fact]
    public async Task HandMadeReplacementsAreNeverOverwritten()
    {
        var first = HdCatalog.Scan(HdCatalog.SourcesIn(Game)).Textures[0];
        Directory.CreateDirectory(Textures);
        var mine = Path.Combine(Textures, TextureKey.FileName(first.Key));
        File.WriteAllText(mine, "hand made");
        var report = await Step(new FakeEsrgan(FakeEsrgan.Mode.Faithful)).RunAsync(new HdRequest(Game, Sd), new Progress<HdProgress>(), TextWriter.Null, CancellationToken.None);
        Assert.Equal("hand made", File.ReadAllText(mine));
        Assert.Equal((4, 1), (report.Written, report.Existing));
    }

    [Fact]
    public async Task CancellingLeavesOnlyCompleteFiles()
    {
        using var cancel = new CancellationTokenSource();
        var progress = new SyncProgress<HdProgress>(p => { if (p.Done >= 2) cancel.Cancel(); });
        await Assert.ThrowsAnyAsync<OperationCanceledException>(() =>
            Step(new FakeEsrgan(FakeEsrgan.Mode.Faithful)).RunAsync(new HdRequest(Game, Sd, BatchSize: 2), progress, TextWriter.Null, cancel.Token));
        var files = Directory.GetFiles(Textures);
        Assert.Equal(2, files.Length);
        Assert.All(files, f => Assert.EndsWith(".png", f));
        Assert.All(files, f => PngCodec.Decode(File.ReadAllBytes(f)));
    }

    [Fact]
    public async Task WithoutTheAiTheReportSaysSo()
    {
        var report = await new HdTextures(Paths, new FakeEsrgan(FakeEsrgan.Mode.Failing), (_, _) => Task.FromResult<string?>(null))
            .RunAsync(new HdRequest(Game, null), new Progress<HdProgress>(), TextWriter.Null, CancellationToken.None);
        Assert.False(report.AiAvailable);
        Assert.Equal((5, 0, 5), (report.Written, report.Ai, report.Classic));
        Assert.Equal(Path.Combine(Paths.Output, "Textures"), report.OutputDir);
    }
}
```

Run: `dotnet test builder/Builder.Core.Tests --filter HdTexturesTests`
Expected: falla la compilación (`HdTextures` no existe).

- [ ] **Step 2: `HdTextures`**

```csharp
// builder/Builder.Core/Hd/HdTextures.cs
using System.Diagnostics;

namespace Vcsnx.Builder.Core.Hd;

public sealed record HdRequest(string GameFolder, string? SdRoot, int Limit = int.MaxValue, string Model = HdTextures.DefaultModel, int BatchSize = 64);
public sealed record HdProgress(int Done, int Total, TimeSpan? Remaining);
public sealed record HdReport(int Written, int Ai, int Classic, int Existing, int Skipped2D, int SkippedFormat, int SkippedSmall,
    int Duplicates, bool AiAvailable, string OutputDir);

/// <summary>The "Create HD textures" step (spec §4): scan, upscale in batches, write each texture as
/// Textures/&lt;key&gt;.png (via a .tmp, so a cancelled run leaves only whole files), never over an existing file,
/// and a summary in salida\texturas_hd.log.</summary>
public sealed class HdTextures(BuildPaths paths, IProcessRunner runner, Func<TextWriter, CancellationToken, Task<string?>> findEsrgan)
{
    public const string DefaultModel = "realesrgan-x4plus";

    public async Task<HdReport> RunAsync(HdRequest request, IProgress<HdProgress> progress, TextWriter log, CancellationToken ct)
    {
        var outputDir = request.SdRoot is { } sd ? Path.Combine(sd, "switch", "vcsnx", "Textures") : Path.Combine(paths.Output, "Textures");
        Directory.CreateDirectory(outputDir);
        var scan = HdCatalog.Scan(HdCatalog.SourcesIn(request.GameFolder));
        var candidates = scan.Textures.Where(t => HdPrep.ScaleFor(t.Width, t.Height) > 1).Take(request.Limit).ToList();
        var todo = candidates.Where(t => !File.Exists(Path.Combine(outputDir, TextureKey.FileName(t.Key)))).ToList();
        var existing = candidates.Count - todo.Count;
        var upscaler = new HdUpscaler(runner, await findEsrgan(log, ct), request.Model, Path.Combine(paths.Work, "hd"));
        var clock = Stopwatch.StartNew();
        int done = 0, ai = 0, classic = 0;
        progress.Report(new HdProgress(0, todo.Count, null));
        foreach (var batch in todo.Chunk(request.BatchSize))
        {
            ct.ThrowIfCancellationRequested();
            var outcomes = await upscaler.UpscaleAsync(batch, ct);
            for (var i = 0; i < batch.Length; i++)
            {
                var final = Path.Combine(outputDir, TextureKey.FileName(batch[i].Key));
                var temporary = final + ".tmp";
                await File.WriteAllBytesAsync(temporary, PngCodec.EncodeRgba(outcomes[i].Rgba, outcomes[i].Width, outcomes[i].Height), CancellationToken.None);
                File.Move(temporary, final, overwrite: false);
                if (outcomes[i].UsedAi) ai++; else classic++;
            }
            done += batch.Length;
            progress.Report(new HdProgress(done, todo.Count, Eta.Remaining(done, todo.Count, 0, clock.Elapsed)));
        }
        var report = new HdReport(done, ai, classic, existing, scan.Skipped2D, scan.SkippedFormat, scan.SkippedSmall, scan.Duplicates,
            upscaler.AiAvailable, outputDir);
        Directory.CreateDirectory(paths.Output);
        await File.WriteAllTextAsync(Path.Combine(paths.Output, "texturas_hd.log"),
            $"read {scan.Textures.Count} written {report.Written} ai {ai} lanczos {classic} existing {existing} " +
            $"skipped 2d {scan.Skipped2D} format {scan.SkippedFormat} small {scan.SkippedSmall} duplicates {scan.Duplicates} " +
            $"ai_available {report.AiAvailable} model {request.Model} seconds {clock.Elapsed.TotalSeconds:F0}{Environment.NewLine}", CancellationToken.None);
        log.WriteLine($"HD textures: {report}");
        return report;
    }
}
```

Run: `dotnet test builder/Builder.Core.Tests`
Expected: toda la suite en verde.

- [ ] **Step 3: Textos**

En `builder/Builder.Core/Strings.cs`, antes de `["StageDone"]`:

```csharp
        ["CreateHd"] = ("Crear texturas HD", "Create HD textures"),
        ["StageHd"] = ("Texturas HD {0}/{1}", "HD textures {0}/{1}"),
        ["DoneHd"] = ("Listo: {0} texturas HD en {1} ({2} con IA, {3} con el método clásico).",
                      "Done: {0} HD textures in {1} ({2} with AI, {3} with the classic method)."),
        ["HdNoAi"] = ("La IA no pudo usar tu tarjeta gráfica; todas las texturas usaron el método clásico.",
                      "The AI could not use your graphics card; every texture used the classic method."),
```

- [ ] **Step 4: `Builder.Cli`**

En `builder/Builder.Cli/Program.cs`, después del bloque `--hd-list` de la tarea 3:

```csharp
if (options.ContainsKey("hd"))
{
    var hdDefaults = BuildPaths.NextTo(options.GetValueOrDefault("base", AppContext.BaseDirectory));
    var hdPaths = hdDefaults with { Work = options.GetValueOrDefault("work", hdDefaults.Work), Output = options.GetValueOrDefault("out", hdDefaults.Output) };
    using var hdCancel = new CancellationTokenSource();
    Console.CancelKeyPress += (_, e) => { e.Cancel = true; hdCancel.Cancel(); };
    var http = new HttpClient();
    var step = new Vcsnx.Builder.Core.Hd.HdTextures(hdPaths, new SystemProcessRunner(),
        (log, ct) => options.TryGetValue("esrgan", out var exe) ? Task.FromResult<string?>(exe)
            : Vcsnx.Builder.Core.Hd.RealEsrgan.EnsureAsync(Path.Combine(hdPaths.Work, "realesrgan"), http, log, ct));
    var hdLast = "";
    var hdProgress = new Progress<Vcsnx.Builder.Core.Hd.HdProgress>(p =>
    {
        var line = Strings.Get("StageHd", p.Done, p.Total);
        if (line != hdLast) Console.WriteLine(hdLast = line);
    });
    var request = new Vcsnx.Builder.Core.Hd.HdRequest(gameFolder, options.GetValueOrDefault("sd"),
        int.Parse(options.GetValueOrDefault("hd-limit", int.MaxValue.ToString())),
        options.GetValueOrDefault("hd-model", Vcsnx.Builder.Core.Hd.HdTextures.DefaultModel));
    var report = await step.RunAsync(request, hdProgress, Console.Out, hdCancel.Token);
    if (!report.AiAvailable) Console.WriteLine(Strings.Get("HdNoAi"));
    Console.WriteLine(Strings.Get("DoneHd", report.Written, report.OutputDir, report.Ai, report.Classic));
    return 0;
}
```

Agregar al comentario de uso: `--hd 1 [--hd-limit N] [--hd-model name] [--esrgan <exe>]`.

Run: `dotnet build builder/Builder.Cli -c Release`
Expected: `Build succeeded` sin warnings nuevos.

- [ ] **Step 5: La ventana**

En `builder/Builder.App/MainForm.cs`:
- Campo nuevo debajo de `_create`:
  `readonly Button _createHd = new() { AutoSize = true, Text = Strings.Get("CreateHd"), Enabled = false };`
- En el layout, cambiar `Row(_create)` por `Row(_create, _createHd)`.
- Debajo de `_create.Click += …`, agregar `_createHd.Click += async (_, _) => await CreateHdAsync();`.
- En `CheckGameAsync`, cada `_create.Enabled = …` pasa a fijar los dos: `_create.Enabled = _createHd.Enabled = …;`.
- Método nuevo:

```csharp
    async Task CreateHdAsync()
    {
        _run = new CancellationTokenSource();
        _create.Enabled = _createHd.Enabled = false;
        _cancel.Enabled = true;
        _log.Clear();
        Directory.CreateDirectory(_paths.Output);
        await using var logFile = new StreamWriter(Path.Combine(_paths.Output, "builder.log"), append: false) { AutoFlush = true };
        var log = TextWriter.Synchronized(new Tee(logFile, line => BeginInvoke(new Action(() => _log.AppendText(line + Environment.NewLine)))));
        var http = new HttpClient();
        var step = new Vcsnx.Builder.Core.Hd.HdTextures(_paths, new LoggingRunner(new SystemProcessRunner(Path.Combine(Path.GetFullPath(_paths.Work), "tmp")), log),
            (l, ct) => Vcsnx.Builder.Core.Hd.RealEsrgan.EnsureAsync(Path.Combine(_paths.Work, "realesrgan"), http, l, ct));
        var progress = new Progress<Vcsnx.Builder.Core.Hd.HdProgress>(p =>
        {
            _stage.Text = Strings.Get("StageHd", p.Done, p.Total);
            _bar.Value = p.Total == 0 ? 1000 : 1000 * p.Done / p.Total;
            _remaining.Text = p.Remaining is { } r ? Strings.Get("Remaining", Math.Max(1, (int)Math.Ceiling(r.TotalMinutes))) : "";
        });
        try
        {
            var report = await step.RunAsync(new Vcsnx.Builder.Core.Hd.HdRequest(_game.Text, (_sd.SelectedItem as SdChoice)?.Root), progress, log, _run.Token);
            var text = Strings.Get("DoneHd", report.Written, report.OutputDir, report.Ai, report.Classic);
            MessageBox.Show(this, report.AiAvailable ? text : Strings.Get("HdNoAi") + Environment.NewLine + text, Title);
        }
        catch (OperationCanceledException) when (_run.IsCancellationRequested) { _stage.Text = Strings.Get("Cancelled"); }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException)
        {
            log.WriteLine(e.ToString());
            MessageBox.Show(this, e.Message, Title, MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
        catch (Exception e)
        {
            log.WriteLine(e.ToString());
            MessageBox.Show(this, Strings.Get("Unexpected", e.Message), Title, MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
        finally
        {
            _cancel.Enabled = false;
            _create.Enabled = _createHd.Enabled = true;
            _run.Dispose();
            _run = null;
        }
    }
```

Run: `dotnet build builder/Builder.App -c Release`
Expected: `Build succeeded` (`EnableWindowsTargeting` ya está en el `.csproj`).

- [ ] **Step 6: Prueba real en el Mac (local)**

```bash
dotnet run --project builder/Builder.Cli -c Release -- --game <SD copy>/switch/vcsnx/PSP_DATA \
  --hd 1 --esrgan .cache/realesrgan/realesrgan-ncnn-vulkan --out local/notes/hd/full
```

Expected:
- `Listo: N texturas HD en …/local/notes/hd/full/Textures (A con IA, C con el método clásico).`
- `texturas_hd.log` con el tiempo total, que se anota en el ledger para las notas del release.
- Se abren 10 PNG al azar para revisarlos a ojo. Todo queda en `local/`.

Después se hace el paso 6 de la tarea 5 (elegir el modelo).

- [ ] **Step 7: Commit**

```bash
git add builder/Builder.Core/Hd/HdTextures.cs builder/Builder.Core.Tests/Hd/HdTexturesTests.cs builder/Builder.Core/Strings.cs builder/Builder.Cli/Program.cs builder/Builder.App/MainForm.cs
git commit -m "feat(builder): the Create HD textures step, in the window and the command line"
```

---

### Task 8: Sesión en la consola con las texturas HD

**Files:**
- Modify: `docs/backlog.md` (punto 18: estado de la parte 1)
- Modify (solo si la medición lo pide): `vendor/vcs-recomp/profiles/vcs/host/vcs_texture_replace.cpp` y `UPSTREAM.md`

- [ ] **Step 1: Copiar las texturas a la SD (con "ftp listo")**

Se suben los PNG de `local/notes/hd/full/Textures/` a `sdmc:/switch/vcsnx/Textures/` sin pisar los que ya están,
en una sola conexión, y se verifica una muestra de 20 por sha256. `DumpOriginals=false` y `Replacement=true` en el
`VCSNative.ini` de la SD.

- [ ] **Step 2: Sesión del autor**

El autor juega unos 5 minutos: personajes de cerca, varios carros, un arma, una pantalla de carga.

Después se bajan los logs a `local/console-logs/<fecha>-hd-p1/` y se revisa:
- Memoria: `[alloc-diag]` y `[vulkan] … texture replacement budget`. ¿Se llegó al presupuesto?
- Tirones: `[ge-slow]` y `ge_ms_max` en perf.log, comparados con `2026-10-13-perf-p15`.
- La opinión del autor y sus fotos.

- [ ] **Step 3: Corrección solo si hace falta (TDD)**

Si el hilo de reemplazos le quita tiempo al principal, se le baja la prioridad o se mueve de núcleo; eso se ve en
`present_ms_max` o `[vcsnx-watchdog]` peores que en p15. El cambio va en `texture_pipeline_submit`, al crear
`p.worker`. Va con su test en `tests/platform/`, una fila en `UPSTREAM.md` y otra sesión. Si no hace falta, se
anota `Ruling: sin cambios en la consola`.

- [ ] **Step 4: Backlog y commit**

En `docs/backlog.md`, al punto 18 se le agrega cuántas texturas cubre la parte 1, la memoria medida y lo que
falta (la ciudad y la parte 3).

```bash
git add docs/backlog.md
git commit -m "docs: backlog 18, HD textures part 1 measured on the console"
```

---

### Task 9: Builder 0.2.0 (empaquetado; publicar solo con confirmación)

**Files:**
- Modify: `builder/RELEASE_NOTES.md`, `builder/README.txt` (el botón nuevo, en español e inglés)
- Modify: `tools/package_builder.py` (solo si hace falta para la versión 0.2.0 del zip)

- [ ] **Step 1:** Las notas y el README explican:
  - el botón "Crear texturas HD";
  - que necesita una tarjeta gráfica con Vulkan (si no, usa el método clásico);
  - el tiempo medido en la tarea 7;
  - que las texturas se crean con la copia del jugador y no se comparten;
  - que no hace falta volver a crear `vcs_game.so`.
- [ ] **Step 2:** `python3 -I tools/package_builder.py` arma `dist/VCSNX-Builder-0.2.0/` y su zip. Se comprueba que el
  zip no trae Real-ESRGAN, ni nombres o códigos de texturas, ni archivos del juego. La lista exacta de lo que se
  revisa va en el ledger.
- [ ] **Step 3:** Commit de notas y README. **Publicar el zip es una decisión del autor:** se le pregunta antes.

```bash
git add builder/RELEASE_NOTES.md builder/README.txt tools/package_builder.py
git commit -m "docs(builder): Builder 0.2.0, the Create HD textures button"
```
