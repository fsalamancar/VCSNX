namespace Vcsnx.Builder.Core;

public sealed record SdTarget(string Root, string Label);
public sealed record CopyReport(int Copied, int Skipped);

/// <summary>Puts the game on the Switch's SD card: sdmc:/switch/vcsnx/ with vcsnx.nro, vcs_game.so,
/// VCSNative.ini (only if the player has none, so their settings stay) and PSP_DATA.</summary>
public static class SdCard
{
    public static IReadOnlyList<SdTarget> Candidates() =>
        DriveInfo.GetDrives()
            .Where(d => d.DriveType == DriveType.Removable && d.IsReady && Directory.Exists(Path.Combine(d.RootDirectory.FullName, "switch")))
            .Select(d => new SdTarget(d.RootDirectory.FullName, string.IsNullOrEmpty(d.VolumeLabel) ? d.Name : $"{d.Name} ({d.VolumeLabel})"))
            .ToList();

    public static string GameFolder(string sdRoot) => Path.Combine(sdRoot, "switch", "vcsnx");

    public static CopyReport Install(string sdRoot, string nro, string module, string ini, string dataRoot, IProgress<string>? progress, CancellationToken ct)
    {
        var game = GameFolder(sdRoot);
        Directory.CreateDirectory(game);
        var copied = 0;
        var skipped = 0;

        void Copy(string from, string to)
        {
            ct.ThrowIfCancellationRequested();
            if (Same(from, to))
            {
                ++skipped;
                return;
            }
            Directory.CreateDirectory(Path.GetDirectoryName(to)!);
            progress?.Report(Path.GetRelativePath(sdRoot, to));
            File.Copy(from, to, overwrite: true);
            File.SetLastWriteTimeUtc(to, File.GetLastWriteTimeUtc(from));
            ++copied;
        }

        Copy(nro, Path.Combine(game, "vcsnx.nro"));
        Copy(module, Path.Combine(game, "vcs_game.so"));
        var sdIni = Path.Combine(game, "VCSNative.ini");
        if (!File.Exists(sdIni)) Copy(ini, sdIni);
        foreach (var file in Directory.EnumerateFiles(dataRoot, "*", SearchOption.AllDirectories).Order(StringComparer.Ordinal))
        {
            var name = Path.GetFileName(file);
            if (name.StartsWith("._", StringComparison.Ordinal) || name == ".DS_Store") continue;
            Copy(file, Path.Combine(game, "PSP_DATA", Path.GetRelativePath(dataRoot, file)));
        }
        return new CopyReport(copied, skipped);
    }

    /// <summary>FAT keeps write times to 2 s: same size and time is the same file; same size alone gets a hash check.</summary>
    public static bool Same(string a, string b)
    {
        if (!File.Exists(b)) return false;
        var fa = new FileInfo(a);
        var fb = new FileInfo(b);
        if (fa.Length != fb.Length) return false;
        if (Math.Abs((fa.LastWriteTimeUtc - fb.LastWriteTimeUtc).TotalSeconds) <= 2) return true;
        return HashList.Sha256Of(a) == HashList.Sha256Of(b);
    }
}
