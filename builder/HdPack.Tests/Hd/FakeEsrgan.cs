using Vcsnx.HdPack;

namespace Vcsnx.HdPack.Tests;

/// <summary>Stands in for realesrgan-ncnn-vulkan: reads every PNG of -i and writes a 4x one to -o. Per model (-n):
/// Faithful (nearest neighbour), Inverting (wrong colors) or Failing (exit code 1, no output).</summary>
internal sealed class FakeEsrgan(Func<string, FakeEsrgan.Mode> modeFor) : IProcessRunner
{
    public enum Mode { Faithful, Inverting, Failing }
    public FakeEsrgan(Mode mode) : this(_ => mode) { }
    public List<IReadOnlyList<string>> Calls { get; } = [];

    public Task<ProcessResult> RunAsync(string file, IReadOnlyList<string> args, CancellationToken ct)
    {
        Calls.Add(args);
        var list = args.ToList();
        var mode = modeFor(list[list.IndexOf("-n") + 1]);
        if (mode == Mode.Failing) return Task.FromResult(new ProcessResult(1, "vkCreateInstance failed"));
        string input = list[list.IndexOf("-i") + 1], output = list[list.IndexOf("-o") + 1];
        Directory.CreateDirectory(output);
        foreach (var path in Directory.GetFiles(input, "*.png"))
        {
            var (w, h, rgba) = PngCodec.Decode(File.ReadAllBytes(path));
            var up = new byte[w * 4 * h * 4 * 4];
            for (var y = 0; y < h * 4; y++)
            for (var x = 0; x < w * 4; x++)
            for (var c = 0; c < 4; c++)
            {
                var v = rgba[((y / 4) * w + x / 4) * 4 + c];
                up[(y * w * 4 + x) * 4 + c] = mode == Mode.Inverting && c < 3 ? (byte)(255 - v) : v;
            }
            File.WriteAllBytes(Path.Combine(output, Path.GetFileName(path)), PngCodec.EncodeRgba(up, w * 4, h * 4));
        }
        return Task.FromResult(new ProcessResult(0, ""));
    }
}
