using Vcsnx.HdPack;

namespace Vcsnx.HdPack.Tests;

public class PspRasterTests
{
    [Theory] // values of ge_renderer.cpp swizzled_offset(byte_x, y, row_bytes)
    [InlineData(0, 0, 16, 0)]
    [InlineData(15, 7, 16, 127)]
    [InlineData(0, 8, 16, 128)]
    [InlineData(17, 9, 32, 401)]
    public void SwizzleMatchesTheConsole(int byteX, int y, int rowBytes, int expected) =>
        Assert.Equal(expected, PspRaster.SwizzledOffset(byteX, y, rowBytes));

    [Fact]
    public void ThePaletteFollowsEveryMipLevel()
    {
        // 32x64, 4 bits, 5 levels: 16x64 + 16x32 + 16x16 + 16x8 + 16x4 = 1984 bytes after the texels.
        Assert.Equal(0x90 + 1984, PspRaster.PaletteOffset(new RasterInfo(0x90, 32, 64, 4, 5, 16)));
        // One level: right after level 0.
        Assert.Equal(0x40 + 512 * 256, PspRaster.PaletteOffset(new RasterInfo(0x40, 512, 256, 8, 1, 512)));
    }

    [Fact]
    public void A4BitTextureDecodesLowNibbleFirstThroughItsPalette()
    {
        var palette = TexChunkBuilder.Palette(16);
        var chunk = TexChunkBuilder.Build(TexChunkBuilder.Indexed("t", 32, 8, 4, 2, (x, y) => (x + y) & 15, palette));
        var (_, raster) = Assert.Single(TexChunkReader.Textures(chunk));
        var info = PspRaster.Read(chunk, raster)!;
        var rgba = PspRaster.DecodeLevel0(chunk, info)!;
        Assert.Equal(32 * 8 * 4, rgba.Length);
        for (var y = 0; y < 8; y++)
        for (var x = 0; x < 32; x++)
            Assert.Equal(palette.AsSpan(((x + y) & 15) * 4, 4).ToArray(), rgba.AsSpan((y * 32 + x) * 4, 4).ToArray());
    }

    [Fact]
    public void An8BitTextureUsesA256ColorPalette()
    {
        var palette = TexChunkBuilder.Palette(256);
        var chunk = TexChunkBuilder.Build(TexChunkBuilder.Indexed("t", 16, 16, 8, 3, (x, y) => (x * 16 + y) & 255, palette));
        var (_, raster) = Assert.Single(TexChunkReader.Textures(chunk));
        var rgba = PspRaster.DecodeLevel0(chunk, PspRaster.Read(chunk, raster)!)!;
        Assert.Equal(palette.AsSpan(((5 * 16 + 3) & 255) * 4, 4).ToArray(), rgba.AsSpan((3 * 16 + 5) * 4, 4).ToArray());
    }

    [Fact]
    public void OtherDepthsAndRastersOutsideTheChunkAreRejected()
    {
        var chunk = TexChunkBuilder.Build(TexChunkBuilder.Indexed("t", 16, 16, 8, 1, (_, _) => 0, TexChunkBuilder.Palette(256)));
        var (_, raster) = Assert.Single(TexChunkReader.Textures(chunk));
        chunk[raster + 12] = 16;
        Assert.Null(PspRaster.Read(chunk, raster));
        Assert.Null(PspRaster.Read(chunk, chunk.Length - 8));
    }
}
