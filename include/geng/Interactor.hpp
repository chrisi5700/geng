#ifndef GENG_INTERACTOR_HPP
#define GENG_INTERACTOR_HPP

#include <glm/vec2.hpp>

namespace geng
{
class Figure;

/// Maps normalized input gestures onto a @ref Figure's view. Inputs are surface-independent [0,1]
/// fractions (origin top-left), so GLFW, Qt, or a test harness all feed the same logic — each
/// backend only converts its native event coordinates to fractions. A no-op while the figure's
/// @ref Fit mode is active (autoscale owns the view then).
class Interactor
{
	 public:
	explicit Interactor(Figure& figure) noexcept;

	/// Scroll/zoom toward the cursor. @p detents is the wheel delta (positive = zoom in).
	void on_scroll(glm::vec2 cursor_fraction, float detents);
	/// Drag-pan by a fractional delta of the viewport (typically while a button is held).
	void on_drag(glm::vec2 delta_fraction);

	void set_zoom_per_detent(float factor) noexcept; ///< Default 0.9 (≈10% per notch).
	void set_pan_enabled(bool enabled) noexcept;
	void set_zoom_enabled(bool enabled) noexcept;

	 private:
	Figure* m_figure;
	float	m_zoom_per_detent = 0.9F;
	bool	m_pan_enabled	  = true;
	bool	m_zoom_enabled	  = true;
};
} // namespace geng

#endif // GENG_INTERACTOR_HPP
