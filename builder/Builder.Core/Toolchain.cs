namespace Vcsnx.Builder.Core;

/// <summary>devkitPro's toolchain folder (its opt/devkitpro tree): /opt/devkitpro on the Mac, toolchain\ in the Builder.</summary>
public sealed record Toolchain(string Root)
{
    public string Gxx => Path.Combine(Root, Path.Combine("devkitA64", "bin", "aarch64-none-elf-g++")) + (OperatingSystem.IsWindows() ? ".exe" : "");

    /// <summary>devkitPro's Windows tools take only ASCII paths (spaces are fine: arguments are passed one by one).</summary>
    public bool PathIsUsable => Root.All(c => c >= ' ' && c < 127);
}
