namespace Vcsnx.Builder.Core.Tests;

/// <summary>Stands in for the compiler: writes the file after -o.</summary>
internal sealed class FakeRunner : IProcessRunner
{
    readonly object _lock = new();
    int _running;
    public List<(string File, IReadOnlyList<string> Args)> Calls { get; } = [];
    /// <summary>What each @response file said when the command ran.</summary>
    public List<string> ResponseFiles { get; } = [];
    public Func<IReadOnlyList<string>, bool> FailWhen { get; set; } = _ => false;
    public int MaxConcurrent { get; private set; }

    public async Task<ProcessResult> RunAsync(string file, IReadOnlyList<string> args, CancellationToken ct)
    {
        lock (_lock)
        {
            Calls.Add((file, args));
            ResponseFiles.AddRange(args.Where(a => a.StartsWith('@') && File.Exists(a[1..])).Select(a => File.ReadAllText(a[1..])));
            MaxConcurrent = Math.Max(MaxConcurrent, ++_running);
        }
        try
        {
            await Task.Delay(5, ct);
            if (FailWhen(args)) return new ProcessResult(1, "error: boom");
            File.WriteAllText(args[args.ToList().IndexOf("-o") + 1], "out " + string.Join(' ', args));
            return new ProcessResult(0, "");
        }
        finally
        {
            lock (_lock) --_running;
        }
    }
}

/// <summary>An IProgress that reports on the calling thread.</summary>
internal sealed class SyncProgress<T>(Action<T> report) : IProgress<T>
{
    readonly object _lock = new();
    public void Report(T value) { lock (_lock) report(value); }
}
