using System.IO.Compression;
using System.Net;
using System.Security.Cryptography;
using System.Text;

namespace Vcsnx.Builder.Core.Tests;

public sealed class CorpusTests
{
    static string Sha(string text) => Convert.ToHexStringLower(SHA256.HashData(Encoding.UTF8.GetBytes(text)));

    static readonly Dictionary<string, string> Expected = new()
    {
        ["generated_unit_0000.cpp"] = Sha("unit 0"),
        ["generated_units.hpp"] = Sha("header"),
    };

    public static string MakeZip(string dir, Dictionary<string, string> files)
    {
        var zipPath = Path.Combine(dir, "pspr.zip");
        using var zip = ZipFile.Open(zipPath, ZipArchiveMode.Create);
        foreach (var (name, text) in files)
        {
            using var writer = new StreamWriter(zip.CreateEntry(name).Open());
            writer.Write(text);
        }
        return zipPath;
    }

    static string GoodZip(string dir) => MakeZip(dir, new()
    {
        [Corpus.ArchivePrefix + "generated_unit_0000.cpp"] = "unit 0",
        [Corpus.ArchivePrefix + "generated_units.hpp"] = "header",
        [$"PSPRecomp-{Corpus.Commit}/README.md"] = "not part of generated/",
    });

    [Fact]
    public void Extracting_the_archive_gives_exactly_the_expected_files()
    {
        var dir = TestFiles.TempDir();
        var corpus = new Corpus(Path.Combine(dir, "generated"), Expected);
        Assert.Equal(2, corpus.Problems().Count);
        corpus.ExtractFrom(GoodZip(dir));
        Assert.Empty(corpus.Problems());
        Assert.False(File.Exists(Path.Combine(dir, "generated", "README.md")));
    }

    [Fact]
    public void A_changed_file_is_a_problem()
    {
        var dir = TestFiles.TempDir();
        var corpus = new Corpus(Path.Combine(dir, "generated"), Expected);
        corpus.ExtractFrom(GoodZip(dir));
        File.WriteAllText(Path.Combine(dir, "generated", "generated_units.hpp"), "tampered");
        Assert.Equal(new[] { "generated_units.hpp" }, corpus.Problems());
    }

    sealed class Serve(HttpStatusCode status, byte[] body) : HttpMessageHandler
    {
        protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken ct) =>
            Task.FromResult(new HttpResponseMessage(status) { Content = new ByteArrayContent(body) });
    }

    [Fact]
    public async Task A_download_lands_whole_or_not_at_all()
    {
        var dir = TestFiles.TempDir();
        var target = Path.Combine(dir, "pspr.zip");
        await Corpus.DownloadAsync(new HttpClient(new Serve(HttpStatusCode.OK, [1, 2, 3])), Corpus.ArchiveUri, target, default);
        Assert.Equal(new byte[] { 1, 2, 3 }, File.ReadAllBytes(target));
        Assert.False(File.Exists(target + ".part"));
        var missing = Path.Combine(dir, "missing.zip");
        await Assert.ThrowsAsync<HttpRequestException>(() =>
            Corpus.DownloadAsync(new HttpClient(new Serve(HttpStatusCode.NotFound, [])), Corpus.ArchiveUri, missing, default));
        Assert.False(File.Exists(missing));
    }
}
