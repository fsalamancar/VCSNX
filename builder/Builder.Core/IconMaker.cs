using SkiaSharp;

namespace Vcsnx.Builder.Core;

/// <summary>The HOME-menu icon from the player's own image: the centre square, 256x256, JPEG.</summary>
public static class IconMaker
{
    public const int Size = 256;

    public static void Make(string image, string jpg)
    {
        using var source = SKBitmap.Decode(image) ?? throw new BuilderException("IconUnreadable");
        var side = Math.Min(source.Width, source.Height);
        var crop = SKRect.Create((source.Width - side) / 2f, (source.Height - side) / 2f, side, side);
        using var square = new SKBitmap(Size, Size);
        using (var canvas = new SKCanvas(square))
        using (var picture = SKImage.FromBitmap(source))
        {
            canvas.Clear(SKColors.Black);
            canvas.DrawImage(picture, crop, SKRect.Create(0, 0, Size, Size), new SKSamplingOptions(SKCubicResampler.Mitchell), null);
        }
        using var data = square.Encode(SKEncodedImageFormat.Jpeg, 92);
        using var file = File.Create(jpg);
        data.SaveTo(file);
    }
}
