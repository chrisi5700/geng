#ifndef GENG_EXAMPLE_POPULATION_DATA_HPP
#define GENG_EXAMPLE_POPULATION_DATA_HPP

// Shared helper for the population example(s): parse the World Bank country_data.xml (a uniform,
// machine-generated dump, so a line-wise scan is enough — no XML dependency), drop the regional and
// income-group aggregates, and expose the most-populated countries for a given year (as a bar chart on
// a geng::Figure) with a stable per-country color.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <functional>
#include <geng/Figure.hpp>
#include <glm/vec4.hpp>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace population
{
/// One country's population in a single year, ready to draw (value already in millions of people).
struct Bar
{
	std::string name;
	float		millions = 0.0F;
	glm::vec4	color;
};

/// All parsed records, grouped by year, with the year span that was seen.
struct Dataset
{
	std::unordered_map<int, std::vector<std::pair<std::string, double>>> by_year;
	int																	 min_year = 0;
	int																	 max_year = 0;
};

/// World Bank "Country or Area" keys that are regional / income aggregates, not countries. Without this
/// filter "World", "Low & middle income", the regions, etc. would swamp any most-populated ranking.
inline bool is_aggregate(std::string_view key)
{
	static const std::unordered_set<std::string_view> AGGREGATES{
		"AFE", "AFW", "ARB", "CEB", "CSS", "EAP", "EAR", "EAS", "ECA", "ECS", "EMU", "EUU", "FCS",
		"HIC", "HPC", "IBD", "IBT", "IDA", "IDB", "IDX", "INX", "LAC", "LCN", "LDC", "LIC", "LMC",
		"LMY", "LTE", "MEA", "MIC", "MNA", "NAC", "OED", "OSS", "PRE", "PSS", "PST", "SAS", "SSA",
		"SSF", "SST", "TEA", "TEC", "TLA", "TMN", "TSA", "TSS", "UMC", "WLD"};
	return AGGREGATES.contains(key);
}

/// A stable, distinct color per country: the name's hash spread around the hue wheel, so a country
/// keeps its color as it moves up and down the ranking across years. HSV -> RGB is done component-wise
/// (no glm member writes) at a fixed saturation/value.
inline glm::vec4 country_color(const std::string& name)
{
	const std::size_t hash = std::hash<std::string>{}(name);
	const float		  hue  = static_cast<float>(hash % 360U) / 360.0F;

	const float sat		= 0.55F;
	const float val		= 0.90F;
	const float sextant = hue * 6.0F;
	const float base	= std::floor(sextant);
	const float frac	= sextant - base;
	const float fall	= val * (1.0F - sat);
	const float descend = val * (1.0F - (frac * sat));
	const float ascend	= val * (1.0F - ((1.0F - frac) * sat));
	switch (static_cast<int>(base) % 6)
	{
		case 0: return glm::vec4{val, ascend, fall, 1.0F};
		case 1: return glm::vec4{descend, val, fall, 1.0F};
		case 2: return glm::vec4{fall, val, ascend, 1.0F};
		case 3: return glm::vec4{fall, descend, val, 1.0F};
		case 4: return glm::vec4{ascend, fall, val, 1.0F};
		default: return glm::vec4{val, fall, descend, 1.0F};
	}
}

namespace detail
{
/// The text between a `<field …>` tag and its `</field>`; empty if the element is self-closing/empty.
inline std::string field_text(const std::string& line)
{
	const auto open	 = line.find('>');
	const auto close = line.find("</field>");
	if (open == std::string::npos || close == std::string::npos || close <= open)
	{
		return {};
	}
	return line.substr(open + 1, close - open - 1);
}

/// The value of the `key="…"` attribute on a line, or empty if there is none.
inline std::string field_key(const std::string& line)
{
	const auto pos = line.find("key=\"");
	if (pos == std::string::npos)
	{
		return {};
	}
	const auto start = pos + 5; // past key="
	const auto end	 = line.find('"', start);
	return line.substr(start, end - start);
}
} // namespace detail

/// Parse country_data.xml at @p path. Returns nullopt if the file cannot be opened. Aggregates and
/// records with an empty population value are dropped.
inline std::optional<Dataset> load(const std::string& path)
{
	std::ifstream file(path);
	if (!file)
	{
		return std::nullopt;
	}

	Dataset		data;
	std::string line;
	std::string name;
	std::string key;
	int			year	= 0;
	bool		country = false;
	bool		dated	= false;
	bool		first	= true;
	while (std::getline(file, line))
	{
		if (line.find("Country or Area") != std::string::npos)
		{
			key		= detail::field_key(line);
			name	= detail::field_text(line);
			country = true;
		}
		else if (line.find("name=\"Year\"") != std::string::npos)
		{
			const std::string text = detail::field_text(line);
			year				   = text.empty() ? 0 : std::stoi(text);
			dated				   = !text.empty();
		}
		else if (line.find("name=\"Value\"") != std::string::npos)
		{
			const std::string text = detail::field_text(line);
			if (country && dated && !text.empty() && !is_aggregate(key))
			{
				data.by_year[year].emplace_back(name, std::stod(text));
				data.min_year = first ? year : std::min(data.min_year, year);
				data.max_year = first ? year : std::max(data.max_year, year);
				first		  = false;
			}
			dated = false; // each record carries one value; reset for the next
		}
	}
	if (first)
	{
		return std::nullopt; // parsed nothing usable
	}
	return data;
}

/// The @p count most-populated countries in @p year, descending, ready to draw.
inline std::vector<Bar> top_n(const Dataset& data, int year, std::size_t count)
{
	std::vector<Bar> bars;
	const auto		 found = data.by_year.find(year);
	if (found == data.by_year.end())
	{
		return bars;
	}
	std::vector<std::pair<std::string, double>> ranked = found->second;
	std::ranges::sort(ranked, [](const auto& lhs, const auto& rhs) { return lhs.second > rhs.second; });

	bars.reserve(std::min(count, ranked.size()));
	for (const auto& [name, value] : ranked | std::views::take(count))
	{
		bars.push_back(Bar{.name = name, .millions = static_cast<float>(value / 1.0e6), .color = country_color(name)});
	}
	return bars;
}

/// Drive @p series on @p figure to show the top @p count countries for @p year (pass a count larger
/// than the country set to draw them all). Resets the categorical x-axis first — each year is a fresh,
/// re-sorted set of countries — but does NOT touch the view, so the caller's scroll/zoom position
/// carries across year changes.
inline void show_year(geng::Figure& figure, geng::SeriesId series, const Dataset& data, int year, std::size_t count)
{
	const std::vector<Bar>					   bars = top_n(data, year, count);
	std::vector<std::pair<std::string, float>> keyed;
	std::vector<glm::vec4>					   colors;
	keyed.reserve(bars.size());
	colors.reserve(bars.size());
	for (const Bar& bar : bars)
	{
		keyed.emplace_back(bar.name, bar.millions);
		colors.push_back(bar.color);
	}
	figure.clear_categories(); // this year's countries number from 0 again
	figure.set_data(series, keyed);
	figure.set_point_colors(series, std::move(colors));
}

/// The population (millions) of the single most-populated country in @p year — the natural top of the
/// y-axis for framing the chart. Returns 0 if the year is absent.
inline float peak_millions(const Dataset& data, int year)
{
	const std::vector<Bar> top = top_n(data, year, 1);
	return top.empty() ? 0.0F : top.front().millions;
}
} // namespace population

#endif // GENG_EXAMPLE_POPULATION_DATA_HPP
