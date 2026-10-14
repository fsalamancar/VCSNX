using Vcsnx.HdPack;

namespace Vcsnx.HdPack.Tests;

public class FidelityTests
{
    [Fact]
    public void TheSameImageIsInfinitelyFaithful()
    {
        byte[] rgba = [10, 20, 30, 255, 40, 50, 60, 255];
        Assert.True(Fidelity.Psnr(rgba, rgba) >= Fidelity.MinPsnr);
    }

    [Fact]
    public void ColorsUnderFullyTransparentTexelsDoNotCount()
    {
        byte[] original = [10, 20, 30, 255, 0, 0, 0, 0];
        byte[] candidate = [10, 20, 30, 255, 200, 200, 200, 0];
        Assert.Equal(double.PositiveInfinity, Fidelity.Psnr(original, candidate));
    }

    [Fact]
    public void InvertedColorsFail()
    {
        byte[] original = [10, 20, 30, 255, 200, 100, 50, 255];
        var inverted = original.Select((v, i) => i % 4 == 3 ? v : (byte)(255 - v)).ToArray();
        Assert.True(Fidelity.Psnr(original, inverted) < Fidelity.MinPsnr);
    }
}
