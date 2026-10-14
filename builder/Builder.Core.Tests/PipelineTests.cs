using System.Net;
using System.Security.Cryptography;
using System.Text;

namespace Vcsnx.Builder.Core.Tests;

public sealed class PipelineTests
{
    sealed class Serve(byte[] zip) : HttpMessageHandler
    {
        public int Requests;
        protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken ct)
        {
            ++Requests;
            return Task.FromResult(new HttpResponseMessage(HttpStatusCode.OK) { Content = new ByteArrayContent(zip) });
        }
    }

    static string Sha(byte[] data) => Convert.ToHexStringLower(SHA256.HashData(data));

    sealed record World(BuildPaths Paths, string Game, Serve Server, FakeRunner Runner);

    static World MakeWorld(string? baseDir = null)
    {
        var dir = baseDir ?? TestFiles.TempDir();
        var paths = BuildPaths.NextTo(dir);
        Directory.CreateDirectory(paths.Toolchain);
        ToolchainKitTests.MakeKit(paths.Kit, ["-o", "{output}", "-c", "{source}"], ["-shared", "{generated_objects}", "-o", "{output}"],
            ["generated_registry", "generated_unit_0000"]);
        var files = new Dictionary<string, string> { ["generated_registry.cpp"] = "registry", ["generated_unit_0000.cpp"] = "unit 0", ["generated_units.hpp"] = "header" };
        Directory.CreateDirectory(paths.Hashes);
        File.WriteAllLines(Path.Combine(paths.Hashes, "generated.sha256"), files.Select(f => $"{Sha(Encoding.UTF8.GetBytes(f.Value))}  {f.Key}"));
        var elf = TestFiles.Elf([4, 2]);
        File.WriteAllText(Path.Combine(paths.Hashes, "executable.sha256"), $"{Sha(elf)}  BOOT.BIN\n");
        var game = Path.Combine(dir, "PSP_DATA");
        TestFiles.Write(Path.Combine(game, "PSP_GAME", "SYSDIR", "BOOT.BIN"), elf);
        var zip = CorpusTests.MakeZip(TestFiles.TempDir(), files.ToDictionary(f => Corpus.ArchivePrefix + f.Key, f => f.Value));
        return new World(paths, game, new Serve(File.ReadAllBytes(zip)), new FakeRunner());
    }

    static Pipeline PipelineFor(World w, long free = 10L << 30) =>
        new(w.Paths, new HttpClient(w.Server), w.Runner, cores: 4, memoryBytes: 16L << 30, freeSpace: _ => free);

    static Task<BuildResult> Run(World w, BuildRequest request, long free = 10L << 30) =>
        PipelineFor(w, free).RunAsync(request, new SyncProgress<BuildProgress>(_ => { }), TextWriter.Null, default);

    [Fact]
    public async Task A_first_build_downloads_compiles_links_and_packs_in_order()
    {
        var w = MakeWorld();
        var stages = new List<Stage>();
        var result = await PipelineFor(w).RunAsync(new BuildRequest(w.Game, null, null), new SyncProgress<BuildProgress>(p => stages.Add(p.Stage)), TextWriter.Null, default);
        Assert.True(File.Exists(result.Module));
        Assert.Equal(File.ReadAllBytes(Path.Combine(w.Paths.Kit, "vcsnx.nro")), File.ReadAllBytes(result.Nro));
        Assert.Equal(1, w.Server.Requests);
        Assert.Equal(new[] { Stage.Checking, Stage.Downloading, Stage.Compiling, Stage.Linking, Stage.Packaging, Stage.Done }, stages.Distinct());
        Assert.Null(result.Sd);
    }

    [Fact]
    public async Task Building_again_for_a_new_icon_or_sd_does_not_download_compile_or_link()
    {
        var w = MakeWorld();
        await Run(w, new BuildRequest(w.Game, null, null));
        w.Runner.Calls.Clear();
        var sd = Path.Combine(TestFiles.TempDir(), "sd");
        Directory.CreateDirectory(Path.Combine(sd, "switch"));
        var result = await Run(w, new BuildRequest(w.Game, null, sd));
        Assert.Equal(1, w.Server.Requests);
        Assert.Empty(w.Runner.Calls);
        Assert.True(File.Exists(Path.Combine(SdCard.GameFolder(sd), "vcs_game.so")));
        Assert.True(File.Exists(Path.Combine(SdCard.GameFolder(sd), "vcsnx.nro")));
        Assert.NotNull(result.Sd);
    }

    static string HdFolderWith(params string[] names)
    {
        var folder = TestFiles.TempDir();
        foreach (var name in names) TestFiles.Write(Path.Combine(folder, "Textures", name), [7, 7]);
        return folder;
    }

    [Fact]
    public async Task The_hd_textures_the_player_picks_go_to_the_sd_with_the_game()
    {
        var w = MakeWorld();
        var sd = Path.Combine(TestFiles.TempDir(), "sd");
        Directory.CreateDirectory(Path.Combine(sd, "switch"));
        var result = await Run(w, new BuildRequest(w.Game, null, sd, HdFolder: HdFolderWith("0123456789abcdef.png")));
        Assert.True(File.Exists(Path.Combine(SdCard.GameFolder(sd), "Textures", "0123456789abcdef.png")));
        Assert.Equal(1, result.Hd!.Textures.Copied);
    }

    [Fact]
    public async Task The_hd_movies_in_the_folder_go_to_the_sd_with_the_textures()
    {
        var w = MakeWorld();
        var sd = Path.Combine(TestFiles.TempDir(), "sd");
        Directory.CreateDirectory(Path.Combine(sd, "switch"));
        var folder = HdFolderWith("0123456789abcdef.png");
        TestFiles.Write(Path.Combine(folder, "Textures", "LOGO.mp4"), [3]);
        var result = await Run(w, new BuildRequest(w.Game, null, sd, HdFolder: folder));
        Assert.True(File.Exists(Path.Combine(SdCard.GameFolder(sd), "HD", "LOGO.mp4")));
        Assert.Equal(1, result.Hd!.Movies.Copied);
    }

    [Fact]
    public async Task Without_an_sd_the_hd_textures_go_next_to_the_nro()
    {
        var w = MakeWorld();
        await Run(w, new BuildRequest(w.Game, null, null, HdFolder: HdFolderWith("0123456789abcdef.png")));
        Assert.True(File.Exists(Path.Combine(w.Paths.Output, "Textures", "0123456789abcdef.png")));
    }

    [Fact]
    public async Task Without_an_hd_folder_the_textures_on_the_sd_are_left_as_they_are()
    {
        var w = MakeWorld();
        var sd = Path.Combine(TestFiles.TempDir(), "sd");
        var mine = TestFiles.Write(Path.Combine(SdCard.GameFolder(sd), "Textures", "fedcba9876543210.png"), [9]);
        var result = await Run(w, new BuildRequest(w.Game, null, sd));
        Assert.Equal(new[] { mine }, Directory.GetFiles(Path.Combine(SdCard.GameFolder(sd), "Textures")));
        Assert.Equal(new byte[] { 9 }, File.ReadAllBytes(mine));
        Assert.Null(result.Hd);
    }

    // VCSNX's Settings > Controls picture and the menu legend's A and B, in the kit (tools/make_builder_kit.py).
    static string KitPicture(World w, byte[] content) =>
        TestFiles.Write(Path.Combine(w.Paths.Kit, "Textures", "cc29539db154b0c7.png"), content);

    [Fact]
    public async Task The_kits_menu_pictures_go_to_the_sd_with_the_game_next_to_the_players_own()
    {
        var w = MakeWorld();
        KitPicture(w, [1, 2, 3]);
        var sd = Path.Combine(TestFiles.TempDir(), "sd");
        var mine = TestFiles.Write(Path.Combine(SdCard.GameFolder(sd), "Textures", "fedcba9876543210.png"), [9]);
        await Run(w, new BuildRequest(w.Game, null, sd));
        Assert.Equal(new byte[] { 1, 2, 3 }, File.ReadAllBytes(Path.Combine(SdCard.GameFolder(sd), "Textures", "cc29539db154b0c7.png")));
        Assert.Equal(new byte[] { 9 }, File.ReadAllBytes(mine));
    }

    [Fact]
    public async Task Without_an_sd_the_kits_menu_pictures_go_next_to_the_nro()
    {
        var w = MakeWorld();
        KitPicture(w, [1, 2, 3]);
        await Run(w, new BuildRequest(w.Game, null, null));
        Assert.Equal(new byte[] { 1, 2, 3 }, File.ReadAllBytes(Path.Combine(w.Paths.Output, "Textures", "cc29539db154b0c7.png")));
    }

    [Fact]
    public async Task The_kits_menu_picture_wins_over_an_hd_folders_copy_of_the_same_texture()
    {
        // An HD folder can hold an upscale of the PSP's drawing under the same key; the runtime lays out VCSNX's own.
        var w = MakeWorld();
        KitPicture(w, [1, 2, 3]);
        var sd = Path.Combine(TestFiles.TempDir(), "sd");
        Directory.CreateDirectory(Path.Combine(sd, "switch"));
        await Run(w, new BuildRequest(w.Game, null, sd, HdFolder: HdFolderWith("cc29539db154b0c7.png")));
        Assert.Equal(new byte[] { 1, 2, 3 }, File.ReadAllBytes(Path.Combine(SdCard.GameFolder(sd), "Textures", "cc29539db154b0c7.png")));
    }

    [Fact]
    public async Task An_hd_folder_without_textures_is_refused_before_anything_runs()
    {
        var w = MakeWorld();
        var empty = TestFiles.TempDir();
        var error = await Assert.ThrowsAsync<BuilderException>(() => Run(w, new BuildRequest(w.Game, null, null, HdFolder: empty)));
        Assert.Equal("HdFolderEmpty", error.Key);
        Assert.Empty(w.Runner.Calls);
        Assert.Equal(0, w.Server.Requests);
    }

    [Fact]
    public async Task A_folder_with_accents_is_refused_before_anything_runs_and_one_with_spaces_works()
    {
        var accented = MakeWorld(Path.Combine(TestFiles.TempDir(), "VCSNX-Bú"));
        var error = await Assert.ThrowsAsync<BuilderException>(() => Run(accented, new BuildRequest(accented.Game, null, null)));
        Assert.Equal("PathNotAscii", error.Key);
        Assert.Empty(accented.Runner.Calls);
        var spaced = MakeWorld(Path.Combine(TestFiles.TempDir(), "VCSNX Builder"));
        Assert.True(File.Exists((await Run(spaced, new BuildRequest(spaced.Game, null, null))).Module));
    }

    [Fact]
    public async Task Not_enough_space_and_a_wrong_game_are_player_messages()
    {
        var w = MakeWorld();
        Assert.Equal("NotEnoughSpace", (await Assert.ThrowsAsync<BuilderException>(() => Run(w, new BuildRequest(w.Game, null, null), free: 1L << 30))).Key);
        File.WriteAllBytes(Path.Combine(w.Game, "PSP_GAME", "SYSDIR", "BOOT.BIN"), TestFiles.Elf([0]));
        Assert.Equal("GameUnknown", (await Assert.ThrowsAsync<BuilderException>(() => Run(w, new BuildRequest(w.Game, null, null)))).Key);
    }

    [Fact]
    public async Task A_failed_sd_copy_says_so_and_keeps_the_files()
    {
        var w = MakeWorld();
        var notADrive = TestFiles.Write(Path.Combine(TestFiles.TempDir(), "sd"), [0]); // a file where the SD folder should be
        var error = await Assert.ThrowsAsync<BuilderException>(() => Run(w, new BuildRequest(w.Game, null, notADrive)));
        Assert.Equal("SdCopyFailed", error.Key);
        Assert.True(File.Exists(Path.Combine(w.Paths.Output, "vcs_game.so")));
        Assert.True(File.Exists(Path.Combine(w.Paths.Output, "vcsnx.nro")));
    }

    [Theory]
    [InlineData(0, 235, 0, 60, null)]
    [InlineData(10, 235, 0, 60, 1350.0)]
    [InlineData(110, 235, 100, 60, 750.0)]
    [InlineData(235, 235, 0, 600, 0.0)]
    public void Time_left_counts_only_what_was_really_compiled(int done, int total, int cached, int elapsedSeconds, double? remainingSeconds) =>
        Assert.Equal(remainingSeconds, Eta.Remaining(done, total, cached, TimeSpan.FromSeconds(elapsedSeconds))?.TotalSeconds);

    sealed class TimesOut : HttpMessageHandler
    {
        protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken ct) =>
            throw new TaskCanceledException("The request was canceled due to the configured HttpClient.Timeout of 100 seconds elapsing.");
    }

    [Fact]
    public async Task A_download_that_times_out_is_a_download_problem_not_a_cancel()
    {
        var w = MakeWorld();
        var pipeline = new Pipeline(w.Paths, new HttpClient(new TimesOut()), w.Runner, cores: 4, memoryBytes: 16L << 30, freeSpace: _ => 10L << 30);
        var error = await Assert.ThrowsAsync<BuilderException>(() =>
            pipeline.RunAsync(new BuildRequest(w.Game, null, null), new SyncProgress<BuildProgress>(_ => { }), TextWriter.Null, default));
        Assert.Equal("DownloadFailed", error.Key);
    }
}
