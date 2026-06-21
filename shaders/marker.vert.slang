// geng — instanced point-marker rendering. Each scatter point is one instance (SV_InstanceID): a
// quad of 6 SV_VertexID vertices, fabricated from the builtins alone (no vertex buffer) and sized in
// *pixel* space around the point's projected position, so a marker keeps a constant on-screen size
// under any zoom or aspect ratio. The fragment shader shades the actual symbol (disc, ring, square,
// cross, …) from a signed distance field, so every shape shares this one pipeline and a single
// batched draw — the per-instance `shape` field selects which. (Orthographic projection ⇒ w == 1.)

struct Marker
{
    float2 center;       // data-space position
    float  size_px;      // bounding size in pixels (already scaled to the bake resolution)
    float  thickness_px; // stroke width for the hollow shapes
    float4 color;        // straight-alpha RGBA
    uint   shape;        // geng::MarkerShape value
    uint   pad0;
    uint   pad1;
    uint   pad2;
};
[[vk::binding(0, 0)]] StructuredBuffer<Marker> markers;

struct PushData
{
    float4x4 view_proj; // data-space → clip; glm is column-major, so mul(v, M) == M * v
    float2   extent;    // framebuffer size in pixels
};
[[vk::push_constant]] PushData pc;

struct VSOut
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 local_px : TEXCOORD0;       // signed pixels from the marker center
    [[vk::location(1)]] float4 color : COLOR0;
    [[vk::location(2)]] float half_px : TEXCOORD1;         // half the bounding size, in pixels
    [[vk::location(3)]] float thickness_px : TEXCOORD2;
    [[vk::location(4)]] nointerpolation uint shape : TEXCOORD3;
};

// A unit quad as two triangles; corner ∈ {-1,+1}² scales to the padded marker half-size.
static const float2 QUAD[6] = {
    float2(-1.0, -1.0), float2(1.0, -1.0), float2(-1.0, 1.0),
    float2(-1.0, 1.0),  float2(1.0, -1.0), float2(1.0, 1.0),
};

[shader("vertex")]
VSOut main(uint vid : SV_VertexID, uint inst : SV_InstanceID)
{
    Marker m = markers[inst];

    // Grow the quad past the shape so the SDF anti-aliasing fringe (and any stroke) has room.
    float half_px = 0.5 * m.size_px;
    float pad_px  = half_px + m.thickness_px + 2.0;

    float4 clip        = mul(float4(m.center, 0.0, 1.0), pc.view_proj);
    float2 half_extent = 0.5 * pc.extent;
    float2 corner      = QUAD[vid];
    float2 offset_px   = corner * pad_px;
    float2 offset_ndc  = offset_px / half_extent;

    VSOut output;
    output.position     = clip + float4(offset_ndc * clip.w, 0.0, 0.0);
    output.local_px     = offset_px;
    output.color        = m.color;
    output.half_px      = half_px;
    output.thickness_px = m.thickness_px;
    output.shape        = m.shape;
    return output;
}
