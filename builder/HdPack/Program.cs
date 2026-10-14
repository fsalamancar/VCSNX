using Vcsnx.HdPack;

// The author's tool for an HD textures folder, which the Builder then copies to the SD ("4. HD textures"). Not in the
// Builder's zip. Usage: HdPack --game <PSP_DATA> --list <file>   writes every texture's key, size, source and name.
//   HdPack --game <PSP_DATA> [--out <folder>] [--sd <SD root>] [--work <dir>] [--limit N] [--model a,b] [--esrgan <exe>]
//   upscales them into <folder>\Textures (default salida\Textures next to this tool), or the SD's switch\vcsnx\Textures.
var options = new Dictionary<string, string>(StringComparer.Ordinal);
for (var i = 0; i + 1 < args.Length; i += 2)
{
    if (!args[i].StartsWith("--", StringComparison.Ordinal)) { Console.Error.WriteLine($"unexpected '{args[i]}'"); return 2; }
    options[args[i][2..]] = args[i + 1];
}
if (!options.TryGetValue("game", out var gameFolder)) { Console.Error.WriteLine("usage: HdPack --game <PSP_DATA> [options]"); return 2; }
if (options.TryGetValue("list", out var listPath))
{
    var scan = HdCatalog.Scan(HdCatalog.SourcesIn(gameFolder));
    File.WriteAllLines(listPath, scan.Textures.Concat(scan.MapTiles ?? []).Select(t => $"{t.Key:x16} {t.Width} {t.Height} {t.Source} {t.Name}"));
    Console.WriteLine($"{scan.Textures.Count} textures, skipped 2D {scan.Skipped2D} format {scan.SkippedFormat} small {scan.SkippedSmall} duplicates {scan.Duplicates}");
    return 0;
}
var defaults = BuildPaths.NextTo(AppContext.BaseDirectory);
var paths = defaults with { Work = options.GetValueOrDefault("work", defaults.Work), Output = options.GetValueOrDefault("out", defaults.Output) };
using var cancel = new CancellationTokenSource();
Console.CancelKeyPress += (_, e) => { e.Cancel = true; cancel.Cancel(); };
var http = new HttpClient();
var step = new HdTextures(paths, new SystemProcessRunner(),
    (log, ct) => options.TryGetValue("esrgan", out var exe) ? Task.FromResult<string?>(exe)
        : RealEsrgan.EnsureAsync(Path.Combine(paths.Work, "realesrgan"), http, log, ct));
var last = "";
var progress = new Progress<HdProgress>(p =>
{
    var line = $"HD textures {p.Done}/{p.Total}";
    if (line != last) Console.WriteLine(last = line);
});
var request = new HdRequest(gameFolder, options.GetValueOrDefault("sd"),
    int.Parse(options.GetValueOrDefault("limit", int.MaxValue.ToString())),
    options.TryGetValue("model", out var models) ? models.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries) : null);
try
{
    var report = await step.RunAsync(request, progress, Console.Out, cancel.Token);
    if (!report.AiAvailable) Console.WriteLine("The AI could not use the graphics card; every texture used the classic method.");
    Console.WriteLine($"Done: {report.Written} HD textures in {report.OutputDir} ({report.Ai} with AI, {report.Classic} with the classic method).");
    return 0;
}
catch (OperationCanceledException) { Console.Error.WriteLine("Cancelled."); return 130; }
