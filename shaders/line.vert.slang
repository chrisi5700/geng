// geng — thin-quad polyline expansion, done entirely in the vertex shader (no GPU line
// primitive, so it is safe on older hardware that lacks reliable wide-line support).
//
// The polyline's points live in a StructuredBuffer. The draw is instanced: one *segment* per
// instance (SV_InstanceID), each drawn as 6 SV_VertexID vertices = two triangles = one quad.
// There is no vertex buffer — the geometry is fabricated from the two builtins alone.
//
// Each segment quad is offset in *pixel* space by a constant half-width and then converted
// back to clip space, so the line keeps a constant pixel thickness under any zoom or aspect
// ratio. (Orthographic projection ⇒ w == 1, so there is no perspective-divide subtlety.)

[[vk::binding(0, 0)]] StructuredBuffer<float2> points;

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

    uint count  = 0;
    uint stride = 0;
    points.GetDimensions(count, stride);

    // set_instances_from gives one instance per point (N), but a polyline of N points has only
    // N-1 segments — collapse the extra trailing instance to a degenerate vertex outside clip.
    if (seg + 1u >= count)
    {
        output.position = float4(0.0, 0.0, 2.0, 1.0); // z = 2 (beyond the far plane) ⇒ clipped
        return output;
    }

    float2 p0 = points[seg];
    float2 p1 = points[seg + 1u];

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
    return output;
}
