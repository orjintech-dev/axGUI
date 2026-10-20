// axRichFonts.cpp - ported from NotePen fonts.cpp (font family download / fontlib and Android paths removed)
#include "axRichFonts.h"
#include "axFont.h"   // axFont (axGUI) + stb_truetype.h
#include <fstream>
#include <vector>
#include <memory>
#include <unordered_map>
#include <map>
#include <cmath>
#include <cstdio>
#include <algorithm>

static RtFonts g_fonts;
RtFonts &rtFonts() { return g_fonts; }

// ------------------------------------------------------------------ glyph sets
static std::vector<int> fullCodepoints()
{
    std::vector<int> cp;
    auto range = [&](int a, int b) { for (int i = a; i <= b; i++) cp.push_back(i); };
    range(32, 126);        // ASCII
    range(160, 255);       // Latin-1
    range(0x100, 0x17F);   // Latin Extended-A (Turkish letters etc.)
    range(0x370, 0x3FF);   // Greek
    range(0x400, 0x45F);   // Cyrillic (basic)
    range(0x2010, 0x2027); // dash, quotes, bullet, ellipsis
    range(0x2030, 0x203A);
    cp.push_back(0x20AC); cp.push_back(0x20BA);           // € ₺
    range(0x2190, 0x2195);                                // arrows
    cp.push_back(0x2122);
    cp.push_back(0x25CF); cp.push_back(0x25CB); cp.push_back(0x25A0); cp.push_back(0x25A1);
    cp.push_back(0x2610); cp.push_back(0x2611); cp.push_back(0x2612);
    cp.push_back(0x2713); cp.push_back(0x2714); cp.push_back(0x2715); cp.push_back(0x2716);
    return cp;
}

// Symbols: arrows, math, box drawing (│ ├ └ ─ ┌ ...), block elements, geometric shapes.
// So pasted tree / table diagrams do not show "?". Only for atlases <= 64 px to save memory (~370 extra glyphs).
static std::vector<int> symbolCodepoints()
{
    std::vector<int> cp = fullCodepoints();
    auto range = [&](int a, int b) { for (int i = a; i <= b; i++) cp.push_back(i); };
    range(0x2196, 0x21FF);   // arrows (0x2190-0x2195 in the full set)
    for (int c : { 0x2212, 0x221A, 0x221E, 0x2248, 0x2260, 0x2264, 0x2265, 0x2211, 0x2206, 0x2318, 0x23CE })
        cp.push_back(c);     // − √ ∞ ≈ ≠ ≤ ≥ ∑ ∆ ⌘ ⏎
    range(0x2500, 0x257F);   // box drawing
    range(0x2580, 0x259F);   // block elements
    range(0x25A0, 0x25FF);   // geometric shapes
    for (int c : { 0x2605, 0x2606, 0x2665, 0x2717, 0x2718, 0x279C, 0x27A4 }) cp.push_back(c);   // ★ ☆ ♥ ✗ ✘ ➜ ➤
    std::sort(cp.begin(), cp.end());
    cp.erase(std::unique(cp.begin(), cp.end()), cp.end());
    return cp;
}

// Reduced set for large atlases (memory): Latin + punctuation
static std::vector<int> latinCodepoints()
{
    std::vector<int> cp;
    auto range = [&](int a, int b) { for (int i = a; i <= b; i++) cp.push_back(i); };
    range(32, 126); range(160, 255); range(0x100, 0x17F); range(0x2010, 0x2027);
    cp.push_back(0x20AC); cp.push_back(0x20BA);
    return cp;
}

// Screen pixels -> atlas size (quantized so only a limited number of atlases are created)
static int quantPx(float px)
{
    int p = (int)ceilf(px - 0.05f);
    if (p < 6) p = 6;
    if (p <= 28) return p;
    if (p <= 48) return (p + 1) / 2 * 2;
    if (p <= 80) return (p + 3) / 4 * 4;
    if (p <= 128) return (p + 15) / 16 * 16;
    if (p <= 160) return 160;
    return 192;   // above that: the 192 atlas is drawn scaled up
}

// ------------------------------------------------------------------ internals
struct RtFonts::Impl
{
    struct GlyphM { int index = 0; int adv = 0; };
    struct Variant {
        bool ok = false;
        std::string path;
        std::shared_ptr<std::vector<unsigned char>> data;
        stbtt_fontinfo info{};
        int ascent = 0, descent = 0, lineGap = 0;
        std::unordered_map<int, GlyphM> glyphs;
        GlyphM glyph(int cp) {
            auto it = glyphs.find(cp);
            if (it != glyphs.end()) return it->second;
            GlyphM g;
            g.index = stbtt_FindGlyphIndex(&info, cp);
            int adv = 0, lsb = 0;
            stbtt_GetGlyphHMetrics(&info, g.index, &adv, &lsb);
            g.adv = adv;
            glyphs[cp] = g;
            return g;
        }
    };
    struct Atlas { Font font{}; long lastUsed = 0; int set = 1; };   // set: 2 with symbols, 1 full, 0 Latin

    Variant var[RFV_COUNT];
    // downloaded font families: 4 variants (RFV_REGULAR..RFV_BOLD_ITALIC); mono always comes from the default
    struct Family { std::string name; Variant var[4]; };
    std::vector<Family> fams;
    std::unordered_map<std::string, int> famIndex;
    int cur = -1;                          // current family (-1: default)
    std::map<int, Atlas> atlases;          // key: variant index (variantIndex) * 1000 + px
    std::map<int, axFont *> guiFonts;      // px -> axFont (fixed, never deleted)
    std::vector<int> fullCp = fullCodepoints(), latinCp = latinCodepoints(), symCp = symbolCodepoints();
    long frame = 0;
    bool anyFile = false;

    // variant index: 0..RFV_COUNT-1 default; 8 + family * 8 + variant (family). Missing variant: bold-italic -> bold,
    // italic -> regular (within the family; no synthetic slant)
    int variantIndex(RtFontVariant fv) {
        if (fv != RFV_MONO && cur >= 0 && cur < (int)fams.size()) {
            Family &F = fams[(size_t)cur];
            int i = (int)fv;
            if (!F.var[i].ok) i = (i == RFV_BOLD_ITALIC && F.var[RFV_BOLD].ok) ? RFV_BOLD : (i == RFV_BOLD_ITALIC && F.var[RFV_ITALIC].ok) ? RFV_ITALIC : RFV_REGULAR;
            return 8 + cur * 8 + i;
        }
        if (var[fv].ok) return fv;
        if (fv == RFV_BOLD_ITALIC && var[RFV_BOLD].ok) return RFV_BOLD;
        return RFV_REGULAR;
    }
    Variant &byIndex(int vi) { return vi < 8 ? var[vi] : fams[(size_t)((vi - 8) / 8)].var[(vi - 8) % 8]; }
    Variant &v(RtFontVariant fv) { return byIndex(variantIndex(fv)); }

    float gamma = 1.3f;   // text contrast (setTextGamma)

    // same steps as LoadFontFromMemory; a contrast curve is applied to the alpha before the atlas is uploaded to the texture
    Font loadAtlas(int vi, int px, int set)
    {
        Variant &V = byIndex(vi);
        std::vector<int> &cps = set == 2 ? symCp : set == 1 ? fullCp : latinCp;
        Font f = { 0 };
        f.baseSize = px;
        f.glyphs = LoadFontData(V.data->data(), (int)V.data->size(), px, cps.data(), (int)cps.size(), FONT_DEFAULT, &f.glyphCount);
        if (!f.glyphs) return GetFontDefault();
        f.glyphPadding = 4;
        Image atlas = GenImageFontAtlas(f.glyphs, &f.recs, f.glyphCount, f.baseSize, f.glyphPadding, 0);
        if (gamma > 1.001f || gamma < 0.999f) {
            unsigned char lut[256];
            for (int i = 0; i < 256; i++) lut[i] = (unsigned char)lroundf(255.0f * powf(i / 255.0f, 1.0f / gamma));
            unsigned char *d = (unsigned char *)atlas.data;
            int n = atlas.width * atlas.height;
            if (atlas.format == PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA) for (int i = 0; i < n; i++) d[i * 2 + 1] = lut[d[i * 2 + 1]];
            else if (atlas.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) for (int i = 0; i < n; i++) d[i * 4 + 3] = lut[d[i * 4 + 3]];
        }
        f.texture = LoadTextureFromImage(atlas);
        for (int i = 0; i < f.glyphCount; i++) { UnloadImage(f.glyphs[i].image); f.glyphs[i].image = ImageFromImage(atlas, f.recs[i]); }
        UnloadImage(atlas);
        if (f.texture.id) SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
        return f;
    }

    Font &atlas(int vi, int px)
    {
        int key = vi * 1000 + px;
        auto it = atlases.find(key);
        if (it == atlases.end()) {
            Atlas a;
            a.set = px <= 64 ? 2 : px <= 128 ? 1 : 0;
            a.font = loadAtlas(vi, px, a.set);
            it = atlases.emplace(key, a).first;
        }
        it->second.lastUsed = frame;
        return it->second.font;
    }
};

// ------------------------------------------------------------------ loading
// Read the file into memory
static bool readFontFile(const std::string &path, std::vector<unsigned char> &out)
{
    out.clear();
    if (!FileExists(path.c_str())) return false;
    int size = 0;
    unsigned char *d = LoadFileData(path.c_str(), &size);
    if (!d || size <= 0) { if (d) UnloadFileData(d); return false; }
    out.assign(d, d + size);
    UnloadFileData(d);
    return true;
}

bool RtFonts::load(const std::string &fontDir)
{
    if (impl_) unload();
    impl_ = new Impl();
    std::string fd = fontDir;
    if (!fd.empty() && fd.back() != '/' && fd.back() != '\\') fd += "/";
    std::vector<std::string> cand[RFV_COUNT] = {
        { fd+"regular.ttf", "C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf",
          "/System/Library/Fonts/Supplemental/Arial.ttf", "/Library/Fonts/Arial.ttf",
          "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "/usr/share/fonts/TTF/DejaVuSans.ttf" },
        { fd+"bold.ttf", "C:/Windows/Fonts/segoeuib.ttf", "C:/Windows/Fonts/arialbd.ttf",
          "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
          "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf" },
        { fd+"italic.ttf", "C:/Windows/Fonts/segoeuii.ttf", "C:/Windows/Fonts/ariali.ttf",
          "/System/Library/Fonts/Supplemental/Arial Italic.ttf",
          "/usr/share/fonts/truetype/dejavu/DejaVuSans-Oblique.ttf", "/usr/share/fonts/TTF/DejaVuSans-Oblique.ttf" },
        { fd+"bolditalic.ttf", "C:/Windows/Fonts/segoeuiz.ttf", "C:/Windows/Fonts/arialbi.ttf",
          "/System/Library/Fonts/Supplemental/Arial Bold Italic.ttf",
          "/usr/share/fonts/truetype/dejavu/DejaVuSans-BoldOblique.ttf", "/usr/share/fonts/TTF/DejaVuSans-BoldOblique.ttf" },
        { fd+"mono.ttf", "C:/Windows/Fonts/consola.ttf", "C:/Windows/Fonts/cour.ttf",
          "/System/Library/Fonts/Supplemental/Courier New.ttf",
          "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", "/usr/share/fonts/TTF/DejaVuSansMono.ttf" },
    };
    for (int i = 0; i < RFV_COUNT; i++) {
        Impl::Variant &V = impl_->var[i];
        std::vector<unsigned char> bytes;
        for (auto &p : cand[i]) {
            if (p.empty() || !readFontFile(p, bytes)) continue;
            auto data = std::make_shared<std::vector<unsigned char>>(std::move(bytes));
            stbtt_fontinfo info{};
            if (!stbtt_InitFont(&info, data->data(), stbtt_GetFontOffsetForIndex(data->data(), 0))) continue;
            V.path = p;
            V.data = data;
            V.info = info;
            stbtt_GetFontVMetrics(&V.info, &V.ascent, &V.descent, &V.lineGap);
            V.ok = true;
            impl_->anyFile = true;
            break;
        }
    }
    if (!impl_->var[RFV_REGULAR].ok) {
        // if there is no regular variant, treat the first variant found as regular
        for (int i = 1; i < RFV_COUNT; i++) if (impl_->var[i].ok) { impl_->var[RFV_REGULAR] = impl_->var[i]; break; }
    }
    if (!impl_->anyFile) TraceLog(LOG_WARNING, "axGUI rich: TTF font bulunamadı, raylib varsayılan fontu kullanılacak");
    beginFrame();
    return true;
}

void RtFonts::unload()
{
    if (!impl_) return;
    for (auto &kv : impl_->atlases) if (kv.second.font.texture.id) UnloadFont(kv.second.font);
    for (auto &kv : impl_->guiFonts) { if (kv.second->font.texture.id) UnloadFont(kv.second->font); delete kv.second; }
    delete impl_;
    impl_ = nullptr;
}

void RtFonts::beginFrame()
{
    if (!impl_) return;
    impl_->frame++;
    // logical -> physical pixels (1 without FLAG_WINDOW_HIGHDPI: raylib draws 1:1)
    {
        int sw = GetScreenWidth(), rw = GetRenderWidth();
        float k = (sw > 0 && rw > 0) ? (float)rw / (float)sw : 1.0f;
        dpi_ = (k > 0.5f && k < 8.0f) ? k : 1.0f;
    }
    // drop atlases unused for a long time (intermediate sizes accumulated during zoom)
    if (impl_->atlases.size() > 24 && (impl_->frame % 60) == 0) {
        for (auto it = impl_->atlases.begin(); it != impl_->atlases.end();) {
            if (impl_->frame - it->second.lastUsed > 600) { UnloadFont(it->second.font); it = impl_->atlases.erase(it); }
            else ++it;
        }
    }
}

std::string RtFonts::resolvedPath(RtFontVariant v) const { return impl_ ? impl_->var[v].path : ""; }

void RtFonts::setTextGamma(float g)
{
    if (!impl_) return;
    g = fminf(3.0f, fmaxf(0.5f, g));
    if (fabsf(g - impl_->gamma) < 1e-3f) return;
    impl_->gamma = g;
    // rich-text atlases: regenerated on first use
    for (auto &kv : impl_->atlases) if (kv.second.font.texture.id) UnloadFont(kv.second.font);
    impl_->atlases.clear();
    // GUI text: axFont objects are kept outside and refreshed in place
    for (auto &kv : impl_->guiFonts) {
        if (kv.second->font.texture.id) UnloadFont(kv.second->font);
        kv.second->font = impl_->var[RFV_REGULAR].ok ? impl_->loadAtlas(RFV_REGULAR, kv.first, true) : GetFontDefault();
    }
    generation_++;
}

float RtFonts::textGamma() const { return impl_ ? impl_->gamma : 1.0f; }

axFont *RtFonts::guiFont(float logicalSize)
{
    int px = quantPx(logicalSize * dpi_);
    auto it = impl_->guiFonts.find(px);
    if (it != impl_->guiFonts.end()) return it->second;
    axFont *f = new axFont();
    if (impl_->var[RFV_REGULAR].ok) f->font = impl_->loadAtlas(RFV_REGULAR, px, true);
    else f->font = GetFontDefault();
    f->loaded = true;
    f->fontSize = px;
    impl_->guiFonts[px] = f;
    return f;
}

// ------------------------------------------------------------------ measure / draw
static inline int decodeUtf8(const unsigned char *&p)
{
    if (*p < 0x80) return *p++;
    int cp = 0, extra = 0;
    if ((*p & 0xE0) == 0xC0) { cp = *p & 0x1F; extra = 1; }
    else if ((*p & 0xF0) == 0xE0) { cp = *p & 0x0F; extra = 2; }
    else if ((*p & 0xF8) == 0xF0) { cp = *p & 0x07; extra = 3; }
    else { p++; return 0xFFFD; }
    p++;
    for (int i = 0; i < extra; i++) {
        if ((*p & 0xC0) != 0x80) return 0xFFFD;
        cp = (cp << 6) | (*p & 0x3F);
        p++;
    }
    return cp;
}

float RtFonts::measure(const std::string &s, float size, RtFontVariant fv)
{
    if (s.empty()) return 0;
    if (!impl_ || !impl_->anyFile) return MeasureTextEx(GetFontDefault(), s.c_str(), size, size / 10).x;
    Impl::Variant &V = impl_->v(fv);
    float sc = stbtt_ScaleForPixelHeight(&V.info, size);
    float w = 0;
    int prev = 0;
    const unsigned char *p = (const unsigned char *)s.c_str();
    while (*p) {
        int cp = decodeUtf8(p);
        if (cp == '\n' || cp == '\r') continue;
        Impl::GlyphM g = V.glyph(cp);
        if (prev && g.index) w += stbtt_GetGlyphKernAdvance(&V.info, prev, g.index) * sc;
        w += g.adv * sc;
        prev = g.index;
    }
    return w;
}

float RtFonts::advances(const std::string &s, float size, RtFontVariant fv, std::vector<float> &xs)
{
    xs.clear();
    if (s.empty()) return 0;
    if (!impl_ || !impl_->anyFile) {
        // fallback: measure each code point separately
        float x = 0;
        const unsigned char *p = (const unsigned char *)s.c_str();
        while (*p) {
            const unsigned char *st = p;
            decodeUtf8(p);
            xs.push_back(x);
            std::string ch((const char *)st, p - st);
            x += MeasureTextEx(GetFontDefault(), ch.c_str(), size, size / 10).x + size / 10;
        }
        return x;
    }
    Impl::Variant &V = impl_->v(fv);
    float sc = stbtt_ScaleForPixelHeight(&V.info, size);
    float x = 0;
    int prev = 0;
    const unsigned char *p = (const unsigned char *)s.c_str();
    while (*p) {
        int cp = decodeUtf8(p);
        Impl::GlyphM g = V.glyph(cp);
        if (prev && g.index) x += stbtt_GetGlyphKernAdvance(&V.info, prev, g.index) * sc;
        xs.push_back(x);
        x += g.adv * sc;
        prev = g.index;
    }
    return x;
}

float RtFonts::ascent(float size, RtFontVariant fv)
{
    if (!impl_ || !impl_->anyFile) return size * 0.8f;
    Impl::Variant &V = impl_->v(fv);
    return V.ascent * stbtt_ScaleForPixelHeight(&V.info, size);
}

void RtFonts::draw(const std::string &s, Vector2 pos, float size, Color c, RtFontVariant fv)
{
    if (s.empty() || size <= 0) return;
    if (!impl_ || !impl_->anyFile) { DrawTextEx(GetFontDefault(), s.c_str(), pos, size, size / 10, c); return; }
    const float pxPerUnit = rScale_ * dpi_;
    const float px = size * pxPerUnit;
    if (px < 1.5f) return;                           // too small to be legible
    int vi = impl_->variantIndex(fv);
    Impl::Variant &V = impl_->byIndex(vi);
    Font &f = impl_->atlas(vi, quantPx(px));
    float sc = stbtt_ScaleForPixelHeight(&V.info, size);

    auto snapX = [&](float w) { return (roundf((w * rScale_ + rOffset_.x) * dpi_) / dpi_ - rOffset_.x) / rScale_; };
    auto snapY = [&](float w) { return (roundf((w * rScale_ + rOffset_.y) * dpi_) / dpi_ - rOffset_.y) / rScale_; };

    float x = pos.x;
    float y = snap_ ? snapY(pos.y) : pos.y;
    int prev = 0;
    const unsigned char *p = (const unsigned char *)s.c_str();
    while (*p) {
        int cp = decodeUtf8(p);
        if (cp == '\n') { x = pos.x; y += size * 1.2f; prev = 0; continue; }
        if (cp == '\r') continue;
        Impl::GlyphM g = V.glyph(cp);
        if (prev && g.index) x += stbtt_GetGlyphKernAdvance(&V.info, prev, g.index) * sc;
        if (cp != ' ' && cp != 0xA0) {
            Font *use = &f;
            if (f.baseSize > 128) {
                // glyph missing from the reduced set: draw from the full-set 128px atlas
                int gi = GetGlyphIndex(f, cp);
                if (f.glyphs[gi].value != cp) use = &impl_->atlas(vi, 128);
            }
            float gx = snap_ ? snapX(x) : x;
            DrawTextCodepoint(*use, cp, { gx, y }, size, c);
        }
        x += g.adv * sc;
        prev = g.index;
    }
}

// ------------------------------------------------------------------ font families
static bool loadVariantFile(const std::string &path, RtFonts::Impl::Variant &V)
{
    std::vector<unsigned char> bytes;
    if (path.empty() || !readFontFile(path, bytes)) return false;
    auto data = std::make_shared<std::vector<unsigned char>>(std::move(bytes));
    stbtt_fontinfo info{};
    int off = stbtt_GetFontOffsetForIndex(data->data(), 0);
    if (off < 0 || !stbtt_InitFont(&info, data->data(), off)) return false;
    V.path = path;
    V.data = data;
    V.info = info;
    stbtt_GetFontVMetrics(&V.info, &V.ascent, &V.descent, &V.lineGap);
    V.ok = true;
    return true;
}

bool RtFonts::addFamily(const std::string &family, const std::string files[4])
{
    if (!impl_ || family.empty()) return false;
    Impl::Family F;
    F.name = family;
    for (int i = 0; i < 4; i++) loadVariantFile(files[i], F.var[i]);
    if (!F.var[RFV_REGULAR].ok) return false;
    auto it = impl_->famIndex.find(family);
    if (it != impl_->famIndex.end()) {
        // reload: drop the old atlases
        int base = 8 + it->second * 8;
        for (auto a = impl_->atlases.begin(); a != impl_->atlases.end();) {
            int vi = a->first / 1000;
            if (vi >= base && vi < base + 8) { UnloadFont(a->second.font); a = impl_->atlases.erase(a); }
            else ++a;
        }
        impl_->fams[(size_t)it->second] = std::move(F);
    } else {
        impl_->famIndex[family] = (int)impl_->fams.size();
        impl_->fams.push_back(std::move(F));
    }
    generation_++;
    if (famName_ == family) impl_->cur = impl_->famIndex[family];
    return true;
}

bool RtFonts::hasFamily(const std::string &family) const
{
    return impl_ && impl_->famIndex.count(family) != 0;
}

void RtFonts::setFamily(const std::string &family)
{
    famName_ = family;
    if (!impl_) return;
    if (family.empty()) { impl_->cur = -1; return; }
    auto it = impl_->famIndex.find(family);
    if (it != impl_->famIndex.end()) { impl_->cur = it->second; return; }
    impl_->cur = -1;                 // not registered yet (not added via addFamily): draw with the default
}

float rtMeasureText(const std::string &s, float size, bool bold, bool italic, bool mono)
{
    return rtFonts().measure(s, size, rtFontVariant(bold, italic, mono));
}

void rtDrawText(const std::string &s, Vector2 pos, float size, Color c, bool bold, bool italic, bool mono)
{
    rtFonts().draw(s, pos, size, c, rtFontVariant(bold, italic, mono));
}
