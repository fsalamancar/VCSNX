using System.Buffers.Binary;
using System.IO.Compression;

namespace Vcsnx.HdPack;

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
