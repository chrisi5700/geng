// geng — display pass: composite the cached, supersampled layers and resolve onto the screen. The
// plot raster (grid + axes + curves), the bar layer, the scatter-marker layer, and the tick-label
// layer are each rendered at SUPERSAMPLE × the screen, so sampling them here is a downsampling resolve
// — veng's render-target sampler (linear, clamp-to-edge) gives the SSAA for free. Layers bind by
// reflected name; UV maps each cache 1:1. Order is back to front: opaque plot, bars, markers, labels.

struct PSIn
{
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};

[[vk::binding(0, 0)]] Texture2D plot;
[[vk::binding(1, 0)]] Texture2D bars;
[[vk::binding(2, 0)]] Texture2D markers;
[[vk::binding(3, 0)]] Texture2D text;
[[vk::binding(4, 0)]] SamplerState samp;

[shader("fragment")]
float4 main(PSIn input) : SV_Target
{
    float3 base   = plot.Sample(samp, input.uv).rgb;
    float4 bar    = bars.Sample(samp, input.uv);    // straight-alpha bar coverage
    base          = lerp(base, bar.rgb, bar.a);
    float4 marker = markers.Sample(samp, input.uv); // straight-alpha marker coverage
    base          = lerp(base, marker.rgb, marker.a);
    float4 label  = text.Sample(samp, input.uv);    // straight-alpha label coverage
    base          = lerp(base, label.rgb, label.a);
    return float4(base, 1.0);
}
