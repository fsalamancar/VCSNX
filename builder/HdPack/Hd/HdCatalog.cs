namespace Vcsnx.HdPack;

public sealed record HdSource(string Path, int Alignment);
public sealed record HdTexture(ulong Key, int Width, int Height, byte[] Rgba, string Source, string Name);
/// <summary>MapTiles: every radar tile with its position kept, duplicates included (open-sea tiles repeat).</summary>
public sealed record HdScan(IReadOnlyList<HdTexture> Textures, int Skipped2D, int SkippedFormat, int SkippedSmall, int Duplicates,
    IReadOnlyList<HdTexture>? MapTiles = null);

/// <summary>Part 1's textures (spec §3): the loading and legal screens (every RUNDATA *.XTX, one chunk each) and the
/// models' textures in GTA3PSP.IMG (chunks at 2048-byte boundaries), minus 2D art, by key.</summary>
public static class HdCatalog
{
    // "radar" left the list (author, 2026-10-13): the map tiles are upscaled as one image (HdMap).
    public static readonly string[] TwoDWords = ["font", "hud", "icon", "button"];
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
        var mapTiles = new List<HdTexture>();
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
                    if (source.Alignment != 0 && HdMap.TileIndex(name) is not null)
                    {
                        mapTiles.Add(new HdTexture(key, info.Width, info.Height, rgba, Path.GetFileName(source.Path), name));
                        continue;
                    }
                    if (!keys.Add(key)) { duplicates++; continue; }
                    textures.Add(new HdTexture(key, info.Width, info.Height, rgba, Path.GetFileName(source.Path), name));
                }
            }
        }
        return new HdScan(textures, twoD, format, small, duplicates, mapTiles);
    }
}
