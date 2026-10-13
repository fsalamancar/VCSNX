using System.Text;

namespace Vcsnx.Builder.Core;

public enum ExecutableState { Missing, Encrypted, Elf }

public sealed record GameCheck(string? DataRoot, string? Executable, ExecutableState State, bool KnownVersion, string? DiscVersion)
{
    public bool Ok => State == ExecutableState.Elf && KnownVersion;
}

/// <summary>The player's copy of the game: where its PSP_DATA is and whether its executable is the one VCSNX runs.</summary>
public static class GameCopy
{
    /// <summary>The game's own list and order (vendor profiles/vcs/host/vcs_bootstrap_paths.cpp): the console uses the first that exists.</summary>
    public static readonly string[] Candidates =
    [
        "PSP_GAME/SYSDIR/EBOOT_DECRYPTED.ELF", "EBOOT_DECRYPTED.ELF", "PSP_GAME/SYSDIR/EBOOT_DECRYPTED.BIN", "EBOOT_DECRYPTED.BIN",
        "PSP_GAME/SYSDIR/BOOT.BIN", "BOOT.BIN", "PSP_GAME/SYSDIR/EBOOT.BIN", "EBOOT.BIN",
    ];

    static readonly byte[] ElfMagic = [0x7F, (byte)'E', (byte)'L', (byte)'F'];

    /// <summary>PSP_DATA for what the player picked: PSP_DATA itself, the folder that holds it, or PSP_GAME inside it.</summary>
    public static string? FindDataRoot(string picked)
    {
        if (string.IsNullOrWhiteSpace(picked) || !Directory.Exists(picked)) return null;
        var full = Path.GetFullPath(picked).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
        string[] tries = [full, Path.Combine(full, "PSP_DATA"), Path.GetDirectoryName(full) ?? full];
        return tries.FirstOrDefault(dir => Directory.Exists(Path.Combine(dir, "PSP_GAME")) || Candidates.Any(c => File.Exists(Path.Combine(dir, c))));
    }

    public static GameCheck Check(string picked, IReadOnlySet<string> knownSha256)
    {
        var root = FindDataRoot(picked);
        if (root is null) return new GameCheck(null, null, ExecutableState.Missing, false, null);
        var version = ReadDiscVersion(Path.Combine(root, "PSP_GAME", "PARAM.SFO"));
        var executable = Candidates.Select(c => Path.Combine(root, c)).FirstOrDefault(File.Exists);
        if (executable is null) return new GameCheck(root, null, ExecutableState.Missing, false, version);
        var state = Classify(executable);
        var known = state == ExecutableState.Elf && knownSha256.Contains(HashList.Sha256Of(executable));
        return new GameCheck(root, executable, state, known, version);
    }

    public static ExecutableState Classify(string file)
    {
        Span<byte> head = stackalloc byte[4];
        using var stream = File.OpenRead(file);
        return stream.ReadAtLeast(head, 4, throwOnEndOfStream: false) == 4 && head.SequenceEqual(ElfMagic)
            ? ExecutableState.Elf
            : ExecutableState.Encrypted;
    }

    /// <summary>DISC_VERSION from a PSP PARAM.SFO ("1.03"), or null.</summary>
    public static string? ReadDiscVersion(string paramSfo)
    {
        if (!File.Exists(paramSfo)) return null;
        var data = File.ReadAllBytes(paramSfo);
        if (data.Length < 20 || !data.AsSpan(0, 4).SequenceEqual("\0PSF"u8)) return null;
        var keyTable = BitConverter.ToInt32(data, 8);
        var dataTable = BitConverter.ToInt32(data, 12);
        var count = BitConverter.ToInt32(data, 16);
        for (var i = 0; i < count; ++i)
        {
            var entry = 20 + i * 16;
            if (entry + 16 > data.Length) return null;
            var keyStart = keyTable + BitConverter.ToUInt16(data, entry);
            var length = BitConverter.ToInt32(data, entry + 4);
            var valueStart = dataTable + BitConverter.ToInt32(data, entry + 12);
            if (keyStart < 0 || keyStart >= data.Length || valueStart < 0 || length < 0 || (long)valueStart + length > data.Length) return null;
            var keyEnd = Array.IndexOf(data, (byte)0, keyStart);
            if (keyEnd < 0) return null;
            if (Encoding.ASCII.GetString(data, keyStart, keyEnd - keyStart) == "DISC_VERSION")
                return Encoding.UTF8.GetString(data, valueStart, length).TrimEnd('\0');
        }
        return null;
    }
}
