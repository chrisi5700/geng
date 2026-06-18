// geng — fullscreen-triangle vertex shader for the display pass. No vertex buffer: three
// vertices fabricated from SV_VertexID cover the whole viewport (draw with vertex_count = 3).
// Emits a UV in [0,1] with (0,0) at the top-left, matching the orientation the bake pass
// renders in (Vulkan y-down clip) — so the sampled cache texture needs no flip.

struct VSOut
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};

[shader("vertex")]
VSOut main(uint vid : SV_VertexID)
{
    // vid 0,1,2 -> uv (0,0),(2,0),(0,2) -> pos (-1,-1),(3,-1),(-1,3): a triangle covering the
    // [-1,1] square; its [0,1] UV sub-range maps exactly onto the screen.
    const float2 uv = float2((vid << 1) & 2, vid & 2);
    VSOut output;
    output.uv = uv;
    output.position = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    return output;
}
