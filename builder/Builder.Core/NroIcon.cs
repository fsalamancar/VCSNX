namespace Vcsnx.Builder.Core;

/// <summary>Puts the player's icon into vcsnx.nro. An NRO is the program (its header at 0x10 says "NRO0", and the
/// word at 0x18 is the program's size) followed by its asset section: "ASET", a version, and the icon, NACP and
/// RomFS as (offset, size) pairs relative to the section. Only the asset section is rewritten.</summary>
public static class NroIcon
{
    const int AsetHeader = 0x38;

    public static void Replace(string nro, string jpg, string output)
    {
        var data = File.ReadAllBytes(nro);
        var program = ProgramSize(data, nro);
        var nacp = Asset(data, program, 1);
        var romfs = Asset(data, program, 2);
        var icon = File.ReadAllBytes(jpg);
        var header = new byte[AsetHeader];
        "ASET"u8.CopyTo(header);
        var offset = (ulong)AsetHeader;
        void Put(int index, int length)
        {
            BitConverter.TryWriteBytes(header.AsSpan(8 + index * 16), offset);
            BitConverter.TryWriteBytes(header.AsSpan(16 + index * 16), (ulong)length);
            offset += (ulong)length;
        }
        Put(0, icon.Length);
        Put(1, nacp.Length);
        Put(2, romfs.Length);
        using var file = File.Create(output);
        file.Write(data, 0, program);
        file.Write(header);
        file.Write(icon);
        file.Write(nacp);
        file.Write(romfs);
    }

    public static byte[] ReadIcon(string nro)
    {
        var data = File.ReadAllBytes(nro);
        return Asset(data, ProgramSize(data, nro), 0);
    }

    static int ProgramSize(byte[] data, string name)
    {
        if (data.Length < 0x80 || !data.AsSpan(0x10, 4).SequenceEqual("NRO0"u8)) throw new InvalidDataException($"{name} is not an NRO");
        var size = BitConverter.ToUInt32(data, 0x18);
        if ((long)size + AsetHeader > data.Length || !data.AsSpan((int)size, 4).SequenceEqual("ASET"u8))
            throw new InvalidDataException($"{name} has no asset section");
        return (int)size;
    }

    static byte[] Asset(byte[] data, int program, int index)
    {
        var offset = BitConverter.ToUInt64(data, program + 8 + index * 16);
        var size = BitConverter.ToUInt64(data, program + 16 + index * 16);
        if (size == 0) return [];
        if ((ulong)program + offset + size > (ulong)data.Length) throw new InvalidDataException("asset outside the NRO");
        return data.AsSpan(program + (int)offset, (int)size).ToArray();
    }
}
