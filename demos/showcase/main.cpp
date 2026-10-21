// axGUI-demo - showcase and development project for axGUI (the axGUI class + axDrawBot)
// raylib 6.0 (GLFW desktop backend). No Qt dependency; qmake is only the build system.
//
// Frame loop (same layout as NotePen):
//   BeginDrawing -> gui.startFrame -> canvas + windows -> popup -> gui.endFrame -> EndDrawing
//
// Windows:
//   "Controls"   - button, toggle, checkbox, drag number, slider, dropdown, color, text box
//   "Data"       - tabs: table (sorting, multi-select, column sizing), text editor, info
//   "Catalog"    - all remaining widgets: layout, selectable, group box, shape / color buttons,
//                  color pickers, vector and Param sliders, window flags, custom table, state dump
//   Canvas       - axDrawBot drawings in the background; right-click menu (popupMenu)

#include "raylib.h"
#include "axGuiSystem.h"
#include "axGUITextBox.h"
#include "axGUIRichText.h"
#include "axRichFonts.h"

#include <string>
#include <vector>
#include <cmath>
#include <cstdio>
#include <algorithm>
#ifdef PLATFORM_WEB
#include <emscripten/emscripten.h>
// web: the canvas starts at the browser window size (raylib then tracks resizes)
EM_JS(int, webInnerWidth, (), { return window.innerWidth; });
EM_JS(int, webInnerHeight, (), { return window.innerHeight; });
#endif

extern TextBoxConfig textcfg;   // axGUITextBox.cpp

// ------------------------------------------------------------------ resource folder
// Prefers resources/ next to the exe, otherwise the resource folder passed at build time (DEMO_RES_DIR).
static std::string resPath(const char *rel)
{
    std::string p = std::string(GetApplicationDirectory()) + "resources/" + rel;
    if (FileExists(p.c_str())) return p;
#ifdef DEMO_RES_DIR
    p = std::string(DEMO_RES_DIR) + "/" + rel;
    if (FileExists(p.c_str())) return p;
#endif
    return std::string("resources/") + rel;
}

// ------------------------------------------------------------------ font
// Uses the raylib Font part of axFont (axDrawBot draws text with it). Includes Turkish letters.
static axFont *loadGuiFont(const std::string &path, int px)
{
    std::vector<int> cps;
    for (int c = 32; c < 127; c++) cps.push_back(c);
    for (int c = 0xA0; c <= 0x17F; c++) cps.push_back(c);   // Latin-1 + Latin Extended-A (ç ğ ı İ ö ş ü ...)
    for (int c : { 0x2022, 0x2026, 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x20AC, 0x2190, 0x2192, 0x2191, 0x2193, 0x2713 })
        cps.push_back(c);

    axFont *f = new axFont();
    if (FileExists(path.c_str())) f->font = LoadFontEx(path.c_str(), px, cps.data(), (int)cps.size());
    if (f->font.texture.id == 0) {
        TraceLog(LOG_WARNING, "DEMO: font yüklenemedi (%s), raylib varsayılan fontu kullanılıyor", path.c_str());
        f->font = GetFontDefault();
    } else {
        GenTextureMipmaps(&f->font.texture);
        SetTextureFilter(f->font.texture, TEXTURE_FILTER_TRILINEAR);
    }
    f->loaded = true;
    f->fontSize = px;
    return f;
}

// ------------------------------------------------------------------ theme
struct Theme {
    Color bg, grid;
    cColor idle, hover, btn, btnHover, btnSel, toggle, text, textHl, textSel, winBack, winBorder;
    cColor inBg, inBorder, inFocus, inText, inPlaceholder, inSel, caret;
    cColor tipBg, tipText;
    SyntaxColors syntax;
};

static Theme themeDark()
{
    Theme t;
    t.bg = { 24, 25, 28, 255 };          t.grid = { 255, 255, 255, 12 };
    t.idle = cColor(50, 52, 58);         t.hover = cColor(40, 70, 110);
    t.btn = cColor(48, 50, 56);          t.btnHover = cColor(66, 69, 77);
    t.btnSel = cColor(40, 70, 110);      t.toggle = cColor(66, 140, 230);
    t.text = cColor(228, 230, 234);      t.textHl = cColor(255, 255, 255);
    t.textSel = cColor(90, 170, 255);    t.winBack = cColor(34, 35, 39, 245);
    t.winBorder = cColor(64, 66, 72);
    t.inBg = cColor(26, 27, 30);         t.inBorder = cColor(72, 74, 80);
    t.inFocus = cColor(66, 165, 245);    t.inText = cColor(228, 230, 234);
    t.inPlaceholder = cColor(110, 114, 122); t.inSel = cColor(66, 165, 245, 80);
    t.caret = cColor(235, 236, 240);
    t.tipBg = cColor(14, 15, 18, 240);   t.tipText = cColor(235, 236, 240);
    // syntax: SyntaxColors defaults (dark)
    return t;
}

static Theme themeLight()
{
    Theme t;
    t.bg = { 238, 240, 244, 255 };       t.grid = { 0, 0, 0, 14 };
    t.idle = cColor(226, 229, 234);      t.hover = cColor(219, 236, 253);
    t.btn = cColor(232, 234, 238);       t.btnHover = cColor(214, 218, 225);
    t.btnSel = cColor(219, 236, 253);    t.toggle = cColor(33, 150, 243);
    t.text = cColor(38, 40, 44);         t.textHl = cColor(10, 12, 16);
    t.textSel = cColor(33, 120, 220);    t.winBack = cColor(250, 250, 251, 245);
    t.winBorder = cColor(210, 213, 218);
    t.inBg = cColor(255, 255, 255);      t.inBorder = cColor(200, 203, 208);
    t.inFocus = cColor(33, 150, 243);    t.inText = cColor(38, 40, 44);
    t.inPlaceholder = cColor(160, 164, 170); t.inSel = cColor(33, 150, 243, 70);
    t.caret = cColor(20, 20, 20);
    t.tipBg = cColor(40, 42, 48, 245);   t.tipText = cColor(245, 246, 248);
    // syntax (dark, saturated tones for a light background)
    t.syntax.keyword = cColor(0, 92, 197);       t.syntax.type = cColor(0, 128, 128);
    t.syntax.string = cColor(163, 21, 21);       t.syntax.comment = cColor(80, 128, 60);
    t.syntax.number = cColor(9, 134, 88);        t.syntax.function = cColor(121, 94, 38);
    t.syntax.preprocessor = cColor(130, 50, 180); t.syntax.operatorColor = cColor(60, 60, 60);
    t.syntax.normal = cColor(40, 40, 40);
    return t;
}

static void applyTheme(axGUI &gui, const Theme &t)
{
    gui.colors[GUI_IDLE_COLOR] = t.idle;
    gui.colors[GUI_HOVER_COLOR] = t.hover;
    gui.colors[GUI_BTN_COLOR] = t.btn;
    gui.colors[GUI_BTN_HOVER_COLOR] = t.btnHover;
    gui.colors[GUI_BTN_SEL_COLOR] = t.btnSel;
    gui.colors[GUI_BTN_TOGGLE_COLOR] = t.toggle;
    gui.colors[GUI_TEXT_COLOR] = t.text;
    gui.colors[GUI_TEXT_HL_COLOR] = t.textHl;
    gui.colors[GUI_TEXT_SEL_COLOR] = t.textSel;
    gui.colors[GUI_WIN_BACKCLR] = t.winBack;
    gui.colors[GUI_WIN_BORDERCLR] = t.winBorder;
    gui.colors[GUI_TOOLTIP_BG] = t.tipBg;
    gui.colors[GUI_TOOLTIP_TEXT] = t.tipText;
    syntaxColors = t.syntax;

    textcfg.bgColorSingle = t.inBg;
    textcfg.bgColorMulti = t.inBg;
    textcfg.borderColor = t.inBorder;
    textcfg.borderColorFocused = t.inFocus;
    textcfg.textColor = t.inText;
    textcfg.placeholderColor = t.inPlaceholder;
    textcfg.selectionColor = t.inSel;
    textcfg.cursorColor = t.caret;

    // tab bar (TabBarStyle default is a fixed light theme)
    TabBarStyle &ts = gui.tabStyle;
    ts.barBackground = t.winBack;  ts.barBackground.a = 255;
    ts.tabNormal = t.btn;          ts.tabHovered = t.btnHover;
    ts.tabActive = t.idle;         ts.tabTextNormal = t.text;  ts.tabTextNormal.a = 170;
    ts.tabTextActive = t.textHl;   ts.underlineColor = t.toggle;
    ts.separatorColor = t.winBorder;
}

static Color toColor(const cColor &c) { return Color{ (unsigned char)c.r, (unsigned char)c.g, (unsigned char)c.b, (unsigned char)c.a }; }

// ------------------------------------------------------------------ sample data (table)
struct Row { std::string name, city; int year; float score; };
static std::vector<Row> g_rows = {
    { "Ayşe Yılmaz", "İstanbul", 1988, 92.5f }, { "Mehmet Demir", "Ankara", 1975, 71.0f },
    { "Zeynep Kaya", "İzmir", 1993, 88.25f },   { "Ali Çelik", "Edirne", 2001, 64.5f },
    { "Elif Şahin", "Bursa", 1999, 97.0f },     { "Can Öztürk", "Antalya", 1984, 55.75f },
    { "Selin Aydın", "Eskişehir", 1990, 79.0f }, { "Burak Arslan", "Trabzon", 1979, 83.5f },
    { "Deniz Koç", "Çanakkale", 2003, 90.0f },  { "Gökhan Güneş", "Konya", 1968, 68.0f },
    { "İrem Polat", "Muğla", 1996, 74.25f },    { "Oğuz Kurt", "Kayseri", 1987, 86.0f },
    { "Pınar Erdem", "Samsun", 1981, 59.5f },   { "Umut Şen", "Gaziantep", 1994, 95.5f },
    { "Ece Yıldız", "Tekirdağ", 2000, 81.0f },  { "Tolga Aksoy", "Kırklareli", 1972, 77.75f },
};
// English equivalents (TR, EN): applyLanguage translates the names / cities in the rows when the language changes
static const char *const kRowNames[][2] = {
    { "Ayşe Yılmaz", "Emma Johnson" },   { "Mehmet Demir", "James Smith" },  { "Zeynep Kaya", "Olivia Brown" },
    { "Ali Çelik", "Liam Wilson" },      { "Elif Şahin", "Sophia Taylor" },  { "Can Öztürk", "Noah Davies" },
    { "Selin Aydın", "Ava Evans" },      { "Burak Arslan", "Ethan Thomas" }, { "Deniz Koç", "Mia Roberts" },
    { "Gökhan Güneş", "Henry Walker" },  { "İrem Polat", "Grace Wright" },   { "Oğuz Kurt", "Jack Green" },
    { "Pınar Erdem", "Chloe Hall" },     { "Umut Şen", "Oliver Clarke" },    { "Ece Yıldız", "Lily Turner" },
    { "Tolga Aksoy", "George Baker" },
};
static const char *const kRowCities[][2] = {
    { "İstanbul", "London" },   { "Ankara", "Manchester" },   { "İzmir", "Liverpool" },  { "Edirne", "Bristol" },
    { "Bursa", "Leeds" },       { "Antalya", "Brighton" },    { "Eskişehir", "Oxford" }, { "Trabzon", "Newcastle" },
    { "Çanakkale", "Cambridge" }, { "Konya", "Birmingham" },  { "Muğla", "Bath" },       { "Kayseri", "Sheffield" },
    { "Samsun", "Plymouth" },   { "Gaziantep", "Nottingham" }, { "Tekirdağ", "York" },   { "Kırklareli", "Exeter" },
};
static void translateRows()
{
    const int to = axLang == AX_LANG_TR ? 0 : 1;
    auto tr = [to](std::string &v, const char *const (*tbl)[2], size_t n) {
        for (size_t i = 0; i < n; i++) if (v == tbl[i][0] || v == tbl[i][1]) { v = tbl[i][to]; return; }
    };
    for (Row &r : g_rows) {
        tr(r.name, kRowNames, sizeof(kRowNames) / sizeof(kRowNames[0]));
        tr(r.city, kRowCities, sizeof(kRowCities) / sizeof(kRowCities[0]));
    }
}

// ------------------------------------------------------------------ demo durumu
struct Demo {
    // rich text
    RichTextState rich;
    TextBoxState richLink, richExport;
    int richView = 0;          // 0 edit, 1 Delta, 2 HTML
    int richEdits = 0;
    std::string richImagePath;

    bool dark = true;
    bool showGrid = true;
    bool animate = true;
    bool showStyle = false;            // F6: style editor (gui.showStyleWindow)
    bool showControls = true, showData = false, showRich = false;   // Data: F2
    bool guiDebug = false;

    // canvas
    int shapeCount = 7;
    float speed = 1.0f;
    float radius = 120.0f;
    float shapeSize = 28.0f;
    float thickness = 2.0f;
    cColor shapeColor = cColor(66, 165, 245);
    int shapeKind = 0;   // 0 circle, 1 star, 2 square
    double animTime = 0;
    vec2 center = vec2(0, 0);
    bool centerSet = false;

    // controls
    int clicks = 0;
    bool optA = true, optB = false, optC = true;
    int combo = 1;
    float sliderV = 0.35f;
    int counter = 42;

    // text
    TextBoxState name, notes, editor;

    // table
    TableState table;
    bool tblMulti = false, tblGrid = false;

    // catalog
    bool showCatalog = true;
    int listSel = 1;
    vec2 vec2Val = vec2(120, -40);
    float vec3Val[3] = { 0.25f, -1.5f, 3.0f };
    Param pOpacity{ "Opaklık", 0.8f, 0.0f, 1.0f, 0.01f };
    Param pSegments{ "Segment", 12, 3, 64 };
    cColor btnColor = cColor(240, 120, 60);
    cColor selColor = cColor(120, 200, 90);
    bool selOpen = false;
    vec2 selPos;
    float rgba[4] = { 66, 165, 245, 255 };
    bool tabOpen[2] = { true, true };   // Closable tabs in the Data window: Editor, Info
    bool showTopmost = false;
    int statsMode = 0;                  // 0 off, 1.. printGUIStats(mode-1)
    TableState taskTable;

    // popup
    PopupState popup{};
    vec2 popupPos;
    std::string status;
};

// item icons (IdStrPair::icon): generated from drawings in makeDrawnIcons
static const std::vector<IdStrPair> kShapeItemsEN = { { 0, "Circle", "", "shape_circle" }, { 1, "Star", "", "shape_star" }, { 2, "Square", "", "shape_square" } };
static const std::vector<IdStrPair> kShapeItemsTR = { { 0, "Daire", "", "shape_circle" }, { 1, "Yıldız", "", "shape_star" }, { 2, "Kare", "", "shape_square" } };
static const std::vector<IdStrPair> &shapeItems() { return axLang == AX_LANG_TR ? kShapeItemsTR : kShapeItemsEN; }

// ------------------------------------------------------------------ icons from drawings
// Shapes drawn with axDrawBot are rendered to a texture and registered as icons (bot.addIconFromDrawing).
// Drawn in white: drawIcon / imageButton tint them. Drawn at 64 px; mipmaps keep them smooth at small sizes.
static void makeDrawnIcons(axDrawBot &bot)
{
    const cColor W(255);
    bot.addIconFromDrawing("draw_star", 64, [W](axDrawBot &b, float s) {
        // concave: central pentagon + 5 tip triangles (aaFillConvex takes convex pieces)
        vec2 c(s * 0.5f, s * 0.53f);
        float R = s * 0.46f, r = s * 0.19f;
        vec2 outer[5], inner[5];
        for (int i = 0; i < 5; i++) {
            float a = -PI / 2 + i * 2 * PI / 5, a2 = a + PI / 5;
            outer[i] = c + vec2(cosf(a), sinf(a)) * R;
            inner[i] = c + vec2(cosf(a2), sinf(a2)) * r;
        }
        std::vector<vec2> pent(inner, inner + 5);
        b.aaFillConvex(pent, b.aaColor(W));
        for (int i = 0; i < 5; i++) b.fillTriangle(outer[i], inner[i], inner[(i + 4) % 5], W);
    });
    bot.addIconFromDrawing("draw_heart", 64, [W](axDrawBot &b, float s) {
        float r = s * 0.24f;
        b.circle(vec2(s * 0.30f, s * 0.36f), r, W);
        b.circle(vec2(s * 0.70f, s * 0.36f), r, W);
        std::vector<vec2> tri = { vec2(s * 0.08f, s * 0.43f), vec2(s * 0.92f, s * 0.43f), vec2(s * 0.5f, s * 0.90f) };
        b.aaFillConvex(tri, b.aaColor(W));
        std::vector<vec2> mid = { vec2(s * 0.30f, s * 0.20f), vec2(s * 0.70f, s * 0.20f), vec2(s * 0.70f, s * 0.50f), vec2(s * 0.30f, s * 0.50f) };
        b.aaFillConvex(mid, b.aaColor(W));
    });
    bot.addIconFromDrawing("draw_check", 64, [W](axDrawBot &b, float s) {
        float lw = s * 0.13f;
        vec2 p0(s * 0.18f, s * 0.52f), p1(s * 0.41f, s * 0.75f), p2(s * 0.84f, s * 0.26f);
        b.drawLine(p0, p1, W, lw);
        b.drawLine(p1, p2, W, lw);
        for (vec2 p : { p0, p1, p2 }) b.circle(p, lw * 0.5f, W);   // round caps and joins
    });
    // shape icons in the dropdown (Canvas > Shape)
    bot.addIconFromDrawing("shape_circle", 64, [W](axDrawBot &b, float s) { b.circle(vec2(s * 0.5f, s * 0.5f), s * 0.42f, W); });
    bot.addIconFromDrawing("shape_star", 64, [W](axDrawBot &b, float s) { b.star(vec2(s * 0.5f, s * 0.54f), s * 0.48f, 5, 0, W, W); });
    bot.addIconFromDrawing("shape_square", 64, [W](axDrawBot &b, float s) { b.roundedFRect(vec2(s * 0.1f, s * 0.1f), vec2(s * 0.8f, s * 0.8f), s * 0.16f, W); });
    bot.addIconFromDrawing("draw_ring", 64, [W](axDrawBot &b, float s) {
        b.circle(vec2(s * 0.5f, s * 0.5f), s * 0.38f, cTransparent, s * 0.09f, W);
        b.circle(vec2(s * 0.5f, s * 0.5f), s * 0.14f, W);
    });
}

// ------------------------------------------------------------------ canvas
static void drawCanvas(axDrawBot &bot, Demo &d, const Theme &th, float w, float h)
{
    if (d.showGrid) {
        for (float x = 0; x < w; x += 32) DrawLineV({ x, 0 }, { x, h }, th.grid);
        for (float y = 0; y < h; y += 32) DrawLineV({ 0, y }, { w, y }, th.grid);
    }
    if (!d.centerSet) { d.center = vec2(w * 0.5f, h * 0.55f); d.centerSet = true; }

    vec2 c = d.center;
    cColor ring = d.shapeColor; ring.a = 70;
    bot.circle(c, d.radius, cColor(0, 0, 0, 0), 1.0f, ring);
    bot.circle(c, 6, d.shapeColor);

    int n = std::max(1, d.shapeCount);
    for (int i = 0; i < n; i++) {
        float a = (float)(d.animTime * d.speed) + i * 2.0f * PI / n;
        vec2 p = c + vec2(cosf(a), sinf(a)) * d.radius;
        cColor fc = d.shapeColor;
        fc.a = (unsigned char)(120 + 135 * i / std::max(1, n - 1));
        cColor sc = d.dark ? cColor(255, 255, 255, 160) : cColor(0, 0, 0, 120);
        float s = d.shapeSize;
        switch (d.shapeKind) {
            case 1: bot.star(p, s, 5, d.thickness, fc, sc); break;
            case 2: bot.roundedFRect(p - vec2(s, s) * 0.5f, vec2(s, s), s * 0.2f, fc, d.thickness, sc); break;
            default: bot.circle(p, s * 0.5f, fc, d.thickness, sc); break;
        }
        bot.drawLine(c, p, ring, 1.0f);
    }
}

// ------------------------------------------------------------------ layout
// Main window positions / sizes are derived from the screen size, not fixed numbers.
// On web the canvas starts at the browser window size, so fixed values would push Catalog off the right edge and break the heights.
//   very wide (>= 1600): Controls | Rich text | Data (Style right-aligned below) | Catalog; all open
//   wide      (>= 1388): Controls | Rich text / Data | Catalog
//   medium    (>=  832): Controls | Rich text / Data / Catalog (same column, stacked)
//   narrow    (<   832): single column, full width
static vec2 gCtrlPos(16, 64),  gCtrlSize(360, 700);
static vec2 gDataPos(392, 64), gDataSize(600, 470);
static vec2 gCatPos(1004, 64), gCatSize(420, 780);
static vec2 gRichPos(392, 64), gRichSize(600, 770);
static vec2 gTopPos(560, 560), gTopSize(280, 110);

enum DemoLayout { LAYOUT_XL, LAYOUT_WIDE, LAYOUT_MEDIUM, LAYOUT_NARROW };

static void placeWin(axGUI &gui, const char *id, vec2 &pos, vec2 &size, vec2 p, vec2 s)
{
    pos = p; size = s;
    auto it = gui.windowStates.find(id);   // also the state of an already opened window (otherwise pos/size are used on first open)
    if (it != gui.windowStates.end() && it->second.init) it->second.setGeometry(p, s);
}

static DemoLayout layoutWindows(axGUI &gui, float W, float H)
{
    if (W >= 1600) {
        // wide layout: proportions tuned at 2000 px (Controls ~14%, Catalog ~16%, Rich text 52% in the middle)
        const float m = 12, g = 8, top = 56;
        const float h = std::max(240.0f, H - top - m);
        const float cw = std::clamp(W * 0.14f, 345.0f, 380.0f);
        const float kw = std::clamp(W * 0.16f, 400.0f, 440.0f);
        const float x1 = m + cw + g, x2 = W - m - kw - g, mid = x2 - x1;
        const float rw = std::round(mid * 0.52f), dw = mid - rw - g;
        const float dh = std::round(h * 0.40f), rh = std::round(h * 0.67f);
        const float sw = std::max(360.0f, std::round(dw * 0.62f));
        placeWin(gui, "controls", gCtrlPos, gCtrlSize, vec2(m, top), vec2(cw, h));
        placeWin(gui, "rich", gRichPos, gRichSize, vec2(x1, top), vec2(rw, rh));
        placeWin(gui, "data", gDataPos, gDataSize, vec2(x1 + rw + g, top), vec2(dw, dh));
        placeWin(gui, "axgui_style", gui.styleWinPos, gui.styleWinSize, vec2(x2 - sw, top + dh + g), vec2(sw, h - dh - g));
        placeWin(gui, "catalog", gCatPos, gCatSize, vec2(W - m - kw, top), vec2(kw, h));
        placeWin(gui, "topmost", gTopPos, gTopSize, vec2(x1 + rw * 0.5f - 140, top + rh + 2 * g), vec2(280, 110));
        return LAYOUT_XL;
    }
    const float m = 16, gap = 16, top = 64;
    const float h = std::max(240.0f, H - top - m);
    const float cw = 360, kw = 420, centerMin = 520;
    DemoLayout L = W >= m + cw + gap + centerMin + gap + kw + m ? LAYOUT_WIDE
                 : W >= m + cw + gap + 440 + m                   ? LAYOUT_MEDIUM : LAYOUT_NARROW;
    vec2 cPos, cSize, mPos, mSize, kPos, kSize;
    if (L == LAYOUT_NARROW) {
        const float w = std::max(200.0f, W - 2 * m);
        cPos = mPos = kPos = vec2(m, top);
        cSize = mSize = kSize = vec2(w, h);
    } else {
        cPos = vec2(m, top); cSize = vec2(cw, h);
        mPos = vec2(m + cw + gap, top);
        if (L == LAYOUT_WIDE) {
            mSize = vec2(W - (m + cw + gap) - gap - kw - m, h);
            kPos = vec2(W - m - kw, top); kSize = vec2(kw, h);
        } else {
            mSize = vec2(W - (m + cw + gap) - m, h);
            kPos = mPos; kSize = mSize;
        }
    }
    placeWin(gui, "controls", gCtrlPos, gCtrlSize, cPos, cSize);
    placeWin(gui, "rich", gRichPos, gRichSize, mPos, mSize);
    placeWin(gui, "data", gDataPos, gDataSize, mPos, vec2(mSize.x, std::min(mSize.y, 520.0f)));
    placeWin(gui, "catalog", gCatPos, gCatSize, kPos, kSize);
    const vec2 ts(std::min(280.0f, W - 2 * m), 110);
    placeWin(gui, "topmost", gTopPos, gTopSize, vec2((W - ts.x) * 0.5f, std::max(top, H - ts.y - 3 * m)), ts);
    const vec2 ss(std::min(500.0f, W - 2 * m), std::min(640.0f, h));
    placeWin(gui, "axgui_style", gui.styleWinPos, gui.styleWinSize, vec2(W - m - ss.x, top), ss);
    return L;
}

// ------------------------------------------------------------------ windows
static void controlsWindow(axGUI &gui, Demo &d)
{
    vec2 &pos = gCtrlPos, &size = gCtrlSize;
    gui.beginWindow(axTr("Controls###controls", "Kontroller###controls"), pos, size, WIN_TITLE_BAR | WIN_SCROLLBAR | WIN_RESIZABLE | WIN_AUTO_NEWLINE);
    const float rowH = 28;
    const float fullW = gui.getContainerWidth();

    if (gui.beginSection(axTr("Buttons###s_buttons", "Düğmeler###s_buttons"))) {
        if (gui.button(axTr("Click", "Tıkla"), vec2(0, rowH), 0, axTr("Increments the counter", "Sayaç artar"))) { d.clicks++; d.status = axTr("Button clicked", "Düğmeye tıklandı"); }
        gui.label(TextFormat(axTr("clicks: %d", "tıklama: %d"), d.clicks), vec2(0, rowH));
        gui.newline();
        gui.toggleButton(axTr("Animation", "Animasyon"), &d.animate, vec2(0, rowH), axTr("Canvas animation", "Tuval animasyonu"));
        gui.toggleButton(axTr("Grid", "Izgara"), &d.showGrid, vec2(0, rowH));
        gui.toggleButton(axTr("Dark theme", "Koyu tema"), &d.dark, vec2(0, rowH));
        gui.newline();
        gui.checkbox(axTr("Option A", "Seçenek A"), &d.optA, vec2(108, 22));
        gui.checkbox(axTr("Option B", "Seçenek B"), &d.optB, vec2(108, 22));
        gui.checkbox(axTr("Option C", "Seçenek C"), &d.optC, vec2(108, 22));
        gui.newline();
        gui.endSection();
    }

    if (gui.beginSection(axTr("Canvas###s_canvas", "Tuval###s_canvas"))) {
        gui.label(axTr("Shape", "Şekil"), vec2(90, rowH));
        gui.comboBox(vec2(140, rowH), shapeItems(), d.shapeKind, axTr("Shape type on the canvas", "Tuvaldeki şekil türü"));
        gui.newline();
        gui.label(axTr("Count", "Adet"), vec2(90, rowH));
        gui.dragInt(vec2(140, rowH), 1, 64, &d.shapeCount, 0.2f, true, axTr("Drag, or double-click to type", "Sürükle ya da çift tıklayıp yaz"));
        gui.newline();
        gui.label(axTr("Speed", "Hız"), vec2(90, rowH));
        gui.dragFloat(vec2(140, rowH), -5, 5, &d.speed, 1.0f, 0.01f, "%.2f", true);
        gui.newline();
        gui.label(axTr("Radius", "Yarıçap"), vec2(90, rowH));
        gui.dragFloat(vec2(140, rowH), 20, 400, &d.radius, 120, 1.0f, "%.0f");
        gui.newline();
        gui.label(axTr("Size", "Boyut"), vec2(90, rowH));
        gui.dragFloat(vec2(140, rowH), 4, 96, &d.shapeSize, 28, 0.25f, "%.1f");
        gui.newline();
        gui.label(axTr("Line", "Çizgi"), vec2(90, rowH));
        gui.slider(vec2(0, rowH * 0.5f), vec2(140, rowH), 0.0f, 8.0f, &d.thickness);   // writes the value itself
        gui.newline();
        gui.label(axTr("Color", "Renk"), vec2(90, rowH));
        gui.dragColor(vec2(std::max(140.0f, fullW - 100), rowH), d.shapeColor, 1.0f, "%.0f");
        gui.newline();
        if (gui.button(axTr("Center", "Ortala"), vec2(0, rowH))) { d.centerSet = false; d.status = axTr("Canvas centered", "Tuval ortalandı"); }
        if (gui.button(axTr("Reset", "Sıfırla"), vec2(0, rowH))) {
            d.shapeCount = 7; d.speed = 1; d.radius = 120; d.shapeSize = 28; d.thickness = 2;
            d.shapeColor = cColor(66, 165, 245); d.shapeKind = 0; d.status = axTr("Canvas settings reset", "Tuval ayarları sıfırlandı");
        }
        gui.newline();
        gui.endSection();
    }

    if (gui.beginSection(axTr("Text###s_text", "Metin###s_text"))) {
        gui.label(axTr("Name", "Ad"), vec2(60, rowH));
        if (gui.textBox(d.name, vec2(std::max(160.0f, fullW - 70), rowH)))
            d.status = "Enter: " + (d.name.text.empty() ? std::string(axTr("(empty)", "(boş)")) : d.name.text);
        gui.newline();
        gui.textBox(d.notes, vec2(fullW, 110));
        gui.newline();
        gui.label(TextFormat(axTr("%d characters  |  focus: %s", "%d karakter  |  odak: %s"), (int)d.notes.text.size(), d.notes.focused ? axTr("yes", "evet") : axTr("no", "hayır")), vec2(0, 22));
        gui.newline();
        gui.endSection();
    }

    if (gui.beginSection(axTr("Other###s_other", "Diğer###s_other"))) {
        gui.label(axTr("Counter", "Sayaç"), vec2(90, rowH));
        gui.dragInt(vec2(140, rowH), -1000, 1000, &d.counter, 1.0f, true);
        gui.newline();
        gui.label(axTr("Ratio", "Oran"), vec2(90, rowH));
        gui.slider(vec2(0, rowH * 0.5f), vec2(140, rowH), 0.0f, 1.0f, &d.sliderV);
        gui.newline();
        gui.toggleButton(axTr("GUI debug", "GUI hata ayıklama"), &d.guiDebug, vec2(0, rowH), axTr("axGUI::debug: layout boxes", "axGUI::debug: yerleşim kutuları"));
        gui.toggleButton(axTr("Smooth text", "Akıcı yazı"), &gui.smoothText, vec2(0, rowH),
                         axTr("gui.smoothText: text always at sub-pixel positions (otherwise only while animating)",
                              "gui.smoothText: yazı hep kesirli konumda (kapalıyken yalnızca animasyon sırasında)"));
        gui.toggleButton(axTr("Window snap", "Pencere yapışma"), &gui.winSnap, vec2(0, rowH),
                         axTr("gui.winSnap: windows snap to each other and to screen edges while moving / resizing",
                              "gui.winSnap: pencereler taşırken / boyutlandırırken birbirine ve ekran kenarına yapışır"));
        gui.newline();
        gui.endSection();
    }

    gui.endWindow("###controls");
}

static void dataWindow(axGUI &gui, Demo &d)
{
    vec2 &pos = gDataPos, &size = gDataSize;
    gui.beginWindow(axTr("Data###data", "Veri###data"), pos, size, WIN_TITLE_BAR | WIN_RESIZABLE | WIN_AUTO_NEWLINE);

    gui.beginTabBar("veri_sekmeleri");

    if (gui.beginTabItem(axTr("Table###t_table", "Tablo###t_table"), nullptr)) {
        gui.toggleButton(axTr("Multi-select", "Çoklu seçim"), &d.tblMulti, vec2(0, 26));
        gui.toggleButton(axTr("Grid", "Izgara"), &d.tblGrid, vec2(0, 26));
        if (gui.button(axTr("Add row", "Satır ekle"), vec2(0, 26))) {
            static int k = 1;
            g_rows.push_back({ TextFormat(axTr("New person %d", "Yeni kişi %d"), k++), "Kipi", 1970 + GetRandomValue(0, 35), (float)GetRandomValue(400, 1000) / 10.0f });
            d.table.invalidate();
        }
        gui.newline();

        const std::vector<TableColumn> cols = {   // per language, every frame (small)
            TableColumn(axTr("Name", "Ad"), 0, BOT_ALIGN_LEFT, 80),
            TableColumn(axTr("City", "Şehir"), 120, BOT_ALIGN_LEFT, 60),
            TableColumn(axTr("Year", "Yıl"), 70, BOT_ALIGN_RIGHT, 50),
            TableColumn(axTr("Score", "Puan"), 80, BOT_ALIGN_RIGHT, 50),
        };
        int flags = TBL_DEFAULT | (d.tblMulti ? TBL_MULTISELECT : 0) | (d.tblGrid ? TBL_GRID : 0);
        float th = std::max(120.0f, gui.layoutArea.bottom() - gui.cursorPos().y - 34);
        bool ev = gui.table(d.table, vec2(0, th), cols, (int)g_rows.size(),
            [](int r, int c) -> std::string {
                const Row &row = g_rows[(size_t)r];
                switch (c) {
                    case 0: return row.name;
                    case 1: return row.city;
                    case 2: return std::to_string(row.year);
                    default: return TextFormat("%.2f", row.score);
                }
            }, flags,
            // TableDrawFn: draws a bar in the Score column; returning false also draws the default text
            [&gui](int r, int c, const rect &cr, bool) -> bool {
                if (c != 3) return false;
                float t = Clamp(g_rows[(size_t)r].score / 100.0f, 0.0f, 1.0f);
                cColor bar = gui.colors[GUI_BTN_TOGGLE_COLOR]; bar.a = 60;
                gui.bot->roundedFRect(cr.pos + vec2(4, cr.size.y * 0.2f), vec2((cr.size.x - 8) * t, cr.size.y * 0.6f), 3, bar);
                return false;
            });
        if (ev) {
            if (d.table.activatedRow >= 0) d.status = axTr("Opened: ", "Açıldı: ") + g_rows[(size_t)d.table.activatedRow].name;
            else if (d.table.sortChanged) d.status = d.table.sortCol < 0 ? axTr("Sorting removed", "Sıralama kaldırıldı") : TextFormat(axTr("Sort: column %d %s", "Sıralama: sütun %d %s"), d.table.sortCol, d.table.sortAsc ? axTr("ascending", "artan") : axTr("descending", "azalan"));
            else if (d.table.selectionChanged) d.status = TextFormat(axTr("Selected rows: %d", "Seçili satır: %d"), (int)d.table.selected.size());
        }
        gui.newline();
        gui.label(TextFormat(axTr("%d rows  |  double-click / Enter: open  |  click header: sort", "%d satır  |  çift tık / Enter: aç  |  başlığa tık: sırala"), (int)g_rows.size()), vec2(0, 22));
        gui.endTabItem();
    }

    // closable tabs: passing pOpen adds an × to the tab; once closed, the caller stops drawing it
    if (d.tabOpen[0] && gui.beginTabItem(axTr("Editor###t_editor", "Editör###t_editor"), &d.tabOpen[0])) {
        float w = gui.getContainerWidth();
        float h = std::max(120.0f, gui.layoutArea.bottom() - gui.cursorPos().y - 8);
        gui.textBox(d.editor, vec2(w, h));
        gui.endTabItem();
    }

    if (d.tabOpen[1] && gui.beginTabItem(axTr("Info###t_info", "Bilgi###t_info"), &d.tabOpen[1])) {
        auto line = [&](const char *t) { gui.label(t, vec2(0, 22)); gui.newline(); };
        line(TextFormat("raylib %s  |  %d FPS  |  %.2f ms", RAYLIB_VERSION, GetFPS(), GetFrameTime() * 1000.0f));
        line(TextFormat(axTr("Screen: %d x %d (render %d x %d)", "Ekran: %d x %d (render %d x %d)"), GetScreenWidth(), GetScreenHeight(), GetRenderWidth(), GetRenderHeight()));
        Vector2 dpi = GetWindowScaleDPI();
        line(TextFormat(axTr("DPI scale: %.2f x %.2f", "DPI ölçeği: %.2f x %.2f"), dpi.x, dpi.y));
        line(TextFormat(axTr("Mouse: %.0f, %.0f  |  GUI captures mouse: %s", "Fare: %.0f, %.0f  |  GUI fareyi tutuyor: %s"), gui.ms.x, gui.ms.y, gui.wantCaptureMouse ? axTr("yes", "evet") : axTr("no", "hayır")));
        line(TextFormat("hoverID %d  focusID %d  hoverWinID %d", gui.hoverID, gui.focusID, gui.hoverWinID));
        line(TextFormat(axTr("Window states: %d  |  item states: %d", "Pencere durumu: %d  |  öğe durumu: %d"), (int)gui.windowStates.size(), (int)gui.itemStates.size()));
        gui.newline();
        line(axTr("Shortcuts: F1 controls, F2 data, F3 theme, F4 catalog, F5 rich text, F6 style, F7 language, F11 fullscreen", "Kısayollar: F1 kontroller, F2 veri, F3 tema, F4 katalog, F5 zengin metin, F6 stil, F7 dil, F11 tam ekran"));
        gui.endTabItem();
    }

    gui.endTabBar();
    gui.endWindow("###data");
}

static void catalogWindow(axGUI &gui, Demo &d)
{
    vec2 &pos = gCatPos, &size = gCatSize;
    gui.beginWindow(axTr("Catalog###catalog", "Katalog###catalog"), pos, size, WIN_TITLE_BAR | WIN_SCROLLBAR | WIN_RESIZABLE | WIN_AUTO_NEWLINE);
    const float rowH = 26;
    const float W = gui.getContainerWidth();
    const cColor accent = gui.colors[GUI_BTN_TOGGLE_COLOR];

    // --- layout helpers
    if (gui.beginSection(axTr("Layout###s_layout", "Yerleşim###s_layout"))) {
        gui.button("A", vec2(40, rowH));
        gui.sameline();
        gui.button(axTr("B with sameline()", "sameline() ile B"), vec2(0, rowH));
        gui.spacerH(24);
        gui.button("spacerH(24)", vec2(0, rowH));
        gui.newline();
        gui.spacerV(8);
        gui.text(axTr("text(): free text, ", "text(): serbest metin, "), gui.fontSize, gui.colors[GUI_TEXT_COLOR]);
        gui.text(axTr("colored", "renkli"), gui.fontSize, accent);
        gui.newline();
        gui.separator(16, axTr("separator(titled)", "separator(başlıklı)"));
        ItemState &dm = gui.dummy(vec2(W, 36), axTr("dummy: clickable empty area", "dummy: tıklanabilir boş alan"));
        gui.newline();
        gui.label(dm.focused ? axTr("dummy focused (clicked)", "dummy odakta (tıklandı)") : axTr("dummy: click the area above", "dummy: üstteki alana tıkla"), vec2(0, 22));
        gui.newline();
        gui.endSection();
    }

    // --- selectable
    if (gui.beginSection(axTr("Selectable list###s_selectable_list", "Seçilebilir liste###s_selectable_list"))) {
        const char *items[] = { axTr("Inbox", "Gelen kutusu"), axTr("Drafts", "Taslaklar"), axTr("Sent", "Gönderilenler"), axTr("Archive", "Arşiv"), axTr("Trash", "Çöp kutusu") };
        for (int i = 0; i < 5; i++)
            if (gui.selectable(items[i], d.listSel == i, true)) { d.listSel = i; d.status = std::string("selectable: ") + items[i]; }
        gui.newline();
        gui.endSection();
    }

    // --- group box (collapsible, vertical scrolling)
    if (gui.beginSection(axTr("Group box###s_group_box", "Grup kutusu###s_group_box"))) {
        GroupStyle gs{ 6, false, gui.colors[GUI_IDLE_COLOR], 1, gui.colors[GUI_WIN_BORDERCLR] };
        if (gui.beginGroupBox(axTr("Scrollable group###g_scroll", "Kaydırılabilir grup###g_scroll"), vec2(W, 170), true, 2, gs, axTr("Click the header: collapse / expand", "Başlığa tıkla: katla / aç"))) {
            for (int i = 1; i <= 14; i++) {
                gui.label(TextFormat(axTr("Row %d", "Satır %d"), i), vec2(80, 22));
                if (gui.button(TextFormat(axTr("Button %d", "Düğme %d"), i), vec2(0, 22))) d.status = TextFormat(axTr("Group: button %d", "Grup: düğme %d"), i);
                gui.newline();
            }
        }
        gui.endGroupBox();   // called whether open or closed
        gui.newline();
        gui.endSection();
    }

    // --- shape and color buttons, color pickers
    if (gui.beginSection(axTr("Shapes and colors###s_shapes_and_colors", "Şekil ve renk###s_shapes_and_colors"))) {
        static const char *shapes[] = { "circle", "gradY+", "gradY-", "gradX+", "gradX-" };
        for (const char *sh : shapes)
            if (gui.shapeButton(vec2(34, 34), sh, vec2(26, 26), d.shapeColor, 1, cColor(120), sh))
                d.status = std::string("shapeButton: ") + sh;
        gui.newline();

        gui.colorButton(vec2(34, 34), vec2(26, 26), &d.btnColor, 1, cColor(120), axTr("colorButton: opens the built-in color picker", "colorButton: yerleşik renk seçiciyi açar"));
        gui.label(axTr("colorButton (opens drawColorPicker)", "colorButton (drawColorPicker açar)"), vec2(0, 34));
        gui.newline();

        if (gui.button("colorSelector…", vec2(0, rowH))) {
            d.selOpen = true;
            // right below the button: layout position + window scroll = screen position (independent of where the button was clicked).
            // Fitting to the screen is handled in the library (WIN_POPUP).
            d.selPos = gui.lastItemPos() + gui.winScroll + vec2(0, rowH + 4);
            auto it = gui.windowStates.find("colorSelector");   // if already open, move it there (operator[] would create an empty state)
            if (it != gui.windowStates.end()) it->second.pos = d.selPos;
        }
        gui.shapeButton(vec2(rowH, rowH), "circle", vec2(rowH - 4, rowH - 4), d.selColor, 1, cColor(120), axTr("selected color", "seçilen renk"));
        gui.newline();

        static const char *ch[] = { "R", "G", "B", "A" };
        static const cColor chc[] = { cColor(230, 60, 60), cColor(60, 200, 90), cColor(60, 120, 240), cColor(180, 180, 180) };
        for (int i = 0; i < 4; i++) {
            gui.label(ch[i], vec2(18, 20));
            gui.label(TextFormat("%3.0f", d.rgba[i]), vec2(36, 20));
            gui.drawColorSlider(vec2(0, 4), vec2(W - 90, 12), &d.rgba[i], 255, chc[i]);
            gui.newline();
        }
        cColor mix((int)d.rgba[0], (int)d.rgba[1], (int)d.rgba[2], (int)d.rgba[3]);
        const vec2 mp = gui.advanceCursor(vec2(W, 26));   // place first, then draw at the returned position
        gui.bot->roundedFRect(mp, vec2(W, 26), 5, mix, 1, gui.colors[GUI_WIN_BORDERCLR]);
        gui.newline();
        gui.endSection();
    }

    // --- vector and Param sliders
    // --- icons from drawings: icons created with bot.addIconFromDrawing, used with imageButton
    if (gui.beginSection(axTr("Drawn icons###s_drawn_icons", "Çizimden simge###s_drawn_icons"))) {
        static const char *names[] = { "draw_star", "draw_heart", "draw_check", "draw_ring" };
        const float sizes[] = { 16, 24, 32 };
        for (float sz : sizes) {
            for (const char *n : names)
                if (gui.imageButton(n, vec2(sz + 8, sz + 8), sz == 32 ? accent : gui.colors[GUI_TEXT_COLOR], n))
                    d.status = std::string("imageButton: ") + n;
            gui.newline();
        }
        gui.label(axTr("64 px drawing, shown at 16 / 24 / 32", "64 px çizim, 16 / 24 / 32 gösterim"), vec2(0, 22));
        gui.newline();
        gui.endSection();
    }

    if (gui.beginSection(axTr("Vectors and Param###s_vectors_and_param", "Vektör ve Param###s_vectors_and_param"))) {
        gui.label("dragVec2", vec2(90, rowH));
        gui.dragVec2(axTr("position", "konum"), 0, 0, vec2(110, rowH), -1000, 1000, &d.vec2Val, vec2(0, 0), 1.0f, "%.0f", false, axTr("position", "konum"));
        gui.newline();
        gui.label("dragFloat3", vec2(90, rowH));
        gui.dragFloat3(vec2(W - 100, rowH), -10, 10, d.vec3Val, 0.01f, "%.2f");
        gui.newline();
        gui.label("Param float", vec2(90, rowH));
        gui.dragFloat(vec2(140, rowH), d.pOpacity, 0.01f, "%.2f", true, axTr("Param: min/max/default/speed from the Param", "Param: min/max/varsayılan/hız Param'dan"));
        gui.label(TextFormat("= %.2f", d.pOpacity.value.getFloat()), vec2(0, rowH));
        gui.newline();
        gui.label("Param int", vec2(90, rowH));
        gui.dragInt(vec2(140, rowH), d.pSegments, 0.2f, true);
        gui.label(TextFormat("= %d", d.pSegments.getInt()), vec2(0, rowH));
        gui.newline();
        gui.endSection();
    }

    // --- window flags
    if (gui.beginSection(axTr("Windows###s_windows", "Pencereler###s_windows"))) {
        gui.toggleButton(axTr("WIN_TOPMOST window", "WIN_TOPMOST penceresi"), &d.showTopmost, vec2(0, rowH), axTr("Window that stays on top", "Hep üstte kalan pencere"));
        if (gui.button(axTr("Reopen tabs", "Sekmeleri geri aç"), vec2(0, rowH), 0, axTr("Tabs closed with × in the Data window", "Veri penceresinde × ile kapatılan sekmeler")))
            d.tabOpen[0] = d.tabOpen[1] = true;
        gui.newline();
        gui.label(axTr("WIN_POPUP: canvas right-click menu and colorSelector", "WIN_POPUP: tuvalde sağ tık menüsü ve colorSelector"), vec2(0, 22));
        gui.newline();
        // window inside a window: position is relative to the parent window's cursor
        vec2 cp(0, 0), cs(W, 92);
        gui.beginWindow(axTr("Inner window###inner", "İç pencere###inner"), cp, cs, WIN_TITLE_BAR | WIN_AUTO_NEWLINE);
        gui.label(axTr("Window inside a window", "Pencere içinde pencere"), vec2(0, 24));
        if (gui.button(axTr("Click", "Tıkla"), vec2(0, 24))) d.status = axTr("Inner window button", "İç pencere düğmesi");
        gui.endWindow("###inner");
        gui.newline();
        gui.endSection();
    }

    // --- table: custom cell drawing, no selection, grid
    if (gui.beginSection(axTr("Table (custom drawing)###s_table_custom_drawing", "Tablo (özel çizim)###s_table_custom_drawing"))) {
        struct Task { const char *en, *tr; float progress; };
        static const Task tasks[] = { { "Design", "Tasarım", 1.0f }, { "Coding", "Kodlama", 0.7f }, { "Testing", "Test", 0.35f },
                                      { "Docs", "Belgeler", 0.1f }, { "Release", "Yayın", 0.0f } };
        const std::vector<TableColumn> cols = { TableColumn(axTr("Task", "Görev"), 120), TableColumn(axTr("Progress", "İlerleme"), 0, BOT_ALIGN_LEFT, 80, false) };
        gui.table(d.taskTable, vec2(W, 0), cols, 5,
            [](int r, int c) -> std::string { return c == 0 ? axTr(tasks[r].en, tasks[r].tr) : TextFormat("%%%d", (int)(tasks[r].progress * 100)); },
            TBL_HEADER | TBL_NOSELECT | TBL_GRID,
            [&gui, accent](int r, int c, const rect &cr, bool) -> bool {
                if (c != 1) return false;
                vec2 p = cr.pos + vec2(6, cr.size.y * 0.3f), s(cr.size.x - 60, cr.size.y * 0.4f);
                gui.bot->roundedFRect(p, s, s.y / 2, gui.colors[GUI_IDLE_COLOR]);
                if (tasks[r].progress > 0) gui.bot->roundedFRect(p, vec2(s.x * tasks[r].progress, s.y), s.y / 2, accent);
                gui.bot->text(TextFormat("%%%d", (int)(tasks[r].progress * 100)), p + vec2(s.x + 8, s.y / 2), gui.fontSize * 0.9f,
                              gui.colors[GUI_TEXT_COLOR], BOT_ALIGN_LEFT | BOT_ALIGN_MIDDLE);
                return true;   // don't draw the default text
            });
        gui.newline();
        gui.endSection();
    }

    // --- state dump
    if (gui.beginSection(axTr("Debug###s_debug", "Hata ayıklama###s_debug"))) {
        const std::vector<IdStrPair> modes = { { 0, axTr("Off", "Kapalı") }, { 1, axTr("Windows", "Pencereler") }, { 2, axTr("Items", "Öğeler") }, { 3, axTr("Combo boxes", "Açılır listeler") }, { 4, axTr("Animations", "Animasyonlar") } };
        gui.label("printGUIStats", vec2(110, rowH));
        gui.comboBox(vec2(160, rowH), modes, d.statsMode);
        gui.newline();
        if (d.statsMode > 0) gui.printGUIStats(d.statsMode - 1);
        gui.newline();
        gui.endSection();
    }

    gui.endWindow("###catalog");
}

static void topmostWindow(axGUI &gui, Demo &d)
{
    if (!d.showTopmost) return;
    vec2 &pos = gTopPos, &size = gTopSize;
    gui.beginWindow(axTr("Topmost###topmost", "Üstte kalan###topmost"), pos, size, WIN_TITLE_BAR | WIN_TOPMOST | WIN_AUTO_NEWLINE);
    gui.label(axTr("WIN_TOPMOST: above other windows", "WIN_TOPMOST: diğer pencerelerin üstünde"), vec2(0, 24));
    gui.newline();
    if (gui.button(axTr("Close", "Kapat"), vec2(0, 26))) d.showTopmost = false;
    gui.endWindow("###topmost");
}

// ------------------------------------------------------------------ rich text window
// The format bar lives in the demo itself: the library only provides the richTextBox + RichEditor API.
static bool tbBtn(axGUI &gui, const char *label, bool active, const char *hint = nullptr, float w = 0)
{
    cColor save = gui.colors[GUI_BTN_COLOR];
    if (active) gui.colors[GUI_BTN_COLOR] = gui.colors[GUI_BTN_TOGGLE_COLOR];
    bool r = gui.button(label, vec2(w, 26), 0, hint);
    gui.colors[GUI_BTN_COLOR] = save;
    return r;
}

static std::string richSampleDelta(const std::string &imagePath)
{
    RtJson ops = RtJson::array();
    auto put = [&](const std::string &text, RtJson attrs = RtJson()) {
        RtJson op = RtJson::object();
        op["insert"] = text;
        if (attrs.is_object() && !attrs.empty()) op["attributes"] = attrs;
        ops.push_back(op);
    };
    auto line = [&](RtJson block = RtJson()) { put("\n", block); };
    put(axTr("axGUI rich text", "axGUI zengin metin")); line({ { "header", 1 } });
    put(axTr("The document is stored as ", "Belge ")); put("Quill Delta", { { "bold", true } }); put(axTr(" (same as NotePen). ", " biçiminde saklanır (NotePen ile aynı). "));
    put(axTr("Bold", "Kalın"), { { "bold", true } }); put(", "); put(axTr("italic", "italik"), { { "italic", true } }); put(", ");
    put(axTr("underlined", "altı çizili"), { { "underline", true } }); put(", "); put(axTr("strikethrough", "üstü çizili"), { { "strike", true } }); put(", ");
    put(axTr("colored", "renkli"), { { "color", "#ffe53935" } }); put(", "); put(axTr("highlighted", "vurgulu"), { { "background", "#fffff176" } }); put(", ");
    put(axTr("inline code", "satır içi kod"), { { "code", true } }); put(axTr(" and ", " ve ")); put(axTr("large text", "büyük yazı"), { { "size", "24" } }); put("."); line();
    put(axTr("Links: ", "Bağlantılar: ")); put("raylib.com", { { "link", "https://www.raylib.com" } });
    put(axTr(" (Ctrl+click opens) and an in-document link ", " (Ctrl+tık açar) ve belge içi ")); put(axTr("to the Lists heading", "Listeler başlığına"), { { "link", axTr("#lists", "#listeler") } }); put("."); line();
    put(axTr("Lists", "Listeler")); line({ { "header", 2 } });
    put(axTr("Bulleted list", "Madde listesi")); line({ { "list", "bullet" } });
    put(axTr("Nested item (Tab / Shift+Tab)", "İç içe madde (Tab / Shift+Tab)")); line({ { "list", "bullet" }, { "indent", 1 } });
    put(axTr("Numbered list", "Numaralı liste")); line({ { "list", "ordered" } });
    put(axTr("Second item", "İkinci madde")); line({ { "list", "ordered" } });
    put(axTr("To do: click the box", "Yapılacak: kutuya tıkla")); line({ { "list", "unchecked" } });
    put(axTr("Done", "Bitti")); line({ { "list", "checked" } });
    put(axTr("Blocks", "Bloklar")); line({ { "header", 2 } });
    put(axTr("Quote block: click the arrow left of a heading to collapse its section.", "Alıntı bloğu: başlıkların solundaki oka tıklayınca bölüm katlanır.")); line({ { "blockquote", true } });
    put("for (int i = 0; i < 3; i++)"); line({ { "code-block", true } });
    put("    gui.richTextBox(rt, vec2(0, 300));"); line({ { "code-block", true } });
    put(axTr("Centered paragraph", "Ortalanmış paragraf")); line({ { "align", "center" } });
    put(axTr("Right-aligned paragraph", "Sağa yaslı paragraf")); line({ { "align", "right" } });
    put(axTr("Image", "Görsel")); line({ { "header", 3 } });
    if (!imagePath.empty()) {
        RtJson op = RtJson::object();
        op["insert"] = RtJson{ { "image", imagePath } };
        op["attributes"] = RtJson{ { "width", "220" } };
        ops.push_back(op);
        line();
    }
    put(axTr("You can drag an image file into this box or paste one from the clipboard.", "Görsel dosyasını bu kutuya sürükleyip bırakabilir ya da panodan yapıştırabilirsin.")); line();
    return ops.dump();
}

// ------------------------------------------------------------------ language (TR / EN)
// Placeholders and sample texts follow the language; unmodified sample text switches to the new language too.
static const char *kNotesEN = "axGUI text box: selection, copy / paste, backspace,\ndouble-click selects a word, triple-click a line.";
static const char *kNotesTR = "axGUI metin kutusu: seçim, kopyala / yapıştır, geri silme,\nçift tık kelime, üçlü tık satır seçer.";
static const char *kEditorEN =
    "// axGUI layout example\n"
    "gui.beginWindow(\"Controls\", pos, size, WIN_TITLE_BAR | WIN_SCROLLBAR);\n"
    "if (gui.button(\"Click\")) clicks++;\n"
    "gui.sameline();\n"
    "gui.dragFloat(vec2(140, 28), 0, 1, &value);\n"
    "gui.endWindow(\"Controls\");\n";
static const char *kEditorTR =
    "// axGUI düzen örneği\n"
    "gui.beginWindow(\"Kontroller\", pos, size, WIN_TITLE_BAR | WIN_SCROLLBAR);\n"
    "if (gui.button(\"Tıkla\")) clicks++;\n"
    "gui.sameline();\n"
    "gui.dragFloat(vec2(140, 28), 0, 1, &value);\n"
    "gui.endWindow(\"Kontroller\");\n";

static std::string richSampleDelta(const std::string &imagePath);

static void applyLanguage(Demo &d)
{
    d.rich.placeholder = axTr("Start typing…", "Yazmaya başla…");
    d.rich.view.imagePlaceholder = axTr("[image]", "[görsel]");
    d.richLink.placeholder = axTr("https://… or #heading", "https://… ya da #başlık");
    d.name.placeholder = axTr("Type your name, press Enter to confirm", "Adınızı yazın, Enter ile onaylayın");
    d.notes.placeholder = axTr("Multi-line note (Ctrl+Enter confirms)", "Çok satırlı not (Ctrl+Enter onaylar)");
    // sample texts: if never edited (i.e. still the other language's sample), switch to the new language
    if (d.notes.text.empty() || d.notes.text == kNotesEN || d.notes.text == kNotesTR) d.notes.text = axTr(kNotesEN, kNotesTR);
    if (d.editor.text.empty() || d.editor.text == kEditorEN || d.editor.text == kEditorTR) d.editor.text = axTr(kEditorEN, kEditorTR);
    if (d.richEdits == 0) d.rich.loadDeltaString(richSampleDelta(d.richImagePath));
    translateRows();
    d.status = axTr("Ready", "Hazır");
}

static void richWindow(axGUI &gui, Demo &d)
{
    vec2 &pos = gRichPos, &size = gRichSize;
    gui.beginWindow(axTr("Rich text###rich", "Zengin metin###rich"), pos, size, WIN_TITLE_BAR | WIN_RESIZABLE | WIN_AUTO_NEWLINE);
    RichTextState &rt = d.rich;
    RichEditor &ed = rt.editor;
    const float W = gui.getContainerWidth();
    bool act = false;   // a format button was pressed: focus returns to the box

    // --- view
    if (tbBtn(gui, axTr("Edit", "Düzenle"), d.richView == 0)) d.richView = 0;
    if (tbBtn(gui, "Delta", d.richView == 1, axTr("Quill Delta JSON (saved format)", "Quill Delta JSON (kaydedilen biçim)"))) { d.richView = 1; d.richExport.text = rt.deltaString(2); }
    if (tbBtn(gui, "HTML", d.richView == 2, axTr("Exported HTML", "Dışa aktarılan HTML"))) { d.richView = 2; d.richExport.text = rt.html(); }
    gui.spacerH(12);
    if (tbBtn(gui, axTr("Read-only", "Salt okunur"), rt.readOnly, axTr("Read only; links open on click", "Yalnızca okuma; bağlantılar tıkla açılır"))) rt.readOnly = !rt.readOnly;
    if (tbBtn(gui, axTr("Sample", "Örnek"), false, axTr("Reload the sample document", "Örnek belgeyi yeniden yükle"))) rt.loadDeltaString(richSampleDelta(d.richImagePath));
    if (tbBtn(gui, axTr("Clear", "Temizle"), false)) rt.setPlainText("");
    gui.newline();

    if (d.richView == 0) {
        // --- format bar (row 1): undo, inline formats, size, color
        if (tbBtn(gui, axTr("Undo", "Geri al"), false, "Ctrl+Z")) { ed.undo(); act = true; }
        if (tbBtn(gui, axTr("Redo", "Yinele"), false, "Ctrl+Y")) { ed.redo(); act = true; }
        gui.spacerH(8);
        if (tbBtn(gui, "K", ed.inlineActive("bold"), axTr("Bold (Ctrl+B)", "Kalın (Ctrl+B)"), 30)) { ed.toggleInline("bold"); act = true; }
        if (tbBtn(gui, "İ", ed.inlineActive("italic"), axTr("Italic (Ctrl+I)", "İtalik (Ctrl+I)"), 30)) { ed.toggleInline("italic"); act = true; }
        if (tbBtn(gui, "A", ed.inlineActive("underline"), axTr("Underline (Ctrl+U)", "Altı çizili (Ctrl+U)"), 30)) { ed.toggleInline("underline"); act = true; }
        if (tbBtn(gui, "Ü", ed.inlineActive("strike"), axTr("Strikethrough (Ctrl+Shift+X)", "Üstü çizili (Ctrl+Shift+X)"), 30)) { ed.toggleInline("strike"); act = true; }
        if (tbBtn(gui, "</>", ed.inlineActive("code"), axTr("Inline code", "Satır içi kod"), 38)) { ed.toggleInline("code"); act = true; }
        if (tbBtn(gui, axTr("Highlight", "Vurgu"), ed.inlineActive("background"), axTr("Yellow highlight", "Sarı vurgu"))) {
            ed.setInline("background", ed.inlineActive("background") ? RtJson() : RtJson("#fffff176")); act = true;
        }
        {
            const std::vector<IdStrPair> sizes = { { 0, axTr("Size", "Boyut") }, { 1, "12" }, { 2, "14" }, { 3, "18" }, { 4, "24" }, { 5, "32" } };
            static const char *vals[] = { nullptr, "12", "14", "18", "24", "32" };
            RtJson cur = ed.inlineValue("size");
            int sel = 0;
            for (int i = 1; i < 6; i++) if (cur.is_string() && cur.get<std::string>() == vals[i]) sel = i;
            int before = sel;
            gui.comboBox(vec2(84, 26), sizes, sel, axTr("Font size", "Yazı boyu"));
            if (sel != before) { ed.setInline("size", sel == 0 ? RtJson() : RtJson(vals[sel])); act = true; }
        }
        gui.newline();
        // --- row 2: colors
        static const cColor swatches[] = { cColor(229, 57, 53), cColor(251, 140, 0), cColor(67, 160, 71), cColor(30, 136, 229), cColor(142, 36, 170), cColor(120, 120, 120) };
        for (const cColor &c : swatches) {
            if (gui.shapeButton(vec2(26, 26), "circle", vec2(18, 18), c, 1, cColor(120), axTr("Text color", "Yazı rengi"))) {
                ed.setInline("color", RtJson(rtQuillColor(Color{ (unsigned char)c.r, (unsigned char)c.g, (unsigned char)c.b, 255 }))); act = true;
            }
        }
        if (tbBtn(gui, axTr("Color ×", "Renk ×"), false, axTr("Remove text color", "Yazı rengini kaldır"))) { ed.setInline("color", RtJson()); act = true; }
        if (tbBtn(gui, axTr("Clear formatting", "Biçimi temizle"), false)) { ed.clearFormatting(); act = true; }
        gui.newline();
        // --- row 3: blocks
        int h = ed.headerLevel();
        if (tbBtn(gui, "P", h == 0 && !ed.blockActive("code-block"), axTr("Paragraph (Ctrl+Shift+0)", "Paragraf (Ctrl+Shift+0)"), 30)) { ed.setHeader(0); act = true; }
        if (tbBtn(gui, "H1", h == 1, axTr("Heading 1 (Ctrl+Shift+1)", "Başlık 1 (Ctrl+Shift+1)"), 34)) { ed.setHeader(h == 1 ? 0 : 1); act = true; }
        if (tbBtn(gui, "H2", h == 2, axTr("Heading 2", "Başlık 2"), 34)) { ed.setHeader(h == 2 ? 0 : 2); act = true; }
        if (tbBtn(gui, "H3", h == 3, axTr("Heading 3", "Başlık 3"), 34)) { ed.setHeader(h == 3 ? 0 : 3); act = true; }
        gui.spacerH(8);
        std::string lt = ed.listType();
        if (tbBtn(gui, "•", lt == "bullet", axTr("Bulleted list (Ctrl+Shift+8)", "Madde listesi (Ctrl+Shift+8)"), 30)) { ed.toggleList("bullet"); act = true; }
        if (tbBtn(gui, "1.", lt == "ordered", axTr("Numbered list (Ctrl+Shift+7)", "Numaralı liste (Ctrl+Shift+7)"), 30)) { ed.toggleList("ordered"); act = true; }
        if (tbBtn(gui, "[ ]", lt == "checked" || lt == "unchecked", axTr("Checklist (Ctrl+Shift+9)", "Yapılacaklar (Ctrl+Shift+9)"), 34)) { ed.toggleList("unchecked"); act = true; }
        if (tbBtn(gui, "»", false, axTr("Indent (Tab)", "Girinti (Tab)"), 30)) { ed.indent(1); act = true; }
        if (tbBtn(gui, "«", false, axTr("Outdent (Shift+Tab)", "Girintiyi azalt (Shift+Tab)"), 30)) { ed.indent(-1); act = true; }
        gui.spacerH(8);
        std::string al = ed.align();
        if (tbBtn(gui, axTr("Left", "Sol"), al.empty(), nullptr)) { ed.setAlign(""); act = true; }
        if (tbBtn(gui, axTr("Center", "Orta"), al == "center", nullptr)) { ed.setAlign("center"); act = true; }
        if (tbBtn(gui, axTr("Right", "Sağ"), al == "right", nullptr)) { ed.setAlign("right"); act = true; }
        gui.newline();
        // --- row 4: quote, code block, link, image
        if (tbBtn(gui, axTr("Quote", "Alıntı"), ed.blockActive("blockquote"))) { ed.toggleBlock("blockquote"); act = true; }
        if (tbBtn(gui, axTr("Code block", "Kod bloğu"), ed.blockActive("code-block"), "Ctrl+Shift+E")) { ed.toggleBlock("code-block"); act = true; }
        if (!d.richLink.focused) d.richLink.text = ed.currentLink();
        gui.textBox(d.richLink, vec2(std::max(120.0f, W - 330), 26));
        if (tbBtn(gui, axTr("Link", "Bağla"), false, axTr("Apply to the selection / link at the caret (empty: remove)", "Seçime / imleçteki bağlantıya uygula (boş: kaldır)"))) { ed.applyLink(d.richLink.text); act = true; }
        if (tbBtn(gui, axTr("Image", "Görsel"), false, axTr("Insert a sample image (drag-and-drop a file works too)", "Örnek görsel ekle (dosya sürükle-bırak da olur)"))) {
            std::string uri = d.richImagePath.empty() ? std::string() : rtImageFileDataUri(d.richImagePath, 256);
            if (!uri.empty()) { ed.insertImageEmbed(uri, 128); act = true; }
        }
        gui.newline();

        if (act) { rt.focused = true; ed.blinkStart = GetTime(); }

        // --- box
        float h2 = std::max(120.0f, gui.layoutArea.bottom() - gui.cursorPos().y - 40);
        if (gui.richTextBox(rt, vec2(W, h2))) d.richEdits++;
        gui.newline();
        std::string txt = rt.plainText();
        int words = 0; bool inW = false;
        for (unsigned char ch : txt) { bool w = !(ch == ' ' || ch == '\n' || ch == '\t'); if (w && !inW) words++; inW = w; }
        std::string info = TextFormat(axTr("%d words  |  %d lines  |  edits: %d%s", "%d kelime  |  %d satır  |  değişiklik: %d%s"), words, (int)rt.view.doc.lines.size(), d.richEdits,
                                      ed.dirty ? axTr("  |  unsaved", "  |  kaydedilmedi") : "");
        if (!rt.hoverLink.empty()) info += "  |  " + rt.hoverLink;
        gui.label(info.c_str(), vec2(0, 22));
    } else {
        float h2 = std::max(120.0f, gui.layoutArea.bottom() - gui.cursorPos().y - 6);
        gui.textBox(d.richExport, vec2(W, h2));
    }
    gui.endWindow("###rich");
}

// ------------------------------------------------------------------ main
int main(int argc, char **argv)
{
    // unbuffered stdout: axLog() / TraceLog lines show up immediately in Qt Creator's "Application Output"
    // (when writing to a pipe instead of a console, the C runtime buffers output in blocks)
    setvbuf(stdout, nullptr, _IONBF, 0);
    // --screenshot <file.png> [--frames N]: take a screenshot after N frames and exit (for testing / docs)
    const char *shotPath = nullptr;
    int shotFrames = 30;
    bool startLight = false;
    int winW = 1440, winH = 860;
    bool startPopups = false;
    bool startStyle = false;
    axLang = AX_LANG_EN;   // demo starts in English (--lang tr / F7 / top bar)
    for (int i = 1; i < argc; i++) {
        if (TextIsEqual(argv[i], "--screenshot") && i + 1 < argc) shotPath = argv[++i];
        else if (TextIsEqual(argv[i], "--frames") && i + 1 < argc) shotFrames = TextToInteger(argv[++i]);
        else if (TextIsEqual(argv[i], "--light")) startLight = true;
        else if (TextIsEqual(argv[i], "--popups")) startPopups = true;
        else if (TextIsEqual(argv[i], "--style")) startStyle = true;     // testing: start with the style editor open   // testing: start with popups open
        else if (TextIsEqual(argv[i], "--lang") && i + 1 < argc) axLang = TextIsEqual(argv[++i], "en") ? AX_LANG_EN : AX_LANG_TR;
        else if (TextIsEqual(argv[i], "--size") && i + 1 < argc) sscanf(argv[++i], "%dx%d", &winW, &winH);
    }
    int frame = 0;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
#ifdef PLATFORM_WEB
    winW = webInnerWidth(); winH = webInnerHeight();
#endif
    InitWindow(winW, winH, "axGUI demo - raylib " RAYLIB_VERSION);
#ifndef PLATFORM_WEB
    SetWindowMinSize(640, 480);
#else
    SetExitKey(KEY_NULL);   // web: ESC must not end the loop (the canvas would freeze)
#endif
    // GUI and rich text share one font system: a crisp atlas at each font size (rtFonts)
    const float fontSize = 15;
    bool rtOk = rtFonts().load(GetDirectoryPath(resPath("fonts/regular.ttf").c_str()));
    axFont *font = rtOk ? rtFonts().guiFont(fontSize) : loadGuiFont(resPath("fonts/regular.ttf"), 32);

    axDrawBot bot;
    axGUI gui;
    bot.init();
    gui.bot = &bot;
    if (rtOk) bot.fontForSize = [](float px) { return rtFonts().guiFont(px); };
    // icon atlas (section arrows, dropdown circle; also for imageButton)
    makeDrawnIcons(bot);   // icons from drawings (Catalog > Icons from drawing)
    if (!gui.loadIconAtlas(resPath("icons.png").c_str()))
        TraceLog(LOG_WARNING, "DEMO: resources/icons.png yüklenemedi, bölüm okları vektörle çizilecek");
    gui.setfont(font, fontSize);
    gui.itemSpaceX = 6;
    gui.itemSpaceY = 6;
    gui.winRadius = 8;
    gui.btnRadius = 5;
    gui.frameRadius = 5;
    gui.checkRadius = 4;
    textcfg.borderWidth = 1.5f;
    textcfg.cornerRadius = 6;

    Demo d;
    // rich text: fonts (regular / bold / italic / bolditalic; mono from the system), sample document
    d.richImagePath = resPath("icons.png");
    if (!FileExists(d.richImagePath.c_str())) d.richImagePath.clear();
    d.richExport.multiline = true;
    d.dark = !startLight;
    d.showStyle = startStyle;
    if (startPopups) {
        d.showTopmost = true;
        d.selOpen = true; d.selPos = vec2(420, 560);
        gui.showClrPicker = true; gui.clrPickerPtr = &d.btnColor; gui.clrPickerColor = d.btnColor; gui.clrPickerPos = vec2(760, 420);
        d.popup.open = true; d.popup.first = true; d.popupPos = vec2(1060, 600);
    }
    d.notes.multiline = true;
    d.notes.wrapLines = true;
    d.editor.multiline = true;
    d.editor.syntaxHighlight = true;
    d.editor.syntaxLanguage = "cpp";
#ifdef PLATFORM_WEB
    // web showcase: rich text window open, window snapping off.
    // No command line; --lang etc. are desktop-only.
    d.showRich = true;
    gui.winSnap = false;
#endif
    // layout by screen size; on a narrow screen only Controls starts open (others via the top bar)
    // (at medium width the middle column is shared: Catalog starts closed)
    const DemoLayout startLayout = layoutWindows(gui, (float)GetScreenWidth(), (float)GetScreenHeight());
    if (startLayout == LAYOUT_XL) { d.showRich = d.showData = d.showStyle = d.showCatalog = true; }
    if (startLayout == LAYOUT_MEDIUM || startLayout == LAYOUT_NARROW) d.showCatalog = false;
    if (startLayout == LAYOUT_NARROW) { d.showRich = false; d.showData = false; }
    int langApplied = -1;   // on language change: applyLanguage (placeholders, sample texts)

    bool darkApplied = !d.dark;
    Theme theme;
    vec2 lastSize(GetScreenWidth(), GetScreenHeight());

    while (!WindowShouldClose()) {
        // --- shortcuts (when no text box is focused)
        if (!gui.wantCaptureKeyboard) {
            if (IsKeyPressed(KEY_F1)) d.showControls = !d.showControls;
            if (IsKeyPressed(KEY_F2)) d.showData = !d.showData;
            if (IsKeyPressed(KEY_F3)) d.dark = !d.dark;
            if (IsKeyPressed(KEY_F4)) d.showCatalog = !d.showCatalog;
            if (IsKeyPressed(KEY_F5)) d.showRich = !d.showRich;
            if (IsKeyPressed(KEY_F6)) d.showStyle = !d.showStyle;
            if (IsKeyPressed(KEY_F7)) axLang = axLang == AX_LANG_TR ? AX_LANG_EN : AX_LANG_TR;
        }
#ifndef PLATFORM_WEB
        if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();   // web: the browser's own F11
#endif

        if (d.dark != darkApplied) {
            theme = d.dark ? themeDark() : themeLight();
            applyTheme(gui, theme);
            rtTheme().apply(d.dark);   // rich text colors
            gui.resetStyleWindowLook(); // draw the style window with the new theme (unaffected by edits)
            darkApplied = d.dark;
        }
        gui.debug = d.guiDebug;
        if (axLang != langApplied) { applyLanguage(d); langApplied = axLang; }

        vec2 sz(GetScreenWidth(), GetScreenHeight());
        if (sz != lastSize) {
#ifdef PLATFORM_WEB
            layoutWindows(gui, sz.x, sz.y);   // web: when the browser window changes, redo the layout from scratch
#else
            gui.resized(sz, lastSize);        // desktop: the user's layout is scaled proportionally
#endif
            lastSize = sz; d.centerSet = false;
        }
        if (d.animate) d.animTime += GetFrameTime();

        BeginDrawing();
        ClearBackground(theme.bg);

        if (rtOk) { rtFonts().beginFrame(); font = rtFonts().guiFont(gui.fontSize); }   // new atlas if the DPI changes
        gui.setfont(font, gui.fontSize);   // the style editor can change the font size
        gui.startFrame(true);   // retained: windows are drawn in zOrder (focused one on top)

        // --- canvas (behind the windows)
        drawCanvas(bot, d, theme, sz.x, sz.y);
        if (!gui.wantCaptureMouse && !d.popup.open) {
            if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) || (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && IsKeyDown(KEY_SPACE))) {
                Vector2 md = GetMouseDelta();
                d.center = d.center + vec2(md.x, md.y);
            }
            float wh = GetMouseWheelMove();
            if (wh != 0) d.radius = Clamp(d.radius * (1.0f + wh * 0.1f), 20.0f, 400.0f);
            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                d.popup.open = true; d.popup.first = true;
                d.popupPos = vec2(GetMouseX(), GetMouseY());
            }
        }

        // --- top bar
        {
            static vec2 bpos(0, 0), bsize(0, 44);
            bpos = vec2(0, 0); bsize = vec2(sz.x, 44);
            auto &ws = gui.windowStates["ustcubuk"];
            if (ws.init) { ws.pos = bpos; ws.size = bsize; }
            float r = gui.winRadius; gui.winRadius = 0;
            gui.beginWindow("ustcubuk", bpos, bsize, WIN_NO_MOVE);
            gui.winRadius = r;
            gui.label("axGUI demo", vec2(110, 28));
            gui.toggleButton(axTr("Controls", "Kontroller"), &d.showControls, vec2(0, 28), "F1");
            gui.toggleButton(axTr("Data", "Veri"), &d.showData, vec2(0, 28), "F2");
            gui.toggleButton(axTr("Catalog", "Katalog"), &d.showCatalog, vec2(0, 28), "F4");
            gui.toggleButton(axTr("Rich text", "Zengin metin"), &d.showRich, vec2(0, 28), "F5");
            gui.toggleButton(axTr("Style", "Stil"), &d.showStyle, vec2(0, 28), axTr("F6: style editor", "F6: stil düzenleyici"));
            if (gui.button(d.dark ? axTr("Light theme", "Açık tema") : axTr("Dark theme", "Koyu tema"), vec2(0, 28), 0, "F3")) d.dark = !d.dark;
            if (gui.button(axLang == AX_LANG_TR ? "EN" : "TR", vec2(36, 28), 0, axTr("F7: Türkçe", "F7: English")))
                axLang = axLang == AX_LANG_TR ? AX_LANG_EN : AX_LANG_TR;
            gui.label(TextFormat("raylib %s  |  %d FPS  |  %s", RAYLIB_VERSION, GetFPS(), d.status.c_str()), vec2(0, 28));
            gui.label(TextFormat("scrWin: <%s> ", gui.scrollInfo().c_str()), vec2(0, 28));
            gui.endWindow("ustcubuk");
        }

        if (d.showControls) controlsWindow(gui, d);
        if (d.showData) dataWindow(gui, d);
        if (d.showCatalog) catalogWindow(gui, d);
        if (d.showRich) richWindow(gui, d);
        topmostWindow(gui, d);
        gui.showStyleWindow(&d.showStyle);

        // colorSelector: call at top level (called inside a window it becomes a child window)
        if (gui.colorSelector(axTr("Color", "Renk"), d.selOpen, d.selPos, vec2(270, 170), 22, d.selColor))
            d.status = TextFormat("colorSelector: %d, %d, %d", d.selColor.r, d.selColor.g, d.selColor.b);

        // --- canvas right-click menu (topmost)
        const std::vector<cString> menu = { axTr("Center here", "Buraya ortala"), axTr("Toggle animation", "Animasyonu aç / kapat"), axTr("Change shape", "Şekli değiştir"), axTr("Random color", "Rengi rastgele yap") };
        int sel = gui.popupMenu(axTr("Canvas", "Tuval"), d.popup, d.popupPos, vec2(210, 40 + 30 * (float)menu.size()), 26, menu);
        switch (sel) {
            case 0: d.center = d.popupPos; d.status = axTr("Center moved", "Merkez taşındı"); break;
            case 1: d.animate = !d.animate; break;
            case 2: d.shapeKind = (d.shapeKind + 1) % 3; break;
            case 3: d.shapeColor = cColor(GetRandomValue(40, 255), GetRandomValue(40, 255), GetRandomValue(40, 255)); break;
            default: break;
        }

        gui.endFrame();
        EndDrawing();

        if (shotPath && ++frame >= shotFrames) {
            Image img = LoadImageFromScreen();
            ExportImage(img, shotPath);
            UnloadImage(img);
            break;
        }
    }

    bot.cleanup();
    if (!rtOk) {   // rtFonts atlases are owned by it
        if (font->font.texture.id != GetFontDefault().texture.id) UnloadFont(font->font);
        delete font;
    }
    CloseWindow();
    return 0;
}
