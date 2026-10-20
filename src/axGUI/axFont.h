// FontToPath.h - system that converts TTF glyphs to SDFPath and caches them
// Usage: requires stb_truetype.h
//
// #define STB_TRUETYPE_IMPLEMENTATION
// #include "stb_truetype.h"
// #include "FontToPath.h"
//
#pragma once

#include "SdfRenderer2D.h"
#include "stb_truetype.h"
#include <string>
#include <unordered_map>
#include <memory>

// Text alignment flags (combinable: horizontal | vertical)
#ifndef BOT_ALIGN_LEFT
enum BotAlign_ {
    BOT_ALIGN_LEFT     = 1 << 0,
    BOT_ALIGN_CENTER   = 1 << 1,
    BOT_ALIGN_RIGHT    = 1 << 2,
    BOT_ALIGN_TOP      = 1 << 3,
    BOT_ALIGN_MIDDLE   = 1 << 4,
    BOT_ALIGN_BOTTOM   = 1 << 5,
    BOT_ALIGN_BASELINE = 1 << 6,
};
#endif

// Cache key: codepoint + fontSize (quantized)
struct GlyphCacheKey {
    int codepoint;
    int sizeKey;  // fontSize * 10 (for 0.1px precision)

    bool operator==(const GlyphCacheKey& other) const {
        return codepoint == other.codepoint && sizeKey == other.sizeKey;
    }
};

struct GlyphCacheKeyHash {
    size_t operator()(const GlyphCacheKey& k) const {
        return std::hash<int>()(k.codepoint) ^ (std::hash<int>()(k.sizeKey) << 16);
    }
};

// Cached glyph data
struct CachedGlyph {
    SDFPath path;
    float advanceWidth = 0;
    float leftSideBearing = 0;
    bool valid = false;
};

class FontToPath {
public:
    stbtt_fontinfo fontInfo;
    unsigned char* fontBuffer = nullptr;
    float unitsPerEm = 1.0f;
    float ascender = 0;
    float descender = 0;
    float lineGap = 0;
    bool loaded = false;

    // Glyph cache
    std::unordered_map<GlyphCacheKey, CachedGlyph, GlyphCacheKeyHash> glyphCache;
    size_t maxCacheSize = 1024;  // Maximum cached glyphs

    // Stats
    size_t cacheHits = 0;
    size_t cacheMisses = 0;

    FontToPath() = default;

    ~FontToPath() {
        unload();
    }

    void unload() {
        if (fontBuffer) {
            free(fontBuffer);
            fontBuffer = nullptr;
        }
        glyphCache.clear();
        loaded = false;
        cacheHits = 0;
        cacheMisses = 0;
    }

    bool loadFont(const char* filename) {
        unload();

        FILE* f = fopen(filename, "rb");
        if (!f) {
            TraceLog(LOG_WARNING, "FontToPath: Failed to open font file: %s", filename);
            return false;
        }

        fseek(f, 0, SEEK_END);
        size_t size = ftell(f);
        fseek(f, 0, SEEK_SET);

        fontBuffer = (unsigned char*)malloc(size);
        if (!fontBuffer) {
            fclose(f);
            return false;
        }

        fread(fontBuffer, 1, size, f);
        fclose(f);

        if (!stbtt_InitFont(&fontInfo, fontBuffer, stbtt_GetFontOffsetForIndex(fontBuffer, 0))) {
            TraceLog(LOG_WARNING, "FontToPath: Failed to initialize font: %s", filename);
            free(fontBuffer);
            fontBuffer = nullptr;
            return false;
        }

        // Get font metrics
        int asc, desc, gap;
        stbtt_GetFontVMetrics(&fontInfo, &asc, &desc, &gap);
        unitsPerEm = (float)(asc - desc);
        ascender = (float)asc;
        descender = (float)desc;
        lineGap = (float)gap;

        loaded = true;
        TraceLog(LOG_INFO, "FontToPath: Loaded font: %s (unitsPerEm=%.0f)", filename, unitsPerEm);
        return true;
    }

    bool loadFontFromMemory(const unsigned char* data, size_t size) {
        unload();

        fontBuffer = (unsigned char*)malloc(size);
        if (!fontBuffer) return false;

        memcpy(fontBuffer, data, size);

        if (!stbtt_InitFont(&fontInfo, fontBuffer, stbtt_GetFontOffsetForIndex(fontBuffer, 0))) {
            free(fontBuffer);
            fontBuffer = nullptr;
            return false;
        }

        int asc, desc, gap;
        stbtt_GetFontVMetrics(&fontInfo, &asc, &desc, &gap);
        unitsPerEm = (float)(asc - desc);
        ascender = (float)asc;
        descender = (float)desc;
        lineGap = (float)gap;

        loaded = true;
        return true;
    }

    // Get or create cached glyph
    const CachedGlyph& getGlyph(int codepoint, float fontSize) {
        static CachedGlyph emptyGlyph;
        if (!loaded) return emptyGlyph;

        // Quantize fontSize for cache key (0.1px precision)
        int sizeKey = (int)(fontSize * 10.0f + 0.5f);
        GlyphCacheKey key = {codepoint, sizeKey};

        // Check cache
        auto it = glyphCache.find(key);
        if (it != glyphCache.end()) {
            cacheHits++;
            return it->second;
        }

        cacheMisses++;

        // Evict if cache is full (simple LRU-ish: just clear half)
        if (glyphCache.size() >= maxCacheSize) {
            size_t toRemove = maxCacheSize / 2;
            auto removeIt = glyphCache.begin();
            while (toRemove > 0 && removeIt != glyphCache.end()) {
                removeIt = glyphCache.erase(removeIt);
                toRemove--;
            }
        }

        // Create new cached glyph
        CachedGlyph& cached = glyphCache[key];
        buildGlyphPath(codepoint, fontSize, cached);
        return cached;
    }

private:
    void buildGlyphPath(int codepoint, float fontSize, CachedGlyph& out) {
        out.valid = false;
        out.path.clear();

        int glyphIndex = stbtt_FindGlyphIndex(&fontInfo, codepoint);
        if (glyphIndex == 0 && codepoint != 0) {
            // Glyph not found, try to use .notdef or space
            return;
        }

        float scale = fontSize / unitsPerEm;

        // Get metrics
        int advanceWidth, leftSideBearing;
        stbtt_GetGlyphHMetrics(&fontInfo, glyphIndex, &advanceWidth, &leftSideBearing);
        out.advanceWidth = advanceWidth * scale;
        out.leftSideBearing = leftSideBearing * scale;

        // Get shape
        stbtt_vertex* vertices = nullptr;
        int numVerts = stbtt_GetGlyphShape(&fontInfo, glyphIndex, &vertices);

        if (numVerts == 0 || !vertices) {
            // Space or empty glyph - still valid for advance
            out.valid = true;
            return;
        }

        // Convert to SDFPath
        for (int i = 0; i < numVerts; i++) {
            float x = vertices[i].x * scale;
            float y = -vertices[i].y * scale;  // Y flip (TTF: Y-up, screen: Y-down)

            switch (vertices[i].type) {
            case STBTT_vmove:
                out.path.moveTo(x, y);
                break;

            case STBTT_vline:
                out.path.lineTo(x, y);
                break;

            case STBTT_vcurve: {
                // Quadratic Bezier
                float cx = vertices[i].cx * scale;
                float cy = -vertices[i].cy * scale;
                out.path.quadTo(cx, cy, x, y);
                break;
            }

            case STBTT_vcubic: {
                // Cubic Bezier (TrueType doesn't use this, but OpenType/CFF does)
                float c1x = vertices[i].cx * scale;
                float c1y = -vertices[i].cy * scale;
                float c2x = vertices[i].cx1 * scale;
                float c2y = -vertices[i].cy1 * scale;
                out.path.cubicTo(c1x, c1y, c2x, c2y, x, y);
                break;
            }
            }
        }

        out.path.end();
        stbtt_FreeShape(&fontInfo, vertices);
        out.valid = true;
    }

public:
    // ==================== Drawing API ====================

    // Draw single character
    void drawChar(SdfRenderer2D& sdf, int codepoint, float x, float y, float fontSize) {
        const CachedGlyph& glyph = getGlyph(codepoint, fontSize);
        if (glyph.valid && glyph.path.getPointCount() > 0) {
            sdf.drawPath(glyph.path, x, y);
        }
    }

    // Draw text string (ASCII)
    void drawText(SdfRenderer2D& sdf, const char* text, float x, float y, float fontSize) {
        if (!loaded || !text) return;

        float scale = fontSize / unitsPerEm;
        float cursorX = x;
        int prevGlyphIndex = 0;

        sdf.pathFillRule = FILL_NONZERO;

        while (*text) {
            int codepoint = (unsigned char)*text++;
            int glyphIndex = stbtt_FindGlyphIndex(&fontInfo, codepoint);

            // Kerning
            if (prevGlyphIndex && glyphIndex) {
                int kern = stbtt_GetGlyphKernAdvance(&fontInfo, prevGlyphIndex, glyphIndex);
                cursorX += kern * scale;
            }

            // Draw glyph
            const CachedGlyph& glyph = getGlyph(codepoint, fontSize);
            if (glyph.valid && glyph.path.getPointCount() > 0) {
                sdf.drawPath(glyph.path, cursorX, y);
            }

            cursorX += glyph.advanceWidth;
            prevGlyphIndex = glyphIndex;
        }
    }

    // Draw text with std::string
    void drawText(SdfRenderer2D& sdf, const std::string& text, float x, float y, float fontSize) {
        drawText(sdf, text.c_str(), x, y, fontSize);
    }

    // Draw UTF-8 text
    void drawTextUTF8(SdfRenderer2D& sdf, const char* text, float x, float y, float fontSize) {
        if (!loaded || !text) return;

        float scale = fontSize / unitsPerEm;
        float cursorX = x;
        int prevGlyphIndex = 0;

        sdf.pathFillRule = FILL_NONZERO;

        const unsigned char* p = (const unsigned char*)text;
        while (*p) {
            int codepoint = decodeUTF8(p);
            if (codepoint < 0) break;

            int glyphIndex = stbtt_FindGlyphIndex(&fontInfo, codepoint);

            // Kerning
            if (prevGlyphIndex && glyphIndex) {
                int kern = stbtt_GetGlyphKernAdvance(&fontInfo, prevGlyphIndex, glyphIndex);
                cursorX += kern * scale;
            }

            // Draw glyph
            const CachedGlyph& glyph = getGlyph(codepoint, fontSize);
            if (glyph.valid && glyph.path.getPointCount() > 0) {
                sdf.drawPath(glyph.path, cursorX, y);
            }

            cursorX += glyph.advanceWidth;
            prevGlyphIndex = glyphIndex;
        }
    }

    // ==================== Measurement API ====================

    // Measure single character advance
    float getCharAdvance(int codepoint, float fontSize) {
        const CachedGlyph& glyph = getGlyph(codepoint, fontSize);
        return glyph.advanceWidth;
    }

    // Measure text width
    float measureText(const char* text, float fontSize) {
        if (!loaded || !text) return 0;

        float scale = fontSize / unitsPerEm;
        float width = 0;
        int prevGlyphIndex = 0;

        while (*text) {
            int codepoint = (unsigned char)*text++;
            int glyphIndex = stbtt_FindGlyphIndex(&fontInfo, codepoint);

            // Kerning
            if (prevGlyphIndex && glyphIndex) {
                int kern = stbtt_GetGlyphKernAdvance(&fontInfo, prevGlyphIndex, glyphIndex);
                width += kern * scale;
            }

            const CachedGlyph& glyph = getGlyph(codepoint, fontSize);
            width += glyph.advanceWidth;
            prevGlyphIndex = glyphIndex;
        }

        return width;
    }

    float measureText(const std::string& text, float fontSize) {
        return measureText(text.c_str(), fontSize);
    }

    // Measure UTF-8 text width
    float measureTextUTF8(const char* text, float fontSize) {
        if (!loaded || !text) return 0;

        float scale = fontSize / unitsPerEm;
        float width = 0;
        int prevGlyphIndex = 0;

        const unsigned char* p = (const unsigned char*)text;
        while (*p) {
            int codepoint = decodeUTF8(p);
            if (codepoint < 0) break;

            int glyphIndex = stbtt_FindGlyphIndex(&fontInfo, codepoint);

            if (prevGlyphIndex && glyphIndex) {
                int kern = stbtt_GetGlyphKernAdvance(&fontInfo, prevGlyphIndex, glyphIndex);
                width += kern * scale;
            }

            const CachedGlyph& glyph = getGlyph(codepoint, fontSize);
            width += glyph.advanceWidth;
            prevGlyphIndex = glyphIndex;
        }

        return width;
    }

    // Get font metrics scaled to fontSize
    float getAscender(float fontSize) const {
        return (ascender / unitsPerEm) * fontSize;
    }

    float getDescender(float fontSize) const {
        return (descender / unitsPerEm) * fontSize;
    }

    float getLineHeight(float fontSize) const {
        return ((ascender - descender + lineGap) / unitsPerEm) * fontSize;
    }

    // ==================== Cache Management ====================

    void clearCache() {
        glyphCache.clear();
        cacheHits = 0;
        cacheMisses = 0;
    }

    void setMaxCacheSize(size_t size) {
        maxCacheSize = size;
    }

    size_t getCacheSize() const {
        return glyphCache.size();
    }

    float getCacheHitRate() const {
        size_t total = cacheHits + cacheMisses;
        return total > 0 ? (float)cacheHits / total : 0.0f;
    }

    // Preload common ASCII characters
    void preloadASCII(float fontSize) {
        for (int c = 32; c < 127; c++) {
            getGlyph(c, fontSize);
        }
    }

    // Preload specific characters
    void preloadChars(const char* chars, float fontSize) {
        while (*chars) {
            getGlyph((unsigned char)*chars++, fontSize);
        }
    }

    // Preload UTF-8 characters
    void preloadCharsUTF8(const char* chars, float fontSize) {
        const unsigned char* p = (const unsigned char*)chars;
        while (*p) {
            int cp = decodeUTF8(p);
            if (cp < 0) break;
            getGlyph(cp, fontSize);
        }
    }

private:
    // Simple UTF-8 decoder
    static int decodeUTF8(const unsigned char*& p) {
        if (*p == 0) return -1;

        int codepoint;
        if ((*p & 0x80) == 0) {
            // 1-byte (ASCII)
            codepoint = *p++;
        } else if ((*p & 0xE0) == 0xC0) {
            // 2-byte
            codepoint = (*p++ & 0x1F) << 6;
            if ((*p & 0xC0) != 0x80) return -1;
            codepoint |= (*p++ & 0x3F);
        } else if ((*p & 0xF0) == 0xE0) {
            // 3-byte
            codepoint = (*p++ & 0x0F) << 12;
            if ((*p & 0xC0) != 0x80) return -1;
            codepoint |= (*p++ & 0x3F) << 6;
            if ((*p & 0xC0) != 0x80) return -1;
            codepoint |= (*p++ & 0x3F);
        } else if ((*p & 0xF8) == 0xF0) {
            // 4-byte
            codepoint = (*p++ & 0x07) << 18;
            if ((*p & 0xC0) != 0x80) return -1;
            codepoint |= (*p++ & 0x3F) << 12;
            if ((*p & 0xC0) != 0x80) return -1;
            codepoint |= (*p++ & 0x3F) << 6;
            if ((*p & 0xC0) != 0x80) return -1;
            codepoint |= (*p++ & 0x3F);
        } else {
            // Invalid
            p++;
            return -1;
        }

        return codepoint;
    }
};


namespace fs = std::filesystem;

//----------------------------------------------------------
// Raylib Font Wrapper - manages a single font variant
// Includes FontToPath for SDF rendering
//----------------------------------------------------------
struct axFont {
    Font font = {0};
    bool loaded = false;
    int fontSize = 32;  // Base size for loading
    std::shared_ptr<std::vector<unsigned char>> memoryData;  // Keep memory data alive for memory-loaded fonts

    // SDF Path rendering support
    std::shared_ptr<FontToPath> sdfFont;
    bool sdfLoaded = false;

    bool isLoaded() const { return loaded && font.texture.id != 0; }
    bool isSdfLoaded() const { return sdfLoaded && sdfFont && sdfFont->loaded; }

    // Initialize SDF font from the same memory data
    bool initSdfFont() {
        if (!memoryData || memoryData->empty()) return false;
        if (sdfLoaded && sdfFont && sdfFont->loaded) return true;

        sdfFont = std::make_shared<FontToPath>();
        if (sdfFont->loadFontFromMemory(memoryData->data(), memoryData->size())) {
            sdfLoaded = true;
            return true;
        }
        sdfFont.reset();
        return false;
    }

    // Initialize SDF font from file path
    bool initSdfFontFromFile(const std::string& path) {
        if (path.empty()) return false;
        if (sdfLoaded && sdfFont && sdfFont->loaded) return true;

        sdfFont = std::make_shared<FontToPath>();
        if (sdfFont->loadFont(path.c_str())) {
            sdfLoaded = true;
            return true;
        }
        sdfFont.reset();
        return false;
    }

    // Get SDF font (lazy initialization from memory if available)
    FontToPath* getSdfFont() {
        if (!sdfLoaded && memoryData && !memoryData->empty()) {
            initSdfFont();
        }
        return sdfFont ? sdfFont.get() : nullptr;
    }

    // ==================== Unified Drawing API ====================
    // Both SDF and Raylib rendering use stb_truetype float advance
    // for pixel-perfect alignment between modes

    // Helper: decode one UTF-8 codepoint, advance pointer
    static int decodeUTF8(const unsigned char*& p) {
        if (*p < 0x80) return *p++;
        int cp = 0; int extra = 0;
        if ((*p & 0xE0) == 0xC0)      { cp = *p & 0x1F; extra = 1; }
        else if ((*p & 0xF0) == 0xE0)  { cp = *p & 0x0F; extra = 2; }
        else if ((*p & 0xF8) == 0xF0)  { cp = *p & 0x07; extra = 3; }
        else { p++; return 0xFFFD; }
        p++;
        for (int i = 0; i < extra; i++) {
            if ((*p & 0xC0) != 0x80) return 0xFFFD;
            cp = (cp << 6) | (*p & 0x3F);
            p++;
        }
        return cp;
    }

    // Get stb_truetype float advance for a codepoint (industry standard)
    float getStbAdvance(int codepoint, float size) const {
        if (!sdfFont || !sdfFont->loaded) return 0;
        float scale = size / sdfFont->unitsPerEm;
        int glyphIndex = stbtt_FindGlyphIndex(&sdfFont->fontInfo, codepoint);
        int advanceWidth, lsb;
        stbtt_GetGlyphHMetrics(&sdfFont->fontInfo, glyphIndex, &advanceWidth, &lsb);
        return advanceWidth * scale;
    }

    // Get stb_truetype kerning between two glyphs
    float getStbKerning(int cp1, int cp2, float size) const {
        if (!sdfFont || !sdfFont->loaded) return 0;
        float scale = size / sdfFont->unitsPerEm;
        int g1 = stbtt_FindGlyphIndex(&sdfFont->fontInfo, cp1);
        int g2 = stbtt_FindGlyphIndex(&sdfFont->fontInfo, cp2);
        if (g1 && g2) return stbtt_GetGlyphKernAdvance(&sdfFont->fontInfo, g1, g2) * scale;
        return 0;
    }

    // Our DrawTextEx replacement: Raylib texture glyphs + stb_truetype advance
    void drawTextRaylib(const char* text, float x, float y, float size, Color tint) {
        if (!isLoaded()) return;

        // If no SDF font for metrics, fall back to Raylib DrawTextEx
        if (!isSdfLoaded()) {
            DrawTextEx(font, text, {x, y}, size, 0, tint);
            return;
        }

        float scaleFactor = size / (float)font.baseSize;
        float cursorX = x;
        int prevCodepoint = 0;

        const unsigned char* p = (const unsigned char*)text;
        while (*p) {
            int codepoint = decodeUTF8(p);
            if (codepoint <= 0) break;

            if (codepoint == '\n') {
                cursorX = x;
                y += size * 1.5f;  // TODO: configurable line spacing
                prevCodepoint = 0;
                continue;
            }

            // Kerning (stb_truetype float)
            if (prevCodepoint) {
                cursorX += getStbKerning(prevCodepoint, codepoint, size);
            }

            // Draw Raylib glyph texture at stb_truetype position
            int index = GetGlyphIndex(font, codepoint);
            DrawTextCodepoint(font, codepoint, {cursorX, y}, size, tint);

            // Advance using stb_truetype float metrics
            cursorX += getStbAdvance(codepoint, size);
            prevCodepoint = codepoint;
        }
    }

    void drawText(const char* text, float x, float y, float size,
                  Color color = WHITE, SdfRenderer2D* sdf = nullptr,
                  int align = BOT_ALIGN_LEFT | BOT_ALIGN_TOP) {
        if (!text || !text[0]) return;

        // --- Compute alignment offsets ---
        float ox = 0, oy = 0;
        float textW = measureText(text, size);

        // Horizontal
        if (align & BOT_ALIGN_CENTER)      ox = -textW / 2.0f;
        else if (align & BOT_ALIGN_RIGHT)  ox = -textW;

        // Vertical — use real font metrics
        float asc  = getAscender(size);   // positive: top of glyphs above baseline
        float desc = getDescender(size);  // negative: bottom of glyphs below baseline
        float totalH = asc - desc;        // full glyph height (asc is +, desc is -)

        if (align & BOT_ALIGN_MIDDLE)        oy = -totalH / 2.0f;
        else if (align & BOT_ALIGN_BOTTOM)   oy = -totalH;
        else if (align & BOT_ALIGN_BASELINE) oy = -asc;
        // BOT_ALIGN_TOP (default): oy = 0 → y is at the top of the text

        float drawX = x + ox;
        float drawY = y + oy;

        // --- Render ---
        if (sdf && isSdfLoaded()) {
            // SDF path rendering + stb_truetype advance
            // In SDF mode, glyphs are drawn relative to baseline
            float baselineY = drawY + asc;
            float cursorX = drawX;
            int prevCodepoint = 0;

            sdf->pathFillRule = FILL_NONZERO;

            const unsigned char* p = (const unsigned char*)text;
            while (*p) {
                int codepoint = decodeUTF8(p);
                if (codepoint <= 0) break;

                // Kerning
                if (prevCodepoint) {
                    cursorX += getStbKerning(prevCodepoint, codepoint, size);
                }

                // Draw SDF glyph
                const CachedGlyph& glyph = sdfFont->getGlyph(codepoint, size);
                if (glyph.valid && glyph.path.getPointCount() > 0) {
                    sdf->drawPath(glyph.path, cursorX, baselineY);
                }

                // Advance using stb_truetype float metrics
                cursorX += getStbAdvance(codepoint, size);
                prevCodepoint = codepoint;
            }
        } else {
            // Raylib texture rendering + stb_truetype advance
            drawTextRaylib(text, drawX, drawY, size, color);
        }
    }

    void drawText(const std::string& text, float x, float y, float size,
                  Color color = WHITE, SdfRenderer2D* sdf = nullptr,
                  int align = BOT_ALIGN_LEFT | BOT_ALIGN_TOP) {
        drawText(text.c_str(), x, y, size, color, sdf, align);
    }

    float measureText(const char* text, float size) {
        // stb_truetype float measurement (consistent for both modes)
        if (isSdfLoaded()) {
            return sdfFont->measureTextUTF8(text, size);
        }
        if (isLoaded()) {
            Vector2 m = MeasureTextEx(font, text, size, 0);
            return m.x;
        }
        return 0.0f;
    }

    float measureText(const std::string& text, float size) {
        return measureText(text.c_str(), size);
    }

    // ==================== Metrics API ====================

    float getAscender(float size) const {
        if (sdfFont && sdfLoaded) return sdfFont->getAscender(size);
        return size * 0.8f;
    }

    float getDescender(float size) const {
        if (sdfFont && sdfLoaded) return sdfFont->getDescender(size);
        return size * -0.2f;
    }

    float getLineHeight(float size) const {
        if (sdfFont && sdfLoaded) return sdfFont->getLineHeight(size);
        return size * 1.2f;
    }

    // ==================== Preload ====================

    void preloadGlyphs(float size) {
        if (FontToPath* f = getSdfFont()) {
            f->preloadASCII(size);
        }
    }

    void preloadGlyphsUTF8(const char* chars, float size) {
        if (FontToPath* f = getSdfFont()) {
            f->preloadCharsUTF8(chars, size);
        }
    }

    // ==================== Lifecycle ====================

    void unload() {
        if (loaded && font.texture.id != 0) {
            UnloadFont(font);
        }
        font = {0};
        loaded = false;

        // Unload SDF font
        if (sdfFont) {
            sdfFont->unload();
            sdfFont.reset();
        }
        sdfLoaded = false;

        memoryData.reset();
    }

    ~axFont() {
        // Note: Don't unload here - let FontManager handle cleanup
    }
};
