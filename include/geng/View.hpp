#ifndef GENG_VIEW_HPP
#define GENG_VIEW_HPP

#include <geng/Bounds2D.hpp>
#include <glm/vec2.hpp>

namespace geng
{
/// A windowing-agnostic 2D view controller: it owns the data-space rectangle currently shown and
/// the math to move it. Every screen input is a *view-fraction* in [0,1]^2 — (0,0) the top-left of
/// the viewport, (1,1) the bottom-right — so the windowing layer converts pixels to fractions and
/// this type never sees an event, a pixel, or an OS handle. Feed @ref rect into a graph source to
/// drive the projection; call the mutators from input callbacks and re-`set` that source.
///
/// Header-only: the methods touch glm component members, which the library's analysis only tolerates
/// in headers (same as the demo's plot wiring).
class View
{
	 public:
	/// Start showing @p rect (e.g. the data fit).
	explicit View(Bounds2D rect) noexcept
		: m_rect(rect)
	{
	}

	/// The data-space rectangle currently visible.
	[[nodiscard]] Bounds2D rect() const noexcept { return m_rect; }

	/// Jump the view to @p target.
	void focus(const Bounds2D& target) noexcept { m_rect = target; }

	/// Scale the view by @p factor (a size multiplier; < 1 zooms in, > 1 zooms out) about the data
	/// point currently under @p anchor (a view-fraction), keeping that point fixed on screen.
	/// @p factor carries the rate: the caller maps input to it, e.g. `std::pow(0.9F, scroll_ticks)`.
	void zoom_at(glm::vec2 anchor, float factor) noexcept
	{
		const float width  = m_rect.max_x - m_rect.min_x;
		const float height = m_rect.max_y - m_rect.min_y;
		// Keep the data point under `anchor` fixed: solve new_rect so data_at(anchor) is unchanged
		// while the size scales by `factor`. (anchor.y is from the top, where data y is max_y.)
		const float new_min_x = m_rect.min_x + (anchor.x * width * (1.0F - factor));
		const float new_max_y = m_rect.max_y - (anchor.y * height * (1.0F - factor));
		m_rect				  = Bounds2D{.min_x = new_min_x,
										 .max_x = new_min_x + (factor * width),
										 .min_y = new_max_y - (factor * height),
										 .max_y = new_max_y};
	}

	/// Translate the view by @p delta, expressed as a fraction of the current view size, along the
	/// data axes (+x right, +y up).
	void pan(glm::vec2 delta) noexcept
	{
		const float shift_x = delta.x * (m_rect.max_x - m_rect.min_x);
		const float shift_y = delta.y * (m_rect.max_y - m_rect.min_y);
		m_rect.min_x += shift_x;
		m_rect.max_x += shift_x;
		m_rect.min_y += shift_y;
		m_rect.max_y += shift_y;
	}

	/// The data-space point under @p fraction (a view-fraction) — for cursor-to-data, tooltips and
	/// hit-testing.
	[[nodiscard]] glm::vec2 data_at(glm::vec2 fraction) const noexcept
	{
		return glm::vec2{m_rect.min_x + (fraction.x * (m_rect.max_x - m_rect.min_x)),
						 m_rect.max_y - (fraction.y * (m_rect.max_y - m_rect.min_y))};
	}

	 private:
	Bounds2D m_rect;
};
} // namespace geng

#endif // GENG_VIEW_HPP
