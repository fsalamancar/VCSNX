using System.Security.Cryptography;

namespace Vcsnx.Builder.Core.Tests;

public sealed class GameCopyTests
{
    static readonly byte[] KnownElf = TestFiles.Elf([1, 2, 3]);
    static readonly HashSet<string> Known = [Convert.ToHexStringLower(SHA256.HashData(KnownElf))];

    static string DataWith(string executable, byte[] bytes)
    {
        var data = Path.Combine(TestFiles.TempDir(), "PSP_DATA");
        TestFiles.Write(Path.Combine(data, "PSP_GAME", "PARAM.SFO"), TestFiles.Sfo(("DISC_ID", "ULUS10160"), ("DISC_VERSION", "1.03")));
        TestFiles.Write(Path.Combine(data, executable), bytes);
        return data;
    }

    [Fact]
    public void A_decrypted_known_executable_is_ok()
    {
        var check = GameCopy.Check(DataWith("PSP_GAME/SYSDIR/EBOOT_DECRYPTED.ELF", KnownElf), Known);
        Assert.True(check.Ok);
        Assert.Equal("1.03", check.DiscVersion);
    }

    [Fact]
    public void The_folder_that_holds_PSP_DATA_and_PSP_GAME_inside_it_are_found_too()
    {
        var data = DataWith("PSP_GAME/SYSDIR/BOOT.BIN", KnownElf);
        Assert.Equal(data, GameCopy.FindDataRoot(Path.GetDirectoryName(data)!));
        Assert.Equal(data, GameCopy.FindDataRoot(Path.Combine(data, "PSP_GAME")));
        Assert.Equal(data, GameCopy.FindDataRoot(data));
    }

    [Fact]
    public void A_folder_without_the_game_has_no_data_root()
    {
        Assert.Null(GameCopy.FindDataRoot(TestFiles.TempDir()));
        Assert.Equal(ExecutableState.Missing, GameCopy.Check(TestFiles.TempDir(), Known).State);
    }

    [Fact]
    public void An_encrypted_executable_is_reported()
    {
        var check = GameCopy.Check(DataWith("PSP_GAME/SYSDIR/EBOOT.BIN", "~PSP\0\0\0\0"u8.ToArray()), Known);
        Assert.Equal(ExecutableState.Encrypted, check.State);
        Assert.False(check.Ok);
    }

    [Fact]
    public void Another_game_or_region_is_not_known()
    {
        var check = GameCopy.Check(DataWith("PSP_GAME/SYSDIR/EBOOT_DECRYPTED.ELF", TestFiles.Elf([9, 9, 9])), Known);
        Assert.Equal(ExecutableState.Elf, check.State);
        Assert.False(check.KnownVersion);
    }

    [Fact]
    public void The_game_s_own_order_picks_the_executable()
    {
        var data = DataWith("PSP_GAME/SYSDIR/EBOOT_DECRYPTED.ELF", KnownElf);
        TestFiles.Write(Path.Combine(data, "EBOOT.BIN"), TestFiles.Elf([7]));
        Assert.EndsWith("EBOOT_DECRYPTED.ELF", GameCopy.Check(data, Known).Executable);
    }

    [Fact]
    public void Hash_lists_are_read_by_name()
    {
        var list = TestFiles.Write(Path.Combine(TestFiles.TempDir(), "h.sha256"),
            System.Text.Encoding.ASCII.GetBytes($"{new string('a', 64)}  generated_units.hpp\n\n{new string('B', 64)}  BOOT.BIN\n"));
        var byName = HashList.Load(list);
        Assert.Equal(new string('a', 64), byName["generated_units.hpp"]);
        Assert.Equal(new string('b', 64), byName["BOOT.BIN"]);
    }

    [Fact]
    public void Messages_follow_the_language()
    {
        var spanish = Strings.Get(new System.Globalization.CultureInfo("es-CL"), "GameMissing");
        var english = Strings.Get(new System.Globalization.CultureInfo("en-US"), "GameMissing");
        Assert.Contains("PSP_DATA", spanish);
        Assert.NotEqual(spanish, english);
        Assert.Equal(english, new BuilderException("GameMissing").Message);
    }

    [Fact]
    public void A_broken_param_sfo_has_no_version_instead_of_crashing()
    {
        var negativeTable = TestFiles.Sfo(("DISC_VERSION", "1.03"));
        BitConverter.TryWriteBytes(negativeTable.AsSpan(8), -50);
        Assert.Null(GameCopy.ReadDiscVersion(TestFiles.Write(Path.Combine(TestFiles.TempDir(), "PARAM.SFO"), negativeTable)));
        var negativeLength = TestFiles.Sfo(("DISC_VERSION", "1.03"));
        BitConverter.TryWriteBytes(negativeLength.AsSpan(20 + 4), -1);
        Assert.Null(GameCopy.ReadDiscVersion(TestFiles.Write(Path.Combine(TestFiles.TempDir(), "PARAM.SFO"), negativeLength)));
    }
}
