using Vcsnx.HdPack;

namespace Vcsnx.HdPack.Tests;

public class HdMapTests
{
    static HdTexture Tile(int index, int size = 128) => new((ulong)index + 1, size, size,
        Enumerable.Range(0, size * size).SelectMany(p => new[] { (byte)index, (byte)(p % size), (byte)(p / size), (byte)255 }).ToArray(),
        "GTA3PSP.IMG", $"radar{index:D2}");

    [Theory]
    [InlineData("radar00", 0)]
    [InlineData("radar63", 63)]
    [InlineData("RADAR07", 7)]
    public void TilesAreNumberedByName(string name, int index) => Assert.Equal(index, HdMap.TileIndex(name));

    [Theory]
    [InlineData("radar64")]
    [InlineData("radar7")]
    [InlineData("radardisc")]
    [InlineData("car_radar00")]
    public void OtherNamesAreNotTiles(string name) => Assert.Null(HdMap.TileIndex(name));

    [Fact]
    public void SixtyFourTilesMakeOneMapRowByRow()
    {
        var map = HdMap.Assemble(Enumerable.Range(0, 64).Select(i => Tile(i)).Reverse().ToList())!;
        Assert.Equal((1024, 1024), (map.Width, map.Height));
        // Tile 9 is row 1, column 1: its first texel sits at (128, 128) and carries the tile's index in red.
        Assert.Equal(9, map.Rgba[(128 * 1024 + 128) * 4]);
        Assert.Equal(63, map.Rgba[((7 * 128) * 1024 + 7 * 128) * 4]);
    }

    [Fact]
    public void AnIncompleteSetIsNoMap() => Assert.Null(HdMap.Assemble(Enumerable.Range(0, 63).Select(i => Tile(i)).ToList()));

    [Fact]
    public void CuttingTheUpscaledMapGivesEachTileItsPart()
    {
        var map = HdMap.Assemble(Enumerable.Range(0, 64).Select(i => Tile(i)).ToList())!;
        var big = Resample.BoxDown(map.Rgba, 1024, 1024, 1); // stand-in for an upscale by 1
        var tiles = HdMap.Cut(big, 1024, 1024);
        Assert.Equal(64, tiles.Count);
        Assert.Equal(128 * 128 * 4, tiles[9].Length);
        Assert.Equal(9, tiles[9][0]);
        Assert.Equal(Tile(9).Rgba, tiles[9]);
    }
}
