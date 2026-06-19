#ifndef GENG_BOUNDS2D_HPP
#define GENG_BOUNDS2D_HPP

#include <glm/mat4x4.hpp>

namespace geng
{
/// An axis-aligned data-space rectangle: the region of the plane a plot maps onto the screen.
struct Bounds2D
{
	float min_x;
	float max_x;
	float min_y;
	float max_y;

	/// Value equality so a reactive graph edge carrying a `Bounds2D` can memoize: a recomputed
	/// bound equal to the cached one stops propagation (no needless re-bake / re-projection).
	friend bool operator==(const Bounds2D&, const Bounds2D&) noexcept = default;
};

/// Build the data-space → Vulkan-clip-space transform that maps @p bounds onto the full clip
/// volume, with +Y pointing up on screen (Vulkan's clip Y points down, so the mapping is
/// flipped). Multiply a `vec4(x, y, 0, 1)` data position by the result to get clip space.
[[nodiscard]] glm::mat4 ortho_view(const Bounds2D& bounds);

/// Expand @p bounds to viewport aspect @p aspect (width / height), keeping its centre, so it maps to
/// the viewport with equal scale on both axes (circles stay round). Only grows @p bounds, so all of
/// it stays visible — for the initial framing or focusing a region.
[[nodiscard]] Bounds2D aspect_fit(const Bounds2D& bounds, float aspect);

/// Reframe @p bounds to viewport aspect @p aspect, keeping its centre and height (adjusting width).
/// Idempotent / drift-free under repeated calls — for keeping equal scale across window resizes.
[[nodiscard]] Bounds2D reframe_aspect(const Bounds2D& bounds, float aspect);
} // namespace geng

#endif // GENG_BOUNDS2D_HPP
