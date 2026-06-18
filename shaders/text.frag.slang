// geng — glyph fill: the atlas carries coverage in its alpha channel, modulating the per-glyph
// colour's alpha. Drawn with blending enabled into a transparent target, so the label layer the
// display pass composites over the plot is coverage-shaped.

struct PSIn
{
    [[vk::location(0)]] float2 uv : TEXCOORD0;
    [[vk::location(1)]] float4 color : COLOR0;
};

[[vk::binding(1, 0)]] Texture2D atlas;
[[vk::binding(2, 0)]] SamplerState samp;

[shader("fragment")]
float4 main(PSIn input) : SV_Target
{
    float coverage = atlas.Sample(samp, input.uv).a;
    return float4(input.color.rgb, input.color.a * coverage);
}
