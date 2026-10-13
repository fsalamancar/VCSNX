using System.Diagnostics;

namespace Vcsnx.Builder.Core;

public enum Stage { Checking, Downloading, Compiling, Linking, Packaging, CopyingToSd, Done }

public sealed record BuildProgress(Stage Stage, int Done, int Total, TimeSpan? Remaining);
public sealed record BuildRequest(string GameFolder, string? IconImage, string? SdRoot, string? CorpusZip = null);
public sealed record BuildResult(string Module, string Nro, CopyReport? Sd);

public sealed record BuildPaths(string Toolchain, string Kit, string Hashes, string Work, string Output)
{
    public static BuildPaths NextTo(string baseDir) => new(
        Path.Combine(baseDir, "toolchain"), Path.Combine(baseDir, "kit"), Path.Combine(baseDir, "hashes"),
        Path.Combine(baseDir, "trabajo"), Path.Combine(baseDir, "salida"));
}

/// <summary>Check → download the translated code → compile → link vcs_game.so → vcsnx.nro (with the player's
/// icon) → SD card (spec §4).</summary>
public sealed class Pipeline(BuildPaths paths, HttpClient http, IProcessRunner runner, int cores, long memoryBytes, Func<string, long> freeSpace)
{
    public const long RequiredSpace = 3L << 30;

    public async Task<BuildResult> RunAsync(BuildRequest request, IProgress<BuildProgress> progress, TextWriter log, CancellationToken ct)
    {
        progress.Report(new BuildProgress(Stage.Checking, 0, 0, null));
        var toolchain = new Toolchain(Path.GetFullPath(paths.Toolchain));
        if (!toolchain.PathIsUsable) throw new BuilderException("PathNotAscii", toolchain.Root);
        var known = HashList.Load(Path.Combine(paths.Hashes, "executable.sha256")).Values.ToHashSet(StringComparer.Ordinal);
        var game = GameCopy.Check(request.GameFolder, known);
        if (!game.Ok)
            throw new BuilderException(game.State switch
            {
                ExecutableState.Missing => "GameMissing",
                ExecutableState.Encrypted => "GameEncrypted",
                _ => "GameUnknown",
            });
        log.WriteLine($"game: {game.Executable} ({game.DiscVersion})");
        Directory.CreateDirectory(paths.Work);
        Directory.CreateDirectory(paths.Output);
        var free = freeSpace(paths.Work);
        if (free < RequiredSpace) throw new BuilderException("NotEnoughSpace", Math.Ceiling((RequiredSpace - free) / 1e9));
        var kit = new Kit(paths.Kit);
        log.WriteLine($"kit {kit.Info.Version}, interface {kit.Info.Abi}");

        var corpus = new Corpus(Path.Combine(paths.Work, "generated"), HashList.Load(Path.Combine(paths.Hashes, "generated.sha256")));
        if (corpus.Problems().Count > 0)
        {
            progress.Report(new BuildProgress(Stage.Downloading, 0, 0, null));
            await FetchCorpusAsync(corpus, request.CorpusZip, log, ct);
        }

        var compiler = new Compiler(toolchain, kit, corpus.GeneratedDir, Path.Combine(paths.Work, "g"), runner);
        var stems = kit.GeneratedOrder;
        var cached = stems.Count(compiler.UpToDate);
        var jobs = JobPlanner.Jobs(cores, memoryBytes);
        log.WriteLine($"compiling {stems.Count} units, {cached} already done, {jobs} at a time");
        var clock = Stopwatch.StartNew();
        progress.Report(new BuildProgress(Stage.Compiling, cached, stems.Count, null));
        var objects = await compiler.CompileAllAsync(stems, jobs,
            new Relay<int>(done => progress.Report(new BuildProgress(Stage.Compiling, done, stems.Count, Eta.Remaining(done, stems.Count, cached, clock.Elapsed)))), ct);

        progress.Report(new BuildProgress(Stage.Linking, 0, 0, null));
        var module = Path.Combine(paths.Output, "vcs_game.so");
        await new ModuleLinker(toolchain, kit, runner).LinkAsync(objects, module, ct);

        progress.Report(new BuildProgress(Stage.Packaging, 0, 0, null));
        var nro = Path.Combine(paths.Output, "vcsnx.nro");
        if (request.IconImage is { } image)
        {
            var icon = Path.Combine(paths.Work, "icon.jpg");
            IconMaker.Make(image, icon);
            NroIcon.Replace(kit.RuntimeNro, icon, nro);
        }
        else
        {
            File.Copy(kit.RuntimeNro, nro, overwrite: true);
        }

        CopyReport? sd = null;
        if (request.SdRoot is { } sdRoot)
        {
            progress.Report(new BuildProgress(Stage.CopyingToSd, 0, 0, null));
            try
            {
                sd = SdCard.Install(sdRoot, nro, module, kit.Ini, game.DataRoot!, null, ct);
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                throw new BuilderException("SdCopyFailed", e.Message, paths.Output); // the files are already in salida\
            }
            log.WriteLine($"SD: {sd.Copied} copied, {sd.Skipped} already there");
        }
        progress.Report(new BuildProgress(Stage.Done, 0, 0, TimeSpan.Zero));
        return new BuildResult(module, nro, sd);
    }

    async Task FetchCorpusAsync(Corpus corpus, string? localZip, TextWriter log, CancellationToken ct)
    {
        var zip = localZip ?? Path.Combine(paths.Work, "pspr.zip");
        for (var attempt = 0; attempt < 2; ++attempt)
        {
            if (localZip is null && (attempt > 0 || !File.Exists(zip)))
            {
                try
                {
                    await Corpus.DownloadAsync(http, Corpus.ArchiveUri, zip, ct);
                }
                catch (HttpRequestException e)
                {
                    throw new BuilderException("DownloadFailed", e.Message);
                }
                catch (TaskCanceledException e) when (!ct.IsCancellationRequested) // HttpClient's timeout, not the player
                {
                    throw new BuilderException("DownloadFailed", e.Message);
                }
            }
            corpus.ExtractFrom(zip);
            var problems = corpus.Problems();
            if (problems.Count == 0) return;
            log.WriteLine($"translated code does not match: {string.Join(", ", problems.Take(5))}");
            if (localZip is not null) break;
            File.Delete(zip);
        }
        throw new BuilderException("CorpusMismatch");
    }

    sealed class Relay<T>(Action<T> report) : IProgress<T>
    {
        public void Report(T value) => report(value);
    }
}
