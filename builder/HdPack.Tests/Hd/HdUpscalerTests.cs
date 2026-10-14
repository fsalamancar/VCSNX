using Vcsnx.HdPack;

namespace Vcsnx.HdPack.Tests;

public sealed class HdUpscalerTests : IDisposable
{
    readonly string _work = Path.Combine(Path.GetTempPath(), "vcsnx-up-" + Guid.NewGuid().ToString("N"));
    public void Dispose() { if (Directory.Exists(_work)) Directory.Delete(_work, recursive: true); }

    static HdTexture Texture(int size, bool cutout = false) => new(1, size, size,
        Enumerable.Range(0, size * size).SelectMany(i => new[] { (byte)(i * 7), (byte)(i * 3), (byte)i,
            (byte)(cutout && i % 3 == 0 ? 0 : 255) }).ToArray(), "test", "t");

    HdUpscaler Upscaler(IProcessRunner runner, params string[] models) => new(runner, "esrgan", models, _work);

    [Fact]
    public async Task AFaithfulFirstModelIsKeptAtFourAndTwoTimes()
    {
        var runner = new FakeEsrgan(FakeEsrgan.Mode.Faithful);
        var outcomes = await Upscaler(runner, "general", "anime").UpscaleAsync([Texture(16), Texture(128)], CancellationToken.None);
        Assert.All(outcomes, o => Assert.Equal("general", o.Method));
        Assert.Equal((64, 64), (outcomes[0].Width, outcomes[0].Height));
        Assert.Equal((256, 256), (outcomes[1].Width, outcomes[1].Height));
        var models = Path.Combine(Path.GetDirectoryName(Path.GetFullPath("esrgan"))!, "models");
        Assert.Equal(new[] { "-i", Path.Combine(_work, "in"), "-o", Path.Combine(_work, "out"), "-m", models, "-n", "general", "-s", "4", "-f", "png" }, runner.Calls.Single());
    }

    [Fact]
    public async Task WhenTheFirstModelInventsTheSecondOneIsTried()
    {
        var runner = new FakeEsrgan(model => model == "general" ? FakeEsrgan.Mode.Inverting : FakeEsrgan.Mode.Faithful);
        var outcome = (await Upscaler(runner, "general", "anime").UpscaleAsync([Texture(16)], CancellationToken.None)).Single();
        Assert.Equal("anime", outcome.Method);
        Assert.True(outcome.UsedAi);
        Assert.Equal(2, runner.Calls.Count);
    }

    [Fact]
    public async Task WhenEveryModelInventsLanczosIsUsed()
    {
        var outcome = (await Upscaler(new FakeEsrgan(FakeEsrgan.Mode.Inverting), "general", "anime").UpscaleAsync([Texture(16)], CancellationToken.None)).Single();
        Assert.Equal(HdUpscaler.Lanczos, outcome.Method);
        Assert.False(outcome.UsedAi);
        Assert.Equal(64 * 64 * 4, outcome.Rgba.Length);
    }

    [Fact]
    public async Task WithoutAWorkingAiEverythingIsClassicAndItIsNotRetried()
    {
        var runner = new FakeEsrgan(FakeEsrgan.Mode.Failing);
        var upscaler = Upscaler(runner, "general", "anime");
        await upscaler.UpscaleAsync([Texture(16)], CancellationToken.None);
        var second = await upscaler.UpscaleAsync([Texture(32)], CancellationToken.None);
        Assert.False(upscaler.AiAvailable);
        Assert.False(second.Single().UsedAi);
        Assert.Single(runner.Calls);
    }

    [Fact]
    public async Task CutoutsStayCrisp()
    {
        var outcome = (await Upscaler(new FakeEsrgan(FakeEsrgan.Mode.Faithful), "general").UpscaleAsync([Texture(16, cutout: true)], CancellationToken.None)).Single();
        Assert.All(outcome.Rgba.Where((_, i) => i % 4 == 3), a => Assert.True(a is 0 or 255));
    }
}
