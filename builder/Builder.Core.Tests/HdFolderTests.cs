namespace Vcsnx.Builder.Core.Tests;

public sealed class HdFolderTests
{
    const string Key = "0123456789abcdef.png";

    static string Copied(string game, string folder, string name) => Path.Combine(game, folder, name);

    [Theory]
    [InlineData("")]
    [InlineData("Textures")]
    [InlineData("switch/vcsnx/Textures")]  // an SD copy, as in the author's zip
    public void The_textures_are_found_in_the_folder_in_its_Textures_or_in_its_switch_vcsnx_Textures(string inside)
    {
        var chosen = TestFiles.TempDir();
        var textures = Path.Combine(chosen, inside);
        TestFiles.Write(Path.Combine(textures, Key), [1, 2, 3]);
        Assert.Equal(Path.GetFullPath(textures), Path.GetFullPath(HdFolder.Find(chosen)!.Textures!));
    }

    [Theory]
    [InlineData("")]
    [InlineData("Textures")]               // next to the textures, as the author keeps them
    [InlineData("HD")]
    [InlineData("switch/vcsnx/HD")]        // an SD copy
    public void The_hd_movies_are_found_in_the_folder_next_to_the_textures_or_in_its_HD(string inside)
    {
        var chosen = TestFiles.TempDir();
        TestFiles.Write(Path.Combine(chosen, "Textures", Key), [1]);
        TestFiles.Write(Path.Combine(chosen, inside, "LOGO.mp4"), [2]);
        Assert.Equal(new[] { Path.GetFullPath(Path.Combine(chosen, inside, "LOGO.mp4")) },
            HdFolder.Find(chosen)!.Movies.Select(Path.GetFullPath));
    }

    [Fact]
    public void A_folder_with_only_the_hd_movies_is_an_hd_folder()
    {
        var chosen = TestFiles.TempDir();
        TestFiles.Write(Path.Combine(chosen, "TITLES.mp4"), [2]);
        var found = HdFolder.Find(chosen)!;
        Assert.Null(found.Textures);
        Assert.Single(found.Movies);
    }

    [Fact]
    public void A_folder_without_textures_or_movies_is_not_an_hd_folder()
    {
        var chosen = TestFiles.TempDir();
        TestFiles.Write(Path.Combine(chosen, "Textures", "readme.txt"), [1]);
        TestFiles.Write(Path.Combine(chosen, "HD", "trailer.mp4"), [1]);  // not one of the game's movies
        Assert.Null(HdFolder.Find(chosen));
        Assert.Null(HdFolder.Find(Path.Combine(chosen, "missing")));
    }

    [Fact]
    public void Only_texture_files_are_copied_into_the_game_folders_Textures()
    {
        var source = TestFiles.TempDir();
        TestFiles.Write(Path.Combine(source, Key), [1, 2, 3]);
        TestFiles.Write(Path.Combine(source, "FEDCBA9876543210.PNG"), [4]);
        foreach (var other in new[] { "readme.txt", "._0123456789abcdef.png", "notes.png", Key + ".tmp", "0123456789abcde.png" })
            TestFiles.Write(Path.Combine(source, other), [0]);
        var game = Path.Combine(TestFiles.TempDir(), "switch", "vcsnx");
        var report = HdFolder.CopyTo(HdFolder.Find(source)!, game, default);
        Assert.Equal(2, report.Textures.Copied);
        Assert.Equal(new[] { "0123456789abcdef.png", "FEDCBA9876543210.PNG" },
            Directory.GetFiles(Path.Combine(game, "Textures")).Select(Path.GetFileName).Order(StringComparer.Ordinal));
        Assert.Equal(new byte[] { 1, 2, 3 }, File.ReadAllBytes(Copied(game, "Textures", Key)));
    }

    [Fact]
    public void The_movies_go_to_the_game_folders_HD_under_the_names_the_console_looks_for()
    {
        var source = TestFiles.TempDir();
        TestFiles.Write(Path.Combine(source, "logo.MP4"), [5]);
        TestFiles.Write(Path.Combine(source, "TITLES.mp4"), [6]);
        var game = TestFiles.TempDir();
        var report = HdFolder.CopyTo(HdFolder.Find(source)!, game, default);
        Assert.Equal(2, report.Movies.Copied);
        Assert.Equal(new[] { "LOGO.mp4", "TITLES.mp4" },
            Directory.GetFiles(Path.Combine(game, "HD")).Select(Path.GetFileName).Order(StringComparer.Ordinal));
        Assert.Equal(new byte[] { 5 }, File.ReadAllBytes(Copied(game, "HD", "LOGO.mp4")));
        Assert.False(Directory.Exists(Path.Combine(game, "Textures")));
    }

    [Fact]
    public void Copying_again_skips_the_textures_and_movies_already_there()
    {
        var source = TestFiles.TempDir();
        TestFiles.Write(Path.Combine(source, Key), [1, 2, 3]);
        TestFiles.Write(Path.Combine(source, "LOGO.mp4"), [5]);
        var game = TestFiles.TempDir();
        HdFolder.CopyTo(HdFolder.Find(source)!, game, default);
        var again = HdFolder.CopyTo(HdFolder.Find(source)!, game, default);
        Assert.Equal((0, 1), (again.Textures.Copied, again.Textures.Skipped));
        Assert.Equal((0, 1), (again.Movies.Copied, again.Movies.Skipped));
    }
}
