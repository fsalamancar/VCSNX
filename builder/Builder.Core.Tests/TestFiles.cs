using System.Text;

namespace Vcsnx.Builder.Core.Tests;

internal static class TestFiles
{
    public static string TempDir()
    {
        var dir = Path.Combine(Path.GetTempPath(), "vcsnx-builder-tests", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(dir);
        return dir;
    }

    public static string Write(string path, byte[] data)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllBytes(path, data);
        return path;
    }

    public static byte[] Elf(byte[] tail) => [0x7F, (byte)'E', (byte)'L', (byte)'F', .. tail];

    // A PSP PARAM.SFO with UTF-8 string entries.
    public static byte[] Sfo(params (string Key, string Value)[] entries)
    {
        var keys = new MemoryStream();
        var values = new MemoryStream();
        var index = new MemoryStream();
        foreach (var (key, value) in entries)
        {
            var bytes = Encoding.UTF8.GetBytes(value + "\0");
            var length = (bytes.Length + 3) / 4 * 4;
            index.Write(BitConverter.GetBytes((ushort)keys.Length));
            index.Write(BitConverter.GetBytes((ushort)0x0204));
            index.Write(BitConverter.GetBytes(bytes.Length));
            index.Write(BitConverter.GetBytes(length));
            index.Write(BitConverter.GetBytes((int)values.Length));
            keys.Write(Encoding.ASCII.GetBytes(key + "\0"));
            values.Write(bytes);
            values.Write(new byte[length - bytes.Length]);
        }
        while (keys.Length % 4 != 0) keys.WriteByte(0);
        var keyTable = 20 + (int)index.Length;
        var sfo = new MemoryStream();
        sfo.Write([0, (byte)'P', (byte)'S', (byte)'F', 1, 1, 0, 0]);
        sfo.Write(BitConverter.GetBytes(keyTable));
        sfo.Write(BitConverter.GetBytes(keyTable + (int)keys.Length));
        sfo.Write(BitConverter.GetBytes(entries.Length));
        index.WriteTo(sfo);
        keys.WriteTo(sfo);
        values.WriteTo(sfo);
        return sfo.ToArray();
    }

    // A minimal NRO: 0x100 bytes of program (header with "NRO0" and its size) then an ASET with icon and NACP.
    public static byte[] Nro(byte[] icon, byte[] nacp)
    {
        var program = new byte[0x100];
        "NRO0"u8.CopyTo(program.AsSpan(0x10));
        BitConverter.TryWriteBytes(program.AsSpan(0x18), (uint)program.Length);
        for (var i = 0x80; i < program.Length; ++i) program[i] = (byte)i;
        var header = new byte[0x38];
        "ASET"u8.CopyTo(header);
        BitConverter.TryWriteBytes(header.AsSpan(0x08), 0x38UL);
        BitConverter.TryWriteBytes(header.AsSpan(0x10), (ulong)icon.Length);
        BitConverter.TryWriteBytes(header.AsSpan(0x18), (ulong)(0x38 + icon.Length));
        BitConverter.TryWriteBytes(header.AsSpan(0x20), (ulong)nacp.Length);
        BitConverter.TryWriteBytes(header.AsSpan(0x28), (ulong)(0x38 + icon.Length + nacp.Length));
        return [.. program, .. header, .. icon, .. nacp];
    }
}
