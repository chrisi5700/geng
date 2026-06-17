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
} // namespace geng
