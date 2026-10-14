namespace Vcsnx.HdPack;

/// <summary>What the upscaler does to a texture before and after the AI (spec §4.2).</summary>
public static class HdPrep
{
    public static int ScaleFor(int width, int height) => Math.Max(width, height) switch { <= 64 => 4, <= 512 => 2, _ => 1 };

    public static (int X, int Y) MarginFor(int width, int height) => (Math.Min(8, width / 4), Math.Min(8, height / 4));

    /// <summary>Every texel with alpha 0 takes the RGB of the nearest visible texel (breadth-first, wrapping); its
    /// alpha stays 0. An image with nothing visible is returned unchanged.</summary>
    public static byte[] FillHidden(ReadOnlySpan<byte> rgba, int width, int height)
    {
        var output = rgba.ToArray();
        var queue = new Queue<int>();
        var done = new bool[width * height];
        for (var i = 0; i < width * height; i++)
            if (rgba[i * 4 + 3] != 0) { done[i] = true; queue.Enqueue(i); }
        while (queue.Count > 0)
        {
            var i = queue.Dequeue();
            int x = i % width, y = i / width;
            foreach (var (nx, ny) in new[] { (x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1) })
            {
                var n = ((ny + height) % height) * width + (nx + width) % width;
                if (done[n]) continue;
                done[n] = true;
                output.AsSpan(i * 4, 3).CopyTo(output.AsSpan(n * 4, 3));
                queue.Enqueue(n);
            }
        }
        return output;
    }

    public static bool BinaryAlpha(ReadOnlySpan<byte> rgba)
    {
        for (var i = 3; i < rgba.Length; i += 4)
            if (rgba[i] is not (0 or 255)) return false;
        return true;
    }
}
