namespace Vcsnx.Builder.Core;

public static class Eta
{
    /// <summary>Time left from the wall time the units really compiled so far took (cached units finish at once).</summary>
    public static TimeSpan? Remaining(int done, int total, int cached, TimeSpan elapsed)
    {
        if (done >= total) return TimeSpan.Zero;
        var compiled = done - cached;
        return compiled <= 0 ? null : elapsed / compiled * (total - done);
    }
}
