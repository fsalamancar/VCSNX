using Vcsnx.HdPack;

namespace Vcsnx.HdPack.Tests;

public class PngCodecTests
{
    // 3x4 RGB, rows filtered Sub, Up, Average, Paeth (made with Python's zlib for this test; no game data).
    const string FilteredRgb = "iVBORw0KGgoAAAANSUhEUgAAAAMAAAAECAIAAADETxJQAAAAJElEQVR4nGNkOMGg8YgRiJi4GIwgiJk/xVvyoxQQsYAFGIEIAKPsBvXlL9mqAAAAAElFTkSuQmCC";

    [Fact]
    public void RgbaSurvivesARoundTrip()
    {
        var rgba = Enumerable.Range(0, 3 * 2 * 4).Select(i => (byte)(i * 11)).ToArray();
        var (w, h, back) = PngCodec.Decode(PngCodec.EncodeRgba(rgba, 3, 2));
        Assert.Equal((3, 2), (w, h));
        Assert.Equal(rgba, back);
    }

    [Fact]
    public void AnRgbPngWithEveryFilterDecodesToOpaqueRgba()
    {
        var (w, h, rgba) = PngCodec.Decode(Convert.FromBase64String(FilteredRgb));
        Assert.Equal((3, 4), (w, h));
        int[][] expected = [[0, 200, 0], [40, 170, 1], [80, 140, 2], [10, 200, 50], [50, 170, 51], [90, 140, 52],
                            [20, 200, 100], [60, 170, 101], [100, 140, 102], [30, 200, 150], [70, 170, 151], [110, 140, 152]];
        for (var i = 0; i < expected.Length; i++)
            Assert.Equal(expected[i].Select(v => (byte)v).Append((byte)255).ToArray(), rgba.AsSpan(i * 4, 4).ToArray());
    }

    [Fact]
    public void SomethingElseIsRejected() =>
        Assert.Throws<InvalidDataException>(() => PngCodec.Decode([1, 2, 3, 4, 5, 6, 7, 8, 9]));
}
