using System.IO.Compression;
using System.Net;
using System.Security.Cryptography;
using Vcsnx.HdPack;

namespace Vcsnx.HdPack.Tests;

public sealed class RealEsrganTests : IDisposable
{
    readonly string _dir = Path.Combine(Path.GetTempPath(), "vcsnx-esr-" + Guid.NewGuid().ToString("N"));
    public void Dispose() { if (Directory.Exists(_dir)) Directory.Delete(_dir, recursive: true); }

    sealed class Serve(byte[] body) : HttpMessageHandler
    {
        public int Requests { get; private set; }
        protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken ct)
        {
            Requests++;
            return Task.FromResult(new HttpResponseMessage(HttpStatusCode.OK) { Content = new ByteArrayContent(body) });
        }
    }

    static byte[] Zip()
    {
        using var memory = new MemoryStream();
        using (var zip = new ZipArchive(memory, ZipArchiveMode.Create, leaveOpen: true))
        {
            using (var w = new StreamWriter(zip.CreateEntry(RealEsrgan.ExecutableName).Open())) w.Write("exe");
            using (var w = new StreamWriter(zip.CreateEntry("models/m.param").Open())) w.Write("model");
        }
        return memory.ToArray();
    }

    [Fact]
    public async Task AVerifiedDownloadIsUnpackedOnceAndReused()
    {
        var zip = Zip();
        var serve = new Serve(zip);
        var sha = Convert.ToHexStringLower(SHA256.HashData(zip));
        var uri = new Uri("https://example.invalid/r.zip");
        var exe = await RealEsrgan.EnsureAsync(_dir, new HttpClient(serve), TextWriter.Null, CancellationToken.None, uri, sha);
        Assert.Equal(Path.Combine(_dir, RealEsrgan.ExecutableName), exe);
        Assert.True(File.Exists(Path.Combine(_dir, "models", "m.param")));
        Assert.Equal(exe, await RealEsrgan.EnsureAsync(_dir, new HttpClient(serve), TextWriter.Null, CancellationToken.None, uri, sha));
        Assert.Equal(1, serve.Requests);
    }

    [Fact]
    public async Task AZipWithTheWrongHashIsNotUsed()
    {
        var log = new StringWriter();
        var exe = await RealEsrgan.EnsureAsync(_dir, new HttpClient(new Serve(Zip())), log, CancellationToken.None,
            new Uri("https://example.invalid/r.zip"), new string('0', 64));
        Assert.Null(exe);
        Assert.Contains("SHA-256", log.ToString());
        Assert.False(File.Exists(Path.Combine(_dir, RealEsrgan.ExecutableName)));
    }
}
