#include <geng/Bounds2D.hpp>
#include <glm/ext/matrix_clip_space.hpp> // glm::ortho

namespace geng
{
glm::mat4 ortho_view(const Bounds2D& bounds)
{
	// Passing the top/bottom arguments swapped (max_y as "bottom", min_y as "top") flips the Y
	// axis inside the projection, so data +Y points up on screen under Vulkan's Y-down clip
	// space — no flip matrix, no element-wise access. With GLM_FORCE_DEPTH_ZERO_TO_ONE (set on
	// this target to match veng) the depth range is [0, 1]; our line vertices sit at z = 0.
	return glm::ortho(bounds.min_x, bounds.max_x, bounds.max_y, bounds.min_y, -1.0F, 1.0F);
}

Bounds2D aspect_fit(const Bounds2D& bounds, float aspect)
{
	if (aspect <= 0.0F)
	{
		return bounds;
	}
	const float center_x = (bounds.min_x + bounds.max_x) * 0.5F;
	const float center_y = (bounds.min_y + bounds.max_y) * 0.5F;
	float		half_w	 = (bounds.max_x - bounds.min_x) * 0.5F;
	float		half_h	 = (bounds.max_y - bounds.min_y) * 0.5F;
	if (half_w > half_h * aspect)
	{
		half_h = half_w / aspect; // bounds wider than the viewport — grow the height
	}
	else
	{
		half_w = half_h * aspect; // bounds taller — grow the width
	}
	return Bounds2D{
		.min_x = center_x - half_w, .max_x = center_x + half_w, .min_y = center_y - half_h, .max_y = center_y + half_h};
}

Bounds2D reframe_aspect(const Bounds2D& bounds, float aspect)
{
	if (aspect <= 0.0F)
	{
		return bounds;
	}
	const float center_x = (bounds.min_x + bounds.max_x) * 0.5F;
	const float half_w	 = (bounds.max_y - bounds.min_y) * 0.5F * aspect;
	return Bounds2D{
		.min_x = center_x - half_w, .max_x = center_x + half_w, .min_y = bounds.min_y, .max_y = bounds.max_y};
}
} // namespace geng
