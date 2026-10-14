namespace Vcsnx.HdPack;

/// <summary>An upscaled texture and how it was made: a Real-ESRGAN model's name, or <see cref="HdUpscaler.Lanczos"/>.</summary>
public sealed record UpscaleOutcome(byte[] Rgba, int Width, int Height, string Method)
{
    public bool UsedAi => Method != HdUpscaler.Lanczos;
}

/// <summary>Upscales a batch (spec §4.2-4.4): hidden texels filled, a toroidal margin, Real-ESRGAN 4x on the opaque
/// RGB, the margin cut, 2x by box reduction, alpha by Lanczos-3 (cut back to 0/255 for cutouts), then the fidelity
/// check. The models are tried in order, each only on what the previous one failed (author, 2026-10-13: general
/// first, anime if it invents too much); whatever no model passes, or everything when the AI does not run, uses
/// Lanczos-3.</summary>
public sealed class HdUpscaler(IProcessRunner runner, string? esrgan, IReadOnlyList<string> models, string workDir)
{
    public const string Lanczos = "lanczos-3";
    string? _esrgan = esrgan;
    public bool AiAvailable => _esrgan is not null;

    public static UpscaleOutcome Classic(HdTexture t, int? scaleOverride = null)
    {
        var scale = scaleOverride ?? HdPrep.ScaleFor(t.Width, t.Height);
        var up = Resample.Lanczos3Wrap(HdPrep.FillHidden(t.Rgba, t.Width, t.Height), t.Width, t.Height, scale);
        if (HdPrep.BinaryAlpha(t.Rgba)) CutAlpha(up);
        return new UpscaleOutcome(up, t.Width * scale, t.Height * scale, Lanczos);
    }

    /// <param name="scaleOverride">2 or 4 for every texture of the batch instead of HdPrep.ScaleFor (the map).</param>
    public async Task<IReadOnlyList<UpscaleOutcome>> UpscaleAsync(IReadOnlyList<HdTexture> batch, CancellationToken ct, int? scaleOverride = null)
    {
        var classic = batch.Select(t => Classic(t, scaleOverride)).ToArray(); // also each AI result's alpha
        var outcomes = new UpscaleOutcome?[batch.Count];
        var pending = Enumerable.Range(0, batch.Count).ToList();
        foreach (var model in models)
        {
            if (_esrgan is null || pending.Count == 0) break;
            string input = Path.Combine(workDir, "in"), output = Path.Combine(workDir, "out");
            foreach (var dir in new[] { input, output })
            {
                if (Directory.Exists(dir)) Directory.Delete(dir, recursive: true);
                Directory.CreateDirectory(dir);
            }
            foreach (var i in pending)
            {
                var t = batch[i];
                var (mx, my) = HdPrep.MarginFor(t.Width, t.Height);
                var opaque = HdPrep.FillHidden(t.Rgba, t.Width, t.Height);
                for (var a = 3; a < opaque.Length; a += 4) opaque[a] = 255;
                var padded = Resample.WrapPad(opaque, t.Width, t.Height, mx, my);
                await File.WriteAllBytesAsync(Path.Combine(input, $"{i:D5}.png"), PngCodec.EncodeRgba(padded, t.Width + 2 * mx, t.Height + 2 * my), ct);
            }
            // The models sit next to the program (both zips); -m makes that explicit whatever the working directory.
            var modelDir = Path.Combine(Path.GetDirectoryName(Path.GetFullPath(_esrgan))!, "models");
            var result = await runner.RunAsync(_esrgan, ["-i", input, "-o", output, "-m", modelDir, "-n", model, "-s", "4", "-f", "png"], ct);
            if (result.ExitCode != 0)
            {
                _esrgan = null; // no Vulkan GPU, or the program cannot run here: do not try again for every batch
                break;
            }
            var failed = new List<int>();
            foreach (var i in pending)
            {
                var outcome = FromAi(batch[i], classic[i], Path.Combine(output, $"{i:D5}.png"), model, scaleOverride);
                if (outcome is null) failed.Add(i); else outcomes[i] = outcome;
            }
            pending = failed;
        }
        foreach (var i in pending) outcomes[i] = classic[i];
        return outcomes.Select(o => o!).ToList();
    }

    /// <summary>The AI result for one texture, or null when it is missing, unreadable or not faithful.</summary>
    static UpscaleOutcome? FromAi(HdTexture t, UpscaleOutcome classic, string path, string model, int? scaleOverride)
    {
        if (!File.Exists(path)) return null;
        int width, height;
        byte[] ai;
        try { (width, height, ai) = PngCodec.Decode(File.ReadAllBytes(path)); }
        catch (InvalidDataException) { return null; }
        var (mx, my) = HdPrep.MarginFor(t.Width, t.Height);
        if (width != (t.Width + 2 * mx) * 4 || height != (t.Height + 2 * my) * 4) return null;
        var rgb = Resample.Crop(ai, width, mx * 4, my * 4, t.Width * 4, t.Height * 4);
        var scale = scaleOverride ?? HdPrep.ScaleFor(t.Width, t.Height);
        if (scale == 2) rgb = Resample.BoxDown(rgb, t.Width * 4, t.Height * 4, 2);
        for (var a = 3; a < rgb.Length; a += 4) rgb[a] = classic.Rgba[a]; // alpha from Lanczos-3 (already cut for cutouts)
        var back = Resample.BoxDown(rgb, t.Width * scale, t.Height * scale, scale);
        return Fidelity.Psnr(t.Rgba, back) >= Fidelity.MinPsnr ? new UpscaleOutcome(rgb, t.Width * scale, t.Height * scale, model) : null;
    }

    static void CutAlpha(byte[] rgba)
    {
        for (var a = 3; a < rgba.Length; a += 4) rgba[a] = rgba[a] >= 128 ? (byte)255 : (byte)0;
    }
}
