using System.Security.Cryptography;
using System.Text;

namespace Vcsnx.Builder.Core;

public static class JobPlanner
{
    const long PerJob = 2L << 30; // GCC at -O2 on the biggest translated units, with room for Windows

    /// <summary>One compile per core, but no more than one per 2 GB of RAM after 2 GB for everything else.</summary>
    public static int Jobs(int cores, long memoryBytes) =>
        Math.Clamp((int)Math.Min(cores, (memoryBytes - PerJob) / PerJob), 1, Math.Max(1, cores));
}

public sealed class CompileFailedException(string unit, string output) : Exception($"compiling {unit} failed:\n{output}")
{
    public string Unit { get; } = unit;
    public string Output { get; } = output;
}

/// <summary>Compiles the translated units into trabajo\g\. An object is written to .tmp and renamed, and its
/// .key (source hash + compile template + the kit's interface and GCC version) is written last, so an interrupted
/// run never reuses half an object, and a new release's kit recompiles what it must.</summary>
public sealed class Compiler(Toolchain toolchain, Kit kit, string generatedDir, string objectDir, IProcessRunner runner)
{
    public string ObjectFor(string stem) => Path.Combine(objectDir, stem + ".o");
    public string SourceFor(string stem) => Path.Combine(generatedDir, stem + ".cpp");

    public string Fingerprint(string source)
    {
        var text = HashList.Sha256Of(source) + "\n" + string.Join("\n", kit.CompileTemplate) + "\n" + kit.Info.Abi + "\n" + kit.Info.Gcc;
        return Convert.ToHexStringLower(SHA256.HashData(Encoding.UTF8.GetBytes(text)));
    }

    public bool UpToDate(string stem)
    {
        var obj = ObjectFor(stem);
        var key = obj + ".key";
        return File.Exists(obj) && File.Exists(key) && File.ReadAllText(key) == Fingerprint(SourceFor(stem));
    }

    public async Task<IReadOnlyList<string>> CompileAllAsync(IReadOnlyList<string> stems, int jobs, IProgress<int>? done, CancellationToken ct)
    {
        Directory.CreateDirectory(objectDir);
        var finished = 0;
        using var gate = new SemaphoreSlim(Math.Max(1, jobs));
        using var stop = CancellationTokenSource.CreateLinkedTokenSource(ct);

        async Task One(string stem)
        {
            await gate.WaitAsync(stop.Token);
            try
            {
                if (!UpToDate(stem))
                {
                    var obj = ObjectFor(stem);
                    var tmp = obj + ".tmp";
                    File.Delete(obj + ".key");
                    var result = await runner.RunAsync(toolchain.Gxx, kit.CompileArgs(toolchain, generatedDir, SourceFor(stem), tmp), stop.Token);
                    if (result.ExitCode != 0)
                    {
                        File.Delete(tmp);
                        stop.Cancel();
                        throw new CompileFailedException(stem, result.Output);
                    }
                    File.Move(tmp, obj, overwrite: true);
                    File.WriteAllText(obj + ".key", Fingerprint(SourceFor(stem)));
                }
                done?.Report(Interlocked.Increment(ref finished));
            }
            finally
            {
                gate.Release();
            }
        }

        var tasks = stems.Select(One).ToList();
        try
        {
            await Task.WhenAll(tasks);
        }
        catch when (tasks.Any(t => t.Exception?.InnerException is CompileFailedException))
        {
            throw tasks.Select(t => t.Exception?.InnerException).OfType<CompileFailedException>().First();
        }
        return stems.Select(ObjectFor).ToList();
    }
}
