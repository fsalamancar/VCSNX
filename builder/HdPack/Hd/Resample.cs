namespace Vcsnx.HdPack;

/// <summary>RGBA8 resampling for the HD step: box reduction (the fidelity check, and 4x to 2x), Lanczos-3 with
/// wrap-around (classic upscale; textures tile), and the toroidal margin the upscaler adds around each texture.</summary>
public static class Resample
{
    public static byte[] BoxDown(ReadOnlySpan<byte> rgba, int width, int height, int factor)
    {
        int w = width / factor, h = height / factor, n = factor * factor;
        var output = new byte[w * h * 4];
        for (var y = 0; y < h; y++)
        for (var x = 0; x < w; x++)
        for (var c = 0; c < 4; c++)
        {
            var sum = 0;
            for (var dy = 0; dy < factor; dy++)
            for (var dx = 0; dx < factor; dx++)
                sum += rgba[((y * factor + dy) * width + x * factor + dx) * 4 + c];
            output[(y * w + x) * 4 + c] = (byte)((sum + n / 2) / n);
        }
        return output;
    }

    public static byte[] Lanczos3Wrap(ReadOnlySpan<byte> rgba, int width, int height, int scale)
    {
        int w = width * scale, h = height * scale;
        var horizontal = new float[w * height * 4];
        for (var y = 0; y < height; y++)
        for (var x = 0; x < w; x++)
            SampleRow(x, scale, width, rgba.Slice(y * width * 4, width * 4), horizontal.AsSpan((y * w + x) * 4, 4));
        var output = new byte[w * h * 4];
        Span<float> pixel = stackalloc float[4];
        for (var y = 0; y < h; y++)
        for (var x = 0; x < w; x++)
        {
            SampleColumn(y, scale, height, horizontal, w, x, pixel);
            for (var c = 0; c < 4; c++) output[(y * w + x) * 4 + c] = (byte)Math.Clamp((int)MathF.Round(pixel[c]), 0, 255);
        }
        return output;
    }

    static void SampleRow(int outputIndex, int scale, int size, ReadOnlySpan<byte> row, Span<float> result)
    {
        var center = (outputIndex + 0.5f) / scale - 0.5f;
        var first = (int)MathF.Floor(center) - 2;
        result.Clear();
        var total = 0f;
        for (var k = 0; k < 6; k++)
        {
            var weight = Kernel(center - (first + k));
            var source = ((first + k) % size + size) % size;
            for (var c = 0; c < 4; c++) result[c] += weight * row[source * 4 + c];
            total += weight;
        }
        for (var c = 0; c < 4; c++) result[c] /= total;
    }

    static void SampleColumn(int outputIndex, int scale, int size, float[] rows, int rowWidth, int x, Span<float> result)
    {
        var center = (outputIndex + 0.5f) / scale - 0.5f;
        var first = (int)MathF.Floor(center) - 2;
        result.Clear();
        var total = 0f;
        for (var k = 0; k < 6; k++)
        {
            var weight = Kernel(center - (first + k));
            var row = ((first + k) % size + size) % size;
            for (var c = 0; c < 4; c++) result[c] += weight * rows[(row * rowWidth + x) * 4 + c];
            total += weight;
        }
        for (var c = 0; c < 4; c++) result[c] /= total;
    }

    static float Kernel(float d)
    {
        d = MathF.Abs(d);
        if (d < 1e-6f) return 1f;
        if (d >= 3f) return 0f;
        var pd = MathF.PI * d;
        return 3f * MathF.Sin(pd) * MathF.Sin(pd / 3f) / (pd * pd);
    }

    public static byte[] WrapPad(ReadOnlySpan<byte> rgba, int width, int height, int marginX, int marginY)
    {
        int w = width + 2 * marginX, h = height + 2 * marginY;
        var output = new byte[w * h * 4];
        for (var y = 0; y < h; y++)
        for (var x = 0; x < w; x++)
        {
            int sx = ((x - marginX) % width + width) % width, sy = ((y - marginY) % height + height) % height;
            rgba.Slice((sy * width + sx) * 4, 4).CopyTo(output.AsSpan((y * w + x) * 4, 4));
        }
        return output;
    }

    public static byte[] Crop(ReadOnlySpan<byte> rgba, int width, int x, int y, int cropWidth, int cropHeight)
    {
        var output = new byte[cropWidth * cropHeight * 4];
        for (var row = 0; row < cropHeight; row++)
            rgba.Slice(((y + row) * width + x) * 4, cropWidth * 4).CopyTo(output.AsSpan(row * cropWidth * 4));
        return output;
    }
}
