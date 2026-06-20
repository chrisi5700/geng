// Default LeakSanitizer suppressions, compiled into each example so they run clean under the ASan
// preset with no LSAN_OPTIONS ceremony. The suppressed leaks are third-party and unavoidable: the
// Vulkan loader/driver's one-time global init (reached through vkCreateInstance) and libdbus session
// registration. The matchers are scoped to those libraries, so a leak originating in geng or example
// code is still reported in full.
//
// __lsan_default_suppressions / __lsan_default_options are weak hooks the sanitizer runtime calls at
// startup; they are reserved names by design (hence the NOLINT). Defined unconditionally — in a build
// without the sanitizer they are simply unused.

// NOLINTNEXTLINE(bugprone-reserved-identifier,readability-identifier-naming,cert-dcl37-c,cert-dcl51-cpp)
extern "C" const char* __lsan_default_suppressions()
{
	return "leak:libvulkan\n"
		   "leak:libdbus\n"
		   "leak:vkCreateInstance\n";
}

// NOLINTNEXTLINE(bugprone-reserved-identifier,readability-identifier-naming,cert-dcl37-c,cert-dcl51-cpp)
extern "C" const char* __lsan_default_options()
{
	return "print_suppressions=0";
}
