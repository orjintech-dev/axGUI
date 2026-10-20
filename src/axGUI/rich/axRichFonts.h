// axRichFonts.h - axGUI rich-text font system (ported from NotePen fonts.h)
//
// For sharp text:
//  * Glyphs are drawn from atlases rasterized at the size closest to the actual on-screen pixel size (fontSize * scale * DPI);
//    atlases are cached per size and unused ones are dropped after a while.
//  * Measurement / layout is size-independent: stb_truetype float advance + kerning. Line breaks do not change
//    when zooming.
//  * Glyph origins are snapped to the physical pixel grid.
// Variants: normal, bold, italic, bold-italic, monospace (code). Extra font families via addFamily.
#pragma once
#include "raylib.h"
#include <string>
#include <vector>

struct axFont;

enum RtFontVariant { RFV_REGULAR = 0, RFV_BOLD = 1, RFV_ITALIC = 2, RFV_BOLD_ITALIC = 3, RFV_MONO = 4, RFV_COUNT };

inline RtFontVariant rtFontVariant(bool bold, bool italic, bool mono = false)
{
    if (mono) return RFV_MONO;
    if (bold && italic) return RFV_BOLD_ITALIC;
    if (bold) return RFV_BOLD;
    if (italic) return RFV_ITALIC;
    return RFV_REGULAR;
}

class RtFonts
{
public:
    // fontDir: looks for regular.ttf, bold.ttf, italic.ttf, bolditalic.ttf, mono.ttf; missing ones come from system fonts
    bool load(const std::string &fontDir);
    void unload();
    bool loaded() const { return impl_ != nullptr; }
    void beginFrame();          // update DPI, release old atlases (once per frame; called by richTextBox)

    float dpi() const { return dpi_; }

    // sharp font (from the same TTF) at the given logical size, for axGUI (axDrawBot / textBox)
    axFont *guiFont(float logicalSize);

    // Scale and offset of drawn coordinates to the screen (logical pixels). UI: (1, {0,0}).
    void setRenderTransform(float scale, Vector2 offset) { rScale_ = scale; rOffset_ = offset; }
    void resetRenderTransform() { rScale_ = 1; rOffset_ = { 0, 0 }; }
    void setSnap(bool on) { snap_ = on; }
    // Text contrast: glyph alpha a' = a^(1/gamma). stb_truetype does no hinting, so thin stems spread over two pixels and look faint;
    // 1.2-1.5 sharpens them. 1: off. Atlases are regenerated when it changes.
    void setTextGamma(float g);
    float textGamma() const;
    bool snap() const { return snap_; }

    float measure(const std::string &s, float size, RtFontVariant v);
    // Start x of each code point (kerning included, identical to draw()); returns the total width
    float advances(const std::string &s, float size, RtFontVariant v, std::vector<float> &xs);
    void draw(const std::string &s, Vector2 pos, float size, Color c, RtFontVariant v);
    float ascent(float size, RtFontVariant v);   // top to baseline

    std::string resolvedPath(RtFontVariant v) const;

    // ---- font families (Quill "font" attribute). "" = default.
    void setFamily(const std::string &family);
    const std::string &family() const { return famName_; }
    // files: paths to regular, bold, italic, bolditalic (may be empty)
    bool addFamily(const std::string &family, const std::string files[4]);
    bool hasFamily(const std::string &family) const;
    // grows as families / fonts are loaded: lets layout caches re-measure
    int generation() const { return generation_; }

    struct Impl;
private:
    Impl *impl_ = nullptr;
    float dpi_ = 1.0f;
    float rScale_ = 1.0f;
    Vector2 rOffset_{ 0, 0 };
    bool snap_ = true;
    std::string famName_;
    int generation_ = 0;
};

RtFonts &rtFonts();

// Shortcuts
float rtMeasureText(const std::string &s, float size, bool bold = false, bool italic = false, bool mono = false);
void rtDrawText(const std::string &s, Vector2 pos, float size, Color c, bool bold = false, bool italic = false, bool mono = false);

// RAII helper that switches the font family for the duration of a scope ("" = default)
struct RtFontFamilyGuard
{
    std::string prev;
    explicit RtFontFamilyGuard(const std::string &family) : prev(rtFonts().family()) { if (family != prev) rtFonts().setFamily(family); }
    ~RtFontFamilyGuard() { if (rtFonts().family() != prev) rtFonts().setFamily(prev); }
};
