using System.Diagnostics;

namespace Vcsnx.HdPack;

public sealed record HdRequest(string GameFolder, string? SdRoot, int Limit = int.MaxValue, IReadOnlyList<string>? Models = null, int BatchSize = 64);
public sealed record HdProgress(int Done, int Total, TimeSpan? Remaining);
public sealed record HdReport(int Written, int Ai, int Classic, int Existing, int Skipped2D, int SkippedFormat, int SkippedSmall,
    int Duplicates, bool AiAvailable, string OutputDir, IReadOnlyDictionary<string, int> ByMethod, int MapTiles = 0, string MapMethod = "");

/// <summary>HdPack's one step (spec §4; the Builder no longer runs it, 2026-10-13): scan, upscale in batches, write each texture as
/// Textures/&lt;key&gt;.png (via a .tmp, so a cancelled run leaves only whole files), never over an existing file,
/// and a summary in salida\texturas_hd.log.</summary>
public sealed class HdTextures(BuildPaths paths, IProcessRunner runner, Func<TextWriter, CancellationToken, Task<string?>> findEsrgan)
{
    /// <summary>The author's choice (2026-10-13): the general model first, the anime one where it invents too much.</summary>
    public static readonly IReadOnlyList<string> DefaultModels = ["realesrgan-x4plus", "realesr-animevideov3"];

    public async Task<HdReport> RunAsync(HdRequest request, IProgress<HdProgress> progress, TextWriter log, CancellationToken ct)
    {
        var outputDir = request.SdRoot is { } sd ? Path.Combine(sd, "switch", "vcsnx", "Textures") : Path.Combine(paths.Output, "Textures");
        Directory.CreateDirectory(outputDir);
        var scan = HdCatalog.Scan(HdCatalog.SourcesIn(request.GameFolder));
        var mapTiles = scan.MapTiles ?? [];
        var candidates = scan.Textures.Where(t => HdMap.TileIndex(t.Name) is null && HdPrep.ScaleFor(t.Width, t.Height) > 1)
            .Take(request.Limit).ToList();
        var todo = candidates.Where(t => !File.Exists(Path.Combine(outputDir, TextureKey.FileName(t.Key)))).ToList();
        var existing = candidates.Count - todo.Count;
        var models = request.Models ?? DefaultModels;
        var upscaler = new HdUpscaler(runner, await findEsrgan(log, ct), models, Path.Combine(paths.Work, "hd"));
        var clock = Stopwatch.StartNew();
        var byMethod = new Dictionary<string, int>(StringComparer.Ordinal);
        var done = 0;
        var (mapWritten, mapMethod) = await WriteMapAsync(mapTiles, upscaler, outputDir, ct);
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
                byMethod[outcomes[i].Method] = byMethod.GetValueOrDefault(outcomes[i].Method) + 1;
            }
            done += batch.Length;
            progress.Report(new HdProgress(done, todo.Count, Eta.Remaining(done, todo.Count, 0, clock.Elapsed)));
        }
        var classic = byMethod.GetValueOrDefault(HdUpscaler.Lanczos);
        var report = new HdReport(done, done - classic, classic, existing, scan.Skipped2D, scan.SkippedFormat, scan.SkippedSmall, scan.Duplicates,
            upscaler.AiAvailable, outputDir, byMethod, mapWritten, mapMethod);
        Directory.CreateDirectory(paths.Output);
        var methods = string.Join(" ", byMethod.OrderBy(kv => kv.Key, StringComparer.Ordinal).Select(kv => $"{kv.Key} {kv.Value}"));
        await File.WriteAllTextAsync(Path.Combine(paths.Output, "texturas_hd.log"),
            $"read {scan.Textures.Count} written {done} ({methods}) existing {existing} " +
            $"skipped 2d {scan.Skipped2D} format {scan.SkippedFormat} small {scan.SkippedSmall} duplicates {scan.Duplicates} " +
            $"map tiles {mapWritten} ({mapMethod}) ai_available {report.AiAvailable} models {string.Join(",", models)} seconds {clock.Elapsed.TotalSeconds:F0}{Environment.NewLine}",
            CancellationToken.None);
        log.WriteLine($"HD textures: written {done} ({methods}), existing {existing}, map tiles {mapWritten} ({mapMethod}), AI available {report.AiAvailable}, in {outputDir}");
        return report;
    }

    /// <summary>The map (HdMap): assembled, upscaled 4x as one image with the cascade, cut back into its tiles, each
    /// written under the tile's own key unless it exists. Returns the tiles written and the method used.</summary>
    static async Task<(int Written, string Method)> WriteMapAsync(IReadOnlyList<HdTexture> tiles, HdUpscaler upscaler, string outputDir, CancellationToken ct)
    {
        var map = HdMap.Assemble(tiles);
        if (map is null) return (0, "");
        var byIndex = tiles.Where(t => HdMap.TileIndex(t.Name) is not null).ToDictionary(t => HdMap.TileIndex(t.Name)!.Value);
        if (byIndex.Values.All(t => File.Exists(Path.Combine(outputDir, TextureKey.FileName(t.Key))))) return (0, "existing");
        var whole = new HdTexture(0, map.Width, map.Height, map.Rgba, "GTA3PSP.IMG", "radar map");
        var outcome = (await upscaler.UpscaleAsync([whole], ct, scaleOverride: 4))[0];
        var pieces = HdMap.Cut(outcome.Rgba, outcome.Width, outcome.Height);
        int tileWidth = outcome.Width / HdMap.Tiles, tileHeight = outcome.Height / HdMap.Tiles, written = 0;
        for (var i = 0; i < pieces.Count; i++)
        {
            if (!byIndex.TryGetValue(i, out var tile)) continue;
            var final = Path.Combine(outputDir, TextureKey.FileName(tile.Key));
            written++;
            if (File.Exists(final)) continue;  // identical tiles (open sea) share one key
            await File.WriteAllBytesAsync(final + ".tmp", PngCodec.EncodeRgba(pieces[i], tileWidth, tileHeight), CancellationToken.None);
            File.Move(final + ".tmp", final, overwrite: false);
        }
        return (written, outcome.Method);
    }
}
