// geng — the single translation unit that compiles the stb_truetype implementation. Kept apart so
// the library's other TUs include only the declarations. stb is a SYSTEM include, so its own
// warnings stay out of geng's -Werror / clang-tidy analysis.

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
