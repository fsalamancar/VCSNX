using Vcsnx.HdPack;

namespace Vcsnx.HdPack.Tests;

public class TextureKeyTests
{
    [Fact]
    public void A4x4ImageOfBytes0To63HasTheKeySharedWithTheConsoleTest()
    {
        var rgba = Enumerable.Range(0, 64).Select(i => (byte)i).ToArray();
        Assert.Equal(0x7b21ac69b86eb9d5UL, TextureKey.Of(rgba, 4, 4));
        Assert.Equal("7b21ac69b86eb9d5.png", TextureKey.FileName(0x7b21ac69b86eb9d5UL));
    }

    [Fact]
    public void WidthIsMixedBeforeHeight()
    {
        var rgba = Enumerable.Range(0, 128).Select(i => (byte)(i * 3)).ToArray();
        Assert.Equal(0x163a1f6ba6829169UL, TextureKey.Of(rgba, 8, 4));
        Assert.NotEqual(TextureKey.Of(rgba, 8, 4), TextureKey.Of(rgba, 4, 8));
    }
}
