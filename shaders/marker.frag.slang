// geng — signed-distance shading for point markers. `local_px` is the fragment's signed-pixel offset
// from the marker center; each shape evaluates an SDF in those pixel units (negative inside the
// symbol), and a smoothstep one pixel-derivative wide across the zero crossing gives an analytic,
// resolution-independent anti-aliased coverage. The output is straight-alpha so the marker layer
// composites over the plot. SHAPE_* mirror geng::MarkerShape (include/geng/Series.hpp) by value —
// keep the two in sync.

static const uint SHAPE_POINT    = 0u; // filled disc
static const uint SHAPE_CIRCLE   = 1u; // hollow ring
static const uint SHAPE_SQUARE   = 2u; // filled square
static const uint SHAPE_DIAMOND  = 3u; // filled diamond
static const uint SHAPE_CROSS    = 4u; // ✕ strokes
static const uint SHAPE_PLUS     = 5u; // ✚ strokes
static const uint SHAPE_TRIANGLE = 6u; // filled upward triangle

struct PSIn
{
    [[vk::location(0)]] float2 local_px : TEXCOORD0;
    [[vk::location(1)]] float4 color : COLOR0;
    [[vk::location(2)]] float half_px : TEXCOORD1;
    [[vk::location(3)]] float thickness_px : TEXCOORD2;
    [[vk::location(4)]] nointerpolation uint shape : TEXCOORD3;
};

// Exact box SDF (negative inside), b is the half-extent.
float sd_box(float2 p, float2 b)
{
    float2 d = abs(p) - b;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

// Upward equilateral triangle (Inigo Quilez), r ~ the circumradius. local_px is +y-down (Vulkan
// clip), so flip y to make the apex point up on screen.
float sd_triangle(float2 p, float r)
{
    const float k = sqrt(3.0);
    p.y = -p.y;
    p.x = abs(p.x) - r;
    p.y = p.y + r / k;
    if (p.x + k * p.y > 0.0)
        p = float2(p.x - k * p.y, -k * p.x - p.y) / 2.0;
    p.x -= clamp(p.x, -2.0 * r, 0.0);
    return -length(p) * sign(p.y);
}

float shape_sdf(uint shape, float2 p, float half_px, float thickness_px)
{
    float t = 0.5 * thickness_px;
    switch (shape)
    {
    case SHAPE_CIRCLE:
        // Ring whose outer edge sits at half_px, centered on the stroke.
        return abs(length(p) - (half_px - t)) - t;
    case SHAPE_SQUARE:
        return sd_box(p, float2(half_px, half_px));
    case SHAPE_DIAMOND:
        return (abs(p.x) + abs(p.y)) - half_px; // L1 ball = diamond
    case SHAPE_CROSS:
    {
        const float k = 0.70710678; // 1/sqrt(2): rotate the plus by 45° into an ✕
        float2 r = float2(k * (p.x - p.y), k * (p.x + p.y));
        return min(sd_box(r, float2(half_px, t)), sd_box(r, float2(t, half_px)));
    }
    case SHAPE_PLUS:
        return min(sd_box(p, float2(half_px, t)), sd_box(p, float2(t, half_px)));
    case SHAPE_TRIANGLE:
        return sd_triangle(p, half_px * 0.86); // 0.86·circumradius keeps the apex within half_px
    default: // SHAPE_POINT
        return length(p) - half_px;
    }
}

[shader("fragment")]
float4 main(PSIn input) : SV_Target
{
    float d        = shape_sdf(input.shape, input.local_px, input.half_px, input.thickness_px);
    float aa       = max(fwidth(d), 1e-4);
    float coverage = 1.0 - smoothstep(-aa, aa, d);
    return float4(input.color.rgb, input.color.a * coverage);
}
