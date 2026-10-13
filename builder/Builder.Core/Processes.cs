using System.ComponentModel;
using System.Diagnostics;

namespace Vcsnx.Builder.Core;

public sealed record ProcessResult(int ExitCode, string Output);

public interface IProcessRunner
{
    Task<ProcessResult> RunAsync(string file, IReadOnlyList<string> args, CancellationToken ct);
}

/// <summary>Runs the toolchain. With `tempDir` (trabajo\tmp, inside the ASCII-only program folder) the tools write
/// their temporary files there instead of the player's %TEMP%, which may have accents (C:\Users\José\...).</summary>
public sealed class SystemProcessRunner(string? tempDir = null) : IProcessRunner
{
    public async Task<ProcessResult> RunAsync(string file, IReadOnlyList<string> args, CancellationToken ct)
    {
        var start = new ProcessStartInfo(file)
        {
            RedirectStandardOutput = true, RedirectStandardError = true, UseShellExecute = false, CreateNoWindow = true,
        };
        foreach (var arg in args) start.ArgumentList.Add(arg);
        if (tempDir is not null)
        {
            Directory.CreateDirectory(tempDir);
            foreach (var name in new[] { "TMP", "TEMP", "TMPDIR" }) start.Environment[name] = tempDir;
        }
        Process process;
        try
        {
            process = Process.Start(start) ?? throw new InvalidOperationException($"cannot start {file}");
        }
        catch (Win32Exception e)
        {
            // Missing, quarantined or blocked (antivirus, Smart App Control): something the player can act on.
            throw new BuilderException("ToolBlocked", Path.GetFileName(file), e.Message);
        }
        using var _ = process;
        var stdout = process.StandardOutput.ReadToEndAsync(CancellationToken.None);
        var stderr = process.StandardError.ReadToEndAsync(CancellationToken.None);
        try
        {
            await process.WaitForExitAsync(ct);
        }
        catch (OperationCanceledException)
        {
            try { process.Kill(entireProcessTree: true); } catch (InvalidOperationException) { }
            throw;
        }
        return new ProcessResult(process.ExitCode, await stdout + await stderr);
    }
}

/// <summary>Writes every command and what it printed to the build log (salida\builder.log).</summary>
public sealed class LoggingRunner(IProcessRunner inner, TextWriter log) : IProcessRunner
{
    public async Task<ProcessResult> RunAsync(string file, IReadOnlyList<string> args, CancellationToken ct)
    {
        ProcessResult result;
        try
        {
            result = await inner.RunAsync(file, args, ct);
        }
        catch (Exception e) when (e is not OperationCanceledException)
        {
            lock (log)
            {
                log.WriteLine($"$ {file} {string.Join(' ', args)}");
                log.WriteLine(e.Message);
                log.Flush();
            }
            throw;
        }
        lock (log)
        {
            log.WriteLine($"$ {file} {string.Join(' ', args)}");
            if (result.Output.Length > 0) log.WriteLine(result.Output.TrimEnd());
            if (result.ExitCode != 0) log.WriteLine($"(exit code {result.ExitCode})");
            log.Flush();
        }
        return result;
    }
}
