namespace Vcsnx.HdPack;

/// <summary>The map of the pause menu and the radar (author, 2026-10-13): 64 textures radar00..radar63, 128x128 each,
/// the pieces of one 1024x1024 map in 8x8 row-major order. Upscaled one by one they would show seams, so the HD
/// step assembles the map, upscales it as one image and cuts it back into the 64 tiles, each still named by the
/// key the console gives that tile.</summary>
public static class HdMap
{
    public const int Tiles = 8;
    public const int TileSize = 128;

    public sealed record Image(int Width, int Height, byte[] Rgba);

    public static int? TileIndex(string name)
    {
        if (name.Length != 7 || !name.StartsWith("radar", StringComparison.OrdinalIgnoreCase)) return null;
        if (!char.IsAsciiDigit(name[5]) || !char.IsAsciiDigit(name[6])) return null;
        var index = (name[5] - '0') * 10 + (name[6] - '0');
        return index < Tiles * Tiles ? index : null;
    }

    /// <summary>The whole map, or null unless all 64 tiles are there at 128x128.</summary>
    public static Image? Assemble(IReadOnlyList<HdTexture> tiles)
    {
        var byIndex = new HdTexture?[Tiles * Tiles];
        foreach (var t in tiles)
            if (TileIndex(t.Name) is { } i && t.Width == TileSize && t.Height == TileSize) byIndex[i] = t;
        if (byIndex.Any(t => t is null)) return null;
        const int side = Tiles * TileSize;
        var rgba = new byte[side * side * 4];
        for (var i = 0; i < byIndex.Length; i++)
        {
            int x0 = i % Tiles * TileSize, y0 = i / Tiles * TileSize;
            for (var y = 0; y < TileSize; y++)
                byIndex[i]!.Rgba.AsSpan(y * TileSize * 4, TileSize * 4).CopyTo(rgba.AsSpan(((y0 + y) * side + x0) * 4));
        }
        return new Image(side, side, rgba);
    }

    /// <summary>The 64 tiles of an upscaled map, in index order.</summary>
    public static IReadOnlyList<byte[]> Cut(byte[] rgba, int width, int height)
    {
        int tileWidth = width / Tiles, tileHeight = height / Tiles;
        var tiles = new List<byte[]>(Tiles * Tiles);
        for (var i = 0; i < Tiles * Tiles; i++)
            tiles.Add(Resample.Crop(rgba, width, i % Tiles * tileWidth, i / Tiles * tileHeight, tileWidth, tileHeight));
        return tiles;
    }
}
