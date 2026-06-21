// geng — flat fill for bar quads. The bar's color is carried from the vertex shader; the layer is
// straight-alpha so it composites over the plot in the display pass (edges anti-alias via the
// supersample downsample, the same as the grid/curve raster).

struct PSIn
{
    [[vk::location(0)]] float4 color : COLOR0;
};

[shader("fragment")]
float4 main(PSIn input) : SV_Target
{
    return input.color;
}
