using System.Buffers.Binary;
using System.Text;

namespace Vcsnx.HdPack;

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
