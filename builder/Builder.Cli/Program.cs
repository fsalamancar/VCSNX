using Vcsnx.Builder.Core;

// Console front end of Builder.Core: the Mac proof and the Windows proof under Wine (Task 11), and a fallback
// for players. Usage: Builder.Cli --game <PSP_DATA> [--icon <image>] [--sd <SD root>]
//   [--corpus-zip <PSPRecomp zip>] [--base <folder with toolchain, kit, hashes>] [--toolchain <dir>]
//   [--kit <dir>] [--hashes <dir>] [--work <dir>] [--out <dir>]
// HD textures and movies the player already has: --hd-folder <folder with them> (its Textures, or a copy of an SD card).
var options = new Dictionary<string, string>(StringComparer.Ordinal);
for (var i = 0; i + 1 < args.Length; i += 2)
{
    if (!args[i].StartsWith("--", StringComparison.Ordinal)) { Console.Error.WriteLine($"unexpected '{args[i]}'"); return 2; }
    options[args[i][2..]] = args[i + 1];
}
if (!options.TryGetValue("game", out var gameFolder)) { Console.Error.WriteLine("usage: Builder.Cli --game <PSP_DATA> [options]"); return 2; }
var defaults = BuildPaths.NextTo(options.GetValueOrDefault("base", AppContext.BaseDirectory));
var paths = new BuildPaths(options.GetValueOrDefault("toolchain", defaults.Toolchain), options.GetValueOrDefault("kit", defaults.Kit),
    options.GetValueOrDefault("hashes", defaults.Hashes), options.GetValueOrDefault("work", defaults.Work), options.GetValueOrDefault("out", defaults.Output));
Directory.CreateDirectory(paths.Output);
using var logFile = new StreamWriter(Path.Combine(paths.Output, "builder.log"), append: false) { AutoFlush = true };
var log = TextWriter.Synchronized(logFile);
using var cancel = new CancellationTokenSource();
Console.CancelKeyPress += (_, e) => { e.Cancel = true; cancel.Cancel(); };
var pipeline = new Pipeline(paths, new HttpClient(), new LoggingRunner(new SystemProcessRunner(Path.Combine(Path.GetFullPath(paths.Work), "tmp")), log), Environment.ProcessorCount,
    GC.GetGCMemoryInfo().TotalAvailableMemoryBytes, path => new DriveInfo(Path.GetPathRoot(Path.GetFullPath(path))!).AvailableFreeSpace);
var last = "";
var progress = new Progress<BuildProgress>(p =>
{
    var line = p.Stage == Stage.Compiling
        ? $"[{p.Stage}] {p.Done}/{p.Total}" + (p.Remaining is { } r ? $" ~{Math.Ceiling(r.TotalMinutes)} min" : "")
        : $"[{p.Stage}]";
    if (line != last) Console.WriteLine(last = line);
});
try
{
    var result = await pipeline.RunAsync(new BuildRequest(gameFolder, options.GetValueOrDefault("icon"), options.GetValueOrDefault("sd"),
        options.GetValueOrDefault("corpus-zip"), options.GetValueOrDefault("hd-folder")), progress, log, cancel.Token);
    Console.WriteLine(Strings.Get(result.Sd is null ? "Done" : "DoneSd", Path.GetDirectoryName(result.Module)!));
    if (result.Hd is { } hd)
        Console.WriteLine(result.Sd is null
            ? Strings.Get("DoneHd", hd.Textures.Copied + hd.Textures.Skipped, hd.Movies.Copied + hd.Movies.Skipped, Path.GetDirectoryName(result.Module)!)
            : Strings.Get("DoneHdSd", hd.Textures.Copied, hd.Movies.Copied, hd.Textures.Skipped + hd.Movies.Skipped));
    return 0;
}
catch (BuilderException e) { Console.Error.WriteLine(e.Localized); log.WriteLine(e.Message); return 1; }
catch (CompileFailedException e) { Console.Error.WriteLine(Strings.Get("CompileFailed", e.Unit)); log.WriteLine(e.Message); return 1; }
catch (ToolFailedException e) { Console.Error.WriteLine(Strings.Get("ToolFailed", e.Tool)); log.WriteLine(e.Message); return 1; }
catch (OperationCanceledException) { Console.Error.WriteLine(Strings.Get("Cancelled")); return 130; }
