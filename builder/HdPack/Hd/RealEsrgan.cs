using System.IO.Compression;
using System.Runtime.InteropServices;

namespace Vcsnx.HdPack;

/// <summary>Real-ESRGAN ncnn-vulkan v0.2.5.0 (BSD-3; ncnn BSD-3), downloaded on first use and checked by SHA-256
/// like the translated code (spec §4.4); never inside the Builder's zip.</summary>
public static class RealEsrgan
{
    static readonly Uri WindowsZip = new("https://github.com/xinntao/Real-ESRGAN/releases/download/v0.2.5.0/realesrgan-ncnn-vulkan-20220424-windows.zip");
    const string WindowsSha256 = "abc02804e17982a3be33675e4d471e91ea374e65b70167abc09e31acb412802d";
    static readonly Uri MacZip = new("https://github.com/xinntao/Real-ESRGAN/releases/download/v0.2.5.0/realesrgan-ncnn-vulkan-20220424-macos.zip");
    const string MacSha256 = "e0ad05580abfeb25f8d8fb55aaf7bedf552c375b5b4d9bd3c8d59764d2cc333a";

    public static string ExecutableName => OperatingSystem.IsWindows() ? "realesrgan-ncnn-vulkan.exe" : "realesrgan-ncnn-vulkan";

    public static async Task<string?> EnsureAsync(string dir, HttpClient http, TextWriter log, CancellationToken ct, Uri? zip = null, string? sha256 = null)
    {
        zip ??= OperatingSystem.IsWindows() ? WindowsZip : MacZip;
        sha256 ??= OperatingSystem.IsWindows() ? WindowsSha256 : MacSha256;
        var exe = Path.Combine(dir, ExecutableName);
        var stamp = Path.Combine(dir, "verified.sha256");
        if (File.Exists(exe) && File.Exists(stamp) && File.ReadAllText(stamp).Trim() == sha256) return exe;
        Directory.CreateDirectory(dir);
        var download = Path.Combine(dir, "download.zip");
        try
        {
            await Corpus.DownloadAsync(http, zip, download, ct);
            if (HashList.Sha256Of(download) != sha256)
            {
                log.WriteLine($"Real-ESRGAN: SHA-256 of {zip} does not match; HD textures use Lanczos-3 only");
                return null;
            }
            ZipFile.ExtractToDirectory(download, dir, overwriteFiles: true);
            if (!OperatingSystem.IsWindows()) File.SetUnixFileMode(exe, File.GetUnixFileMode(exe) | UnixFileMode.UserExecute);
            await File.WriteAllTextAsync(stamp, sha256, ct);
            return exe;
        }
        catch (Exception e) when (e is HttpRequestException or IOException or InvalidDataException or UnauthorizedAccessException)
        {
            log.WriteLine($"Real-ESRGAN: {e.Message}; HD textures use Lanczos-3 only");
            return null;
        }
        finally
        {
            if (File.Exists(download)) File.Delete(download);
        }
    }
}
