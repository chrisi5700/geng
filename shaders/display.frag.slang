// geng — display pass: sample the cached, supersampled plot raster onto the screen. The bake is
// rendered at SUPERSAMPLE x the screen resolution, so this fullscreen sample is a downsampling
// resolve — veng's render-target sampler (linear, clamp-to-edge) gives the SSAA for free.
//
// `plot` is bound by reflected name (GraphicsNode::add_sampled_image(handle, "plot")); the
// SamplerState is supplied by the node's own sampler. UV maps the whole cache 1:1 for now —
// the view transform (pan/zoom) will remap it into the cached region later.

struct PSIn
{
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};

[[vk::binding(0, 0)]] Texture2D plot;
[[vk::binding(1, 0)]] SamplerState samp;

[shader("fragment")]
float4 main(PSIn input) : SV_Target
{
    return plot.Sample(samp, input.uv);
}
