// Device-free tests for the categorical-axis CPU side: geng::detail::resolve_keyed (string keys ->
// (slot, value) points against a shared registry) and geng::detail::category_ticks (registry -> the
// x-axis tick labels). Whole glm objects are compared and indexing uses .at(), so this main TU keeps
// clang-tidy's union-access and unchecked-container-access rules happy.

#include <catch2/catch_test_macros.hpp>
#include <FigureScene.hpp> // geng::detail::{AxisTicks, resolve_keyed, category_ticks}
#include <glm/vec2.hpp>
#include <string>
#include <utility>
#include <vector>

TEST_CASE("resolve_keyed maps keys to slots in first-appearance order", "[categorical][keys]")
{
	SECTION("new keys are appended; a repeat reuses its slot")
	{
		std::vector<std::string>	 registry;
		const std::vector<glm::vec2> points = geng::detail::resolve_keyed(
			registry, {{"Adelie", 73.0F}, {"Chinstrap", 34.0F}, {"Gentoo", 61.0F}, {"Adelie", 5.0F}});

		REQUIRE(registry.size() == 3); // the duplicate did not grow the registry
		REQUIRE(registry.at(0) == "Adelie");
		REQUIRE(registry.at(1) == "Chinstrap");
		REQUIRE(registry.at(2) == "Gentoo");

		REQUIRE(points.size() == 4);
		REQUIRE(points.at(0) == glm::vec2{0.0F, 73.0F});
		REQUIRE(points.at(1) == glm::vec2{1.0F, 34.0F});
		REQUIRE(points.at(2) == glm::vec2{2.0F, 61.0F});
		REQUIRE(points.at(3) == glm::vec2{0.0F, 5.0F}); // "Adelie" again -> slot 0
	}

	SECTION("a second series resolves against the same registry (shared slots)")
	{
		std::vector<std::string> registry;
		(void)geng::detail::resolve_keyed(registry, {{"Adelie", 73.0F}, {"Chinstrap", 34.0F}, {"Gentoo", 61.0F}});

		// A different series, same categories in a different order plus a new one.
		const std::vector<glm::vec2> female =
			geng::detail::resolve_keyed(registry, {{"Gentoo", 58.0F}, {"Adelie", 73.0F}, {"Emperor", 2.0F}});

		REQUIRE(registry.size() == 4); // only "Emperor" was new
		REQUIRE(registry.at(3) == "Emperor");
		REQUIRE(female.at(0) == glm::vec2{2.0F, 58.0F}); // Gentoo keeps slot 2
		REQUIRE(female.at(1) == glm::vec2{0.0F, 73.0F}); // Adelie keeps slot 0
		REQUIRE(female.at(2) == glm::vec2{3.0F, 2.0F});	 // Emperor appended at slot 3
	}
}

TEST_CASE("category_ticks places each key at its index", "[categorical][ticks]")
{
	SECTION("an empty registry yields empty (numeric) ticks")
	{
		REQUIRE(geng::detail::category_ticks({}).empty());
	}

	SECTION("each category gets a tick at its slot, labelled with its name")
	{
		const geng::detail::AxisTicks ticks = geng::detail::category_ticks({"Adelie", "Chinstrap", "Gentoo"});
		REQUIRE_FALSE(ticks.empty());
		REQUIRE(ticks.positions.size() == 3);
		REQUIRE(ticks.positions.at(0) == 0.0F);
		REQUIRE(ticks.positions.at(2) == 2.0F);
		REQUIRE(ticks.labels.at(0) == "Adelie");
		REQUIRE(ticks.labels.at(2) == "Gentoo");
	}
}
