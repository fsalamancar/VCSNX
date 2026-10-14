using System.Buffers.Binary;

namespace Vcsnx.HdPack;

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
