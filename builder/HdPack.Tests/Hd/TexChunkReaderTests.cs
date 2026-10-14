using System.Buffers.Binary;
using Vcsnx.HdPack;

namespace Vcsnx.HdPack.Tests;

public class TexChunkReaderTests
{
    static TexChunkBuilder.Tex Small(string name) =>
        TexChunkBuilder.Indexed(name, 16, 16, 4, 1, (_, _) => 1, TexChunkBuilder.Palette(16));

    [Fact]
    public void TheListGivesEveryTextureInOrder()
    {
        var chunk = TexChunkBuilder.Build(Small("a"), Small("b"), Small("c"));
        Assert.True(TexChunkReader.IsValid(chunk, 0, out var size));
        Assert.Equal(chunk.Length, size);
        Assert.Equal(new[] { "a", "b", "c" }, TexChunkReader.Textures(chunk).Select(t => t.Name));
    }

    [Fact]
    public void ChunksAreFoundOnlyAtTheAlignment()
    {
        var one = TexChunkBuilder.Build(Small("a"));
        var image = new byte[4096 + one.Length];
        one.CopyTo(image, 0);
        one.CopyTo(image, 4096);
        one.CopyTo(image, 1000); // not on a 2048 boundary: ignored
        Assert.Equal(new[] { 0, 4096 }, TexChunkReader.FindChunks(image, 2048));
    }

    [Fact]
    public void ACyclicListOrPointersOutsideTheChunkEndTheWalk()
    {
        var chunk = TexChunkBuilder.Build(Small("a"), Small("b"));
        var firstLink = BinaryPrimitives.ReadInt32LittleEndian(chunk.AsSpan(0x28));
        BinaryPrimitives.WriteInt32LittleEndian(chunk.AsSpan(firstLink + 0x60), firstLink); // b points back to a
        Assert.Equal(2, TexChunkReader.Textures(chunk).Count);
        BinaryPrimitives.WriteInt32LittleEndian(chunk.AsSpan(0x28), chunk.Length + 100);
        Assert.Empty(TexChunkReader.Textures(chunk));
    }

    [Fact]
    public void AHeaderThatDoesNotFitIsNotAChunk()
    {
        var chunk = TexChunkBuilder.Build(Small("a"));
        BinaryPrimitives.WriteInt32LittleEndian(chunk.AsSpan(8), chunk.Length + 1);
        Assert.False(TexChunkReader.IsValid(chunk, 0, out _));
    }
}
