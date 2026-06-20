#include <cmath>
#include <geng/Figure.hpp>
#include <geng/Interactor.hpp>
#include <geng/Theme.hpp>

namespace geng
{
Interactor::Interactor(Figure& figure) noexcept
	: m_figure(&figure)
{
}

void Interactor::on_scroll(glm::vec2 cursor_fraction, float detents)
{
	if (!m_zoom_enabled || m_figure->autoscale() != Fit::OFF)
	{
		return; // autoscale owns the view; manual zoom is suppressed.
	}
	m_figure->zoom(cursor_fraction, std::pow(m_zoom_per_detent, detents));
}

void Interactor::on_drag(glm::vec2 delta_fraction)
{
	if (!m_pan_enabled || m_figure->autoscale() != Fit::OFF)
	{
		return;
	}
	// Grab-and-drag: content follows the cursor, so the view moves opposite in x; screen-y is already
	// the data-down direction the view expects. (Component-wise multiply avoids glm union access.)
	m_figure->pan(delta_fraction * glm::vec2{-1.0F, 1.0F});
}

void Interactor::set_zoom_per_detent(float factor) noexcept
{
	m_zoom_per_detent = factor;
}
void Interactor::set_pan_enabled(bool enabled) noexcept
{
	m_pan_enabled = enabled;
}
void Interactor::set_zoom_enabled(bool enabled) noexcept
{
	m_zoom_enabled = enabled;
}
} // namespace geng
