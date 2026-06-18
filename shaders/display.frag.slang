// geng — display pass: composite the cached, supersampled plot raster with the tick-label layer and
// resolve onto the screen. Both caches are rendered at SUPERSAMPLE x the screen, so sampling them
// here is a downsampling resolve — veng's render-target sampler (linear, clamp-to-edge) gives the
// SSAA for free. `plot` and `text` bind by reflected name; UV maps each cache 1:1 for now.

struct PSIn
{
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};

[[vk::binding(0, 0)]] Texture2D plot;
[[vk::binding(1, 0)]] Texture2D text;
[[vk::binding(2, 0)]] SamplerState samp;

[shader("fragment")]
float4 main(PSIn input) : SV_Target
{
    float3 base  = plot.Sample(samp, input.uv).rgb;
    float4 label = text.Sample(samp, input.uv); // straight-alpha coverage
    return float4(lerp(base, label.rgb, label.a), 1.0);
}
