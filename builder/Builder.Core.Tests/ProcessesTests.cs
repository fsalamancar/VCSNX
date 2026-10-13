namespace Vcsnx.Builder.Core.Tests;

public sealed class ProcessesTests
{
    [Fact]
    public async Task A_tool_that_cannot_start_is_a_player_message()
    {
        var missing = Path.Combine(TestFiles.TempDir(), "aarch64-none-elf-g++.exe"); // as when an antivirus removed it
        var error = await Assert.ThrowsAsync<BuilderException>(() => new SystemProcessRunner().RunAsync(missing, [], default));
        Assert.Equal("ToolBlocked", error.Key);
        Assert.Equal("aarch64-none-elf-g++.exe", error.Args[0]);
    }

    [Fact]
    public async Task Tools_get_their_temporary_folder_inside_the_program_folder()
    {
        // The player's own %TEMP% may have accents (C:\Users\José\...), which the devkitPro tools do not take.
        var tmp = Path.Combine(TestFiles.TempDir(), "trabajo", "tmp");
        var result = await new SystemProcessRunner(tmp).RunAsync("/bin/sh", ["-c", "printf '%s|%s|%s' \"$TMP\" \"$TEMP\" \"$TMPDIR\""], default);
        Assert.Equal($"{tmp}|{tmp}|{tmp}", result.Output);
        Assert.True(Directory.Exists(tmp));
    }

    [Fact]
    public async Task A_command_that_could_not_start_is_still_in_the_log()
    {
        var log = new StringWriter();
        var missing = Path.Combine(TestFiles.TempDir(), "ld.exe");
        await Assert.ThrowsAsync<BuilderException>(() => new LoggingRunner(new SystemProcessRunner(), log).RunAsync(missing, ["-o", "x"], default));
        Assert.Contains($"$ {missing} -o x", log.ToString());
        Assert.Contains("ld.exe", log.ToString().Split('\n')[1]);
    }
}
