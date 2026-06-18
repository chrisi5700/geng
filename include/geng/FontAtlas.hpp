#ifndef GENG_FONT_ATLAS_HPP
#define GENG_FONT_ATLAS_HPP

#include <array>
#include <cstdint>
#include <expected>
#include <glm/vec2.hpp>
#include <string>
#include <string_view>
#include <vector>
#include <veng/assets/Texture.hpp>
#include <veng/gpu/ImageRef.hpp>

namespace veng
{
class Context;
} // namespace veng

namespace geng
{
/// One laid-out glyph: its quad in pixel space relative to the text origin (pen at (0,0), baseline
/// at y = 0, +y pointing down) and its UV rectangle in the atlas.
struct GlyphQuad
{
	glm::vec2 min_px;
	glm::vec2 max_px;
	glm::vec2 uv0;
	glm::vec2 uv1;
};

enum class FontError : std::uint8_t
{
	FILE_UNREADABLE, ///< The .ttf file could not be opened or read.
	BAKE_FAILED,	 ///< stb_truetype could not fit the glyphs into the atlas.
	UPLOAD,			 ///< The atlas texture upload failed.
};

[[nodiscard]] constexpr std::string_view to_string(FontError error) noexcept
{
	switch (error)
	{
		case FontError::FILE_UNREADABLE: return "font file could not be read";
		case FontError::BAKE_FAILED: return "font glyphs did not fit the atlas";
		case FontError::UPLOAD: return "font atlas upload failed";
	}
	return "unknown font error";
}

/// A baked monospaced bitmap-font atlas: printable ASCII (32..126) rasterised into one GPU texture
/// (glyph coverage in the alpha channel) plus per-glyph metrics for CPU layout. Owns its atlas
/// texture, so keep it alive as long as anything samples the atlas. Move-only.
class FontAtlas
{
	 public:
	/// Bake @p ttf_path at @p pixel_height into an atlas uploaded through @p ctx.
	[[nodiscard]] static std::expected<FontAtlas, FontError> create(const veng::Context& ctx,
																	const std::string& ttf_path, float pixel_height);

	FontAtlas(FontAtlas&&)				   = default;
	FontAtlas& operator=(FontAtlas&&)	   = default;
	FontAtlas(const FontAtlas&)			   = delete;
	FontAtlas& operator=(const FontAtlas&) = delete;
	~FontAtlas()						   = default;

	/// The atlas image ref to feed a graph source node for sampling.
	[[nodiscard]] veng::gpu::ImageRef atlas_ref() const noexcept { return m_atlas.ref(); }

	/// The pixel height the atlas was baked at (a glyph's nominal size, in atlas pixels).
	[[nodiscard]] float pixel_height() const noexcept { return m_pixel_height; }

	/// Lay out @p text from a pen at the origin; one quad per drawable glyph (spaces advance only).
	[[nodiscard]] std::vector<GlyphQuad> layout(std::string_view text) const;

	 private:
	/// Per-glyph metrics in atlas-pixel space (the subset of `stbtt_bakedchar` layout needs).
	struct Baked
	{
		float x0;
		float y0;
		float x1;
		float y1;
		float xoff;
		float yoff;
		float xadvance;
	};
	static constexpr std::size_t GLYPH_COUNT = 95; ///< ASCII 32..126.

	FontAtlas(veng::assets::Texture atlas, std::array<Baked, GLYPH_COUNT> glyphs, std::uint32_t width,
			  std::uint32_t height, float pixel_height) noexcept;

	veng::assets::Texture		   m_atlas;
	std::array<Baked, GLYPH_COUNT> m_glyphs;
	std::uint32_t				   m_atlas_w;
	std::uint32_t				   m_atlas_h;
	float						   m_pixel_height;
};
} // namespace geng

#endif // GENG_FONT_ATLAS_HPP
