using System.Security.Cryptography;

namespace Vcsnx.Builder.Core;

/// <summary>A sha256sum-style list: "&lt;sha256&gt;  &lt;name&gt;" per line (builder/hashes).</summary>
public static class HashList
{
    public static IReadOnlyDictionary<string, string> Load(string path)
    {
        var byName = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var raw in File.ReadAllLines(path))
        {
            var line = raw.Trim();
            if (line.Length == 0 || line.StartsWith('#')) continue;
            if (line.Length < 67 || line.IndexOf("  ", StringComparison.Ordinal) != 64)
                throw new FormatException($"{path}: not a sha256sum line: '{raw}'");
            byName[line[66..]] = line[..64].ToLowerInvariant();
        }
        return byName;
    }

    public static string Sha256Of(string file)
    {
        using var stream = File.OpenRead(file);
        return Convert.ToHexStringLower(SHA256.HashData(stream));
    }
}
