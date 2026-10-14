namespace Vcsnx.HdPack;

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
