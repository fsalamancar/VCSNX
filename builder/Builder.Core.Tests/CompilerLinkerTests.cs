namespace Vcsnx.Builder.Core.Tests;

public sealed class CompilerLinkerTests
{
    static (Compiler Compiler, FakeRunner Runner, string Generated, string Objects, Kit Kit, Toolchain Toolchain) Setup(params string[] stems)
    {
        var dir = TestFiles.TempDir();
        var generated = Path.Combine(dir, "generated");
        foreach (var stem in stems) TestFiles.Write(Path.Combine(generated, stem + ".cpp"), System.Text.Encoding.UTF8.GetBytes(stem));
        var kit = ToolchainKitTests.MakeKit(Path.Combine(dir, "kit"), ["-O2", "-o", "{output}", "-c", "{source}"],
            ["-shared", "{generated_objects}", "-o", "{output}"], stems);
        var runner = new FakeRunner();
        var toolchain = new Toolchain(Path.Combine(dir, "tc"));
        var objects = Path.Combine(dir, "g");
        return (new Compiler(toolchain, kit, generated, objects, runner), runner, generated, objects, kit, toolchain);
    }

    [Theory]
    [InlineData(8, 8L << 30, 3)]
    [InlineData(8, 32L << 30, 8)]
    [InlineData(4, 16L << 30, 4)]
    [InlineData(16, 4L << 30, 1)]
    [InlineData(8, 1L << 30, 1)]
    public void Parallel_compiles_follow_cores_and_memory(int cores, long memory, int jobs) =>
        Assert.Equal(jobs, JobPlanner.Jobs(cores, memory));

    [Fact]
    public async Task Every_unit_is_compiled_and_objects_come_back_in_link_order()
    {
        var (compiler, runner, _, objects, _, _) = Setup("generated_registry", "generated_unit_0000", "generated_unit_0001");
        var result = await compiler.CompileAllAsync(["generated_registry", "generated_unit_0000", "generated_unit_0001"], 2, null, default);
        Assert.Equal(new[] { Path.Combine(objects, "generated_registry.o"), Path.Combine(objects, "generated_unit_0000.o"), Path.Combine(objects, "generated_unit_0001.o") }, result);
        Assert.Equal(3, runner.Calls.Count);
        Assert.True(runner.MaxConcurrent <= 2);
    }

    [Fact]
    public async Task A_second_run_compiles_nothing_and_a_changed_source_is_recompiled()
    {
        var (compiler, runner, generated, _, _, _) = Setup("a", "b");
        await compiler.CompileAllAsync(["a", "b"], 2, null, default);
        runner.Calls.Clear();
        await compiler.CompileAllAsync(["a", "b"], 2, null, default);
        Assert.Empty(runner.Calls);
        File.WriteAllText(Path.Combine(generated, "b.cpp"), "changed");
        await compiler.CompileAllAsync(["a", "b"], 2, null, default);
        Assert.Single(runner.Calls);
    }

    [Fact]
    public async Task A_half_written_object_from_an_interrupted_run_is_never_reused()
    {
        var (compiler, runner, _, objects, _, _) = Setup("a");
        TestFiles.Write(Path.Combine(objects, "a.o"), [1, 2]);   // no .key: the run died before it finished
        TestFiles.Write(Path.Combine(objects, "a.o.tmp"), [3]);
        Assert.False(compiler.UpToDate("a"));
        await compiler.CompileAllAsync(["a"], 1, null, default);
        Assert.Single(runner.Calls);
        Assert.True(compiler.UpToDate("a"));
    }

    [Fact]
    public async Task A_failing_unit_stops_the_build_and_leaves_no_object()
    {
        var (compiler, runner, _, objects, _, _) = Setup("a", "b", "c");
        runner.FailWhen = args => args.Any(a => a.EndsWith("b.cpp"));
        var error = await Assert.ThrowsAsync<CompileFailedException>(() => compiler.CompileAllAsync(["a", "b", "c"], 1, null, default));
        Assert.Equal("b", error.Unit);
        Assert.Contains("boom", error.Output);
        Assert.False(File.Exists(Path.Combine(objects, "b.o")));
    }

    [Fact]
    public async Task Progress_counts_every_unit_including_the_cached_ones()
    {
        var (compiler, _, _, _, _, _) = Setup("a", "b");
        await compiler.CompileAllAsync(["a"], 1, null, default);
        var seen = new List<int>();
        await compiler.CompileAllAsync(["a", "b"], 1, new SyncProgress<int>(seen.Add), default);
        Assert.Equal(new[] { 1, 2 }, seen.Order());
    }

    [Fact]
    public async Task The_module_is_linked_once_and_again_only_when_an_object_changes()
    {
        var (compiler, runner, generated, objects, kit, toolchain) = Setup("a", "b");
        var objectFiles = await compiler.CompileAllAsync(["a", "b"], 1, null, default);
        var linker = new ModuleLinker(toolchain, kit, runner);
        var module = Path.Combine(Path.GetDirectoryName(objects)!, "salida", "vcs_game.so");
        Directory.CreateDirectory(Path.GetDirectoryName(module)!);
        runner.Calls.Clear();
        await linker.LinkAsync(objectFiles, module, default);
        Assert.Single(runner.Calls);
        Assert.Contains("-shared", runner.Calls[0].Args);
        Assert.True(File.Exists(module));
        await linker.LinkAsync(objectFiles, module, default);
        Assert.Single(runner.Calls);
        File.WriteAllText(Path.Combine(generated, "a.cpp"), "changed");
        await compiler.CompileAllAsync(["a", "b"], 1, null, default);
        await linker.LinkAsync(objectFiles, module, default);
        Assert.Equal(3, runner.Calls.Count); // the first link, compiling a again, linking again
        runner.FailWhen = args => args.Contains("-shared");
        File.Delete(module + ".key");
        await Assert.ThrowsAsync<ToolFailedException>(() => linker.LinkAsync(objectFiles, module, default));
    }

    [Fact]
    public async Task A_kit_with_another_interface_or_compiler_recompiles_everything()
    {
        var (compiler, runner, generated, objects, kit, toolchain) = Setup("a");
        await compiler.CompileAllAsync(["a"], 1, null, default);
        Assert.True(compiler.UpToDate("a"));
        File.WriteAllText(Path.Combine(kit.Root, "kit.json"), $$"""{"version": "1.0.0", "gcc": "16.1.0", "abi": "{{new string('b', 64)}}"}""");
        Assert.False(new Compiler(toolchain, new Kit(kit.Root), generated, objects, runner).UpToDate("a"));
        File.WriteAllText(Path.Combine(kit.Root, "kit.json"), $$"""{"version": "1.0.0", "gcc": "16.2.0", "abi": "{{new string('a', 64)}}"}""");
        Assert.False(new Compiler(toolchain, new Kit(kit.Root), generated, objects, runner).UpToDate("a"));
    }

    [Fact]
    public async Task The_module_is_linked_again_when_the_kits_entry_objects_change()
    {
        var (compiler, runner, _, objects, kit, toolchain) = Setup("a");
        TestFiles.Write(Path.Combine(kit.Root, "obj", "module_entry.o"), [1]);
        var objectFiles = await compiler.CompileAllAsync(["a"], 1, null, default);
        var linker = new ModuleLinker(toolchain, kit, runner);
        var module = Path.Combine(Path.GetDirectoryName(objects)!, "salida", "vcs_game.so");
        Directory.CreateDirectory(Path.GetDirectoryName(module)!);
        await linker.LinkAsync(objectFiles, module, default);
        Assert.True(linker.UpToDate(module, objectFiles));
        File.WriteAllBytes(Path.Combine(kit.Root, "obj", "module_entry.o"), [2]); // a new release's interface string
        Assert.False(linker.UpToDate(module, objectFiles));
    }

    [Fact]
    public async Task The_module_link_takes_the_objects_from_a_response_file_in_link_order()
    {
        var (compiler, runner, _, objects, kit, toolchain) = Setup("b", "a");
        var objectFiles = await compiler.CompileAllAsync(["b", "a"], 1, null, default);
        var module = Path.Combine(Path.GetDirectoryName(objects)!, "salida", "vcs_game.so");
        Directory.CreateDirectory(Path.GetDirectoryName(module)!);
        runner.Calls.Clear();
        await new ModuleLinker(toolchain, kit, runner).LinkAsync(objectFiles, module, default);
        Assert.DoesNotContain(runner.Calls[0].Args, a => a.EndsWith(".o"));
        Assert.Equal(Kit.ResponseFile(objectFiles), Assert.Single(runner.ResponseFiles));
        Assert.Empty(Directory.GetFiles(Path.GetDirectoryName(module)!, "*.rsp"));
    }
}
