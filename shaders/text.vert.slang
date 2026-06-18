// geng — instanced glyph-quad expansion for tick labels. One glyph per instance (SV_InstanceID),
// six SV_VertexID vertices = a quad. Each glyph carries a data-space anchor (projected like the
// plot) plus a pixel-space quad rectangle, so the label sits at a data position but keeps a
// constant on-screen size — the same pixel-offset trick the line shader uses.

struct Glyph
{
    float4 color;
    float2 anchor; // data-space attach point (e.g. a tick position)
    float2 min_px; // quad top-left, pixels relative to the anchor
    float2 max_px; // quad bottom-right, pixels relative to the anchor
    float2 uv0;
    float2 uv1;
};
[[vk::binding(0, 0)]] StructuredBuffer<Glyph> glyphs;

struct PushData
{
    float4x4 view_proj; // data-space -> clip (matches the plot's projection)
    float2   extent;    // framebuffer size in pixels
};
[[vk::push_constant]] PushData pc;

struct VSOut
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
    [[vk::location(1)]] float4 color : COLOR0;
};

// Quad corners: (0,0) top-left .. (1,1) bottom-right.
static const float2 CORNERS[6] = {
    float2(0, 0), float2(1, 0), float2(0, 1),
    float2(0, 1), float2(1, 0), float2(1, 1),
};

[shader("vertex")]
VSOut main(uint vid : SV_VertexID, uint gid : SV_InstanceID)
{
    Glyph  glyph  = glyphs[gid];
    float2 corner = CORNERS[vid];
    float2 px     = lerp(glyph.min_px, glyph.max_px, corner);
    float2 uv     = lerp(glyph.uv0, glyph.uv1, corner);

    float4 clip        = mul(float4(glyph.anchor, 0.0, 1.0), pc.view_proj);
    float2 half_extent = 0.5 * pc.extent;
    float2 offset_ndc  = px / half_extent; // pixel space is +y down, matching Vulkan clip

    VSOut output;
    output.position = clip + float4(offset_ndc * clip.w, 0.0, 0.0);
    output.uv       = uv;
    output.color    = glyph.color;
    return output;
}
