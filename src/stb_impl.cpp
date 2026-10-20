// stb_truetype implementation: raylib builds its own copy with STBTT_STATIC, so the stbtt_* symbols used by axFont.h
// (FontToPath, SDF path fonts) are generated here.
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
