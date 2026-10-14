using Vcsnx.HdPack;

namespace Vcsnx.HdPack.Tests;

public sealed class HdCatalogTests : IDisposable
{
    readonly string _game = Path.Combine(Path.GetTempPath(), "vcsnx-hd-" + Guid.NewGuid().ToString("N"));
    string RunData => Path.Combine(_game, "PSP_GAME", "USRDIR", "RUNDATA");

    public HdCatalogTests() => Directory.CreateDirectory(RunData);
    public void Dispose() => Directory.Delete(_game, recursive: true);

    static TexChunkBuilder.Tex Tex(string name, int size, int seed) =>
        TexChunkBuilder.Indexed(name, size, size, 4, 1, (x, y) => (x + y + seed) & 15, TexChunkBuilder.Palette(16));

    void WriteImage(string fileName, params byte[][] chunks)
    {
        var image = new byte[chunks.Length * 65536];
        for (var i = 0; i < chunks.Length; i++) chunks[i].CopyTo(image, i * 65536);
        File.WriteAllBytes(Path.Combine(RunData, fileName), image);
    }

    [Fact]
    public void ScreensAndModelTexturesAreReadAndKeyed()
    {
        File.WriteAllBytes(Path.Combine(RunData, "LOADSC0.XTX"), TexChunkBuilder.Build(Tex("screen", 64, 1)));
        WriteImage("GTA3PSP.IMG", TexChunkBuilder.Build(Tex("car_body", 32, 2)), TexChunkBuilder.Build(Tex("ped_face", 16, 3)));
        var scan = HdCatalog.Scan(HdCatalog.SourcesIn(_game));
        Assert.Equal(new[] { "screen", "car_body", "ped_face" }, scan.Textures.Select(t => t.Name));
        var face = scan.Textures[2];
        Assert.Equal(TextureKey.Of(face.Rgba, 16, 16), face.Key);
    }

    [Fact]
    public void LowercaseFileNamesAreFoundToo()
    {
        File.WriteAllBytes(Path.Combine(RunData, "loadsc0.xtx"), TexChunkBuilder.Build(Tex("screen", 64, 1)));
        WriteImage("gta3psp.img", TexChunkBuilder.Build(Tex("car_body", 32, 2)));
        Assert.Equal(2, HdCatalog.Scan(HdCatalog.SourcesIn(_game)).Textures.Count);
    }

    [Fact]
    public void TwoDTexturesTinyOnesAndDuplicatesAreSkippedAndCounted()
    {
        WriteImage("GTA3PSP.IMG",
            TexChunkBuilder.Build(Tex("HUD_money", 32, 1), Tex("car_body", 32, 2)),
            TexChunkBuilder.Build(Tex("car_copy", 32, 2), TexChunkBuilder.Indexed("dot", 2, 2, 4, 1, (_, _) => 0, TexChunkBuilder.Palette(16))));
        var scan = HdCatalog.Scan(HdCatalog.SourcesIn(_game));
        Assert.Equal(new[] { "car_body" }, scan.Textures.Select(t => t.Name));
        Assert.Equal(1, scan.Skipped2D);
        Assert.Equal(1, scan.Duplicates);
        Assert.Equal(1, scan.SkippedSmall);
    }

    [Fact]
    public void GarbageInAnArchiveIsSkippedNotFatal()
    {
        var bad = TexChunkBuilder.Build(Tex("car_body", 32, 2));
        bad[0x28] = 0xFF; bad[0x29] = 0xFF; // list head far outside the chunk
        WriteImage("GTA3PSP.IMG", bad, TexChunkBuilder.Build(Tex("ped_face", 16, 3)));
        Assert.Equal(new[] { "ped_face" }, HdCatalog.Scan(HdCatalog.SourcesIn(_game)).Textures.Select(t => t.Name));
    }
}
