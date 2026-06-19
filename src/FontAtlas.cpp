#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <geng/FontAtlas.hpp>
#include <ios>
#include <stb_truetype.h>
#include <utility>
#include <vector>
#include <veng/context/Context.hpp>

namespace geng
{
namespace
{
constexpr std::uint32_t ATLAS_DIM  = 1024; // large enough for the glyphs at the demo's pixel height
constexpr int			FIRST_CHAR = 32;
constexpr int			NUM_CHARS  = 95; // printable ASCII 32..126
} // namespace

FontAtlas::FontAtlas(veng::assets::Texture atlas, std::array<Baked, GLYPH_COUNT> glyphs, std::uint32_t width,
					 std::uint32_t height, float pixel_height) noexcept
	: m_atlas(std::move(atlas))
	, m_glyphs(glyphs)
	, m_atlas_w(width)
	, m_atlas_h(height)
	, m_pixel_height(pixel_height)
{
}

std::expected<FontAtlas, FontError> FontAtlas::create(const veng::Context& ctx, const std::string& ttf_path,
													  float pixel_height)
{
	std::ifstream file(ttf_path, std::ios::binary | std::ios::ate);
	if (!file.is_open())
	{
		return std::unexpected(FontError::FILE_UNREADABLE);
	}
	const std::streamsize size = file.tellg();
	file.seekg(0);
	std::vector<unsigned char> ttf(static_cast<std::size_t>(size));
	if (!file.read(reinterpret_cast<char*>(ttf.data()), size))
	{
		return std::unexpected(FontError::FILE_UNREADABLE);
	}

	// Rasterise the glyphs into a single-channel coverage bitmap.
	std::vector<unsigned char>			   coverage(static_cast<std::size_t>(ATLAS_DIM) * ATLAS_DIM);
	std::array<stbtt_bakedchar, NUM_CHARS> baked{};
	const int rows = stbtt_BakeFontBitmap(ttf.data(), 0, pixel_height, coverage.data(), static_cast<int>(ATLAS_DIM),
										  static_cast<int>(ATLAS_DIM), FIRST_CHAR, NUM_CHARS, baked.data());
	if (rows <= 0)
	{
		return std::unexpected(FontError::BAKE_FAILED);
	}

	// Expand to RGBA8: opaque white with the coverage carried in the alpha channel.
	std::vector<std::byte> rgba(coverage.size() * 4);
	for (std::size_t idx = 0; idx < coverage.size(); ++idx)
	{
		rgba.at((idx * 4) + 0) = std::byte{255};
		rgba.at((idx * 4) + 1) = std::byte{255};
		rgba.at((idx * 4) + 2) = std::byte{255};
		rgba.at((idx * 4) + 3) = std::byte{coverage.at(idx)};
	}
	auto texture =
		veng::assets::Texture::from_pixels(ctx, rgba, ATLAS_DIM, ATLAS_DIM, veng::assets::ColorSpace::Linear);
	if (!texture.has_value())
	{
		return std::unexpected(FontError::UPLOAD);
	}

	std::array<Baked, GLYPH_COUNT> glyphs{};
	for (std::size_t idx = 0; idx < GLYPH_COUNT; ++idx)
	{
		const stbtt_bakedchar& src = baked.at(idx);
		glyphs.at(idx)			   = Baked{.x0		 = static_cast<float>(src.x0),
										   .y0		 = static_cast<float>(src.y0),
										   .x1		 = static_cast<float>(src.x1),
										   .y1		 = static_cast<float>(src.y1),
										   .xoff	 = src.xoff,
										   .yoff	 = src.yoff,
										   .xadvance = src.xadvance};
	}
	return FontAtlas(std::move(texture.value()), glyphs, ATLAS_DIM, ATLAS_DIM, pixel_height);
}

std::vector<GlyphQuad> FontAtlas::layout(std::string_view text) const
{
	std::vector<GlyphQuad> quads;
	quads.reserve(text.size());
	float pen_x = 0.0F;
	for (const char chr : text)
	{
		const int code = static_cast<unsigned char>(chr);
		if (code < FIRST_CHAR || code >= FIRST_CHAR + NUM_CHARS)
		{
			continue;
		}
		const Baked& glyph	 = m_glyphs.at(static_cast<std::size_t>(code - FIRST_CHAR));
		const float	 round_x = std::floor(pen_x + glyph.xoff + 0.5F);
		const float	 round_y = std::floor(glyph.yoff + 0.5F); // pen baseline at y = 0
		if (glyph.x1 > glyph.x0 && glyph.y1 > glyph.y0)		  // skip empty glyphs (e.g. space)
		{
			quads.push_back(
				GlyphQuad{.min_px = {round_x, round_y},
						  .max_px = {round_x + (glyph.x1 - glyph.x0), round_y + (glyph.y1 - glyph.y0)},
						  .uv0 = {glyph.x0 / static_cast<float>(m_atlas_w), glyph.y0 / static_cast<float>(m_atlas_h)},
						  .uv1 = {glyph.x1 / static_cast<float>(m_atlas_w), glyph.y1 / static_cast<float>(m_atlas_h)}});
		}
		pen_x += glyph.xadvance;
	}
	return quads;
}
} // namespace geng
