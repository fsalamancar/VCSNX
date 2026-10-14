using Vcsnx.HdPack;

namespace Vcsnx.HdPack.Tests;

public class HdPrepTests
{
    [Theory]
    [InlineData(64, 64, 4)]
    [InlineData(32, 128, 2)]
    [InlineData(512, 256, 2)]
    [InlineData(1024, 512, 1)]
    public void TheScaleFollowsTheLargestSide(int w, int h, int scale) => Assert.Equal(scale, HdPrep.ScaleFor(w, h));

    [Fact]
    public void TheMarginIsEightTexelsOrAQuarterOfTheSide() => Assert.Equal((8, 2), HdPrep.MarginFor(64, 8));

    [Fact]
    public void HiddenTexelsTakeTheNearestVisibleColorAndStayHidden()
    {
        byte[] rgba = [255, 0, 0, 255, 9, 9, 9, 0, 1, 1, 1, 0, 9, 9, 9, 0, 0, 0, 255, 255]; // R . . . B
        var filled = HdPrep.FillHidden(rgba, 5, 1);
        Assert.Equal(new byte[] { 255, 0, 0, 0 }, filled.AsSpan(4, 4).ToArray());
        Assert.Equal(new byte[] { 0, 0, 255, 0 }, filled.AsSpan(12, 4).ToArray());
    }

    [Fact]
    public void BinaryAlphaMeansOnlyHiddenOrFull()
    {
        Assert.True(HdPrep.BinaryAlpha([1, 2, 3, 0, 4, 5, 6, 255]));
        Assert.False(HdPrep.BinaryAlpha([1, 2, 3, 128]));
    }
}
