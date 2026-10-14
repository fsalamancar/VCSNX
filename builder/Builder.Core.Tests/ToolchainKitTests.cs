using System.Text.Json;

namespace Vcsnx.Builder.Core.Tests;

public sealed class ToolchainKitTests
{
    public static Kit MakeKit(string dir, string[] compile, string[] link, string[] order)
    {
        Directory.CreateDirectory(dir);
        File.WriteAllText(Path.Combine(dir, "kit.json"), $$"""{"version": "1.0.0", "gcc": "16.1.0", "abi": "{{new string('a', 64)}}"}""");
        File.WriteAllText(Path.Combine(dir, "compile.json"), JsonSerializer.Serialize(compile));
        File.WriteAllText(Path.Combine(dir, "module_link.json"), JsonSerializer.Serialize(new Dictionary<string, object> { ["args"] = link, ["generated_order"] = order }));
        File.WriteAllText(Path.Combine(dir, "VCSNative.ini"), "[Display]\n");
        File.WriteAllBytes(Path.Combine(dir, "vcsnx.nro"), TestFiles.Nro("default icon"u8.ToArray(), "nacp"u8.ToArray()));
        return new Kit(dir);
    }

    [Theory]
    [InlineData(@"C:\VCSNX-Builder\toolchain", true)]
    [InlineData(@"C:\Program Files\VCSNX\toolchain", true)]
    [InlineData(@"C:\Users\José\VCSNX\toolchain", false)]
    public void The_toolchain_folder_must_be_plain_ASCII(string root, bool usable) =>
        Assert.Equal(usable, new Toolchain(root).PathIsUsable);

    [Fact]
    public void The_compiler_is_inside_the_toolchain()
    {
        var toolchain = new Toolchain(@"C:\VCSNX\toolchain");
        Assert.Contains("aarch64-none-elf-g++", toolchain.Gxx);
        Assert.StartsWith(@"C:\VCSNX\toolchain", toolchain.Gxx);
    }

    [Fact]
    public void The_kit_fills_in_its_placeholders()
    {
        var dir = Path.Combine(TestFiles.TempDir(), "kit");
        var kit = MakeKit(dir, ["-I{generated}", "-isystem", "{toolchain}/libnx/include", "-I{kit}/include/host", "-o", "{output}", "-c", "{source}"],
            ["-shared", "{generated_objects}", "{kit}/obj/module_entry.o", "{kit}/obj/module_eh_end.o", "-o", "{output}"],
            ["generated_registry", "generated_unit_0000"]);
        var toolchain = new Toolchain("/tc");
        var kitPath = dir.Replace('\\', '/');
        Assert.Equal(new[] { "-I/work/generated", "-isystem", "/tc/libnx/include", $"-I{kitPath}/include/host", "-o", "/work/g/a.o", "-c", "/work/generated/a.cpp" },
            kit.CompileArgs(toolchain, "/work/generated", "/work/generated/a.cpp", "/work/g/a.o"));
        // The 236 objects go in a response file: with them on the command line, a long program folder passes
        // Windows' 32,767-character limit and ld gets a cut path.
        Assert.Equal(new[] { "-shared", "@/w/vcs_game.so.rsp", $"{kitPath}/obj/module_entry.o", $"{kitPath}/obj/module_eh_end.o", "-o", "/w/vcs_game.so" },
            kit.ModuleLinkArgs(toolchain, "/w/vcs_game.so", "/w/vcs_game.so.rsp"));
        Assert.Equal("\"/w/g/r.o\"\n\"C:/VCSNX Builder/trabajo/g/u.o\"\n", Kit.ResponseFile(["/w/g/r.o", @"C:\VCSNX Builder\trabajo\g\u.o"]));
        Assert.Equal(new[] { "generated_registry", "generated_unit_0000" }, kit.GeneratedOrder);
        Assert.Equal("1.0.0", kit.Info.Version);
        Assert.Equal(new string('a', 64), kit.Info.Abi);
        Assert.Equal("{toolchain}/libnx/include", kit.CompileTemplate[2]);
        Assert.True(File.Exists(kit.RuntimeNro));
    }

    [Fact]
    public void The_kits_menu_pictures_are_its_key_named_pngs()
    {
        var dir = Path.Combine(TestFiles.TempDir(), "kit");
        var kit = MakeKit(dir, [], [], []);
        Assert.Empty(kit.Textures);  // a kit without Textures\
        TestFiles.Write(Path.Combine(dir, "Textures", "cc29539db154b0c7.png"), [1]);
        TestFiles.Write(Path.Combine(dir, "Textures", "27b80d3a3bb72734.png"), [2]);
        TestFiles.Write(Path.Combine(dir, "Textures", "notes.txt"), [3]);
        Assert.Equal(new[] { "27b80d3a3bb72734.png", "cc29539db154b0c7.png" }, kit.Textures.Select(Path.GetFileName));
    }
}
