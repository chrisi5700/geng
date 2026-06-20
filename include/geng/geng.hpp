#ifndef GENG_GENG_HPP
#define GENG_GENG_HPP

/// @file
/// Umbrella header: include `<geng/geng.hpp>` to pull in geng's public API.
///
/// Convenience include aggregating the public surface — the @ref geng::Figure plot front end, its
/// data/series and styling vocabulary, the programmatic @ref geng::Interactor, and the windowed
/// @ref geng::WindowApp front end. Link the `geng::geng` CMake target (and `geng::geng-glfw` if you
/// use the window). Include the narrower per-type headers directly to keep a translation unit's
/// include cost down.

#include <geng/Bounds2D.hpp>
#include <geng/Figure.hpp>
#include <geng/FontAtlas.hpp>
#include <geng/Interactor.hpp>
#include <geng/Series.hpp>
#include <geng/Theme.hpp>
#include <geng/View.hpp>
#include <geng/Window.hpp>
#include <geng/WindowApp.hpp>

#endif // GENG_GENG_HPP
