using System.Security.Cryptography;
using System.Text;

namespace Vcsnx.Builder.Core;

public sealed class ToolFailedException(string tool, string output) : Exception($"{tool} failed:\n{output}")
{
    public string Tool { get; } = tool;
    public string Output { get; } = output;
}

/// <summary>Links vcs_game.so from the compiled translated code and the kit's entry objects. Like the objects,
/// the module carries a .key (the objects' keys + the link template + the kit's entry objects, which carry the
/// interface string) and is relinked only when that changes.</summary>
public sealed class ModuleLinker(Toolchain toolchain, Kit kit, IProcessRunner runner)
{
    string Fingerprint(IReadOnlyList<string> objects)
    {
        var text = string.Join("\n", objects.Select(o => File.Exists(o + ".key") ? File.ReadAllText(o + ".key") : "")) + "\n" + string.Join("\n", kit.ModuleLinkTemplate)
                   + "\n" + string.Join("\n", kit.EntryObjects.Select(HashList.Sha256Of));
        return Convert.ToHexStringLower(SHA256.HashData(Encoding.UTF8.GetBytes(text)));
    }

    public bool UpToDate(string module, IReadOnlyList<string> objects) =>
        File.Exists(module) && File.Exists(module + ".key") && File.ReadAllText(module + ".key") == Fingerprint(objects);

    public async Task LinkAsync(IReadOnlyList<string> objects, string module, CancellationToken ct)
    {
        if (UpToDate(module, objects)) return;
        File.Delete(module + ".key");
        var tmp = module + ".tmp";
        var responseFile = module + ".rsp";
        File.WriteAllText(responseFile, Kit.ResponseFile(objects));
        ProcessResult result;
        try
        {
            result = await runner.RunAsync(toolchain.Gxx, kit.ModuleLinkArgs(toolchain, tmp, responseFile), ct);
        }
        finally
        {
            File.Delete(responseFile);
        }
        if (result.ExitCode != 0)
        {
            File.Delete(tmp);
            throw new ToolFailedException("link", result.Output);
        }
        File.Move(tmp, module, overwrite: true);
        File.WriteAllText(module + ".key", Fingerprint(objects));
    }
}
