#ifndef AXDRAWBOT_H
#define AXDRAWBOT_H

#include "raylib.h"
#include "rlgl.h"
// #include "raymath.h"
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <stack>
#include <functional>
// #include <algorithm>
#include "axDefs.h"
#include "axFont.h"

#if defined(PLATFORM_WEB) || defined(PLATFORM_ANDROID) || defined(GRAPHICS_API_OPENGL_ES3) || defined(GRAPHICS_API_OPENGL_ES2)   // iOS: ES3 (SDL)
#define SHADER_VERSION "#version 300 es\nprecision mediump float;\nprecision mediump sampler2D;\n"
#else
#define SHADER_VERSION "#version 330\n"
#endif

// ===== Gradient Shader =====
static const char* gradientVS = SHADER_VERSION R"(
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;

uniform mat4 mvp;

out vec2 fragTexCoord;
out vec4 fragColor;
out vec2 fragPosition;

void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragPosition = vertexPosition.xy;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

static const char* linearGradientFS = SHADER_VERSION R"(
in vec2 fragTexCoord;
in vec4 fragColor;
in vec2 fragPosition;

uniform vec2 gradStart;
uniform vec2 gradEnd;
uniform vec4 color0;
uniform vec4 color1;

out vec4 finalColor;

void main() {
    vec2 gradDir = gradEnd - gradStart;
    float gradLen = length(gradDir);
    if (gradLen < 0.001) {
        finalColor = color0;
        return;
    }
    vec2 gradNorm = gradDir / gradLen;
    float t = dot(fragPosition - gradStart, gradNorm) / gradLen;
    t = clamp(t, 0.0, 1.0);
    finalColor = mix(color0, color1, t) * fragColor;
}
)";

static const char* radialGradientFS = SHADER_VERSION R"(
in vec2 fragTexCoord;
in vec4 fragColor;
in vec2 fragPosition;

uniform vec2 center;
uniform float innerRadius;
uniform float outerRadius;
uniform vec4 color0;
uniform vec4 color1;

out vec4 finalColor;

void main() {
    float dist = length(fragPosition - center);
    float t = (dist - innerRadius) / (outerRadius - innerRadius);
    t = clamp(t, 0.0, 1.0);
    finalColor = mix(color0, color1, t) * fragColor;
}
)";

static const char* boxGradientFS = SHADER_VERSION R"(
in vec2 fragTexCoord;
in vec4 fragColor;
in vec2 fragPosition;

uniform vec4 boxRect;  // x, y, w, h
uniform float cornerRadius;
uniform float feather;
uniform vec4 color0;
uniform vec4 color1;

out vec4 finalColor;

float roundedBoxSDF(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

void main() {
    vec2 boxCenter = boxRect.xy + boxRect.zw * 0.5;
    vec2 halfSize = boxRect.zw * 0.5;
    float d = roundedBoxSDF(fragPosition - boxCenter, halfSize, cornerRadius);
    float t = clamp(d / feather + 0.5, 0.0, 1.0);
    finalColor = mix(color0, color1, t) * fragColor;
}
)";

// ===== Filter Shader =====
static const char* filterShaderCode = SHADER_VERSION R"(
in vec2 fragTexCoord;
out vec4 finalColor;

uniform sampler2D texture0;
uniform float brightness;
uniform float contrast;
uniform float saturation;
uniform float hue;
uniform float blur;

vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec4 applyBlur(sampler2D tex, vec2 uv) {
    vec4 color = vec4(0.0);
    vec2 texelSize = 1.0 / vec2(textureSize(tex, 0));
    float total = 0.0;
    float radius = blur;
    for(float x = -radius; x <= radius; x++) {
        for(float y = -radius; y <= radius; y++) {
            vec2 offset = vec2(x, y) * texelSize;
            color += texture(tex, uv + offset);
            total += 1.0;
        }
    }
    return color / total;
}

void main() {
    vec4 texelColor = texture(texture0, fragTexCoord);
    if(blur > 0.0) texelColor = applyBlur(texture0, fragTexCoord);

    vec3 color = texelColor.rgb;
    color += brightness;
    color = (color - 0.5) * (1.0 + contrast) + 0.5;

    vec3 hsv = rgb2hsv(color);
    hsv.x = fract(hsv.x + hue);
    hsv.y *= (1.0 + saturation);
    color = hsv2rgb(hsv);
    color = clamp(color, 0.0, 1.0);

    finalColor = vec4(color, texelColor.a);
}
)";

// ===== Enums =====
enum BotLineCap { BOT_BUTT, BOT_ROUND, BOT_SQUARE, BOT_BEVEL, BOT_MITER };


enum BotWinding { BOT_CCW = 1, BOT_CW = 2 };
enum BotSolidity { BOT_SOLID = 1, BOT_HOLE = 2 };

enum BotImageFlags {
    BOT_IMAGE_GENERATE_MIPMAPS  = 1<<0,
    BOT_IMAGE_REPEATX           = 1<<1,
    BOT_IMAGE_REPEATY           = 1<<2,
    BOT_IMAGE_FLIPY             = 1<<3,
    BOT_IMAGE_PREMULTIPLIED     = 1<<4,
    BOT_IMAGE_NEAREST           = 1<<5,
};

enum BotCompositeOperation {
    BOT_SOURCE_OVER, BOT_SOURCE_IN, BOT_SOURCE_OUT, BOT_ATOP,
    BOT_DESTINATION_OVER, BOT_DESTINATION_IN, BOT_DESTINATION_OUT,
    BOT_DESTINATION_ATOP, BOT_LIGHTER, BOT_COPY, BOT_XOR,
};

enum TextAlign { TEXT_ALIGN_LEFT = 0, TEXT_ALIGN_CENTER = 1, TEXT_ALIGN_RIGHT = 2 };
enum TextBaseline { TEXT_BASELINE_TOP = 0, TEXT_BASELINE_MIDDLE = 1, TEXT_BASELINE_BOTTOM = 2 };
enum class FilterMode { NONE, BRIGHTNESS_CONTRAST, GRAYSCALE, SEPIA, BLUR, VIGNETTE };

// ===== Transform State =====
struct TransformState {
    Rectangle scissor = {0, 0, 0, 0};
    bool hasScissor = false;
    float globalAlpha = 1.0f;
    mat2d transform = mat2d::Identity();
};

/*
// ===== Font Manager =====
class FontManager {
public:
    // Font key: "name:size"
    struct FontKey {
        std::string name;
        int size;

        bool operator<(const FontKey& other) const {
            if (name != other.name) return name < other.name;
            return size < other.size;
        }
    };

    // Font info
    struct FontInfo {
        std::string path;
        std::vector<int> codepoints;
        bool hasTurkish = false;
        bool hasExtended = false;
        int atlasSize = 128;  // default high resolution
        std::vector<int> atlasSizes = {24, 48, 96};  // multi-atlas sizes
        bool useMultiAtlas = true;  // use multiple atlases
    };

    // Public for debugging
    std::map<FontKey, Font> fontCache;
    std::map<std::string, FontInfo> fontInfos;
    Font defaultFont;
    bool defaultFontLoaded = false;

private:
    static std::vector<int> getASCIICodepoints() {
        std::vector<int> cp;
        for (int i = 32; i < 127; i++) cp.push_back(i);
        return cp;
    }

    static std::vector<int> getTurkishCodepoints() {
        return {
            0x011E, 0x011F, 0x0130, 0x0131, 0x015E, 0x015F,
            0x00C7, 0x00E7, 0x00D6, 0x00F6, 0x00DC, 0x00FC
        };
    }

    static std::vector<int> getExtendedLatinCodepoints() {
        std::vector<int> cp;
        for (int i = 0x0100; i <= 0x017F; i++) cp.push_back(i);
        for (int i = 0x0180; i <= 0x024F; i++) cp.push_back(i);
        return cp;
    }

    static std::vector<int> getSymbolCodepoints() {
        return {
            0x00A9, 0x00AE, 0x2122, 0x00B0, 0x00B1, 0x00D7, 0x00F7,
            0x2026, 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D,
            0x2022, 0x20AC, 0x00A3, 0x00A5,
            0x2190, 0x2191, 0x2192, 0x2193, 0x2714, 0x2716, 0x2764
        };
    }

    static std::vector<int> getCyrillicCodepoints() {
        std::vector<int> cp;
        for (int i = 0x0400; i <= 0x04FF; i++) cp.push_back(i);
        return cp;
    }

    static std::vector<int> getGreekCodepoints() {
        std::vector<int> cp;
        for (int i = 0x0370; i <= 0x03FF; i++) cp.push_back(i);
        return cp;
    }

    static std::vector<int> mergeCodepoints(const std::vector<std::vector<int>>& lists) {
        std::vector<int> result;
        for (const auto& list : lists) {
            result.insert(result.end(), list.begin(), list.end());
        }
        std::sort(result.begin(), result.end());
        result.erase(std::unique(result.begin(), result.end()), result.end());
        return result;
    }

    Font loadFontInternal(const std::string& path, int size, const std::vector<int>& codepoints) {
        Font font;
        if (codepoints.empty()) {
            font = LoadFontEx(path.c_str(), size, nullptr, 0);
        } else {
            std::vector<int> cp = codepoints;
            font = LoadFontEx(path.c_str(), size, cp.data(), (int)cp.size());
        }
        if (font.texture.id != 0) {
            GenTextureMipmaps(&font.texture);
            SetTextureFilter(font.texture, TEXTURE_FILTER_TRILINEAR);
        }
        return font;
    }

public:
    std::string currentFontName = "default";
    float currentSize = 16.0f;

    FontManager() = default;

    ~FontManager() {
        for (auto& [key, font] : fontCache) {
            if (font.texture.id != 0 && font.texture.id != GetFontDefault().texture.id) {
                UnloadFont(font);
            }
        }
        fontCache.clear();
    }

    void setDefaultFont() {
        if (!defaultFontLoaded) {
            defaultFont = GetFontDefault();
            defaultFontLoaded = true;
            fontInfos["default"] = {"", {}, false, false};
        }
    }

    // ===== Font Registration =====

    bool registerFont(const std::string& name, const std::string& path, int atlasSize = 128) {
        FontInfo info;
        info.path = path;
        info.codepoints = getASCIICodepoints();
        info.atlasSize = atlasSize;
        fontInfos[name] = info;
        return true;
    }

    bool registerFontTurkish(const std::string& name, const std::string& path, int atlasSize = 128) {
        FontInfo info;
        info.path = path;
        info.codepoints = mergeCodepoints({getASCIICodepoints(), getTurkishCodepoints()});
        info.hasTurkish = true;
        info.atlasSize = atlasSize;
        fontInfos[name] = info;
        TraceLog(LOG_INFO, "FONT: Registered '%s' (Turkish, atlas:%d) from '%s'", name.c_str(), atlasSize, path.c_str());
        return true;
    }

    bool registerFontExtended(const std::string& name, const std::string& path, int atlasSize = 128) {
        FontInfo info;
        info.path = path;
        info.codepoints = mergeCodepoints({
            getASCIICodepoints(), getTurkishCodepoints(),
            getExtendedLatinCodepoints(), getSymbolCodepoints()
        });
        info.hasTurkish = true;
        info.hasExtended = true;
        info.atlasSize = atlasSize;
        fontInfos[name] = info;
        return true;
    }

    bool registerFontFull(const std::string& name, const std::string& path, int atlasSize = 128) {
        FontInfo info;
        info.path = path;
        info.codepoints = mergeCodepoints({
            getASCIICodepoints(), getTurkishCodepoints(),
            getExtendedLatinCodepoints(), getSymbolCodepoints(),
            getCyrillicCodepoints(), getGreekCodepoints()
        });
        info.hasTurkish = true;
        info.hasExtended = true;
        info.atlasSize = atlasSize;
        fontInfos[name] = info;
        return true;
    }

    bool registerFontCustom(const std::string& name, const std::string& path,
                            const std::vector<int>& customCodepoints, int atlasSize = 128) {
        FontInfo info;
        info.path = path;
        info.codepoints = mergeCodepoints({getASCIICodepoints(), customCodepoints});
        info.atlasSize = atlasSize;
        fontInfos[name] = info;
        return true;
    }

    bool registerFontUnicodeRange(const std::string& name, const std::string& path,
                                  int rangeStart, int rangeEnd, int atlasSize = 128) {
        std::vector<int> range;
        for (int i = rangeStart; i <= rangeEnd; i++) range.push_back(i);
        return registerFontCustom(name, path, range, atlasSize);
    }

    // ===== Font Access =====

    // Default font atlas size
    static constexpr int DEFAULT_ATLAS_SIZE = 128;

    // Find the best atlas size for the draw size
    int findBestAtlasSize(const FontInfo& info, int drawSize) const {
        if (!info.useMultiAtlas || info.atlasSizes.empty()) {
            return info.atlasSize;
        }

        // Pick the smallest atlas 1.5-2x above the draw size
        int targetSize = drawSize * 2;
        int bestSize = info.atlasSize;  // fallback

        for (int size : info.atlasSizes) {
            if (size >= targetSize) {
                return size;
            }
            bestSize = size;
        }

        // If even the largest atlas is too small, use the main atlas
        if (bestSize < targetSize && info.atlasSize >= targetSize) {
            return info.atlasSize;
        }

        return bestSize;
    }

    Font& getFont(const std::string& name, int drawSize = 0) {
        if (name == "default" || name.empty()) {
            setDefaultFont();
            return defaultFont;
        }

        auto infoIt = fontInfos.find(name);
        if (infoIt == fontInfos.end()) {
            TraceLog(LOG_WARNING, "FONT: '%s' not registered, using default", name.c_str());
            setDefaultFont();
            return defaultFont;
        }

        // Pick the best atlas for the draw size
        int atlasSize = (drawSize > 0)
                            ? findBestAtlasSize(infoIt->second, drawSize)
                            : infoIt->second.atlasSize;

        FontKey key{name, atlasSize};

        auto it = fontCache.find(key);
        if (it != fontCache.end()) return it->second;

        // Load
        Font font = loadFontInternal(infoIt->second.path, atlasSize, infoIt->second.codepoints);
        if (font.texture.id == 0) {
            TraceLog(LOG_WARNING, "FONT: Failed to load '%s' from '%s', using default",
                     name.c_str(), infoIt->second.path.c_str());
            setDefaultFont();
            return defaultFont;
        }

        TraceLog(LOG_INFO, "FONT: Loaded '%s' at %dpx (for draw:%dpx) from '%s'",
                 name.c_str(), atlasSize, drawSize, infoIt->second.path.c_str());
        fontCache[key] = font;
        return fontCache[key];
    }

    Font& current() {
        return getFont(currentFontName, (int)currentSize);
    }

    // Preload a font
    void preloadFont(const std::string& name) {
        getFont(name);
    }

    void preloadFonts(const std::vector<std::string>& names) {
        for (const auto& name : names) preloadFont(name);
    }

    // Change multi-atlas settings
    void setFontAtlasSizes(const std::string& name, const std::vector<int>& sizes) {
        auto it = fontInfos.find(name);
        if (it != fontInfos.end()) {
            it->second.atlasSizes = sizes;
            it->second.useMultiAtlas = !sizes.empty();
        }
    }

    // Disable multi-atlas (use a single atlas)
    void disableMultiAtlas(const std::string& name) {
        auto it = fontInfos.find(name);
        if (it != fontInfos.end()) {
            it->second.useMultiAtlas = false;
        }
    }

    // ===== Helpers =====

    bool hasFont(const std::string& name) const {
        return fontInfos.find(name) != fontInfos.end() || name == "default";
    }

    bool isFontLoaded(const std::string& name, int size) const {
        if (name == "default") return defaultFontLoaded;
        return fontCache.find({name, size}) != fontCache.end();
    }

    std::vector<std::string> getFontNames() const {
        std::vector<std::string> names;
        names.push_back("default");
        for (const auto& [name, info] : fontInfos) names.push_back(name);
        return names;
    }

    size_t getCacheSize() const { return fontCache.size(); }

    void unloadFont(const std::string& name) {
        std::vector<FontKey> toRemove;
        for (const auto& [key, font] : fontCache) {
            if (key.name == name) toRemove.push_back(key);
        }
        for (const auto& key : toRemove) {
            if (fontCache[key].texture.id != 0) UnloadFont(fontCache[key]);
            fontCache.erase(key);
        }
        fontInfos.erase(name);
    }

    void clearCache() {
        for (auto& [key, font] : fontCache) {
            if (font.texture.id != 0 && font.texture.id != GetFontDefault().texture.id) {
                UnloadFont(font);
            }
        }
        fontCache.clear();
    }

    // Debug: Print font status
    void debugPrint() const {
        TraceLog(LOG_INFO, "=== FontManager Debug ===");
        TraceLog(LOG_INFO, "currentFontName: '%s'", currentFontName.c_str());
        TraceLog(LOG_INFO, "currentSize: %.1f", currentSize);
        TraceLog(LOG_INFO, "defaultFontLoaded: %d", defaultFontLoaded);
        TraceLog(LOG_INFO, "Registered fonts (%zu):", fontInfos.size());
        for (const auto& [name, info] : fontInfos) {
            std::string sizes;
            for (int s : info.atlasSizes) sizes += std::to_string(s) + ",";
            TraceLog(LOG_INFO, "  - '%s' -> '%s' (atlas:%d, multi:[%s] Turkish:%d)",
                     name.c_str(), info.path.c_str(), info.atlasSize, sizes.c_str(), info.hasTurkish);
        }
        TraceLog(LOG_INFO, "Cached fonts (%zu):", fontCache.size());
        for (const auto& [key, font] : fontCache) {
            TraceLog(LOG_INFO, "  - '%s:%d' texId:%d baseSize:%d",
                     key.name.c_str(), key.size, font.texture.id, font.baseSize);
        }
        TraceLog(LOG_INFO, "=========================");
    }

    // Backward compatibility
    void addFont(const std::string& name, Font font) {
        FontKey key{name, font.baseSize};
        fontCache[key] = font;
        fontInfos[name] = {"", {}, false, false};
    }
};
*/

// ===== PaintStyle =====
struct PaintStyle {
    enum Kind { SOLID, LINEAR, RADIAL, BOX, IMAGE_PATTERN } kind = SOLID;

    cColor c0 = {255, 255, 255, 255};
    cColor c1 = {255, 255, 255, 255};

    vec2 p0 = {0, 0}, p1 = {0, 0};
    bool useAbsLine = false;
    float angleRad = 0.0f;

    vec2 center = {0, 0};
    float innerR = 0.0f, outerR = 0.0f;

    float cornerR = 0.0f;
    float feather = 0.0f;

    int image = -1;
    vec2 patternOrigin = {0, 0};
    vec2 patternSize = {0, 0};
    float patternAngle = 0.0f;
    float patternAlpha = 1.0f;
    bool tintPremul = false;

    static PaintStyle Solid(const cColor& col) {
        PaintStyle s; s.kind = SOLID; s.c0 = col; return s;
    }

    static PaintStyle LinearBetween(const vec2& a, const vec2& b, const cColor& start, const cColor& end) {
        PaintStyle s; s.kind = LINEAR; s.c0 = start; s.c1 = end;
        s.p0 = a; s.p1 = b; s.useAbsLine = true; return s;
    }

    static PaintStyle LinearAngle(float angleRad, const cColor& start, const cColor& end) {
        PaintStyle s; s.kind = LINEAR; s.c0 = start; s.c1 = end;
        s.angleRad = angleRad; s.useAbsLine = false; return s;
    }

    static PaintStyle Radial(const vec2& cen, float innerR, float outerR, const cColor& inner, const cColor& outer) {
        PaintStyle s; s.kind = RADIAL; s.c0 = inner; s.c1 = outer;
        s.center = cen; s.innerR = innerR; s.outerR = outerR; return s;
    }

    static PaintStyle Box(const cColor& inner, const cColor& outer, float cornerR, float feather) {
        PaintStyle s; s.kind = BOX; s.c0 = inner; s.c1 = outer;
        s.cornerR = cornerR; s.feather = feather; return s;
    }

    static PaintStyle ImagePattern(int image, const vec2& origin, const vec2& size,
                                   float angleRad, float alpha, const cColor& tint, bool tintPremultiplied = false) {
        PaintStyle s; s.kind = IMAGE_PATTERN; s.image = image;
        s.patternOrigin = origin; s.patternSize = size; s.patternAngle = angleRad;
        s.patternAlpha = alpha; s.c0 = tint; s.tintPremul = tintPremultiplied; return s;
    }

    Color toRaylib(const cColor& c) const {
        return {(unsigned char)c.r, (unsigned char)c.g, (unsigned char)c.b, (unsigned char)c.a};
    }
};

// ===== Draw Command Types =====
enum DrawCommandType {
    CMD_SAVE, CMD_RESTORE, CMD_TRANSLATE, CMD_ROTATE, CMD_SCALE, CMD_TRANSFORM, CMD_RESET_TRANSFORM,
    CMD_SCISSOR, CMD_RESET_SCISSOR, CMD_ROUNDED_FRECT, CMD_ROUNDED_RECT,
    CMD_TEXT, CMD_LINE, CMD_LINE_ARROW, CMD_CIRCLE, CMD_ELLIPSE,
    CMD_SPEECH_BUBBLE, CMD_TRIANGLE, CMD_ICON, CMD_IMAGE, CMD_FONT,
    CMD_SCISSOR_INTERSECT, CMD_CALLBACK
};

struct DrawCommand {
    DrawCommandType type;
    vec2 v1, v2, v3;
    float f1, f2, f3, f4;
    cColor fCol, sCol;
    PaintStyle paintStyle;
    bool usePaintStyle = false;
    std::string str;
    int i1, i2;
    void *data = nullptr;
    mat2d matrix;
    std::function<void()> fn;   // CMD_CALLBACK: draws directly with raylib (rich text, etc.)

    DrawCommand(DrawCommandType t) : type(t), v1(0,0), v2(0,0), v3(0,0),
        f1(0), f2(0), f3(0), f4(0), fCol(0), sCol(0), usePaintStyle(false), i1(0), i2(0) {}

    DrawCommand(DrawCommandType t, const vec2& p, const vec2& s, float rad,
                const cColor& fCol, float strk, const cColor& sCol)
        : type(t), v1(p), v2(s), f1(rad), fCol(fCol), f2(strk), sCol(sCol), usePaintStyle(false) {}

    DrawCommand(DrawCommandType t, const vec2& p, const vec2& s, float rad,
                const PaintStyle& fill, float strk, const cColor& sCol)
        : type(t), v1(p), v2(s), f1(rad), paintStyle(fill), usePaintStyle(true), f2(strk), sCol(sCol) {}
};

// ===== RaylibText Class =====
class RaylibText {
public:
    Font font;
    float fontSize = 16;
    float letterSpacing = 1;
    float lineHeight = 1.2f;
    TextAlign align = TEXT_ALIGN_LEFT;
    TextBaseline baseline = TEXT_BASELINE_TOP;
    Color color = BLACK;

    void loadFont(const char* path, float size) {
        fontSize = size;
        font = LoadFontEx(path, (int)size, nullptr, 0);
        GenTextureMipmaps(&font.texture);
        SetTextureFilter(font.texture, TEXTURE_FILTER_TRILINEAR);
    }

    void loadFontTurkish(const char* path, float size) {
        fontSize = size;
        static int codepoints[512];
        int count = 0;
        for (int i = 32; i < 127; i++) codepoints[count++] = i;
        int turkish[] = {0x011E, 0x011F, 0x0130, 0x0131, 0x015E, 0x015F,
                         0x00C7, 0x00E7, 0x00D6, 0x00F6, 0x00DC, 0x00FC};
        for (int c : turkish) codepoints[count++] = c;
        font = LoadFontEx(path, (int)size, codepoints, count);
        GenTextureMipmaps(&font.texture);
        SetTextureFilter(font.texture, TEXTURE_FILTER_TRILINEAR);
    }

    Vector2 measureText(const char* text) {
        return MeasureTextEx(font, text, fontSize, letterSpacing);
    }

    Rectangle textBounds(float x, float y, const char* text) {
        Vector2 size = measureText(text);
        float ox = 0, oy = 0;
        if (align == TEXT_ALIGN_CENTER) ox = -size.x / 2;
        else if (align == TEXT_ALIGN_RIGHT) ox = -size.x;
        if (baseline == TEXT_BASELINE_MIDDLE) oy = -size.y / 2;
        else if (baseline == TEXT_BASELINE_BOTTOM) oy = -size.y;
        return {x + ox, y + oy, size.x, size.y};
    }

    void drawText(const char* text, float x, float y) {
        Vector2 size = measureText(text);
        float ox = 0, oy = 0;
        if (align == TEXT_ALIGN_CENTER) ox = -size.x / 2;
        else if (align == TEXT_ALIGN_RIGHT) ox = -size.x;
        if (baseline == TEXT_BASELINE_MIDDLE) oy = -size.y / 2;
        else if (baseline == TEXT_BASELINE_BOTTOM) oy = -size.y;
        DrawTextEx(font, text, {x + ox, y + oy}, fontSize, letterSpacing, color);
    }

    void drawTextRotated(const char* text, float x, float y, float angleDeg) {
        Vector2 size = measureText(text);
        Vector2 origin = {0, 0};
        if (align == TEXT_ALIGN_CENTER) origin.x = size.x / 2;
        else if (align == TEXT_ALIGN_RIGHT) origin.x = size.x;
        if (baseline == TEXT_BASELINE_MIDDLE) origin.y = size.y / 2;
        else if (baseline == TEXT_BASELINE_BOTTOM) origin.y = size.y;
        DrawTextPro(font, text, {x, y}, origin, angleDeg, fontSize, letterSpacing, color);
    }

    void drawTextBox(const char* text, float x, float y, float maxWidth) {
        std::vector<std::string> lines = wrapText(text, maxWidth);
        float lineY = y;
        float lineHeightPx = fontSize * lineHeight;

        if (baseline == TEXT_BASELINE_MIDDLE) lineY -= (lines.size() * lineHeightPx) / 2;
        else if (baseline == TEXT_BASELINE_BOTTOM) lineY -= lines.size() * lineHeightPx;

        for (const auto& line : lines) {
            Vector2 size = MeasureTextEx(font, line.c_str(), fontSize, letterSpacing);
            float lx = x;
            if (align == TEXT_ALIGN_CENTER) lx = x + (maxWidth - size.x) / 2;
            else if (align == TEXT_ALIGN_RIGHT) lx = x + maxWidth - size.x;
            DrawTextEx(font, line.c_str(), {lx, lineY}, fontSize, letterSpacing, color);
            lineY += lineHeightPx;
        }
    }

    struct GlyphPosition { int index; float x, minX, maxX; };

    std::vector<GlyphPosition> getGlyphPositions(const char* text, float x, float y) {
        std::vector<GlyphPosition> positions;
        float currentX = x;
        int i = 0;
        const char* p = text;

        while (*p) {
            int codepoint, bytes = 1;
            if ((*p & 0x80) == 0) codepoint = *p;
            else if ((*p & 0xE0) == 0xC0) { codepoint = ((*p & 0x1F) << 6) | (*(p+1) & 0x3F); bytes = 2; }
            else if ((*p & 0xF0) == 0xE0) { codepoint = ((*p & 0x0F) << 12) | ((*(p+1) & 0x3F) << 6) | (*(p+2) & 0x3F); bytes = 3; }
            else { codepoint = ((*p & 0x07) << 18) | ((*(p+1) & 0x3F) << 12) | ((*(p+2) & 0x3F) << 6) | (*(p+3) & 0x3F); bytes = 4; }

            int glyphIndex = GetGlyphIndex(font, codepoint);
            GlyphInfo info = GetGlyphInfo(font, codepoint);
            float scale = fontSize / font.baseSize;
            float glyphWidth = (info.advanceX ? info.advanceX : font.recs[glyphIndex].width) * scale;

            positions.push_back({i, currentX, currentX, currentX + glyphWidth});
            currentX += glyphWidth + letterSpacing;
            p += bytes;
            i++;
        }
        return positions;
    }

private:
    std::vector<std::string> wrapText(const char* text, float maxWidth) {
        std::vector<std::string> lines;
        std::string currentLine, word;
        const char* p = text;

        while (*p) {
            if (*p == ' ' || *p == '\n') {
                if (!word.empty()) {
                    std::string testLine = currentLine.empty() ? word : currentLine + " " + word;
                    Vector2 size = MeasureTextEx(font, testLine.c_str(), fontSize, letterSpacing);
                    if (size.x <= maxWidth) currentLine = testLine;
                    else {
                        if (!currentLine.empty()) lines.push_back(currentLine);
                        currentLine = word;
                    }
                    word.clear();
                }
                if (*p == '\n') { lines.push_back(currentLine); currentLine.clear(); }
            } else word += *p;
            p++;
        }

        if (!word.empty()) {
            std::string testLine = currentLine.empty() ? word : currentLine + " " + word;
            Vector2 size = MeasureTextEx(font, testLine.c_str(), fontSize, letterSpacing);
            if (size.x <= maxWidth) currentLine = testLine;
            else {
                if (!currentLine.empty()) lines.push_back(currentLine);
                currentLine = word;
            }
        }
        if (!currentLine.empty()) lines.push_back(currentLine);
        return lines;
    }
};

struct IconDef
{
    int image = -1;      // raylib texture handle
    rect irect;            // sub-rect in atlas pixels (negative height means vertically flipped: RenderTexture)
    float pad = 0.0f;    // optional padding inside src
    bool owned = false;  // icon created from a texture: released when re-added under the same name or removed
};

// ===== Main Drawing Class =====
class axDrawBot {
    std::map<int, std::vector<DrawCommand>> layers;
    int currentLayer = 0;
    std::stack<TransformState> stateStack;
    std::map<int, Texture2D> textures;
    int nextTexId = 1;
    TransformState currentState;

    // Shaders
    Shader linearGradShader;
    Shader radialGradShader;
    Shader boxGradShader;
    Shader filterShader;

    // Shader locations
    int linearGradStartLoc, linearGradEndLoc, linearColor0Loc, linearColor1Loc;
    int radialCenterLoc, radialInnerLoc, radialOuterLoc, radialColor0Loc, radialColor1Loc;
    int boxRectLoc, boxCornerLoc, boxFeatherLoc, boxColor0Loc, boxColor1Loc;

    void initShaders() {
        linearGradShader = LoadShaderFromMemory(gradientVS, linearGradientFS);
        linearGradStartLoc = GetShaderLocation(linearGradShader, "gradStart");
        linearGradEndLoc = GetShaderLocation(linearGradShader, "gradEnd");
        linearColor0Loc = GetShaderLocation(linearGradShader, "color0");
        linearColor1Loc = GetShaderLocation(linearGradShader, "color1");

        radialGradShader = LoadShaderFromMemory(gradientVS, radialGradientFS);
        radialCenterLoc = GetShaderLocation(radialGradShader, "center");
        radialInnerLoc = GetShaderLocation(radialGradShader, "innerRadius");
        radialOuterLoc = GetShaderLocation(radialGradShader, "outerRadius");
        radialColor0Loc = GetShaderLocation(radialGradShader, "color0");
        radialColor1Loc = GetShaderLocation(radialGradShader, "color1");

        boxGradShader = LoadShaderFromMemory(gradientVS, boxGradientFS);
        boxRectLoc = GetShaderLocation(boxGradShader, "boxRect");
        boxCornerLoc = GetShaderLocation(boxGradShader, "cornerRadius");
        boxFeatherLoc = GetShaderLocation(boxGradShader, "feather");
        boxColor0Loc = GetShaderLocation(boxGradShader, "color0");
        boxColor1Loc = GetShaderLocation(boxGradShader, "color1");
    }

    // clamp channels to 0..255: ops like "color + cColor(40)" wrap around in unsigned char past 255 (266 -> 10),
    // which would give black instead of white in a light theme
    // segments per corner. raylib's automatic count (segments=0) yields only 1-2 for small radii (5 px -> 1: chamfer-like corner).
    // Grows with the radius, minimum 6.
    static int cornerSegments(const Rectangle& r, float roundness) {
        float rad = fminf(r.width, r.height) * fminf(roundness, 1.0f) * 0.5f;
        int n = (int)ceilf(rad * 0.6f) + 4;
        return n < 6 ? 6 : (n > 24 ? 24 : n);
    }
    static unsigned char clampByte(int v) { return (unsigned char)(v < 0 ? 0 : (v > 255 ? 255 : v)); }
    Color toColor(const cColor& c) const { return {clampByte(c.r), clampByte(c.g), clampByte(c.b), clampByte(c.a)}; }

public:
    bool retainedMode = false;
    float zoom = 1.0f;
    float width, height;
    int iconsImage=0;
    std::map<cString, Font> fonts;
    cString fontFace = "default";
    std::unordered_map<std::string, IconDef> iconMap;

    // FontManager fontManager;
    // Font font;
    axFont *curFont=NULL;
    // Size-dependent font: if set, each text is drawn from an atlas at its own pixel size (scaling one atlas up/down blurs text).
    // E.g. bot.fontForSize = [](float px){ return rtFonts().guiFont(px); };
    std::function<axFont*(float)> fontForSize;
    // With per-size atlases the draw size is snapped to the atlas size: a 14 atlas scaled by 0.96 for 13.5 would blur.
    // Measurement uses the same size.
    float fontPx(axFont *f, float size) const {
        return (fontForSize && f && f != curFont && f->font.baseSize > 0) ? (float)f->font.baseSize : size;
    }
    axFont *fontAt(float size)
    {
        if (fontForSize) { axFont *f = fontForSize(size); if (f && f->font.texture.id) return f; }
        return curFont;
    }
    float currentFontSize = 16.0f;
    int currentAlign = BOT_ALIGN_LEFT | BOT_ALIGN_TOP;

    // retained mode: commands are written to layers (window zOrder) and drawn in layer order at endFrame.
    // Drawing outside windows (canvas, etc.) goes to layer 0.
    void beginRetainedMode() { retainedMode = true; layers.clear(); currentLayer = 0; }
    void endRetainedMode() { retainedMode = false; }
    void setLayer(int layer) { currentLayer = layer; }

    void executeAllLayers() {
        for (auto& [layer, commands] : layers) {
            // don't carry the previous layer's clip into the next
            currentState.hasScissor = false;
            EndScissorMode();
            for (auto& cmd : commands) executeCommand(cmd);
        }
        currentState.hasScissor = false;
        EndScissorMode();
    }

    // queue direct-raylib drawing code: in retained mode it runs in its own layer and clip region
    // (otherwise it would end up under the windows); in immediate mode it runs at once.
    void deferred(std::function<void()> fn) {
        if (!fn) return;
        if (retainedMode) { DrawCommand cmd(CMD_CALLBACK); cmd.fn = std::move(fn); layers[currentLayer].push_back(std::move(cmd)); }
        else fn();
    }

    void init() {
        initShaders();
        // fontManager.setDefaultFont();
    }

    void cleanup() {
        UnloadShader(linearGradShader);
        UnloadShader(radialGradShader);
        UnloadShader(boxGradShader);
        // fontManager.clearCache();
    }

    // ===== Frame Management =====
    void beginFrame(float w, float h, float pixRat) {
        layers.clear();
        width = w;
        height = h;
        currentState = TransformState();
        currentState.transform = mat2d::Identity();
        while (!stateStack.empty()) stateStack.pop();
    }

    void endFrame() {}

    // ===== State Management (using rlgl for GPU transforms) =====
    void save() {
        if (retainedMode) {
            layers[currentLayer].push_back(DrawCommand(CMD_SAVE));
        } else {
            stateStack.push(currentState);
            rlPushMatrix();
        }
    }

    void restore() {
        if (retainedMode) {
            layers[currentLayer].push_back(DrawCommand(CMD_RESTORE));
        } else {
            rlPopMatrix();
            if (!stateStack.empty()) {
                currentState = stateStack.top();
                stateStack.pop();
            }
            if (currentState.hasScissor) {
                BeginScissorMode((int)currentState.scissor.x, (int)currentState.scissor.y,
                                 (int)currentState.scissor.width, (int)currentState.scissor.height);
            } else {
                EndScissorMode();
            }
        }
    }

    // Helper: World to screen coordinate conversion
    vec2 worldToScreen(const vec2& p) const {
        return currentState.transform.transformPoint(p);
    }

    void applyCurrentTransform() {
        const mat2d& T = currentState.transform;
        rlLoadIdentity();
        // NOTE: axGUI always calls this after save() (rlPushMatrix); after a push rlgl writes to a separate
        // "transform" matrix and keeps the screen scale (modelview). Adding a scale here would double it.
        float m[16] = {
            T.m[0], T.m[1], 0, 0,
            T.m[2], T.m[3], 0, 0,
            0,      0,      1, 0,
            T.m[4], T.m[5], 0, 1
        };
        rlMultMatrixf(m);
    }

    void translate(vec2 tr) {
        if (retainedMode) {
            DrawCommand cmd(CMD_TRANSLATE);
            cmd.v1 = tr;
            layers[currentLayer].push_back(cmd);
        } else {
            currentState.transform = currentState.transform * mat2d::Translation(tr.x, tr.y);
            applyCurrentTransform();
        }
    }

    void translate(float x, float y) { translate(vec2(x, y)); }

    void rotate(float angle) {
        if (retainedMode) {
            DrawCommand cmd(CMD_ROTATE);
            cmd.f1 = angle;
            layers[currentLayer].push_back(cmd);
        } else {
            currentState.transform = currentState.transform * mat2d::Rotation(angle);
            applyCurrentTransform();
        }
    }

    void scale(float x, float y) {
        if (retainedMode) {
            DrawCommand cmd(CMD_SCALE);
            cmd.v1 = vec2(x, y);
            layers[currentLayer].push_back(cmd);
        } else {
            currentState.transform = currentState.transform * mat2d::Scaling(x, y);
            applyCurrentTransform();
        }
    }

    void scale(float s) { scale(s, s); }

    void transform(const mat2d& M) {
        if (retainedMode) {
            DrawCommand cmd(CMD_TRANSFORM);
            cmd.matrix = M;
            layers[currentLayer].push_back(cmd);
        } else {
            currentState.transform = currentState.transform * M;
            applyCurrentTransform();
        }
    }

    void resetTransform() {
        if (retainedMode) {
            layers[currentLayer].push_back(DrawCommand(CMD_RESET_TRANSFORM));
        } else {
            currentState.transform = mat2d::Identity();
            applyCurrentTransform();
        }
    }

    // Get current transform matrix (for SDF sync)
    const mat2d& getTransform() const {
        return currentState.transform;
    }

    void skewX(float angle) {
        float t = tanf(angle);
        float m[16] = {
            1, 0, 0, 0,
            t, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        };
        rlMultMatrixf(m);
        // Update CPU transform
        mat2d skew;
        skew.m[0] = 1; skew.m[1] = 0;
        skew.m[2] = t; skew.m[3] = 1;
        skew.m[4] = 0; skew.m[5] = 0;
        currentState.transform = currentState.transform * skew;
    }

    void skewY(float angle) {
        float t = tanf(angle);
        float m[16] = {
            1, t, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        };
        rlMultMatrixf(m);
        mat2d skew;
        skew.m[0] = 1; skew.m[1] = t;
        skew.m[2] = 0; skew.m[3] = 1;
        skew.m[4] = 0; skew.m[5] = 0;
        currentState.transform = currentState.transform * skew;
    }

    void globalAlpha(float a) { currentState.globalAlpha = a; }

    void scissor(vec2 pos, vec2 sz)
    {
        if (retainedMode) {
            DrawCommand cmd(CMD_SCISSOR);
            cmd.v1 = pos; cmd.v2 = sz;
            layers[currentLayer].push_back(cmd);
        } else {
            // Transform world coordinates to screen coordinates
            vec2 screenP1 = worldToScreen(pos);
            vec2 screenP2 = worldToScreen(pos + sz);

            // Calculate screen-space rectangle
            float minX = fminf(screenP1.x, screenP2.x);
            float minY = fminf(screenP1.y, screenP2.y);
            float maxX = fmaxf(screenP1.x, screenP2.x);
            float maxY = fmaxf(screenP1.y, screenP2.y);

            currentState.scissor = {minX, minY, maxX - minX, maxY - minY};
            currentState.hasScissor = true;
            BeginScissorMode((int)minX, (int)minY, (int)(maxX - minX), (int)(maxY - minY));
        }
    }

    void scissor(const rect& r) { scissor(r.pos, r.size); }

    // Screen-space scissor (doesn't apply transform)
    void scissorScreen(float x, float y, float w, float h) {
        if (retainedMode) {
            DrawCommand cmd(CMD_SCISSOR);
            cmd.v1 = vec2(x, y); cmd.v2 = vec2(w, h);
            cmd.i1 = 1; // Flag for screen-space
            layers[currentLayer].push_back(cmd);
        } else {
            currentState.scissor = {x, y, w, h};
            currentState.hasScissor = true;
            BeginScissorMode((int)x, (int)y, (int)w, (int)h);
        }
    }

    void resetScissor() {
        if (retainedMode) {
            layers[currentLayer].push_back(DrawCommand(CMD_RESET_SCISSOR));
        } else {
            currentState.hasScissor = false;
            EndScissorMode();
        }
    }

    void globalCompositeOperation(int op) {
        switch (op) {
        case BOT_SOURCE_OVER: rlSetBlendMode(BLEND_ALPHA); break;
        case BOT_LIGHTER: rlSetBlendMode(BLEND_ADDITIVE); break;
        case BOT_COPY: rlSetBlendMode(BLEND_ALPHA); break;
        default: rlSetBlendMode(BLEND_ALPHA); break;
        }
    }

    void intersectScissor(float x, float y, float w, float h) {
        if (currentState.hasScissor) {
            float nx = fmaxf(currentState.scissor.x, x);
            float ny = fmaxf(currentState.scissor.y, y);
            float nw = fminf(currentState.scissor.x + currentState.scissor.width, x + w) - nx;
            float nh = fminf(currentState.scissor.y + currentState.scissor.height, y + h) - ny;
            if (nw > 0 && nh > 0) scissor(vec2(nx, ny), vec2(nw, nh));
        } else {
            scissor(vec2(x, y), vec2(w, h));
        }
    }

    // Converts a world-space rect to screen space and INTERSECTS it with the current scissor area
    // (replaces the plain scissor()). Nothing is drawn if the intersection is empty.
    void intersectScissorWorld(vec2 pos, vec2 sz)
    {
        if (retainedMode) {
            DrawCommand cmd(CMD_SCISSOR_INTERSECT);
            cmd.v1 = pos; cmd.v2 = sz;
            layers[currentLayer].push_back(cmd);
            return;
        }
        vec2 a = worldToScreen(pos), b = worldToScreen(pos + sz);
        float x0 = fminf(a.x, b.x), y0 = fminf(a.y, b.y), x1 = fmaxf(a.x, b.x), y1 = fmaxf(a.y, b.y);
        if (currentState.hasScissor) {
            x0 = fmaxf(x0, currentState.scissor.x);
            y0 = fmaxf(y0, currentState.scissor.y);
            x1 = fminf(x1, currentState.scissor.x + currentState.scissor.width);
            y1 = fminf(y1, currentState.scissor.y + currentState.scissor.height);
        }
        float w = fmaxf(0.0f, x1 - x0), h = fmaxf(0.0f, y1 - y0);
        currentState.scissor = { x0, y0, w, h };
        currentState.hasScissor = true;
        BeginScissorMode((int)x0, (int)y0, (int)w, (int)h);
    }

    void loadIcons()
    {
        iconsImage = addTexture("resources/icons2.png", BOT_IMAGE_GENERATE_MIPMAPS);
    }

    // Regular grid icon atlas (e.g. resources/icons.png, 64 px cells).
    // drawIcon(name, ...) uses this atlas; names are registered with addIconCell / addIcon.
    float iconCell = 64.0f;
    bool loadIconAtlas(const cString &fn, float cell = 64.0f)
    {
        int id = addTexture(fn, BOT_IMAGE_GENERATE_MIPMAPS);
        if (id < 0) return false;
        iconsImage = id;
        iconCell = cell;
        return true;
    }
    void addIconCell(const cString &name, int col, int row, float pad = 0.0f)
    {
        addIcon(name, iconsImage, rect(col * iconCell, row * iconCell, iconCell, iconCell), pad);
    }
    bool hasIcon(const cString &name) const {
        auto it = iconMap.find(name);
        return it != iconMap.end() && textures.count(it->second.image) > 0;
    }

    // ===== Icons from a drawing / texture =====
    // Draws the shape with draw(bot, px) (in 0..px coordinates) and registers it as a px x px icon; use it with drawIcon / imageButton,
    // tintable (draw the icon white to tint it). For transparent edges the drawing uses premultiplied alpha, then the result is read back,
    // un-premultiplied and flipped vertically; mipmaps are generated (draw large, show small).
    // Calling again with the same name updates the icon. Call outside a frame (after InitWindow, preferably before the loop);
    // it also works inside a frame (clipping / transform / retained mode are temporarily disabled).
    bool addIconFromDrawing(const cString &name, int px, const std::function<void(axDrawBot &, float)> &draw)
    {
        if (px <= 0 || !draw) return false;
        RenderTexture2D rt = LoadRenderTexture(px, px);
        if (rt.id == 0) return false;

        // save drawing state: retained mode, clipping, transform
        const bool ret0 = retainedMode;
        const TransformState st0 = currentState;
        retainedMode = false;
        rlDrawRenderBatchActive();
        EndScissorMode();
        currentState = TransformState();
        currentState.transform = mat2d::Identity();

        BeginTextureMode(rt);
        ClearBackground({ 0, 0, 0, 0 });
        // color: with source alpha (result is premultiplied), alpha: accumulated correctly
        rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
        BeginBlendMode(BLEND_CUSTOM_SEPARATE);
        draw(*this, (float)px);
        EndBlendMode();
        EndTextureMode();

        Image img = LoadImageFromTexture(rt.texture);
        UnloadRenderTexture(rt);
        ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        ImageFlipVertical(&img);   // RenderTexture is flipped
        unsigned char *d = (unsigned char *)img.data;
        for (int i = 0, n = img.width * img.height; i < n; i++) {   // premultiplied -> straight alpha
            unsigned char *p = d + i * 4;
            if (p[3] > 0 && p[3] < 255) for (int k = 0; k < 3; k++) { int v = p[k] * 255 / p[3]; p[k] = (unsigned char)(v > 255 ? 255 : v); }
        }
        Texture2D tex = LoadTextureFromImage(img);
        UnloadImage(img);
        GenTextureMipmaps(&tex);
        SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR);

        // restore state
        currentState = st0;
        retainedMode = ret0;
        if (currentState.hasScissor)
            BeginScissorMode((int)currentState.scissor.x, (int)currentState.scissor.y, (int)currentState.scissor.width, (int)currentState.scissor.height);

        if (tex.id == 0) return false;
        removeIcon(name);
        IconDef def; def.image = addTexture(tex); def.irect = rect(0, 0, (float)px, (float)px); def.owned = true;
        iconMap[name] = def;
        return true;
    }

    // Registers an existing texture (e.g. your own RenderTexture: flipY = true) as an icon. The texture stays owned
    // by the caller (not freed); the whole texture if no source sub-rect is given. Note: semi-transparent edges of a
    // RenderTexture drawn onto a transparent background with normal blending can look dark; addIconFromDrawing avoids this.
    bool addIconFromTexture(const cString &name, Texture2D tex, bool flipY = false, rect src = rect(0, 0, 0, 0))
    {
        if (tex.id == 0) return false;
        if (src.size.x <= 0 || src.size.y <= 0) src = rect(0, 0, (float)tex.width, (float)tex.height);
        if (flipY) src = rect(src.pos.x, (float)tex.height - src.pos.y - src.size.y, src.size.x, -src.size.y);
        removeIcon(name);
        int id = nextTexId++;
        textures[id] = tex;
        IconDef def; def.image = id; def.irect = src; def.owned = false;
        iconMap[name] = def;
        return true;
    }

    void removeIcon(const cString &name)
    {
        auto it = iconMap.find(name);
        if (it == iconMap.end()) return;
        if (it->second.owned) removeTexture(it->second.image);
        else if (it->second.image != iconsImage) textures.erase(it->second.image);   // caller's texture: only remove the registration
        iconMap.erase(it);
    }

    int addTexture(Texture2D tex) {
        int id = nextTexId++;
        textures[id] = tex;
        return id;
    }
    int addTexture(const cString &fn, int flags=0)
    {
        Image img = LoadImage(fn.c_str());
        if (img.data == nullptr) {
            axLog("TEXTURE: Failed to load image from '%s'", fn.c_str());
            return -1;
        }

        if (flags & BOT_IMAGE_FLIPY) {
            ImageFlipVertical(&img);
        }

        Texture2D tex = LoadTextureFromImage(img);
        UnloadImage(img);

        if (tex.id == 0) {
            axLog("TEXTURE: Failed to create texture from image '%s'", fn.c_str());
            return -1;
        }

        if (flags & BOT_IMAGE_GENERATE_MIPMAPS) {
            GenTextureMipmaps(&tex);
        }

        if (flags & BOT_IMAGE_NEAREST) {
            SetTextureFilter(tex, TEXTURE_FILTER_POINT);
        } else {
            SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR);
        }

        // if ((flags & BOT_IMAGE_REPEATX) || (flags & BOT_IMAGE_REPEATY)) {
        //     int wrapS = (flags & BOT_IMAGE_REPEATX) ? TEXTURE_WRAP_REPEAT : TEXTURE_WRAP_CLAMP;
        //     int wrapT = (flags & BOT_IMAGE_REPEATY) ? TEXTURE_WRAP_REPEAT : TEXTURE_WRAP_CLAMP;
        //     SetTextureWrap(tex, wrapS, wrapT);
        // }
        axLog("Added texture: "+fn);
        return addTexture(tex);
    }

    int addTextureRGBA(int w, int h, int flags, const unsigned char* data)
    {
        Image img = {(void*)data, w, h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        Texture2D tex = LoadTextureFromImage(img);
        return addTexture(tex);
    }

    Texture2D* getTexture(int id) {
        auto it = textures.find(id);
        if (it != textures.end()) return &it->second;
        return nullptr;
    }

    void removeTexture(int id) {
        auto it = textures.find(id);
        if (it != textures.end()) {
            UnloadTexture(it->second);
            textures.erase(it);
        }
    }

    void getTextureSize(int id, int* w, int* h) {
        Texture2D* tex = getTexture(id);
        if (tex) { *w = tex->width; *h = tex->height; }
        else { *w = 0; *h = 0; }
    }

    void setfont(const std::string& face, float sz = 0) {
        if (retainedMode) {
            DrawCommand cmd(CMD_FONT);
            cmd.str = face; cmd.f1 = sz;
            layers[currentLayer].push_back(cmd);
        }
        // fontManager.currentFontName = face;
        if (sz > 0) {
            currentFontSize = sz;
            // fontManager.currentSize = sz;
        }
        fontFace = face;
    }

    void setfont(axFont *f, float sz = 0) {
        if (retainedMode) {
            DrawCommand cmd(CMD_FONT);
            cmd.data = f; cmd.f1 = sz;
            layers[currentLayer].push_back(cmd);
        }
        // fontManager.currentFontName = face;
        if (sz > 0) {
            currentFontSize = sz;
            // fontManager.currentSize = sz;
        }
        fontFace = "";
        curFont = f;
    }

    void setfontsize(float sz) {
        if (retainedMode) {
            DrawCommand cmd(CMD_FONT);
            cmd.str = ""; cmd.f1 = sz;
            layers[currentLayer].push_back(cmd);
        }
        currentFontSize = sz;
        // fontManager.currentSize = sz;
    }


    void textAlign(int align) { currentAlign = align; }

    // ===== Text Measurement =====
    float textWidth(const std::string& str, float sz = 0) {
        float size = (sz > 0) ? sz : currentFontSize;
        // Font& font = fontManager.getFont(fontManager.currentFontName, (int)size);
        axFont *f = fontAt(size);
        return MeasureTextEx(f->font, str.c_str(), fontPx(f, size), 1).x;
    }

    vec2 textBounds(const std::string& str, float sz = 0) {
        float size = (sz > 0) ? sz : currentFontSize;
        // Font& font = fontManager.getFont(fontManager.currentFontName, (int)size);
        axFont *f = fontAt(size);
        Vector2 measured = MeasureTextEx(f->font, str.c_str(), fontPx(f, size), 1);
        return vec2(measured.x, measured.y);
    }

    float textBounds(const std::string& str, float x, float y, float* bounds) {
        vec2 size = textBounds(str);
        if (bounds) {
            bounds[0] = x; bounds[1] = y;
            bounds[2] = x + size.x; bounds[3] = y + size.y;
        }
        return size.x;
    }

    void textMetrics(float* ascender, float* descender, float* lineh) {
        // Font& font = fontManager.current();
        float scale = currentFontSize / curFont->font.baseSize;
        if (ascender) *ascender = curFont->font.baseSize * scale * 0.8f;
        if (descender) *descender = curFont->font.baseSize * scale * 0.2f;
        if (lineh) *lineh = currentFontSize * 1.2f;
    }

    // ===== Text Drawing =====
    void text(const std::string& str, float x, float y, float sz = 0,
              cColor col = cColor(0,0,0,0), int align = BOT_ALIGN_LEFT | BOT_ALIGN_TOP) {
        if (retainedMode) {
            DrawCommand cmd(CMD_TEXT);
            cmd.str = str; cmd.v1 = vec2(x, y);
            cmd.f1 = sz; cmd.fCol = col; cmd.i2 = align;   // size is a float (13.5 stays 13.5)
            cmd.f2 = textSnap ? 1.0f : 0.0f;              // rounding follows the setting at registration time
            layers[currentLayer].push_back(cmd);
            return;
        }

        float size = (sz > 0) ? sz : currentFontSize;
        axFont *fnt = fontAt(size);
        size = fontPx(fnt, size);
        Vector2 measured = MeasureTextEx(fnt->font, str.c_str(), size, 1);

        float ox = 0, oy = 0;
        if (align & BOT_ALIGN_CENTER) ox = -measured.x / 2;
        else if (align & BOT_ALIGN_RIGHT) ox = -measured.x;
        if (align & BOT_ALIGN_MIDDLE) oy = -measured.y / 2;
        else if (align & BOT_ALIGN_BOTTOM) oy = -measured.y;
        else if (align & BOT_ALIGN_BASELINE) oy = -measured.y * 0.8f;

        Color c = (col.a > 0) ? toColor(col) : BLACK;
        c.a = (unsigned char)(c.a * currentState.globalAlpha);
        // snap to the pixel grid: at fractional positions glyphs spread over two pixels and look blurry
        Vector2 tp = textSnap ? Vector2{ roundf(x + ox), roundf(y + oy) } : Vector2{ x + ox, y + oy };
        DrawTextEx(fnt->font, str.c_str(), tp, size, 1, c);
    }

    void text(const std::string& str, const vec2& p, float sz, cColor col,
              int align = BOT_ALIGN_LEFT | BOT_ALIGN_TOP) {
        text(str, p.x, p.y, sz, col, align);
    }

    void label(const std::string& str, float x, float y, float sz, cColor col, int xAlign = 0) {
        vec2 p(x, y);
        int align = BOT_ALIGN_CENTER | BOT_ALIGN_MIDDLE;
        vec2 rectBB = textBounds(str, sz) + vec2(sz, 0.7f * sz);
        vec2 bp = p;
        float offx = 0;

        if (xAlign < 0) {
            align = BOT_ALIGN_LEFT | BOT_ALIGN_MIDDLE;
            bp = bp + vec2(0, -rectBB.y / 2);
            offx = 0.5f * sz;
        } else if (xAlign > 0) {
            align = BOT_ALIGN_RIGHT | BOT_ALIGN_MIDDLE;
            bp = bp + vec2(-rectBB.x, -rectBB.y / 2);
            offx = -0.5f * sz;
        } else {
            bp = bp + vec2(-rectBB.x / 2, -rectBB.y / 2);
        }

        roundedFRect(bp, rectBB, 0.5f * sz, cColor(32), 0, cColor(0));
        text(str, p.x + offx, p.y + 0.1f * sz, sz, col, align);
    }

    void label(const std::string& str, const vec2& p, float sz, cColor col, int xAlign = 0) {
        label(str, p.x, p.y, sz, col, xAlign);
    }

    float snapToPixel(float v, float pxRatio) {
        return std::floor(v * pxRatio + 0.5f) / pxRatio;
    }

    void text_snapped(const std::string& str, float x, float y, float sz,
                      cColor col, int align = BOT_ALIGN_LEFT | BOT_ALIGN_TOP, float pixelRatio = 1.0f) {
        x = snapToPixel(x, pixelRatio);
        y = snapToPixel(y, pixelRatio);
        const float step = 0.25f;
        float szQ = std::round(sz / step) * step;
        text(str, x, y, szQ, col, align);
    }

    void text_world(const std::string& str, float worldX, float worldY, float basePx,
                    float camScale, float camOffsetX, float camOffsetY,
                    cColor col, int align, float pixelRatio) {
        // This function applies its own camera transform (for text that should be camera-aware)
        float sx = worldX * camScale + camOffsetX;
        float sy = worldY * camScale + camOffsetY;
        sx = std::floor(sx * pixelRatio + 0.5f) / pixelRatio;
        sy = std::floor(sy * pixelRatio + 0.5f) / pixelRatio;

        float fontSize = basePx * camScale;
        // Font& font = fontManager.getFont(fontManager.currentFontName, (int)fontSize);
        Vector2 measured = MeasureTextEx(curFont->font, str.c_str(), fontSize, 1);

        float ox = 0, oy = 0;
        if (align & BOT_ALIGN_CENTER) ox = -measured.x / 2;
        else if (align & BOT_ALIGN_RIGHT) ox = -measured.x;
        if (align & BOT_ALIGN_MIDDLE) oy = -measured.y / 2;
        else if (align & BOT_ALIGN_BOTTOM) oy = -measured.y;

        Color c = toColor(col);
        c.a = (unsigned char)(c.a * currentState.globalAlpha);
        DrawTextEx(curFont->font, str.c_str(), {sx + ox, sy + oy}, fontSize, 1, c);
    }

    // ===== Image Management =====
    // int createImage(const char* fn, int flags) {
    //     Image img = LoadImage(fn);
    //     if (flags & BOT_IMAGE_FLIPY) ImageFlipVertical(&img);
    //     Texture2D tex = LoadTextureFromImage(img);
    //     if (flags & BOT_IMAGE_GENERATE_MIPMAPS) GenTextureMipmaps(&tex);
    //     if (flags & BOT_IMAGE_NEAREST) SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    //     else SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
    //     if (flags & BOT_IMAGE_REPEATX) SetTextureWrap(tex, TEXTURE_WRAP_REPEAT);
    //     if (flags & BOT_IMAGE_REPEATY) SetTextureWrap(tex, TEXTURE_WRAP_REPEAT);
    //     UnloadImage(img);
    //     return textureMgr.addTexture(tex);
    // }


    // void deleteImage(int image) {
    //     textureMgr.removeTexture(image);
    // }

    PaintStyle imagePattern(float cx, float cy, float w, float h, float angle, int image, float alpha) {
        return PaintStyle::ImagePattern(image, vec2(cx, cy), vec2(w, h), angle, alpha, cWhite, false);
    }

    // ===== Anti-aliased drawing (AA fringe, ImGui method) =====
    // Each shape gets a fringe of width aaFringe along its edge, fading to transparent outward. Gives smooth edges
    // with or without MSAA (MSAA 4x gives only 4 levels at an edge; circle outlines would be 1 px GL lines).
    // aaFringe = 0: plain raylib drawing.
    float aaFringe = 1.0f;
    bool pixelSnap = false;
    // whether text position is rounded to whole pixels (crisp). false: fractional position, atlas is bilinearly filtered (smooth motion).
    // axGUI disables rounding for a window during its fold animation; with gui.smoothText = true it is always disabled.
    bool textSnap = true;   // true: roundedFRect fill is snapped to the pixel grid (jumps on moving shapes)

    static float cross2(const vec2 &a, const vec2 &b, const vec2 &c) { return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x); }
    // face culling is enabled in rlgl: pass triangles in the winding order raylib expects
    static void aaTri(vec2 a, Color ca, vec2 b, Color cb, vec2 c, Color cc) {
        if (cross2(a, b, c) > 0) { std::swap(b, c); std::swap(cb, cc); }
        rlColor4ub(ca.r, ca.g, ca.b, ca.a); rlVertex2f(a.x, a.y);
        rlColor4ub(cb.r, cb.g, cb.b, cb.a); rlVertex2f(b.x, b.y);
        rlColor4ub(cc.r, cc.g, cc.b, cc.a); rlVertex2f(c.x, c.y);
    }
    static Color aaTrans(Color c) { c.a = 0; return c; }

    // vertex normals (outward, miter-scaled); closed=false: edge normal at the endpoints
    static void aaNormals(const std::vector<vec2> &pts, bool closed, std::vector<vec2> &nrm) {
        const int n = (int)pts.size();
        std::vector<vec2> en(n);
        float area = 0;
        for (int i = 0; i < n; i++) { const vec2 &a = pts[i], &b = pts[(i + 1) % n]; area += a.x * b.y - b.x * a.y; }
        const float sgn = (closed && area < 0) ? -1.0f : 1.0f;   // outward-facing normal
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            vec2 d = pts[j] - pts[i];
            float l = sqrtf(d.x * d.x + d.y * d.y);
            en[i] = l > 1e-6f ? vec2(d.y / l, -d.x / l) * sgn : vec2(0, 0);
        }
        nrm.resize(n);
        for (int i = 0; i < n; i++) {
            vec2 a = closed ? en[(i + n - 1) % n] : (i > 0 ? en[i - 1] : en[i]);
            vec2 b = (closed || i < n - 1) ? en[i] : en[i - 1];
            vec2 m = (a + b) * 0.5f;
            float d2 = m.x * m.x + m.y * m.y;
            if (d2 > 1e-6f) { float inv = 1.0f / d2; if (inv > 100.0f) inv = 100.0f; m = m * inv; }
            nrm[i] = m;
        }
    }

    // convex polygon fill
    void aaFillConvex(const std::vector<vec2> &pts, Color col) {
        const int n = (int)pts.size();
        if (n < 3 || col.a == 0) return;
        rlSetTexture(rlGetTextureIdDefault());
        if (aaFringe <= 0) {
            rlCheckRenderBatchLimit(3 * n);
            rlBegin(RL_TRIANGLES);
            for (int i = 1; i < n - 1; i++) aaTri(pts[0], col, pts[i], col, pts[i + 1], col);
            rlEnd(); rlSetTexture(0);
            return;
        }
        std::vector<vec2> nrm; aaNormals(pts, true, nrm);
        const float h = aaFringe * 0.5f;
        const Color tr = aaTrans(col);
        rlCheckRenderBatchLimit(9 * n);
        rlBegin(RL_TRIANGLES);
        vec2 in0 = pts[0] - nrm[0] * h;
        for (int i = 1; i < n - 1; i++) aaTri(in0, col, pts[i] - nrm[i] * h, col, pts[i + 1] - nrm[i + 1] * h, col);
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            vec2 ii = pts[i] - nrm[i] * h, io = pts[i] + nrm[i] * h, ji = pts[j] - nrm[j] * h, jo = pts[j] + nrm[j] * h;
            aaTri(ii, col, io, tr, jo, tr);
            aaTri(ii, col, jo, tr, ji, col);
        }
        rlEnd(); rlSetTexture(0);
    }

    // polygon visible from its center (star-shaped): fan from center, AA fringe only on the outer edge
    void aaFillFan(const std::vector<vec2> &pts, vec2 center, Color col) {
        const int n = (int)pts.size();
        if (n < 3 || col.a == 0) return;
        std::vector<vec2> nrm; aaNormals(pts, true, nrm);
        const float h = aaFringe > 0 ? aaFringe * 0.5f : 0.0f;
        const Color tr = aaTrans(col);
        rlSetTexture(rlGetTextureIdDefault());
        rlCheckRenderBatchLimit(9 * n);
        rlBegin(RL_TRIANGLES);
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            vec2 ii = pts[i] - nrm[i] * h, ji = pts[j] - nrm[j] * h;
            aaTri(center, col, ii, col, ji, col);
            if (h > 0) {
                vec2 io = pts[i] + nrm[i] * h, jo = pts[j] + nrm[j] * h;
                aaTri(ii, col, io, tr, jo, tr);
                aaTri(ii, col, jo, tr, ji, col);
            }
        }
        rlEnd(); rlSetTexture(0);
    }

    // line (closed or open path). Visible thickness ~w; if w < aaFringe it thins out via transparency.
    void aaStroke(const std::vector<vec2> &pts, bool closed, float w, Color col) {
        const int n = (int)pts.size();
        if (n < 2 || col.a == 0 || w <= 0) return;
        const float fr = aaFringe > 0 ? aaFringe : 0.0f;
        float core = (w - fr) * 0.5f;
        if (core < 0) { col.a = (unsigned char)(col.a * fmaxf(0.0f, w / fmaxf(fr, 1e-3f))); core = 0; }
        std::vector<vec2> nrm; aaNormals(pts, closed, nrm);
        const Color tr = aaTrans(col);
        const int segs = closed ? n : n - 1;
        rlSetTexture(rlGetTextureIdDefault());
        rlCheckRenderBatchLimit(18 * segs);
        rlBegin(RL_TRIANGLES);
        for (int i = 0; i < segs; i++) {
            int j = (i + 1) % n;
            vec2 a2 = pts[i] + nrm[i] * (core + fr), a1 = pts[i] + nrm[i] * core, a0 = pts[i] - nrm[i] * core, am = pts[i] - nrm[i] * (core + fr);
            vec2 b2 = pts[j] + nrm[j] * (core + fr), b1 = pts[j] + nrm[j] * core, b0 = pts[j] - nrm[j] * core, bm = pts[j] - nrm[j] * (core + fr);
            if (fr > 0) { aaTri(a2, tr, a1, col, b1, col); aaTri(a2, tr, b1, col, b2, tr); }
            if (core > 0) { aaTri(a1, col, a0, col, b0, col); aaTri(a1, col, b0, col, b1, col); }
            if (fr > 0) { aaTri(a0, col, am, tr, bm, tr); aaTri(a0, col, bm, tr, b0, col); }
        }
        rlEnd(); rlSetTexture(0);
    }

    // path generators
    static int aaArcSegments(float r, float sweep) {
        // at most ~0.05 px deviation; at 0.25 px a 9 px radius circle becomes a 14-gon: even with smooth AA edges,
        // the corners show as jaggies around the circle. A quarter arc has at least 4 segments.
        float rr = fmaxf(r, 0.5f);
        float th = 2.0f * acosf(fmaxf(-1.0f, fminf(1.0f, 1.0f - 0.05f / rr)));
        int n = (int)ceilf(sweep / fmaxf(th, 1e-3f));
        int nMin = (int)ceilf(sweep / (0.5f * PI) * 4.0f);
        if (n < nMin) n = nMin;
        return n < 3 ? 3 : (n > 256 ? 256 : n);
    }
    static void aaPathArc(std::vector<vec2> &out, vec2 c, float rx, float ry, float a0, float a1) {
        int n = aaArcSegments(fmaxf(rx, ry), fabsf(a1 - a0));
        for (int i = 0; i <= n; i++) { float a = a0 + (a1 - a0) * i / n; out.push_back(vec2(c.x + cosf(a) * rx, c.y + sinf(a) * ry)); }
    }
    static void aaPathEllipse(std::vector<vec2> &out, vec2 c, float rx, float ry) {
        int n = aaArcSegments(fmaxf(rx, ry), 2.0f * PI);
        if (n < 32) n = 32;
        for (int i = 0; i < n; i++) { float a = 2.0f * PI * i / n; out.push_back(vec2(c.x + cosf(a) * rx, c.y + sinf(a) * ry)); }
    }
    static void aaPathRoundRect(std::vector<vec2> &out, float x, float y, float w, float h, float r) {
        r = fmaxf(0.0f, fminf(r, fminf(w, h) * 0.5f));
        if (r < 0.5f) { out.push_back(vec2(x, y)); out.push_back(vec2(x + w, y)); out.push_back(vec2(x + w, y + h)); out.push_back(vec2(x, y + h)); return; }
        aaPathArc(out, vec2(x + r, y + r), r, r, PI, 1.5f * PI);
        aaPathArc(out, vec2(x + w - r, y + r), r, r, 1.5f * PI, 2.0f * PI);
        aaPathArc(out, vec2(x + w - r, y + h - r), r, r, 0.0f, 0.5f * PI);
        aaPathArc(out, vec2(x + r, y + h - r), r, r, 0.5f * PI, PI);
    }
    Color aaColor(const cColor &c) const { Color k = toColor(c); k.a = (unsigned char)(k.a * currentState.globalAlpha); return k; }

    // ===== Shape Drawing =====
    void roundedFRect(const vec2& p, const vec2& sz, float rad, const PaintStyle& fill,
                      float strk = 0, const cColor& sCol = cBlack) {
        if (retainedMode) {
            layers[currentLayer].push_back(DrawCommand(CMD_ROUNDED_FRECT, p, sz, rad, fill, strk, sCol));
            return;
        }

        Rectangle rec = {p.x, p.y, sz.x, sz.y};
        float roundness = (sz.x > 0 && sz.y > 0) ? rad / fminf(sz.x, sz.y) * 2 : 0;
        roundness = fminf(roundness, 1.0f);

        if (fill.kind == PaintStyle::SOLID) {
            Color c = toColor(fill.c0);
            c.a = (unsigned char)(c.a * currentState.globalAlpha);
            std::vector<vec2> path; aaPathRoundRect(path, rec.x, rec.y, rec.width, rec.height, rad);
            aaFillConvex(path, c);
        } else if (fill.kind == PaintStyle::LINEAR) {
            drawLinearGradientRect(rec, rad, fill);
        } else if (fill.kind == PaintStyle::RADIAL) {
            drawRadialGradientRect(rec, rad, fill);
        } else if (fill.kind == PaintStyle::BOX) {
            drawBoxGradientRect(rec, rad, fill);
        } else if (fill.kind == PaintStyle::IMAGE_PATTERN) {
            Texture2D* tex = getTexture(fill.image);
            if (tex) {
                DrawTexturePro(*tex, {0, 0, (float)tex->width, (float)tex->height},
                               rec, {0, 0}, 0, WHITE);
            }
        }

        if (strk > 0) {
            float hs = strk * 0.5f;   // border inside the rectangle
            std::vector<vec2> path; aaPathRoundRect(path, rec.x + hs, rec.y + hs, rec.width - strk, rec.height - strk, fmaxf(0.0f, rad - hs));
            aaStroke(path, true, strk, aaColor(sCol));
        }
    }

    void roundedFRect(const vec2& p, const vec2& sz, float rad, cColor fCol,
                      float strk = 0, const cColor& sCol = cBlack) {
        if (retainedMode) {
            layers[currentLayer].push_back(DrawCommand(CMD_ROUNDED_FRECT, p, sz, rad, fCol, strk, sCol));
            return;
        }

        // position stays fractional (the AA fringe smooths the edges), so moving shapes don't jump in whole-pixel steps.
        // pixelSnap = true rounds to whole pixels.
        Rectangle rec = {p.x, p.y, sz.x, sz.y};
        if (pixelSnap) { float rx0 = roundf(p.x), ry0 = roundf(p.y); rec = {rx0, ry0, roundf(p.x + sz.x) - rx0, roundf(p.y + sz.y) - ry0}; }
        Color fc = toColor(fCol);
        fc.a = (unsigned char)(fc.a * currentState.globalAlpha);

        Color sc = aaColor(sCol);
        if (strk > 0 && sc.a > 0 && fc.a == 255 && rec.width > 2 * strk && rec.height > 2 * strk) {
            // opaque fill + border: outer shape in border color, inner shape in fill color (two clean edges).
            // A thin line strip overlapping the fill's soft edge would make the outline look speckled / jagged.
            std::vector<vec2> outer; aaPathRoundRect(outer, rec.x, rec.y, rec.width, rec.height, rad);
            aaFillConvex(outer, sc);
            std::vector<vec2> inner; aaPathRoundRect(inner, rec.x + strk, rec.y + strk, rec.width - 2 * strk, rec.height - 2 * strk, fmaxf(0.0f, rad - strk));
            aaFillConvex(inner, fc);
            return;
        }
        {
            std::vector<vec2> path; aaPathRoundRect(path, rec.x, rec.y, rec.width, rec.height, rad);
            aaFillConvex(path, fc);
        }
        if (strk > 0) {
            float hs = strk * 0.5f;   // border inside the rectangle
            std::vector<vec2> path; aaPathRoundRect(path, rec.x + hs, rec.y + hs, rec.width - strk, rec.height - strk, fmaxf(0.0f, rad - hs));
            aaStroke(path, true, strk, sc);
        }
    }

    void roundedFRect(float x, float y, float w, float h, float rad, const cColor& fCol,
                      float strk = 0, const cColor& sCol = cBlack) {
        roundedFRect(vec2(x, y), vec2(w, h), rad, fCol, strk, sCol);
    }

    void roundedFRect(float x, float y, float w, float h, float rad, const PaintStyle& fill) {
        roundedFRect(vec2(x, y), vec2(w, h), rad, fill);
    }

    void roundedFRect(rect r, float rad, cColor fCol, float strk = 0, cColor sCol = cBlack) {
        roundedFRect(r.pos, r.size, rad, fCol, strk, sCol);
    }

    void roundedRect(float x, float y, float w, float h, float rad, float strk, cColor sCol) {
        if (retainedMode) {
            layers[currentLayer].push_back(DrawCommand(CMD_ROUNDED_RECT, vec2(x,y), vec2(w,h), rad, cBlack, strk, sCol));
            return;
        }

        float hs = strk * 0.5f;   // border inside the rectangle
        std::vector<vec2> path; aaPathRoundRect(path, x + hs, y + hs, w - strk, h - strk, fmaxf(0.0f, rad - hs));
        aaStroke(path, true, strk, aaColor(sCol));
    }

    void roundedRect(vec2 p, vec2 sz, float rad, float strk, cColor sCol) {
        roundedRect(p.x, p.y, sz.x, sz.y, rad, strk, sCol);
    }

    void roundedRect(rect r, float rad, float strk, cColor sCol) {
        roundedRect(r.pos.x, r.pos.y, r.size.x, r.size.y, rad, strk, sCol);
    }

    void circle(vec2 p, float rad, cColor fCol, float strk = 0.0f, cColor sCol = cTransparent) {
        if (retainedMode) {
            DrawCommand cmd(CMD_CIRCLE);
            cmd.v1 = p; cmd.f1 = rad; cmd.fCol = fCol; cmd.sCol = sCol; cmd.f2 = strk;
            layers[currentLayer].push_back(cmd);
            return;
        }

        Color fc = aaColor(fCol), sc = aaColor(sCol);
        if (strk > 0 && sc.a > 0 && fc.a == 255 && rad > strk) {   // opaque fill + border: two discs (clean edges)
            std::vector<vec2> outer; aaPathEllipse(outer, p, rad + strk * 0.5f, rad + strk * 0.5f);
            aaFillConvex(outer, sc);
            std::vector<vec2> inner; aaPathEllipse(inner, p, rad - strk * 0.5f, rad - strk * 0.5f);
            aaFillConvex(inner, fc);
            return;
        }
        if (fCol != cTransparent) {
            std::vector<vec2> path; aaPathEllipse(path, p, rad, rad);
            aaFillConvex(path, fc);
        }
        if (strk > 0) {
            std::vector<vec2> path; aaPathEllipse(path, p, rad, rad);
            aaStroke(path, true, strk, sc);
        }
    }

    void ellipse(vec2 p, vec2 sz, cColor fCol, float strk, cColor sCol) {
        if (retainedMode) {
            layers[currentLayer].push_back(DrawCommand(CMD_ELLIPSE, p, sz, 0, fCol, strk, sCol));
            return;
        }

        Color fc = aaColor(fCol), sc = aaColor(sCol);
        if (strk > 0 && sc.a > 0 && fc.a == 255 && fminf(sz.x, sz.y) * 0.5f > strk) {   // opaque fill + border: two ellipses
            float h = strk * 0.5f;
            std::vector<vec2> outer; aaPathEllipse(outer, p, sz.x / 2 + h, sz.y / 2 + h);
            aaFillConvex(outer, sc);
            std::vector<vec2> inner; aaPathEllipse(inner, p, sz.x / 2 - h, sz.y / 2 - h);
            aaFillConvex(inner, fc);
            return;
        }
        {
            std::vector<vec2> path; aaPathEllipse(path, p, sz.x / 2, sz.y / 2);
            aaFillConvex(path, fc);
        }
        if (strk > 0) {
            std::vector<vec2> path; aaPathEllipse(path, p, sz.x / 2, sz.y / 2);
            aaStroke(path, true, strk, aaColor(sCol));
        }
    }

    void ellipse(vec2 p, vec2 sz, const PaintStyle& fill, float strk, cColor sCol) {
        if (retainedMode) {
            layers[currentLayer].push_back(DrawCommand(CMD_ELLIPSE, p, sz, 0, fill, strk, sCol));
            return;
        }

        if (fill.kind == PaintStyle::SOLID) {
            ellipse(p, sz, fill.c0, strk, sCol);
        } else {
            ellipse(p, sz, fill.c0, strk, sCol);
        }
    }

    void ellipse(float cx, float cy, float rx, float ry) {
        DrawEllipse((int)cx, (int)cy, rx, ry, WHITE);
    }

    void ellipse(const vec2& c, const vec2& r) {
        DrawEllipse((int)c.x, (int)c.y, r.x, r.y, WHITE);
    }

    void drawLine(float x, float y, float x2, float y2, cColor col, float strk = 1.0f) {
        if (retainedMode) {
            DrawCommand cmd(CMD_LINE);
            cmd.v1 = vec2(x,y); cmd.v2 = vec2(x2,y2);
            cmd.f1 = strk; cmd.sCol = col;
            layers[currentLayer].push_back(cmd);
            return;
        }

        std::vector<vec2> path = { vec2(x, y), vec2(x2, y2) };
        aaStroke(path, false, strk, aaColor(col));
    }

    void drawLine(vec2 from, vec2 to, cColor col, float strk = 1.0f) {
        drawLine(from.x, from.y, to.x, to.y, col, strk);
    }

    void drawLineWithArrow(vec2 from, vec2 to, cColor col, float strk = 1.0f, float arrowSize = 6.0f) {
        vec2 dir = (to - from).normalized();
        vec2 perp(-dir.y, dir.x);
        vec2 arrowP1 = to - dir * arrowSize + perp * (arrowSize * 0.5f);
        vec2 arrowP2 = to - dir * arrowSize - perp * (arrowSize * 0.5f);

        drawLine(from, to - dir * arrowSize, col, strk);
        fillTriangle(to, arrowP1, arrowP2, col);
    }

    void fillTriangle(vec2 p1, vec2 p2, vec2 p3, cColor fCol) {
        if (retainedMode) {
            DrawCommand cmd(CMD_TRIANGLE);
            cmd.v1 = p1; cmd.v2 = p2; cmd.v3 = p3; cmd.fCol = fCol;
            layers[currentLayer].push_back(cmd);
            return;
        }

        std::vector<vec2> path = { p1, p2, p3 };
        aaFillConvex(path, aaColor(fCol));
    }

    void arc(vec2 pos, float rad, float a0, float a1, float strk, cColor fCol, cColor sCol) {
        Color fc = toColor(fCol);
        fc.a = (unsigned char)(fc.a * currentState.globalAlpha);
        DrawCircleSector({pos.x, pos.y}, rad, a0 * RAD2DEG, a1 * RAD2DEG, 36, fc);

        if (strk > 0) {
            Color sc = toColor(sCol);
            sc.a = (unsigned char)(sc.a * currentState.globalAlpha);
            DrawRing({pos.x, pos.y}, rad - strk/2, rad + strk/2, a0 * RAD2DEG, a1 * RAD2DEG, 36, sc);
        }
    }

    void cross(vec2 p, float r, cColor clr) {
        Color c = toColor(clr);
        c.a = (unsigned char)(c.a * currentState.globalAlpha);
        DrawLineEx({p.x - r, p.y}, {p.x + r, p.y}, 1.5f, c);
        DrawLineEx({p.x, p.y - r}, {p.x, p.y + r}, 1.5f, c);
    }

    // Star: filled as a fan from the center (AA fringe only on the outer edge, so no inner seam when semi-transparent),
    // with an AA outline; recorded in retained mode.
    void star(vec2 pos, float rad, int cnt, float strk, cColor fCol, cColor sCol) {
        if (retainedMode) {
            DrawCommand cmd(CMD_CALLBACK);
            cmd.fn = [this, pos, rad, cnt, strk, fCol, sCol]() { star(pos, rad, cnt, strk, fCol, sCol); };
            layers[currentLayer].push_back(std::move(cmd));
            return;
        }
        if (cnt < 2) return;
        float astp = 2.0f * PI / cnt;
        float angle = -0.5f * PI;   // one point up
        std::vector<vec2> points;
        for (int i = 0; i < cnt; ++i) {
            points.push_back(pos + vec2(cosf(angle), sinf(angle)) * rad);
            angle += 0.5f * astp;
            points.push_back(pos + vec2(cosf(angle), sinf(angle)) * (rad * 0.5f));
            angle += 0.5f * astp;
        }
        Color fc = aaColor(fCol), sc = aaColor(sCol);
        if (fc.a > 0) aaFillFan(points, pos, fc);
        if (strk > 0 && sc.a > 0) aaStroke(points, true, strk, sc);
    }

    void drawShape(vec2 pos, const std::vector<vec2>& points, float strk, cColor fCol, cColor sCol) {
        if (points.empty()) return;

        Color fc = toColor(fCol);
        fc.a = (unsigned char)(fc.a * currentState.globalAlpha);

        for (size_t i = 1; i < points.size() - 1; i++) {
            DrawTriangle({points[0].x, points[0].y},
                         {points[i].x, points[i].y},
                         {points[i+1].x, points[i+1].y}, fc);
        }

        if (strk > 0) {
            Color sc = toColor(sCol);
            sc.a = (unsigned char)(sc.a * currentState.globalAlpha);
            for (size_t i = 0; i < points.size(); i++) {
                size_t next = (i + 1) % points.size();
                DrawLineEx({points[i].x, points[i].y}, {points[next].x, points[next].y}, strk, sc);
            }
        }
    }

    void speechBubble(const cString& str, vec2 pos, vec2 size, float radius, vec2 arrowSize, float fontsize = 12,
                      const cColor &bg = cColor(80), const cColor &fg = cColor(255)) {   // colors are parameters
        float textPadding = 8.0f;
        float arrowW = arrowSize.x;
        float arrowH = arrowSize.y;
        float x = pos.x - size.x / 2.0f;
        float y = pos.y;
        float w = size.x;
        float h = size.y;

        Rectangle bodyRect = {x, y + arrowH, w, h - arrowH};
        float roundness = (w > 0 && h - arrowH > 0) ? radius / fminf(w, h - arrowH) * 2 : 0;
        { std::vector<vec2> path; aaPathRoundRect(path, bodyRect.x, bodyRect.y, bodyRect.width, bodyRect.height, radius); aaFillConvex(path, toColor(bg)); }
        (void)roundness;

        Vector2 arrow[3] = {
            {x + w/2.0f, y},
            {x + w/2.0f - arrowW/2.0f, y + arrowH},
            {x + w/2.0f + arrowW/2.0f, y + arrowH}
        };
        { std::vector<vec2> tri = { vec2(arrow[0].x, arrow[0].y), vec2(arrow[1].x, arrow[1].y), vec2(arrow[2].x, arrow[2].y) }; aaFillConvex(tri, toColor(bg)); }

        // Font& font = fontManager.getFont(fontManager.currentFontName, (int)fontsize);
        float textX = x + textPadding;
        float textY = y + arrowH + textPadding;
        axFont *tf = fontAt(fontsize);
        DrawTextEx(tf->font, str.c_str(), {roundf(textX), roundf(textY)}, fontPx(tf, fontsize), 1, toColor(fg));
    }

    // ===== Gradient Helpers =====
    void fillBoxGrad(const rect& R, float cornerR, float feather, const cColor& cInner, const cColor& cOuter) {
        drawBoxGradientRect({R.pos.x, R.pos.y, R.size.x, R.size.y}, cornerR,
                            PaintStyle::Box(cInner, cOuter, cornerR, feather));
    }

    void fillCircleRadialGrad(const vec2& center, float radius, const cColor& inner, const cColor& outer) {
        int steps = 32;
        for (int i = steps; i >= 0; i--) {
            float t = (float)i / steps;
            float r = radius * t;
            Color c = {
                (unsigned char)(inner.r + (outer.r - inner.r) * t),
                (unsigned char)(inner.g + (outer.g - inner.g) * t),
                (unsigned char)(inner.b + (outer.b - inner.b) * t),
                (unsigned char)((inner.a + (outer.a - inner.a) * t) * currentState.globalAlpha)
            };
            DrawCircleV({center.x, center.y}, r, c);
        }
    }

    void fillRectLinearGrad(const rect& r, const cColor& c0, const cColor& c1, float radius = 0.0f) {
        drawLinearGradientRect({r.pos.x, r.pos.y, r.size.x, r.size.y}, radius,
                               PaintStyle::LinearBetween(r.pos, r.pos + vec2(r.size.x, 0), c0, c1));
    }

    // ===== Image Drawing =====

    IconDef *iconDef(const cString &name)
    {
        auto it = iconMap.find(name);
        if(it == iconMap.end()) return NULL;
        return &it->second;
    }

    void addIcon(const cString &name, int image, rect srcPx, float pad)
    {
        iconMap[name] = IconDef{image, srcPx, pad};
        // for(auto &ch : name) axLog("icon: ",ch);
    }

    void drawIcon(const cString &name, const vec2 &pos, float sz, const cColor &tint)
    {
        auto it = iconMap.find(name);
        if(it == iconMap.end()) { axLog("error icon: "+name); return; }
        IconDef& def = it->second;
        drawIcon(pos, def.irect, sz, tint, def.image);   // icon's own texture (otherwise the atlas)
    }

    // image < 0: icon atlas (iconsImage)
    void drawIcon(const vec2& pos, rect src, float sz, const cColor& tint, int image = -1)
    {
        if (image < 0) image = iconsImage;
        if (retainedMode) {
            DrawCommand cmd(CMD_ICON);
            cmd.v1 = pos; cmd.v2 = src.pos; cmd.v3 = src.size;
            cmd.f1 = sz; cmd.i1 = image; cmd.fCol = tint;
            layers[currentLayer].push_back(cmd);
            return;
        }

        Texture2D* tex = getTexture(image);
        if(!tex) { return; }

        Rectangle srcRect = {src.pos.x, src.pos.y, src.size.x, src.size.y};
        Rectangle dstRect = {pos.x, pos.y, sz, sz};
        Color c = toColor(tint);
        c.a = (unsigned char)(c.a * currentState.globalAlpha);
        DrawTexturePro(*tex, srcRect, dstRect, {0, 0}, 0, c);
    }

    void drawImage(int img, const rect& src, const vec2& pos, const vec2& sz, float angle = 0.0f)
    {
        if (retainedMode) {
            DrawCommand cmd(CMD_IMAGE);
            cmd.i1 = img; cmd.v2 = src.pos; cmd.v3 = src.size;
            cmd.v1 = pos; cmd.f1 = sz.x; cmd.f2 = sz.y; cmd.f3 = angle;
            layers[currentLayer].push_back(cmd);
            return;
        }

        Texture2D* tex = getTexture(img);
        if (!tex) return;

        Rectangle srcRect = {src.pos.x, src.pos.y, src.size.x, src.size.y};
        Rectangle dstRect = {pos.x, pos.y, sz.x, sz.y};
        Color c = WHITE;
        c.a = (unsigned char)(255 * currentState.globalAlpha);
        DrawTexturePro(*tex, srcRect, dstRect, {0, 0}, angle * RAD2DEG, c);
    }

    void roundedRectVarying(float x, float y, float w, float h,
                            float radTopLeft, float radTopRight,
                            float radBottomRight, float radBottomLeft, cColor fCol) {
        float avgRad = (radTopLeft + radTopRight + radBottomRight + radBottomLeft) / 4.0f;
        Rectangle rec = {x, y, w, h};
        Color fc = toColor(fCol);
        fc.a = (unsigned char)(fc.a * currentState.globalAlpha);
        float roundness = (w > 0 && h > 0) ? avgRad / fminf(w, h) * 2 : 0;
        roundness = fminf(roundness, 1.0f);
        DrawRectangleRounded(rec, roundness, cornerSegments(rec, roundness), fc);
    }

    void roundedRectVarying(float x, float y, float w, float h,
                            float radTopLeft, float radTopRight,
                            float radBottomRight, float radBottomLeft,
                            float strk, cColor fCol, cColor sCol) {
        roundedRectVarying(x, y, w, h, radTopLeft, radTopRight, radBottomRight, radBottomLeft, fCol);
        if (strk > 0) {
            float avgRad = (radTopLeft + radTopRight + radBottomRight + radBottomLeft) / 4.0f;
            Rectangle rec = {x, y, w, h};
            Color sc = toColor(sCol);
            sc.a = (unsigned char)(sc.a * currentState.globalAlpha);
            float roundness = (w > 0 && h > 0) ? avgRad / fminf(w, h) * 2 : 0;
            roundness = fminf(roundness, 1.0f);
            DrawRectangleRoundedLinesEx(rec, roundness, cornerSegments(rec, roundness), strk, sc);
        }
    }

    // ===== Path Building (simplified) =====
    std::vector<Vector2> currentPath;
    cColor currentFillColor = cBlack;
    cColor currentStrokeColor = cBlack;
    float currentStrokeWidth = 1.0f;

    void beginPath() { currentPath.clear(); }
    void closePath() { if (!currentPath.empty()) currentPath.push_back(currentPath[0]); }

    void moveTo(vec2 p) {
        currentPath.push_back({p.x, p.y});
    }
    void lineTo(vec2 p) {
        currentPath.push_back({p.x, p.y});
    }

    void bezierTo(vec2 c1, vec2 c2, vec2 p) {
        if (currentPath.empty()) return;
        Vector2 start = currentPath.back();

        for (float t = 0.1f; t <= 1.0f; t += 0.1f) {
            float u = 1.0f - t;
            float tt = t * t;
            float uu = u * u;
            float uuu = uu * u;
            float ttt = tt * t;

            float x = uuu * start.x + 3 * uu * t * c1.x + 3 * u * tt * c2.x + ttt * p.x;
            float y = uuu * start.y + 3 * uu * t * c1.y + 3 * u * tt * c2.y + ttt * p.y;
            currentPath.push_back({x, y});
        }
    }

    void quadTo(vec2 c, vec2 p) {
        if (currentPath.empty()) return;
        Vector2 start = currentPath.back();

        for (float t = 0.1f; t <= 1.0f; t += 0.1f) {
            float u = 1.0f - t;
            float x = u * u * start.x + 2 * u * t * c.x + t * t * p.x;
            float y = u * u * start.y + 2 * u * t * c.y + t * t * p.y;
            currentPath.push_back({x, y});
        }
    }

    void arcTo(vec2 p1, vec2 p2, float radius) {
        if (currentPath.empty()) return;
        currentPath.push_back({p1.x, p1.y});
        currentPath.push_back({p2.x, p2.y});
    }

    void fillColor(const cColor& col) { currentFillColor = col; }
    void strokeColor(const cColor& col) { currentStrokeColor = col; }
    void strokeWidth(float w) { currentStrokeWidth = w; }

    void fill() {
        if (currentPath.size() < 3) return;
        Color fc = toColor(currentFillColor);
        fc.a = (unsigned char)(fc.a * currentState.globalAlpha);

        for (size_t i = 1; i < currentPath.size() - 1; i++) {
            DrawTriangle(currentPath[0], currentPath[i], currentPath[i+1], fc);
        }
    }

    void stroke() {
        if (currentPath.size() < 2) return;
        Color sc = toColor(currentStrokeColor);
        sc.a = (unsigned char)(sc.a * currentState.globalAlpha);

        for (size_t i = 0; i < currentPath.size() - 1; i++) {
            DrawLineEx(currentPath[i], currentPath[i+1], currentStrokeWidth, sc);
        }
    }

    void pathWinding(int dir) { /* Not directly supported in Raylib */ }
    void lineCap(int cap) { /* Not directly supported in Raylib */ }
    void lineJoin(int join) { /* Not directly supported in Raylib */ }
    void miterLimit(float ml) { /* Not directly supported in Raylib */ }

    // ===== Command Execution =====
    void executeCommand(const DrawCommand& cmd) {
        switch (cmd.type) {
        case CMD_ICON:
            drawIcon(cmd.v1, rect(cmd.v2, cmd.v3), cmd.f1, cmd.fCol, cmd.i1);
            break;
        case CMD_IMAGE:
            drawImage(cmd.i1, rect(cmd.v2, cmd.v3), cmd.v1, vec2(cmd.f1, cmd.f2), cmd.f3);
            break;
        case CMD_SAVE: save(); break;
        case CMD_RESTORE: restore(); break;
        case CMD_TRANSLATE: translate(cmd.v1); break;
        case CMD_SCISSOR: scissor(cmd.v1, cmd.v2); break;
        case CMD_SCISSOR_INTERSECT: intersectScissorWorld(cmd.v1, cmd.v2); break;
        case CMD_CALLBACK: if (cmd.fn) cmd.fn(); break;
        case CMD_RESET_SCISSOR: resetScissor(); break;
        case CMD_RESET_TRANSFORM: resetTransform(); break;
        case CMD_FONT:
            if (!cmd.str.empty()) setfont(cmd.str, cmd.f1);
            else if(cmd.data) setfont((axFont*)cmd.data, cmd.f1);
            else if (cmd.f1 > 0) setfontsize(cmd.f1);
            break;
        case CMD_ROUNDED_FRECT:
            if (cmd.usePaintStyle) roundedFRect(cmd.v1, cmd.v2, cmd.f1, cmd.paintStyle, cmd.f2, cmd.sCol);
            else roundedFRect(cmd.v1, cmd.v2, cmd.f1, cmd.fCol, cmd.f2, cmd.sCol);
            break;
        case CMD_ROUNDED_RECT:
            roundedRect(cmd.v1.x, cmd.v1.y, cmd.v2.x, cmd.v2.y, cmd.f1, cmd.f2, cmd.fCol);
            break;
        case CMD_TEXT:
            { bool snap0 = textSnap; textSnap = cmd.f2 > 0.5f; text(cmd.str, cmd.v1, cmd.f1, cmd.fCol, cmd.i2); textSnap = snap0; }
            break;
        case CMD_LINE:
            drawLine(cmd.v1, cmd.v2, cmd.sCol, cmd.f1);
            break;
        case CMD_CIRCLE:
            circle(cmd.v1, cmd.f1, cmd.fCol, cmd.f2, cmd.sCol);
            break;
        case CMD_ELLIPSE:
            if (cmd.usePaintStyle) ellipse(cmd.v1, cmd.v2, cmd.paintStyle, cmd.f2, cmd.sCol);
            else ellipse(cmd.v1, cmd.v2, cmd.fCol, cmd.f2, cmd.sCol);
            break;
        case CMD_TRIANGLE:
            fillTriangle(cmd.v1, cmd.v2, cmd.v3, cmd.fCol);
            break;
        default: break;
        }
    }

    void printLayerInfo(vec2 pos) {
        vec2 p = pos;
        text(axFormat("layers"), p, 10, cBlack);
        for (auto& [layer, commands] : layers) {
            for (auto& cmd : commands) {
                text(axFormat("cmd:%d", cmd.type), p, 10, cBlack);
                p.y += 10;
            }
        }
    }

private:
    void drawLinearGradientRect(Rectangle rec, float rad, const PaintStyle& style) {
        // Calculate gradient start/end points
        vec2 start, end;
        if (style.useAbsLine) {
            start = style.p0;
            end = style.p1;
        } else {
            // Angle-based gradient
            float angle = style.angleRad;
            float cx = rec.x + rec.width / 2;
            float cy = rec.y + rec.height / 2;
            float diagonal = sqrtf(rec.width * rec.width + rec.height * rec.height) / 2;
            start = vec2(cx - cosf(angle) * diagonal, cy - sinf(angle) * diagonal);
            end = vec2(cx + cosf(angle) * diagonal, cy + sinf(angle) * diagonal);
        }

        // Set shader uniforms
        float gradStart[2] = {start.x, start.y};
        float gradEnd[2] = {end.x, end.y};
        float color0[4] = {style.c0.r / 255.0f, style.c0.g / 255.0f, style.c0.b / 255.0f, (style.c0.a / 255.0f) * currentState.globalAlpha};
        float color1[4] = {style.c1.r / 255.0f, style.c1.g / 255.0f, style.c1.b / 255.0f, (style.c1.a / 255.0f) * currentState.globalAlpha};

        SetShaderValue(linearGradShader, linearGradStartLoc, gradStart, SHADER_UNIFORM_VEC2);
        SetShaderValue(linearGradShader, linearGradEndLoc, gradEnd, SHADER_UNIFORM_VEC2);
        SetShaderValue(linearGradShader, linearColor0Loc, color0, SHADER_UNIFORM_VEC4);
        SetShaderValue(linearGradShader, linearColor1Loc, color1, SHADER_UNIFORM_VEC4);

        BeginShaderMode(linearGradShader);
        float roundness = (rec.width > 0 && rec.height > 0) ? rad / fminf(rec.width, rec.height) * 2 : 0;
        roundness = fminf(roundness, 1.0f);
        DrawRectangleRounded(rec, roundness, 16, WHITE);
        EndShaderMode();
    }

    void drawRadialGradientRect(Rectangle rec, float rad, const PaintStyle& style) {
        // Set shader uniforms
        float center[2] = {style.center.x, style.center.y};
        float color0[4] = {style.c0.r / 255.0f, style.c0.g / 255.0f, style.c0.b / 255.0f, (style.c0.a / 255.0f) * currentState.globalAlpha};
        float color1[4] = {style.c1.r / 255.0f, style.c1.g / 255.0f, style.c1.b / 255.0f, (style.c1.a / 255.0f) * currentState.globalAlpha};

        SetShaderValue(radialGradShader, radialCenterLoc, center, SHADER_UNIFORM_VEC2);
        SetShaderValue(radialGradShader, radialInnerLoc, &style.innerR, SHADER_UNIFORM_FLOAT);
        SetShaderValue(radialGradShader, radialOuterLoc, &style.outerR, SHADER_UNIFORM_FLOAT);
        SetShaderValue(radialGradShader, radialColor0Loc, color0, SHADER_UNIFORM_VEC4);
        SetShaderValue(radialGradShader, radialColor1Loc, color1, SHADER_UNIFORM_VEC4);

        BeginShaderMode(radialGradShader);
        float roundness = (rec.width > 0 && rec.height > 0) ? rad / fminf(rec.width, rec.height) * 2 : 0;
        roundness = fminf(roundness, 1.0f);
        DrawRectangleRounded(rec, roundness, 16, WHITE);
        EndShaderMode();
    }

    void drawBoxGradientRect(Rectangle rec, float rad, const PaintStyle& style) {
        // Set shader uniforms
        float boxRect[4] = {rec.x, rec.y, rec.width, rec.height};
        float feather = (style.feather > 0) ? style.feather : rad;
        float color0[4] = {style.c0.r / 255.0f, style.c0.g / 255.0f, style.c0.b / 255.0f, (style.c0.a / 255.0f) * currentState.globalAlpha};
        float color1[4] = {style.c1.r / 255.0f, style.c1.g / 255.0f, style.c1.b / 255.0f, (style.c1.a / 255.0f) * currentState.globalAlpha};

        SetShaderValue(boxGradShader, boxRectLoc, boxRect, SHADER_UNIFORM_VEC4);
        SetShaderValue(boxGradShader, boxCornerLoc, &rad, SHADER_UNIFORM_FLOAT);
        SetShaderValue(boxGradShader, boxFeatherLoc, &feather, SHADER_UNIFORM_FLOAT);
        SetShaderValue(boxGradShader, boxColor0Loc, color0, SHADER_UNIFORM_VEC4);
        SetShaderValue(boxGradShader, boxColor1Loc, color1, SHADER_UNIFORM_VEC4);

        BeginShaderMode(boxGradShader);
        // Draw slightly larger to include feather area
        Rectangle expandedRec = {rec.x - feather, rec.y - feather, rec.width + feather * 2, rec.height + feather * 2};
        DrawRectangleRec(expandedRec, WHITE);
        EndShaderMode();
    }
};

// ===== Message Helper =====
struct Message {
    bool active = false;
    cString text;
    vec2 pos;
    float time = 0.0f, duration = 2.0f;
    float blendInTm = 0.2f;
    float blendOutTm = 0.4f;

    void show(const cString& txt, const vec2& p) {
        text = txt;
        pos = p;
        time = GetTime();
        active = true;
    }

    void draw(axDrawBot& bot) {
        if (!active) return;
        bot.save();
        float t = GetTime() - time;
        bot.label(text, pos, 12, cYellow, 0);
        bot.restore();
        if (t > duration) active = false;
    }
};

#endif // AXDRAWBOT_H
