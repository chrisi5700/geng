#include <geng/Theme.hpp>

namespace geng
{
Theme Theme::dark()
{
	Theme theme; // the in-class defaults are already the dark palette's backdrop/grid/axes/labels.
	theme.palette = {
		{0.30F, 0.80F, 1.00F, 1.0F}, // cyan
		{0.30F, 0.85F, 0.45F, 1.0F}, // green
		{1.00F, 0.62F, 0.32F, 1.0F}, // orange
		{0.95F, 0.45F, 0.55F, 1.0F}, // rose
		{0.70F, 0.55F, 1.00F, 1.0F}, // violet
		{0.95F, 0.85F, 0.40F, 1.0F}, // amber
	};
	return theme;
}

Theme Theme::light()
{
	Theme theme;
	theme.background   = {0.98F, 0.98F, 0.99F, 1.0F};
	theme.grid.color   = {0.86F, 0.87F, 0.90F, 1.0F};
	theme.axes.color   = {0.45F, 0.47F, 0.55F, 1.0F};
	theme.labels.color = {0.25F, 0.27F, 0.32F, 1.0F};
	theme.palette	   = {
		{0.10F, 0.45F, 0.80F, 1.0F}, // blue
		{0.10F, 0.60F, 0.35F, 1.0F}, // green
		{0.85F, 0.45F, 0.10F, 1.0F}, // orange
		{0.80F, 0.20F, 0.35F, 1.0F}, // red
		{0.45F, 0.30F, 0.75F, 1.0F}, // purple
		{0.65F, 0.55F, 0.10F, 1.0F}, // olive
	};
	return theme;
}
} // namespace geng
