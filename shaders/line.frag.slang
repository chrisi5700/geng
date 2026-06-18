// geng — per-segment colour fill for the line quads. The colour is carried from the vertex shader
// (looked up per segment from the `colors` buffer), so the coordinate axes and the curve share one
// pipeline and one draw while keeping distinct colours.

struct PSIn
{
    [[vk::location(0)]] float4 color : COLOR0;
};

[shader("fragment")]
float4 main(PSIn input) : SV_Target
{
    return input.color;
}
