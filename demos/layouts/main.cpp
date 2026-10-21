// axGUI-layouts - layout lab for axGUI
// How widgets are placed: the layout cursor, window flow flags, item spacing, containers and
// layout recipes built from the current primitives. The debug overlay (gui.debugLayout) outlines
// every item, the layout area of each window and the cursor.
//
// Windows:
//   "Settings"    - spacing, flow mode of the Flow window, overlay switches, theme / language
//   "Flow"        - many items in a resizable window: WIN_AUTO_NEWLINE / WIN_NO_SAMELINE / no flag
//   "Cursor"      - sameline, newline, spacerH / spacerV, newline(h), setCursorX, mixed row heights
//   "Recipes"     - equal columns, form rows, right-aligned / centered rows, fill remaining width, split
//   "Containers"  - section, group box (nested, side by side), tab bar, window inside a window
//
// Frame loop: BeginDrawing -> gui.setfont -> gui.startFrame -> windows -> gui.endFrame -> EndDrawing

#include "raylib.h"
#include "axGuiSystem.h"
#include "axGUITextBox.h"
#include "axRichFonts.h"

#include <string>
#include <vector>
#include <cmath>
#include <cstdio>
#include <algorithm>
#ifdef PLATFORM_WEB
#include <emscripten/emscripten.h>
EM_JS(int, webInnerWidth, (), { return window.innerWidth; });
EM_JS(int, webInnerHeight, (), { return window.innerHeight; });
#endif

extern TextBoxConfig textcfg;   // axGUITextBox.cpp

// ------------------------------------------------------------------ resource folder
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

// ------------------------------------------------------------------ theme
struct Theme {
    Color bg;
    cColor idle, hover, btn, btnHover, btnSel, toggle, text, textHl, textSel, winBack, winBorder;
    cColor inBg, inBorder, inFocus, inText, tipBg, tipText;
    cColor code;   // code lines above the examples
};

static Theme themeDark()
{
    Theme t;
    t.bg = { 24, 25, 28, 255 };
    t.idle = cColor(50, 52, 58);         t.hover = cColor(40, 70, 110);
    t.btn = cColor(48, 50, 56);          t.btnHover = cColor(66, 69, 77);
    t.btnSel = cColor(40, 70, 110);      t.toggle = cColor(66, 140, 230);
    t.text = cColor(228, 230, 234);      t.textHl = cColor(255, 255, 255);
    t.textSel = cColor(90, 170, 255);    t.winBack = cColor(34, 35, 39, 245);
    t.winBorder = cColor(64, 66, 72);
    t.inBg = cColor(26, 27, 30);         t.inBorder = cColor(72, 74, 80);
    t.inFocus = cColor(66, 165, 245);    t.inText = cColor(228, 230, 234);
    t.tipBg = cColor(14, 15, 18, 240);   t.tipText = cColor(235, 236, 240);
    t.code = cColor(130, 190, 140);
    return t;
}

static Theme themeLight()
{
    Theme t;
    t.bg = { 238, 240, 244, 255 };
    t.idle = cColor(226, 229, 234);      t.hover = cColor(219, 236, 253);
    t.btn = cColor(232, 234, 238);       t.btnHover = cColor(214, 218, 225);
    t.btnSel = cColor(219, 236, 253);    t.toggle = cColor(33, 150, 243);
    t.text = cColor(38, 40, 44);         t.textHl = cColor(10, 12, 16);
    t.textSel = cColor(33, 120, 220);    t.winBack = cColor(250, 250, 251, 245);
    t.winBorder = cColor(210, 213, 218);
    t.inBg = cColor(255, 255, 255);      t.inBorder = cColor(200, 203, 208);
    t.inFocus = cColor(33, 150, 243);    t.inText = cColor(38, 40, 44);
    t.tipBg = cColor(40, 42, 48, 245);   t.tipText = cColor(245, 246, 248);
    t.code = cColor(40, 120, 60);
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
    textcfg.bgColorSingle = t.inBg;
    textcfg.bgColorMulti = t.inBg;
    textcfg.borderColor = t.inBorder;
    textcfg.borderColorFocused = t.inFocus;
    textcfg.textColor = t.inText;

    TabBarStyle &ts = gui.tabStyle;
    ts.barBackground = t.winBack;  ts.barBackground.a = 255;
    ts.tabNormal = t.btn;          ts.tabHovered = t.btnHover;
    ts.tabActive = t.idle;         ts.tabTextNormal = t.text;  ts.tabTextNormal.a = 170;
    ts.tabTextActive = t.textHl;   ts.underlineColor = t.toggle;
    ts.separatorColor = t.winBorder;
}

// ------------------------------------------------------------------ sample picture
// A small landscape generated at startup (the demo ships no photos); registered as the "sample_photo" image.
static Texture2D makeSamplePhoto(axDrawBot &bot)
{
    const int W = 320, H = 200;
    Image img = GenImageGradientLinear(W, H, 0, Color{ 255, 170, 110, 255 }, Color{ 70, 120, 200, 255 });   // sky
    ImageDrawCircle(&img, 230, 120, 34, Color{ 255, 236, 170, 255 });                                    // sun
    ImageDrawTriangle(&img, Vector2{ -40, 200 }, Vector2{ 210, 200 }, Vector2{ 90, 70 }, Color{ 60, 70, 95, 255 });
    ImageDrawTriangle(&img, Vector2{ 120, 200 }, Vector2{ 380, 200 }, Vector2{ 250, 95 }, Color{ 45, 55, 78, 255 });
    ImageDrawRectangle(&img, 0, 165, W, 35, Color{ 38, 70, 60, 255 });                                    // ground
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    GenTextureMipmaps(&tex);
    SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR);
    bot.addIconFromTexture("sample_photo", tex);
    return tex;
}

// ------------------------------------------------------------------ demo state
enum FlowMode { FLOW_AUTO_NEWLINE = 0, FLOW_NO_SAMELINE, FLOW_NONE };

struct Demo {
    bool dark = true;
    bool debug = false;         // gui.debugLayout: outlines of every item
    bool overlay = false;       // gui.debugLayout: item outlines
    bool showArea = true;      // layout area of each window
    bool showCursor = true;    // layout cursor at the end of each window
    bool showSettings = true, showFlow = true, showCursorWin = true, showRecipes = true, showContainers = true;
    bool winSnap = true;         // windows snap to each other and the screen edges
    int flowMode = FLOW_AUTO_NEWLINE;
    int flowCount = 16;
    bool flowVaryH = false;    // items of different heights in the Flow window

    int cols = 3;              // Recipes: equal columns
    float labelW = 110;        // Recipes: form label column
    float split = 0.3f;        // Recipes: sidebar share
    float vol = 60, speed = 1.5f;
    bool wrap = true;
    int quality = 1;

    bool tabsOpen[2] = { true, true };
    std::string status;
    Theme theme;
};

// ------------------------------------------------------------------ overlay helpers
// layoutArea is in screen coordinates; drawing inside a window is offset by the window scroll, so subtract it.
static void drawLayoutArea(axGUI &gui, const Demo &d, cColor c = cColor(80, 220, 120, 200))
{
    return;
    rect la = gui.layoutArea;
    gui.bot->roundedRect(la.pos - gui.winScroll, la.size, 0, 1, c);
}

static void drawCursor(axGUI &gui, const Demo &d)
{
    if (!d.showCursor) return;
    vec2 p = gui.cursorPos();
    const cColor c(255, 70, 90, 230);
    gui.bot->drawLine(p - vec2(6, 0), p + vec2(6, 0), c, 1.5f);
    gui.bot->drawLine(p - vec2(0, 6), p + vec2(0, 6), c, 1.5f);
    gui.bot->text("cursor", p + vec2(8, -14), 11, c);
}

// code line above an example: a label in the theme's code color, on its own row
static void code(axGUI &gui, const Demo &d, const char *s)
{
    const cColor t = gui.colors[GUI_TEXT_COLOR];
    gui.colors[GUI_TEXT_COLOR] = d.theme.code;
    gui.label(s, vec2(0, 18));
    gui.colors[GUI_TEXT_COLOR] = t;
    gui.newline();
}

// auto width of a button (same rule as axGUI::button: text + 2 * font size)
static float buttonW(axGUI &gui, const char *s) { return gui.bot->textBounds(s, gui.fontSize).x + 2 * gui.fontSize; }

static WinFlags flowFlag(int mode)
{
    switch (mode) {
        case FLOW_AUTO_NEWLINE: return WIN_AUTO_NEWLINE;
        case FLOW_NO_SAMELINE:  return WIN_NO_SAMELINE;
        default:                return WIN_0;
    }
}

// ------------------------------------------------------------------ window placement (from the screen size)
static vec2 gSetPos, gSetSize, gFlowPos, gFlowSize, gCurPos, gCurSize, gRecPos, gRecSize, gConPos, gConSize;

static void placeWin(axGUI &gui, const char *id, vec2 &pos, vec2 &size, vec2 p, vec2 s)
{
    pos = p; size = s;
    auto it = gui.windowStates.find(id);
    if (it != gui.windowStates.end() && it->second.init) it->second.setGeometry(p, s);
}

static void layoutWindows(axGUI &gui, float W, float H)
{
    const float m = 12, g = 10, top = 56;
    const float h = std::max(300.0f, H - top - m);
    const float sw = 300;                                   // Settings column
    const float rest = std::max(400.0f, W - m - sw - g - m);
    const float cw = std::floor((rest - 2 * g) / 3);        // three columns: Flow / Cursor, Recipes, Containers
    const float x1 = m + sw + g, x2 = x1 + cw + g, x3 = x2 + cw + g;
    const float fh = std::round(h * 0.42f);
    placeWin(gui, "settings", gSetPos, gSetSize, vec2(m, top), vec2(sw, h));
    placeWin(gui, "flow", gFlowPos, gFlowSize, vec2(x1, top), vec2(cw, fh));
    placeWin(gui, "cursor", gCurPos, gCurSize, vec2(x1, top + fh + g), vec2(cw, h - fh - g));
    placeWin(gui, "recipes", gRecPos, gRecSize, vec2(x2, top), vec2(cw, h));
    placeWin(gui, "containers", gConPos, gConSize, vec2(x3, top), vec2(cw, h));
}

// ------------------------------------------------------------------ Settings
static void settingsWindow(axGUI &gui, Demo &d)
{
    gui.beginWindow(axTr("Settings###settings", "Ayarlar###settings"), gSetPos, gSetSize, WIN_TITLE_BAR | WIN_SCROLLBAR | WIN_RESIZABLE | WIN_AUTO_NEWLINE);
    const float W = gui.getContainerWidth(), rowH = 26, lw = 120, fw = W - lw - gui.itemSpaceX;

    gui.separator(8, axTr("Overlay", "Kaplama"));
    gui.checkbox(axTr("Item outlines (gui.debugLayout)", "Öğe çerçeveleri (gui.debugLayout)"), &d.overlay, vec2(W, rowH)); gui.newline();
    gui.checkbox(axTr("Layout area", "Yerleşim alanı"), &d.showArea, vec2(W, rowH)); gui.newline();
    gui.checkbox(axTr("Cursor at the end", "Sondaki imleç"), &d.showCursor, vec2(W, rowH)); gui.newline();
    gui.label(axTr("orange: item   cyan: itemSpaceX", "turuncu: öğe   camgöbeği: itemSpaceX"), vec2(0, 20)); gui.newline();
    gui.label(axTr("green: layoutArea   red: cursor", "yeşil: layoutArea   kırmızı: imleç"), vec2(0, 20)); gui.newline();

    gui.separator(8, axTr("Spacing", "Aralıklar"));
    gui.label("itemSpaceX", vec2(lw, rowH)); gui.dragFloat(vec2(fw, rowH), 0, 40, &gui.itemSpaceX, 6, 0.2f, "%.0f"); gui.newline();
    gui.label("itemSpaceY", vec2(lw, rowH)); gui.dragFloat(vec2(fw, rowH), 0, 40, &gui.itemSpaceY, 6, 0.2f, "%.0f"); gui.newline();
    gui.label(axTr("winSpace X / Y", "winSpace X / Y"), vec2(lw, rowH));
    gui.dragFloat(vec2((fw - gui.itemSpaceX) / 2, rowH), 0, 40, &gui.winSpaceX, 8, 0.2f, "%.0f");
    gui.dragFloat(vec2((fw - gui.itemSpaceX) / 2, rowH), 0, 40, &gui.winSpaceY, 4, 0.2f, "%.0f");
    gui.newline();
    gui.label(axTr("Font size", "Yazı boyu"), vec2(lw, rowH)); gui.dragFloat(vec2(fw, rowH), 10, 28, &gui.fontSize, 15, 0.1f, "%.0f"); gui.newline();

    gui.separator(8, axTr("Flow window", "Akış penceresi"));
    const std::vector<IdStrPair> modes = { { FLOW_AUTO_NEWLINE, "WIN_AUTO_NEWLINE" }, { FLOW_NO_SAMELINE, "WIN_NO_SAMELINE" }, { FLOW_NONE, axTr("no flow flag", "akış bayrağı yok") } };
    gui.label(axTr("Flow flag", "Akış bayrağı"), vec2(lw, rowH)); gui.comboBox(vec2(fw, rowH), modes, d.flowMode); gui.newline();
    gui.label(axTr("Items", "Öğe sayısı"), vec2(lw, rowH)); gui.dragInt(vec2(fw, rowH), 1, 60, &d.flowCount, 0.2f); gui.newline();
    gui.checkbox(axTr("Different heights", "Farklı yükseklikler"), &d.flowVaryH, vec2(W, rowH)); gui.newline();

    gui.separator(8, axTr("Windows", "Pencereler"));
    gui.toggleButton(axTr("Flow", "Akış"), &d.showFlow, vec2(0, rowH));
    gui.toggleButton(axTr("Cursor", "İmleç"), &d.showCursorWin, vec2(0, rowH));
    gui.toggleButton(axTr("Recipes", "Tarifler"), &d.showRecipes, vec2(0, rowH));
    gui.toggleButton(axTr("Containers", "Kaplar"), &d.showContainers, vec2(0, rowH));
    gui.newline();
    if (gui.button(axTr("Reset window layout", "Pencere yerleşimini sıfırla"), vec2(W, rowH)))
        layoutWindows(gui, (float)GetScreenWidth(), (float)GetScreenHeight());
    gui.newline();

    if(d.showArea) drawLayoutArea(gui, d);
    drawCursor(gui, d);
    gui.endWindow("###settings");
}

// ------------------------------------------------------------------ Flow
static void flowWindow(axGUI &gui, Demo &d)
{
    gui.beginWindow(axTr("Flow###flow", "Akış###flow"), gFlowPos, gFlowSize, WIN_TITLE_BAR | WIN_SCROLLBAR | WIN_RESIZABLE | flowFlag(d.flowMode));

    gui.beginGrid("grid1",3,1);
    gui.text("Text.1",12,cWhite);
    gui.button("Button.1");
    gui.text("Text.2",22,cWhite);
    gui.button("Button.2");
    gui.icon("sample_photo", vec2(0, 90), "image: fills the cell width, aspect-fit");
    gui.image(resPath("icons.png"), vec2(150, 150), "Image: loaded from a file on first use");
    gui.text("Text.2",18,cWhite);

    for (int i = 0; i < 5; i++) {
        gui.text("Text:"+LS(i),12,cWhite);
        // gui.button(TextFormat("button:%d", i + 1), vec2(80, 24));
        // gui.dummy(vec2(80, 24), TextFormat("dummy:%d", i + 1));
        // gui.empty(vec2(80, 24));
    }
    gui.endGrid();
    static const char *words[] = { "A", "Button", "Wide button", "OK", "Long label here", "B", "Cancel", "Apply", "Item", "Settings…" };
    static const float heights[] = { 26, 40, 26, 32, 52, 26 };
    for (int i = 0; i < d.flowCount; i++) {
        float h = d.flowVaryH ? heights[i % 6] : 26;
        if (gui.button(TextFormat("%s %d", words[i % 10], i + 1), vec2(0, h))) d.status = TextFormat("Flow: %d", i + 1);
    }
    gui.newline();
    gui.label(axTr("Resize the window to see wrapping.", "Satır kaydırmayı görmek için pencereyi boyutlandır."), vec2(0, 22));
    gui.newline();
    if(d.showArea) drawLayoutArea(gui, d);
    drawCursor(gui, d);
    gui.endWindow("###flow");
}

// ------------------------------------------------------------------ Cursor
static void cursorWindow(axGUI &gui, Demo &d)
{
    gui.beginWindow(axTr("Cursor###cursor", "İmleç###cursor"), gCurPos, gCurSize, WIN_TITLE_BAR | WIN_SCROLLBAR | WIN_RESIZABLE | WIN_AUTO_NEWLINE);
    const float rowH = 26;
    rect la = gui.layoutArea;   // rect getters are not const

    code(gui, d, "button(A); button(B);   // flow: same row");
    gui.button("A", vec2(0, rowH)); gui.button("B", vec2(0, rowH)); gui.newline();

    code(gui, d, "button; spacerH(40); button");
    gui.button("A", vec2(0, rowH)); gui.spacerH(40); gui.button("B", vec2(0, rowH)); gui.newline();

    code(gui, d, "newline(); spacerV(20);");
    gui.button(axTr("before", "önce"), vec2(0, rowH)); gui.newline();
    gui.spacerV(20);
    gui.button(axTr("after spacerV(20)", "spacerV(20) sonrası"), vec2(0, rowH)); gui.newline();

    code(gui, d, "mixed heights: 24 / 48 / 32 (top aligned)");
    gui.button("24", vec2(60, 24)); gui.button("48", vec2(60, 48)); gui.button("32", vec2(60, 32)); gui.newline();

    code(gui, d, "newline(56): next row is at least 56 high");
    gui.newline(56);
    gui.button(axTr("row of 56", "56'lık satır"), vec2(0, rowH)); gui.newline();

    code(gui, d, "setCursorX(right - w): right aligned");
    const float w = buttonW(gui, axTr("Right", "Sağ"));
    gui.setCursorX(la.right() - w);
    gui.button(axTr("Right", "Sağ"), vec2(0, rowH)); gui.newline();

    code(gui, d, "setCursorX(left + 80) mid-row");
    gui.button("A", vec2(0, rowH)); gui.setCursorX(la.left() + 80); gui.button("B", vec2(0, rowH)); gui.newline();

    code(gui, d, "label(size.x = 0): auto width");
    gui.label(axTr("auto width label", "otomatik genişlikte etiket"), vec2(0, rowH)); gui.button("→", vec2(0, rowH)); gui.newline();

    if(d.showArea) drawLayoutArea(gui, d);
    drawCursor(gui, d);
    gui.endWindow("###cursor");
}

// ------------------------------------------------------------------ Recipes
static void recipesWindow(axGUI &gui, Demo &d)
{
    gui.beginWindow(axTr("Recipes###recipes", "Tarifler###recipes"), gRecPos, gRecSize, WIN_TITLE_BAR | WIN_SCROLLBAR | WIN_RESIZABLE | WIN_AUTO_NEWLINE);
    const float rowH = 26, sp = gui.itemSpaceX;
    const float W = gui.getContainerWidth();
    rect la = gui.layoutArea;   // rect getters are not const

    // --- equal columns: w = (W - (n-1) * spacing) / n (floored: a fraction over the edge wraps the last item)
    gui.separator(8, axTr("Equal columns", "Eşit sütunlar"));
    gui.label(axTr("Columns", "Sütun"), vec2(d.labelW, rowH)); gui.dragInt(vec2(W - d.labelW - sp, rowH), 1, 8, &d.cols, 0.05f); gui.newline();
    {
        const float cw = std::floor((W - (d.cols - 1) * sp) / d.cols);
        for (int r = 0; r < 2; r++) {
            for (int c = 0; c < d.cols; c++) gui.button(TextFormat("%d,%d", r + 1, c + 1), vec2(cw, rowH));
            gui.newline();
        }
    }

    // --- form rows: fixed label column, field fills the rest
    gui.separator(8, axTr("Form rows", "Form satırları"));
    gui.label(axTr("Label width", "Etiket genişliği"), vec2(d.labelW, rowH)); gui.dragFloat(vec2(W - d.labelW - sp, rowH), 40, 220, &d.labelW, 110, 0.5f, "%.0f"); gui.newline();
    {
        const float fw = W - d.labelW - sp;
        gui.label(axTr("Volume", "Ses"), vec2(d.labelW, rowH), BOT_ALIGN_RIGHT | BOT_ALIGN_MIDDLE); gui.dragFloat(vec2(fw, rowH), 0, 100, &d.vol, 60, 0.5f, "%.0f"); gui.newline();
        gui.label(axTr("Speed", "Hız"), vec2(d.labelW, rowH), BOT_ALIGN_RIGHT | BOT_ALIGN_MIDDLE); gui.dragFloat(vec2(fw, rowH), 0, 5, &d.speed, 1.5f, 0.01f, "%.2f"); gui.newline();
        const std::vector<IdStrPair> q = { { 0, axTr("Low", "Düşük") }, { 1, axTr("Medium", "Orta") }, { 2, axTr("High", "Yüksek") } };
        gui.label(axTr("Quality", "Kalite"), vec2(d.labelW, rowH), BOT_ALIGN_RIGHT | BOT_ALIGN_MIDDLE); gui.comboBox(vec2(fw, rowH), q, d.quality); gui.newline();
        gui.label("", vec2(d.labelW, rowH)); gui.checkbox(axTr("Wrap lines", "Satırları kaydır"), &d.wrap, vec2(fw, rowH)); gui.newline();
    }

    // --- right aligned / centered rows: measure first, then move the cursor
    gui.separator(8, axTr("Aligned rows", "Hizalı satırlar"));
    {
        const char *ok = axTr("OK", "Tamam"), *cancel = axTr("Cancel", "Vazgeç");
        const float total = buttonW(gui, ok) + sp + buttonW(gui, cancel);
        gui.setCursorX(la.right() - total);
        if (gui.button(ok, vec2(0, rowH))) d.status = "OK";
        if (gui.button(cancel, vec2(0, rowH))) d.status = "Cancel";
        gui.newline();
        const char *mid = axTr("Centered", "Ortalı");
        gui.setCursorX(la.left() + std::floor((W - buttonW(gui, mid)) / 2));
        gui.button(mid, vec2(0, rowH));
        gui.newline();
    }

    // --- fill the remaining width: rest = right - cursor.x
    gui.separator(8, axTr("Fill remaining width", "Kalan genişliği doldur"));
    {
        gui.label(axTr("Zoom:", "Yakınlık:"), vec2(0, rowH));
        gui.button("-", vec2(rowH, rowH));
        const float rest = la.right() - gui.cursorPos().x - sp - rowH;
        gui.dragFloat(vec2(rest, rowH), 0, 5, &d.speed, 1.5f, 0.01f, "%.2f");
        gui.button("+", vec2(rowH, rowH));
        gui.newline();
    }

    // --- split: sidebar | content (proportional widths)
    gui.separator(8, axTr("Split", "Bölme"));
    gui.label(axTr("Sidebar", "Kenar çubuğu"), vec2(d.labelW, rowH)); gui.dragFloat(vec2(W - d.labelW - sp, rowH), 0.1f, 0.9f, &d.split, 0.3f, 0.002f, "%.2f"); gui.newline();
    {
        const float a = std::floor((W - sp) * d.split), b = W - sp - a;
        gui.dummy(vec2(a, 90), axTr("sidebar", "kenar çubuğu"));
        gui.dummy(vec2(b, 90), axTr("content", "içerik"));
        gui.newline();
    }

    if(d.showArea) drawLayoutArea(gui, d);
    drawCursor(gui, d);
    gui.endWindow("###recipes");
}

// ------------------------------------------------------------------ Containers
static void containersWindow(axGUI &gui, Demo &d)
{
    gui.beginWindow(axTr("Containers###containers", "Kaplar###containers"), gConPos, gConSize, WIN_TITLE_BAR | WIN_SCROLLBAR | WIN_RESIZABLE | WIN_AUTO_NEWLINE);
    const float rowH = 26, sp = gui.itemSpaceX;
    const float W = gui.getContainerWidth();
    const GroupStyle gs{ 6, false, gui.colors[GUI_IDLE_COLOR], 1, gui.colors[GUI_WIN_BORDERCLR] };

    if (gui.beginSection(axTr("Section###c_section", "Bölüm###c_section"))) {
        for (int i = 1; i <= 7; i++) gui.button(TextFormat(axTr("Item %d", "Öğe %d"), i), vec2(0, rowH));
        gui.newline();
        drawLayoutArea(gui, d, cColor(80, 220, 120, 120));
        gui.endSection();
    }

    gui.separator(8, axTr("Group box (nested)", "Grup kutusu (iç içe)"));
    if (gui.beginGroupBox(axTr("Outer###c_outer", "Dış###c_outer"), vec2(W, 200), true, 0, gs)) {
        const float iw = gui.getContainerWidth();
        gui.label(TextFormat(axTr("inner width: %.0f", "iç genişlik: %.0f"), iw), vec2(0, 22)); gui.newline();
        if (gui.beginGroupBox(axTr("Inner###c_inner", "İç###c_inner"), vec2(iw, 90), true, 0, gs)) {
            for (int i = 1; i <= 5; i++) gui.button(TextFormat("%d", i), vec2(0, rowH));
            gui.newline();
            drawLayoutArea(gui, d, cColor(80, 220, 120, 120));
        }
        gui.endGroupBox();
        gui.newline();
        drawLayoutArea(gui, d, cColor(80, 220, 120, 120));
    }
    gui.endGroupBox();
    gui.newline();

    gui.separator(8, axTr("Group boxes side by side", "Yan yana grup kutuları"));
    {
        const float hw = std::floor((W - sp) / 2);
        if (gui.beginGroupBox(axTr("Left###c_left", "Sol###c_left"), vec2(hw, 110), false, 0, gs)) {
            gui.button("L1", vec2(0, rowH)); gui.button("L2", vec2(0, rowH)); gui.newline();
        }
        gui.endGroupBox();
        gui.sameline();
        if (gui.beginGroupBox(axTr("Right###c_right", "Sağ###c_right"), vec2(hw, 110), false, 0, gs)) {
            gui.button("R1", vec2(0, rowH)); gui.button("R2", vec2(0, rowH)); gui.newline();
        }
        gui.endGroupBox();
        gui.newline();
    }

    gui.separator(8, axTr("Tab bar", "Sekme çubuğu"));
    gui.beginTabBar("c_tabs");
    if (gui.beginTabItem(axTr("First###c_t1", "Birinci###c_t1"), nullptr)) {
        for (int i = 1; i <= 5; i++) gui.button(TextFormat(axTr("Tab item %d", "Sekme öğesi %d"), i), vec2(0, rowH));
        gui.newline();
        gui.endTabItem();
    }
    if (gui.beginTabItem(axTr("Second###c_t2", "İkinci###c_t2"), nullptr)) {
        gui.label(axTr("Second tab", "İkinci sekme"), vec2(0, rowH)); gui.newline();
        gui.endTabItem();
    }
    gui.endTabBar();
    gui.newline();

    gui.separator(8, axTr("Window inside a window", "Pencere içinde pencere"));
    {
        vec2 cp(0, 0), cs(W, 96);
        gui.beginWindow(axTr("Inner window###c_inner_win", "İç pencere###c_inner_win"), cp, cs, WIN_TITLE_BAR | WIN_AUTO_NEWLINE);
        for (int i = 1; i <= 6; i++) gui.button(TextFormat("W%d", i), vec2(0, rowH));
        gui.newline();
        drawLayoutArea(gui, d, cColor(80, 220, 120, 120));
        gui.endWindow("###c_inner_win");
        gui.newline();
    }

    drawLayoutArea(gui, d);
    drawCursor(gui, d);
    gui.endWindow("###containers");
}

// ------------------------------------------------------------------ main
int main(int argc, char **argv)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    // --screenshot <file.png> [--frames N] [--light] [--size WxH] [--lang tr|en] [--no-overlay]
    const char *shotPath = nullptr;
    int shotFrames = 30;
    int winW = 1600, winH = 900;
    Demo d;
    axLang = AX_LANG_EN;
    for (int i = 1; i < argc; i++) {
        if (TextIsEqual(argv[i], "--screenshot") && i + 1 < argc) shotPath = argv[++i];
        else if (TextIsEqual(argv[i], "--frames") && i + 1 < argc) shotFrames = TextToInteger(argv[++i]);
        else if (TextIsEqual(argv[i], "--light")) d.dark = false;
        else if (TextIsEqual(argv[i], "--no-overlay")) d.overlay = d.showArea = d.showCursor = false;
        else if (TextIsEqual(argv[i], "--lang") && i + 1 < argc) axLang = TextIsEqual(argv[++i], "en") ? AX_LANG_EN : AX_LANG_TR;
        else if (TextIsEqual(argv[i], "--size") && i + 1 < argc) sscanf(argv[++i], "%dx%d", &winW, &winH);
    }
    int frame = 0;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
#ifdef PLATFORM_WEB
    winW = webInnerWidth(); winH = webInnerHeight();
#endif
    InitWindow(winW, winH, "axGUI layouts - raylib " RAYLIB_VERSION);
#ifndef PLATFORM_WEB
    SetWindowMinSize(800, 500);
#else
    SetExitKey(KEY_NULL);
#endif

    const float fontSize = 15;
    const bool rtOk = rtFonts().load(GetDirectoryPath(resPath("fonts/regular.ttf").c_str()));
    axFont *font = rtOk ? rtFonts().guiFont(fontSize) : nullptr;
    if (!font) {   // fallback: raylib default font
        font = new axFont();
        font->font = GetFontDefault();
        font->loaded = true;
        font->fontSize = 10;
    }

    PopupState popup{};
    vec2 popupPos;

    axDrawBot bot;
    axGUI gui;
    bot.init();
    gui.bot = &bot;
    if (rtOk) bot.fontForSize = [](float px) { return rtFonts().guiFont(px); };
    if (!gui.loadIconAtlas(resPath("icons.png").c_str()))
        TraceLog(LOG_WARNING, "LAYOUTS: resources/icons.png could not be loaded");
    Texture2D samplePhoto = makeSamplePhoto(bot);
    gui.setfont(font, fontSize);
    gui.itemSpaceX = 6;
    gui.itemSpaceY = 6;
    gui.btnRadius = 5;
    gui.frameRadius = 5;
    textcfg.borderWidth = 1.5f;
    textcfg.cornerRadius = 6;

    layoutWindows(gui, (float)GetScreenWidth(), (float)GetScreenHeight());
    bool darkApplied = !d.dark;
    vec2 lastSize(GetScreenWidth(), GetScreenHeight());

    while (!WindowShouldClose()) {
        if (!gui.wantCaptureKeyboard) {
            if (IsKeyPressed(KEY_F2)) d.overlay = !d.overlay;
            if (IsKeyPressed(KEY_F3)) d.dark = !d.dark;
            if (IsKeyPressed(KEY_F7)) axLang = axLang == AX_LANG_TR ? AX_LANG_EN : AX_LANG_TR;
        }
#ifndef PLATFORM_WEB
        if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();
#endif
        if (d.dark != darkApplied) {
            d.theme = d.dark ? themeDark() : themeLight();
            applyTheme(gui, d.theme);
            darkApplied = d.dark;
        }
        gui.debugLayout = d.overlay;

        vec2 sz(GetScreenWidth(), GetScreenHeight());
        if (sz != lastSize) { layoutWindows(gui, sz.x, sz.y); lastSize = sz; }

        BeginDrawing();
        ClearBackground(d.theme.bg);
        if (rtOk) { rtFonts().beginFrame(); font = rtFonts().guiFont(gui.fontSize); }
        gui.setfont(font, gui.fontSize);
        gui.startFrame(true);

        // --- top bar (no overlay here)
        {
            static vec2 bpos(0, 0), bsize(0, 44);
            bpos = vec2(0, 0); bsize = vec2(sz.x, 44);
            auto &ws = gui.windowStates["topbar"];
            if (ws.init) { ws.pos = bpos; ws.size = bsize; }
            const float r = gui.winRadius; gui.winRadius = 0;
            gui.debugLayout = false;
            gui.beginWindow("topbar", bpos, bsize, WIN_NO_MOVE);
            gui.winRadius = r;
            gui.label("axGUI layouts", vec2(120, 28));
            gui.toggleButton(axTr("Overlay", "Kaplama"), &d.overlay, vec2(0, 28), "F2");
            gui.toggleButton(axTr("Debug", "Debug"), &d.debug, vec2(0, 28), "F9");
            if (gui.button(d.dark ? axTr("Light theme", "Açık tema") : axTr("Dark theme", "Koyu tema"), vec2(0, 28), 0, "F3")) d.dark = !d.dark;
            if (gui.button(axLang == AX_LANG_TR ? "EN" : "TR", vec2(36, 28), 0, "F7"))
                axLang = axLang == AX_LANG_TR ? AX_LANG_EN : AX_LANG_TR;
            gui.label(TextFormat("%d FPS  |  %s", GetFPS(), d.status.c_str()), vec2(0, 28));
            gui.endWindow("topbar");
            gui.debugLayout = d.overlay;
            gui.debug = d.debug;
        }

        if (d.showSettings) settingsWindow(gui, d);
        if (d.showFlow) flowWindow(gui, d);
        if (d.showCursorWin) cursorWindow(gui, d);
        if (d.showRecipes) recipesWindow(gui, d);
        if (d.showContainers) containersWindow(gui, d);

        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            popup.open = true; popup.first = true;
            popupPos = vec2(GetMouseX(), GetMouseY());
        }
        const std::vector<cString> menu = { axTr("Center here", "Buraya ortala"), axTr("Toggle animation", "Animasyonu aç / kapat"), axTr("Change shape", "Şekli değiştir"), axTr("Random color", "Rengi rastgele yap") };
        int sel = gui.popupMenu(axTr("Canvas", "Tuval"), popup, popupPos, vec2(210, 40 + 30 * (float)menu.size()), 26, menu);

        gui.endFrame();
        EndDrawing();

        if (shotPath && ++frame >= shotFrames) {
            Image img = LoadImageFromScreen();
            ExportImage(img, shotPath);
            UnloadImage(img);
            break;
        }
    }

    gui.unloadImageFiles();
    bot.removeIcon("sample_photo");
    UnloadTexture(samplePhoto);
    bot.cleanup();
    if (!rtOk) delete font;
    CloseWindow();
    return 0;
}
