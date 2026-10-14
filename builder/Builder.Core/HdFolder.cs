using System.Text.RegularExpressions;

namespace Vcsnx.Builder.Core;

/// <summary>Where the HD copies are in the folder the player picked: the textures' folder, and the game's movies
/// (LOGO.mp4, TITLES.mp4) found in it.</summary>
public sealed record HdSource(string? Textures, IReadOnlyList<string> Movies);

public sealed record HdCopy(CopyReport Textures, CopyReport Movies);

/// <summary>HD textures and movies the player brings (the author, 2026-10-13: the Builder does not make them): an
/// optional folder whose &lt;key&gt;.png files go to switch\vcsnx\Textures and whose movies go to switch\vcsnx\HD with
/// the game. Without one, both are left as they are.</summary>
public static partial class HdFolder
{
    /// <summary>A replacement as the runtime looks it up: 16 hex digits of the texture's key, then .png.</summary>
    [GeneratedRegex("^[0-9a-f]{16}\\.png$", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant)]
    private static partial Regex TextureName();

    /// <summary>The game's movies (RUNDATA\PSP\MOVIES\*.PMF); the console plays HD\&lt;name&gt;.mp4 in their place.</summary>
    public static readonly IReadOnlyList<string> MovieNames = ["LOGO", "TITLES"];

    public static bool IsTexture(string fileName) => TextureName().IsMatch(fileName);

    /// <summary>The folder picked, its Textures, or its switch\vcsnx\Textures (a copy of an SD card, like the author's
    /// zip) for the textures; for each movie, the first of the textures' folder, the folder picked, its HD and its
    /// switch\vcsnx\HD that has it. Null when there is neither a texture nor a movie.</summary>
    public static HdSource? Find(string chosen)
    {
        var textures = new[] { chosen, Path.Combine(chosen, "Textures"), Path.Combine(chosen, "switch", "vcsnx", "Textures") }
            .FirstOrDefault(dir => Directory.Exists(dir) && Directory.EnumerateFiles(dir).Any(f => IsTexture(Path.GetFileName(f))));
        var places = new[] { textures, chosen, Path.Combine(chosen, "HD"), Path.Combine(chosen, "switch", "vcsnx", "HD") }
            .OfType<string>().Where(Directory.Exists).ToList();
        var movies = MovieNames
            .Select(name => places.SelectMany(dir => Directory.EnumerateFiles(dir))
                .FirstOrDefault(f => string.Equals(Path.GetFileName(f), name + ".mp4", StringComparison.OrdinalIgnoreCase)))
            .OfType<string>().ToList();
        return textures is null && movies.Count == 0 ? null : new HdSource(textures, movies);
    }

    /// <summary>Copies the textures into <paramref name="gameFolder"/>\Textures and the movies into its HD (as
    /// LOGO.mp4 and TITLES.mp4), the ones already there and identical skipped (SdCard.Same).</summary>
    public static HdCopy CopyTo(HdSource source, string gameFolder, CancellationToken ct)
    {
        var textures = source.Textures is { } dir
            ? CopyFiles(Directory.EnumerateFiles(dir).Where(f => IsTexture(Path.GetFileName(f))).Order(StringComparer.Ordinal)
                .Select(f => (f, Path.GetFileName(f))), Path.Combine(gameFolder, "Textures"), ct)
            : new CopyReport(0, 0);
        var movies = CopyFiles(source.Movies.Select(f => (f, Canonical(Path.GetFileName(f)))), Path.Combine(gameFolder, "HD"), ct);
        return new HdCopy(textures, movies);
    }

    /// <summary>Copies <paramref name="textures"/> into <paramref name="gameFolder"/>\Textures, replacing a different
    /// file of the same name (the kit's menu pictures, after the player's HD folder).</summary>
    public static CopyReport CopyTextures(IEnumerable<string> textures, string gameFolder, CancellationToken ct) =>
        CopyFiles(textures.Select(f => (f, Path.GetFileName(f))), Path.Combine(gameFolder, "Textures"), ct);

    static string Canonical(string movieFile) =>
        MovieNames.First(name => string.Equals(movieFile, name + ".mp4", StringComparison.OrdinalIgnoreCase)) + ".mp4";

    static CopyReport CopyFiles(IEnumerable<(string From, string Name)> files, string target, CancellationToken ct)
    {
        var copied = 0;
        var skipped = 0;
        foreach (var (from, name) in files)
        {
            ct.ThrowIfCancellationRequested();
            Directory.CreateDirectory(target);
            var to = Path.Combine(target, name);
            if (SdCard.Same(from, to))
            {
                ++skipped;
                continue;
            }
            File.Copy(from, to, overwrite: true);
            File.SetLastWriteTimeUtc(to, File.GetLastWriteTimeUtc(from));
            ++copied;
        }
        return new CopyReport(copied, skipped);
    }
}
