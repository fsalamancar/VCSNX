namespace Vcsnx.HdPack;

/// <summary>Style A's check (spec §4.3): the upscale, reduced back, against the original. RGB counts only where the
/// original texel is visible (alpha above 0); alpha counts everywhere. Same 32 dB bar as the console's upscaler.</summary>
public static class Fidelity
{
    public const double MinPsnr = 32.0;

    public static double Psnr(ReadOnlySpan<byte> original, ReadOnlySpan<byte> candidate)
    {
        double squared = 0;
        long samples = 0;
        for (var i = 0; i + 3 < original.Length; i += 4)
        {
            if (original[i + 3] != 0)
                for (var c = 0; c < 3; c++)
                {
                    double d = original[i + c] - candidate[i + c];
                    squared += d * d;
                    samples++;
                }
            double da = original[i + 3] - candidate[i + 3];
            squared += da * da;
            samples++;
        }
        if (squared == 0) return double.PositiveInfinity;
        return 10 * Math.Log10(255.0 * 255.0 / (squared / samples));
    }
}
