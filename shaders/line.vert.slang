// geng — thin-quad line rendering, done entirely in the vertex shader (no GPU line primitive, so
// it is safe on older hardware that lacks reliable wide-line support).
//
// Line-LIST: the draw is instanced, one *segment* per instance (SV_InstanceID). A segment is two
// independent endpoints — positions[2*seg] and positions[2*seg + 1] — drawn as 6 SV_VertexID
// vertices = two triangles = one quad. There is no vertex buffer; the geometry is fabricated from
// the two builtins alone. A parallel `colors` buffer gives each segment its own colour, so the
// coordinate axes (one colour) and the curve (another) share a single buffer, draw and pass —
// layered by instance order (later instances overwrite, so the curve goes on top of the axes).
//
// Each segment quad is offset in *pixel* space by a constant half-width and converted back to clip
// space, so the line keeps a constant pixel thickness under any zoom or aspect ratio.
// (Orthographic projection ⇒ w == 1, so there is no perspective-divide subtlety.)

[[vk::binding(0, 0)]] StructuredBuffer<float2> positions; // two endpoints per segment
[[vk::binding(1, 0)]] StructuredBuffer<float4> colors;    // one colour per segment

struct PushData
{
    float4x4 view_proj;  // data-space → clip; glm is column-major, so mul(v, M) == M * v
    float2   extent;     // framebuffer size in pixels
    float    half_width; // half the line width, in pixels
};
[[vk::push_constant]] PushData pc;

struct VSOut
{
    float4 position : SV_Position;
    [[vk::location(0)]] float4 color : COLOR0;
};

// corner.x selects the endpoint (0 ⇒ P0, 1 ⇒ P1); corner.y is the side (+1 / -1).
static const float2 CORNERS[6] = {
    float2(0.0, +1.0), float2(0.0, -1.0), float2(1.0, +1.0),
    float2(1.0, +1.0), float2(0.0, -1.0), float2(1.0, -1.0),
};

[shader("vertex")]
VSOut main(uint vid : SV_VertexID, uint seg : SV_InstanceID)
{
    VSOut output;

    float2 p0 = positions[2u * seg];
    float2 p1 = positions[2u * seg + 1u];

    float4 clip0 = mul(float4(p0, 0.0, 1.0), pc.view_proj);
    float4 clip1 = mul(float4(p1, 0.0, 1.0), pc.view_proj);

    // Build the offset direction in pixel space so the width is pixel-constant.
    float2 half_extent = 0.5 * pc.extent;
    float2 px0         = clip0.xy / clip0.w * half_extent;
    float2 px1         = clip1.xy / clip1.w * half_extent;

    float2 along = px1 - px0;
    float  len   = length(along);
    along        = len > 1e-6 ? along / len : float2(1.0, 0.0);
    float2 normal = float2(-along.y, along.x);

    float2 corner     = CORNERS[vid];
    float4 base       = corner.x < 0.5 ? clip0 : clip1;
    float2 offset_px  = normal * (pc.half_width * corner.y);
    float2 offset_ndc = offset_px / half_extent;

    output.position = base + float4(offset_ndc * base.w, 0.0, 0.0);
    output.color    = colors[seg];
    return output;
}
