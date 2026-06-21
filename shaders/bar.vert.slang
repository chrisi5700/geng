// geng — instanced bar rendering. Each bar is one instance (SV_InstanceID): an axis-aligned rectangle
// in *data* space, fabricated from 6 SV_VertexID vertices (two triangles) with no vertex buffer. The
// four corners are [x_center ± half_width] × [y0, y1] and project straight through view_proj, so bars
// are sized in data units — they scale with zoom, unlike the pixel-sized markers. (Ortho ⇒ w == 1.)

struct Bar
{
    float  x_center;
    float  half_width;
    float  y0; // foot of the bar (baseline)
    float  y1; // head of the bar (value)
    float4 color;
};
[[vk::binding(0, 0)]] StructuredBuffer<Bar> bars;

struct PushData
{
    float4x4 view_proj; // data-space → clip; glm is column-major, so mul(v, M) == M * v
};
[[vk::push_constant]] PushData pc;

struct VSOut
{
    float4 position : SV_Position;
    [[vk::location(0)]] float4 color : COLOR0;
};

// corner.x: -1 => left edge, +1 => right edge; corner.y: 0 => baseline (y0), 1 => value (y1).
static const float2 CORNERS[6] = {
    float2(-1.0, 0.0), float2(1.0, 0.0), float2(-1.0, 1.0),
    float2(-1.0, 1.0), float2(1.0, 0.0), float2(1.0, 1.0),
};

[shader("vertex")]
VSOut main(uint vid : SV_VertexID, uint inst : SV_InstanceID)
{
    Bar    b      = bars[inst];
    float2 corner = CORNERS[vid];
    float  x      = b.x_center + (corner.x * b.half_width);
    float  y      = lerp(b.y0, b.y1, corner.y);

    VSOut output;
    output.position = mul(float4(x, y, 0.0, 1.0), pc.view_proj);
    output.color    = b.color;
    return output;
}
