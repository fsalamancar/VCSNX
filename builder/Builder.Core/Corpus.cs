using System.IO.Compression;

namespace Vcsnx.Builder.Core;

/// <summary>The game's translated code, as PSPRecomp publishes it: downloaded on the player's PC, checked
/// file by file against builder/hashes/generated.sha256, never shipped with the Builder.</summary>
public sealed class Corpus(string generatedDir, IReadOnlyDictionary<string, string> expected)
{
    public const string Commit = "f6e7d415c7f447b934cc3865a31eb725f353d659";
    public static readonly Uri ArchiveUri = new($"https://codeload.github.com/jessicanataliagta/PSPRecomp/zip/{Commit}");
    public const string ArchivePrefix = $"PSPRecomp-{Commit}/profiles/vcs/generated/";

    public string GeneratedDir => generatedDir;

    /// <summary>The expected files that are missing or differ, by name.</summary>
    public IReadOnlyList<string> Problems() =>
        expected.Where(kv =>
            {
                var file = Path.Combine(generatedDir, kv.Key);
                return !File.Exists(file) || HashList.Sha256Of(file) != kv.Value;
            })
            .Select(kv => kv.Key).Order(StringComparer.Ordinal).ToList();

    public void ExtractFrom(string zipPath)
    {
        Directory.CreateDirectory(generatedDir);
        using var zip = ZipFile.OpenRead(zipPath);
        foreach (var entry in zip.Entries)
        {
            if (!entry.FullName.StartsWith(ArchivePrefix, StringComparison.Ordinal)) continue;
            var name = entry.FullName[ArchivePrefix.Length..];
            if (name.Length == 0 || name.Contains('/') || !expected.ContainsKey(name)) continue;
            entry.ExtractToFile(Path.Combine(generatedDir, name), overwrite: true);
        }
    }

    public static async Task DownloadAsync(HttpClient http, Uri uri, string destination, CancellationToken ct)
    {
        var partial = destination + ".part";
        using (var response = await http.GetAsync(uri, HttpCompletionOption.ResponseHeadersRead, ct))
        {
            response.EnsureSuccessStatusCode();
            await using var file = File.Create(partial);
            await response.Content.CopyToAsync(file, ct);
        }
        File.Move(partial, destination, overwrite: true);
    }
}
