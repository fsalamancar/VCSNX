using System.Text.Json;

namespace Vcsnx.Builder.Core;

public sealed record KitInfo(string Version, string Gcc, string Abi);

/// <summary>The kit tools/make_builder_kit.py builds: the compile line of the translated code and the link line of
/// vcs_game.so, with placeholders {kit} {toolchain} {generated} {source} {output} {generated_objects}; the module's
/// own entry objects; the runtime vcsnx.nro; VCSNative.ini; VCSNX's own menu pictures (Textures\).</summary>
public sealed class Kit
{
    static readonly JsonSerializerOptions Json = new() { PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower };
    sealed record LinkFile(string[] Args, string[] GeneratedOrder);

    readonly string[] _compile;
    readonly string[] _link;

    public Kit(string root)
    {
        Root = Path.GetFullPath(root);
        Info = JsonSerializer.Deserialize<KitInfo>(File.ReadAllText(Path.Combine(Root, "kit.json")), Json)
               ?? throw new InvalidDataException("kit.json is empty");
        _compile = JsonSerializer.Deserialize<string[]>(File.ReadAllText(Path.Combine(Root, "compile.json")))
                   ?? throw new InvalidDataException("compile.json is empty");
        var link = JsonSerializer.Deserialize<LinkFile>(File.ReadAllText(Path.Combine(Root, "module_link.json")), Json)
                   ?? throw new InvalidDataException("module_link.json is empty");
        _link = link.Args;
        GeneratedOrder = link.GeneratedOrder;
    }

    public string Root { get; }
    public KitInfo Info { get; }
    /// <summary>The translated sources' names without .cpp, in the original link order.</summary>
    public IReadOnlyList<string> GeneratedOrder { get; }
    /// <summary>compile.json as written (what an object's fingerprint depends on).</summary>
    public IReadOnlyList<string> CompileTemplate => _compile;
    /// <summary>module_link.json's arguments as written (what the module's fingerprint depends on).</summary>
    public IReadOnlyList<string> ModuleLinkTemplate => _link;
    /// <summary>kit\obj\*: the module's own entry objects, in a stable order.</summary>
    public IReadOnlyList<string> EntryObjects =>
        Directory.Exists(Path.Combine(Root, "obj")) ? Directory.GetFiles(Path.Combine(Root, "obj")).Order(StringComparer.Ordinal).ToList() : [];
    public string RuntimeNro => Path.Combine(Root, "vcsnx.nro");
    public string Ini => Path.Combine(Root, "VCSNative.ini");
    /// <summary>kit\Textures\&lt;key&gt;.png: VCSNX's Settings &gt; Controls picture and the menu legend's A and B,
    /// copied to switch\vcsnx\Textures on every build.</summary>
    public IReadOnlyList<string> Textures =>
        Directory.Exists(Path.Combine(Root, "Textures"))
            ? Directory.GetFiles(Path.Combine(Root, "Textures")).Where(f => HdFolder.IsTexture(Path.GetFileName(f))).Order(StringComparer.Ordinal).ToList()
            : [];

    public IReadOnlyList<string> CompileArgs(Toolchain toolchain, string generatedDir, string source, string output) =>
        _compile.Select(a => Fill(a, toolchain, generatedDir).Replace("{source}", Fwd(source)).Replace("{output}", Fwd(output))).ToList();

    /// <summary>The link line, with the translated objects as one @responseFile (see ResponseFile): on the command
    /// line, 236 paths under a long program folder pass Windows' 32,767-character limit.</summary>
    public IReadOnlyList<string> ModuleLinkArgs(Toolchain toolchain, string output, string responseFile) =>
        _link.Select(a => a == "{generated_objects}" ? "@" + Fwd(responseFile) : Fill(a, toolchain, "").Replace("{output}", Fwd(output))).ToList();

    /// <summary>The objects for GCC's @file, one per line, quoted (folders may have spaces), in link order.</summary>
    public static string ResponseFile(IReadOnlyList<string> objects) => string.Concat(objects.Select(o => $"\"{Fwd(o)}\"\n"));

    string Fill(string arg, Toolchain toolchain, string generatedDir) =>
        arg.Replace("{kit}", Fwd(Root)).Replace("{toolchain}", Fwd(toolchain.Root)).Replace("{generated}", Fwd(generatedDir));

    static string Fwd(string path) => path.Replace('\\', '/');
}
