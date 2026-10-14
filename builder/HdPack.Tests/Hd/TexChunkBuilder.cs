using System.Buffers.Binary;
using System.Text;
using Vcsnx.HdPack;

namespace Vcsnx.HdPack.Tests;

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
