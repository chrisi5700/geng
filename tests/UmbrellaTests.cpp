// Compile-guard for geng's public umbrella header. If <geng/geng.hpp> ever stops compiling
// standalone, this translation unit fails to build. Compile-time assertions only — no device.

#include <catch2/catch_test_macros.hpp>
#include <geng/geng.hpp>
#include <type_traits>

TEST_CASE("the geng umbrella header compiles and exposes the public API", "[umbrella][api]")
{
	STATIC_REQUIRE(std::is_class_v<geng::Figure>);	   // plot front end
	STATIC_REQUIRE(std::is_class_v<geng::WindowApp>);  // windowed front end
	STATIC_REQUIRE(std::is_class_v<geng::Theme>);	   // styling
	STATIC_REQUIRE(std::is_class_v<geng::SeriesId>);   // data/series vocabulary
	STATIC_REQUIRE(std::is_class_v<geng::Interactor>); // programmatic input mapping
}
