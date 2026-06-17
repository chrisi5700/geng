// geng — flat-colour fill for the line quads. The colour is fixed for this first slice; it
// becomes a per-curve uniform/push constant once the public API takes shape.

[shader("fragment")]
float4 main() : SV_Target
{
    return float4(0.30, 0.80, 1.0, 1.0); // cyan
}
