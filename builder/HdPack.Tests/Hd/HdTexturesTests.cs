using Vcsnx.HdPack;

namespace Vcsnx.HdPack.Tests;

public sealed class HdTexturesTests : IDisposable
{
    readonly string _root = Path.Combine(Path.GetTempPath(), "vcsnx-hdt-" + Guid.NewGuid().ToString("N"));
    string Game => Path.Combine(_root, "PSP_DATA");
    string Sd => Path.Combine(_root, "sd");
    string Textures => Path.Combine(Sd, "switch", "vcsnx", "Textures");
    BuildPaths Paths => new(Path.Combine(_root, "tc"), Path.Combine(_root, "kit"), Path.Combine(_root, "h"), Path.Combine(_root, "work"), Path.Combine(_root, "out"));

    public HdTexturesTests()
    {
        var runData = Path.Combine(Game, "PSP_GAME", "USRDIR", "RUNDATA");
        Directory.CreateDirectory(runData);
        var textures = Enumerable.Range(0, 5).Select(i =>
            TexChunkBuilder.Indexed($"car_{i}", 16, 16, 4, 1, (x, y) => (x * (i + 1) + y) & 15, TexChunkBuilder.Palette(16))).ToArray();
        File.WriteAllBytes(Path.Combine(runData, "GTA3PSP.IMG"), TexChunkBuilder.Build([.. textures, TexChunkBuilder.Indexed("hud_x", 16, 16, 4, 1, (_, _) => 3, TexChunkBuilder.Palette(16))]));
    }

    public void Dispose() => Directory.Delete(_root, recursive: true);

    HdTextures Step(IProcessRunner runner) => new(Paths, runner, (_, _) => Task.FromResult<string?>("esrgan"));

    [Fact]
    public async Task EveryModelTextureLandsOnTheSdAsItsKey()
    {
        var report = await Step(new FakeEsrgan(FakeEsrgan.Mode.Faithful)).RunAsync(new HdRequest(Game, Sd), new Progress<HdProgress>(), TextWriter.Null, CancellationToken.None);
        Assert.Equal((5, 5, 0, 1), (report.Written, report.Ai, report.Classic, report.Skipped2D));
        var scan = HdCatalog.Scan(HdCatalog.SourcesIn(Game));
        foreach (var t in scan.Textures)
        {
            var (w, h, _) = PngCodec.Decode(File.ReadAllBytes(Path.Combine(Textures, TextureKey.FileName(t.Key))));
            Assert.Equal((64, 64), (w, h));
        }
        Assert.True(File.Exists(Path.Combine(Paths.Output, "texturas_hd.log")));
    }

    [Fact]
    public async Task HandMadeReplacementsAreNeverOverwritten()
    {
        var first = HdCatalog.Scan(HdCatalog.SourcesIn(Game)).Textures[0];
        Directory.CreateDirectory(Textures);
        var mine = Path.Combine(Textures, TextureKey.FileName(first.Key));
        File.WriteAllText(mine, "hand made");
        var report = await Step(new FakeEsrgan(FakeEsrgan.Mode.Faithful)).RunAsync(new HdRequest(Game, Sd), new Progress<HdProgress>(), TextWriter.Null, CancellationToken.None);
        Assert.Equal("hand made", File.ReadAllText(mine));
        Assert.Equal((4, 1), (report.Written, report.Existing));
    }

    [Fact]
    public async Task CancellingLeavesOnlyCompleteFiles()
    {
        using var cancel = new CancellationTokenSource();
        var progress = new SyncProgress<HdProgress>(p => { if (p.Done >= 2) cancel.Cancel(); });
        await Assert.ThrowsAnyAsync<OperationCanceledException>(() =>
            Step(new FakeEsrgan(FakeEsrgan.Mode.Faithful)).RunAsync(new HdRequest(Game, Sd, BatchSize: 2), progress, TextWriter.Null, cancel.Token));
        var files = Directory.GetFiles(Textures);
        Assert.Equal(2, files.Length);
        Assert.All(files, f => Assert.EndsWith(".png", f));
        Assert.All(files, f => PngCodec.Decode(File.ReadAllBytes(f)));
    }

    [Fact]
    public async Task TheMapIsUpscaledWholeAndWrittenAsItsTiles()
    {
        var runData = Path.Combine(Game, "PSP_GAME", "USRDIR", "RUNDATA");
        var tiles = Enumerable.Range(0, 64).Select(i =>
            TexChunkBuilder.Indexed($"radar{i:D2}", 128, 128, 4, 1, (x, y) => (x / 16 + y / 16 + i) & 15, TexChunkBuilder.Palette(16))).ToArray();
        File.WriteAllBytes(Path.Combine(runData, "GTA3PSP.IMG"), TexChunkBuilder.Build(tiles));
        var report = await Step(new FakeEsrgan(FakeEsrgan.Mode.Faithful)).RunAsync(new HdRequest(Game, Sd), new Progress<HdProgress>(), TextWriter.Null, CancellationToken.None);
        var keys = HdCatalog.Scan(HdCatalog.SourcesIn(Game)).MapTiles!.Select(t => t.Key).Distinct().ToList();
        Assert.Equal(64, report.MapTiles);
        Assert.NotEmpty(keys);
        foreach (var key in keys)
        {
            var (w, h, _) = PngCodec.Decode(File.ReadAllBytes(Path.Combine(Textures, TextureKey.FileName(key))));
            Assert.Equal((512, 512), (w, h));
        }
    }

    [Fact]
    public async Task WithoutTheAiTheReportSaysSo()
    {
        var report = await new HdTextures(Paths, new FakeEsrgan(FakeEsrgan.Mode.Failing), (_, _) => Task.FromResult<string?>(null))
            .RunAsync(new HdRequest(Game, null), new Progress<HdProgress>(), TextWriter.Null, CancellationToken.None);
        Assert.False(report.AiAvailable);
        Assert.Equal((5, 0, 5), (report.Written, report.Ai, report.Classic));
        Assert.Equal(Path.Combine(Paths.Output, "Textures"), report.OutputDir);
    }
}
