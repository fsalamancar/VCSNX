using SkiaSharp;

namespace Vcsnx.Builder.Core.Tests;

public sealed class IconTests
{
    [Fact]
    public void The_icon_is_the_centre_square_at_256_pixels()
    {
        var dir = TestFiles.TempDir();
        var image = Path.Combine(dir, "art.png");
        using (var bitmap = new SKBitmap(400, 200))
        {
            for (var x = 0; x < 400; ++x)
                for (var y = 0; y < 200; ++y)
                    bitmap.SetPixel(x, y, x < 200 ? SKColors.Red : SKColors.Blue);
            using var data = bitmap.Encode(SKEncodedImageFormat.Png, 100);
            File.WriteAllBytes(image, data.ToArray());
        }
        var jpg = Path.Combine(dir, "icon.jpg");
        IconMaker.Make(image, jpg);
        using var icon = SKBitmap.Decode(jpg);
        Assert.Equal((256, 256), (icon.Width, icon.Height));
        Assert.True(icon.GetPixel(64, 128).Red > 200 && icon.GetPixel(64, 128).Blue < 60, "left half of the centre square is red");
        Assert.True(icon.GetPixel(192, 128).Blue > 200 && icon.GetPixel(192, 128).Red < 60, "right half is blue");
    }

    [Fact]
    public void An_unreadable_image_is_a_player_message()
    {
        var dir = TestFiles.TempDir();
        var bad = TestFiles.Write(Path.Combine(dir, "bad.png"), [1, 2, 3]);
        Assert.Equal("IconUnreadable", Assert.Throws<BuilderException>(() => IconMaker.Make(bad, Path.Combine(dir, "i.jpg"))).Key);
    }

    [Fact]
    public void Replacing_the_nro_icon_keeps_the_program_and_the_nacp()
    {
        var dir = TestFiles.TempDir();
        var original = TestFiles.Nro("old icon"u8.ToArray(), "the nacp"u8.ToArray());
        var nro = TestFiles.Write(Path.Combine(dir, "vcsnx.nro"), original);
        var jpg = TestFiles.Write(Path.Combine(dir, "icon.jpg"), "a much longer new icon"u8.ToArray());
        var output = Path.Combine(dir, "out.nro");
        NroIcon.Replace(nro, jpg, output);
        var result = File.ReadAllBytes(output);
        Assert.Equal(original.AsSpan(0, 0x100).ToArray(), result.AsSpan(0, 0x100).ToArray());
        Assert.Equal("a much longer new icon"u8.ToArray(), NroIcon.ReadIcon(output));
        Assert.EndsWith("the nacp", System.Text.Encoding.ASCII.GetString(result));
        Assert.Throws<InvalidDataException>(() => NroIcon.Replace(jpg, jpg, output));
    }
}
