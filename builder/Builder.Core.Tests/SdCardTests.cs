namespace Vcsnx.Builder.Core.Tests;

public sealed class SdCardTests
{
    static (string Sd, string Nro, string Module, string Ini, string Data) Setup()
    {
        var dir = TestFiles.TempDir();
        var sd = Path.Combine(dir, "sd");
        Directory.CreateDirectory(Path.Combine(sd, "switch"));
        var nro = TestFiles.Write(Path.Combine(dir, "salida", "vcsnx.nro"), [1, 2, 3]);
        var module = TestFiles.Write(Path.Combine(dir, "salida", "vcs_game.so"), [4, 5, 6]);
        var ini = TestFiles.Write(Path.Combine(dir, "kit", "VCSNative.ini"), "[Display]\n"u8.ToArray());
        var data = Path.Combine(dir, "PSP_DATA");
        TestFiles.Write(Path.Combine(data, "PSP_GAME", "SYSDIR", "BOOT.BIN"), TestFiles.Elf([1]));
        TestFiles.Write(Path.Combine(data, "PSP_GAME", "USRDIR", "DATA.BIN"), [5, 6, 7]);
        TestFiles.Write(Path.Combine(data, "PSP_GAME", "._DATA.BIN"), [0]);
        return (sd, nro, module, ini, data);
    }

    [Fact]
    public void Installing_puts_the_runtime_the_module_and_the_game_in_switch_vcsnx()
    {
        var (sd, nro, module, ini, data) = Setup();
        var report = SdCard.Install(sd, nro, module, ini, data, null, default);
        var game = SdCard.GameFolder(sd);
        Assert.Equal(5, report.Copied);
        Assert.True(File.Exists(Path.Combine(game, "vcsnx.nro")));
        Assert.True(File.Exists(Path.Combine(game, "vcs_game.so")));
        Assert.True(File.Exists(Path.Combine(game, "VCSNative.ini")));
        Assert.True(File.Exists(Path.Combine(game, "PSP_DATA", "PSP_GAME", "USRDIR", "DATA.BIN")));
        Assert.False(File.Exists(Path.Combine(game, "PSP_DATA", "PSP_GAME", "._DATA.BIN")));
    }

    [Fact]
    public void A_second_install_copies_nothing_and_keeps_the_player_s_settings()
    {
        var (sd, nro, module, ini, data) = Setup();
        SdCard.Install(sd, nro, module, ini, data, null, default);
        var sdIni = Path.Combine(SdCard.GameFolder(sd), "VCSNative.ini");
        File.WriteAllText(sdIni, "[Display]\nHudScale=0.9\n");
        Assert.Equal(0, SdCard.Install(sd, nro, module, ini, data, null, default).Copied);
        Assert.Contains("0.9", File.ReadAllText(sdIni));
    }

    [Fact]
    public void A_changed_file_of_the_same_size_is_copied_again()
    {
        var (sd, nro, module, ini, data) = Setup();
        SdCard.Install(sd, nro, module, ini, data, null, default);
        var source = Path.Combine(data, "PSP_GAME", "USRDIR", "DATA.BIN");
        File.WriteAllBytes(source, [9, 9, 9]);
        File.SetLastWriteTimeUtc(source, DateTime.UtcNow.AddHours(1));
        Assert.Equal(1, SdCard.Install(sd, nro, module, ini, data, null, default).Copied);
        Assert.Equal(new byte[] { 9, 9, 9 }, File.ReadAllBytes(Path.Combine(SdCard.GameFolder(sd), "PSP_DATA", "PSP_GAME", "USRDIR", "DATA.BIN")));
    }
}
