using Vcsnx.HdPack;

namespace Vcsnx.HdPack.Tests;

public class ResampleTests
{
    static byte[] Image(int w, int h, Func<int, int, int, byte> value) =>
        Enumerable.Range(0, w * h * 4).Select(i => value(i / 4 % w, i / 4 / w, i % 4)).ToArray();

    [Fact]
    public void BoxDownAveragesEachBlock()
    {
        var rgba = Image(2, 2, (x, y, c) => (byte)(c == 3 ? 255 : (x + 2 * y) * 10)); // 0, 10, 20, 30
        Assert.Equal(new byte[] { 15, 15, 15, 255 }, Resample.BoxDown(rgba, 2, 2, 2));
    }

    [Fact]
    public void LanczosKeepsAFlatImageFlatAndScalesTheSize()
    {
        var flat = Image(8, 4, (_, _, c) => (byte)(c == 3 ? 255 : 77));
        var up = Resample.Lanczos3Wrap(flat, 8, 4, 4);
        Assert.Equal(32 * 16 * 4, up.Length);
        Assert.All(up.Where((_, i) => i % 4 != 3), v => Assert.Equal(77, v));
    }

    [Fact]
    public void LanczosWrapsAroundSoATileStaysATile()
    {
        var tile = Image(8, 8, (x, y, c) => (byte)(c == 3 ? 255 : (x * 31 + y * 7 + c * 50) & 255));
        var rolled = Image(8, 8, (x, y, c) => tile[(y * 8 + (x + 7) % 8) * 4 + c]); // shifted right by one texel
        var up = Resample.Lanczos3Wrap(tile, 8, 8, 2);
        var upRolled = Resample.Lanczos3Wrap(rolled, 8, 8, 2);
        for (var y = 0; y < 16; y++)
        for (var x = 0; x < 16; x++)
        for (var c = 0; c < 4; c++)
            Assert.Equal(up[(y * 16 + (x + 14) % 16) * 4 + c], upRolled[(y * 16 + x) * 4 + c]);
    }

    [Fact]
    public void WrapPadCopiesTheOppositeSideAndCropUndoesIt()
    {
        var rgba = Image(4, 4, (x, y, c) => (byte)(x * 16 + y * 4 + c));
        var padded = Resample.WrapPad(rgba, 4, 4, 1, 2);
        Assert.Equal(6 * 8 * 4, padded.Length);
        Assert.Equal(rgba.AsSpan((2 * 4 + 3) * 4, 4).ToArray(), padded.AsSpan((4 * 6 + 0) * 4, 4).ToArray()); // left margin = last column
        Assert.Equal(rgba, Resample.Crop(padded, 6, 1, 2, 4, 4));
    }
}
