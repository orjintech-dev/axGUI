// axGUIStyle.cpp - style editor (similar to ImGui ShowStyleEditor)
// Sizes, colors, text box (textcfg) and tab bar (tabStyle) are edited live;
// saved to / loaded from JSON, copied to the clipboard as C++ lines.
#include "axGuiSystem.h"
#include "axGUITextBox.h"
#include <fstream>
#include <cstdio>

extern TextBoxConfig textcfg;   // axGUITextBox.cpp

namespace {

struct FloatItem { const char *group, *key, *label, *cpp; float *v; float mn, mx, speed; const char *fmt; };
struct ColorItem { const char *group, *key, *label; std::string cpp; cColor *c; };
struct Group     { const char *key, *en, *tr; const char *title() const { return axTr(en, tr); } };

const char *kColorKeys[GUI_COLOR_COUNT] = {
    "GUI_IDLE_COLOR", "GUI_HOVER_COLOR", "GUI_BTN_COLOR", "GUI_BTN_HOVER_COLOR", "GUI_BTN_SEL_COLOR",
    "GUI_BTN_TOGGLE_COLOR", "GUI_TEXT_COLOR", "GUI_TEXT_HL_COLOR", "GUI_TEXT_SEL_COLOR",
    "GUI_WIN_BACKCLR", "GUI_WIN_BORDERCLR", "GUI_TOOLTIP_BG", "GUI_TOOLTIP_TEXT" };
const char *kColorLabelsTR[GUI_COLOR_COUNT] = {
    "Zemin", "Üstüne gelince", "Düğme", "Düğme (üstünde)", "Düğme (seçili)",
    "Vurgu / anahtar", "Yazı", "Yazı (vurgulu)", "Yazı (seçili, bağlantı)",
    "Pencere zemini", "Pencere çerçevesi", "İpucu zemini", "İpucu yazısı" };
const char *kColorLabelsEN[GUI_COLOR_COUNT] = {
    "Background", "Hover", "Button", "Button (hover)", "Button (selected)",
    "Accent / toggle", "Text", "Text (highlight)", "Text (selected, link)",
    "Window background", "Window border", "Tooltip background", "Tooltip text" };

const Group kFloatGroups[] = { {"window", "Window", "Pencere"}, {"items", "Items", "Öğeler"}, {"behavior", "Behavior", "Davranış"},
                               {"textbox", "Text box", "Metin kutusu"}, {"tabs", "Tabs", "Sekmeler"} };
const Group kColorGroups[] = { {"gui", "Interface", "Arayüz"}, {"textbox", "Text box", "Metin kutusu"}, {"syntax", "Syntax", "Sözdizimi"}, {"tabs", "Tabs", "Sekmeler"} };

void collect(axGUI &g, std::vector<FloatItem> &F, std::vector<ColorItem> &C)
{
    TabBarStyle &t = g.tabStyle;
    F = {
        { "window", "radius",       axTr("Window rounding", "Pencere yuvarlaklığı"), "gui.winRadius",        &g.winRadius,  0, 24, 0.1f, "%.1f" },
        { "window", "paddingX",     axTr("Padding X", "İç boşluk X"),       "gui.winSpaceX",        &g.winSpaceX,  0, 32, 0.1f, "%.1f" },
        { "window", "paddingY",     axTr("Padding Y", "İç boşluk Y"),       "gui.winSpaceY",        &g.winSpaceY,  0, 32, 0.1f, "%.1f" },
        { "window", "titleBarH",    axTr("Title bar height", "Başlık yüksekliği"), "titleBarH",            &titleBarH,   16, 48, 0.1f, "%.0f" },
        { "items",  "spacingX",     axTr("Item spacing X", "Öğe aralığı X"),     "gui.itemSpaceX",       &g.itemSpaceX, 0, 24, 0.1f, "%.1f" },
        { "items",  "spacingY",     axTr("Item spacing Y", "Öğe aralığı Y"),     "gui.itemSpaceY",       &g.itemSpaceY, 0, 24, 0.1f, "%.1f" },
        { "items",  "radius",       axTr("Button rounding", "Düğme yuvarlaklığı"),     "gui.btnRadius",   &g.btnRadius,   0, 16, 0.1f, "%.1f" },
        { "items",  "frameRadius",  axTr("Frame rounding", "Giriş alanı yuvarlaklığı"), "gui.frameRadius", &g.frameRadius, 0, 16, 0.1f, "%.1f" },
        { "items",  "checkRadius",  axTr("Checkbox rounding", "Onay kutusu yuvarlaklığı"), "gui.checkRadius", &g.checkRadius, 0, 12, 0.1f, "%.1f" },
        { "items",  "textMargin",   axTr("Text left margin", "Yazı sol boşluğu"),  "gui.leftMargin",       &g.leftMargin, 0, 24, 0.1f, "%.1f" },
        { "items",  "fontSize",     axTr("Font size", "Yazı boyutu"),       "gui.fontSize",         &g.fontSize,   8, 32, 0.05f, "%.0f" },
        { "behavior", "collapse",   axTr("Collapse duration", "Katlanma süresi"),   "gui.collapseDuration", &g.collapseDuration, 0, 1, 0.005f, axTr("%.2f s", "%.2f sn") },
        { "behavior", "hintDelay",  axTr("Tooltip delay", "İpucu gecikmesi"), "gui.hintTime", &g.hintTime, 0, 2000, 2.0f, "%.0f ms" },
        { "behavior", "hover",      axTr("Hover transition", "Üstüne gelme geçişi"), "gui.hoverDuration", &g.hoverDuration, 0, 0.5f, 0.002f, axTr("%.2f s", "%.2f sn") },
        { "behavior", "wheelStep",  axTr("Wheel step", "Tekerlek adımı"),    "gui.wheelStep",        &g.wheelStep,  8, 200, 0.5f, "%.0f px" },
        { "behavior", "wheelLatch", axTr("Wheel latch", "Tekerlek kilidi"),   "gui.wheelLatchDelay",  &g.wheelLatchDelay, 0, 2, 0.005f, axTr("%.2f s", "%.2f sn") },
        { "textbox", "border",      axTr("Border width", "Çerçeve kalınlığı"), "textcfg.borderWidth",  &textcfg.borderWidth,  0, 6, 0.02f, "%.1f" },
        { "textbox", "radius",      axTr("Corner radius", "Köşe yarıçapı"),     "textcfg.cornerRadius", &textcfg.cornerRadius, 0, 16, 0.1f, "%.1f" },
        { "tabs", "height",         axTr("Tab height", "Sekme yüksekliği"),  "gui.tabStyle.tabHeight",       &t.tabHeight,       16, 48, 0.1f, "%.0f" },
        { "tabs", "paddingX",       axTr("Padding X", "İç boşluk X"),       "gui.tabStyle.tabPaddingH",     &t.tabPaddingH,      0, 24, 0.1f, "%.1f" },
        { "tabs", "spacing",        axTr("Tab spacing", "Sekme aralığı"),     "gui.tabStyle.tabSpacing",      &t.tabSpacing,       0, 16, 0.1f, "%.1f" },
        { "tabs", "radius",         axTr("Corner radius", "Köşe yarıçapı"),     "gui.tabStyle.tabBorderRadius", &t.tabBorderRadius,  0, 12, 0.1f, "%.1f" },
        { "tabs", "underline",      axTr("Underline", "Alt çizgi"),         "gui.tabStyle.underlineHeight", &t.underlineHeight,  0, 8,  0.05f, "%.1f" },
        { "tabs", "closeSize",      axTr("Close button", "Kapatma düğmesi"),   "gui.tabStyle.closeButtonSize", &t.closeButtonSize,  8, 24, 0.1f, "%.0f" },
    };
    C.clear();
    for (int i = 0; i < GUI_COLOR_COUNT; i++)
        C.push_back({ "gui", kColorKeys[i], axLang == AX_LANG_TR ? kColorLabelsTR[i] : kColorLabelsEN[i], std::string("gui.colors[") + kColorKeys[i] + "]", &g.colors[i] });
    auto add = [&](const char *grp, const char *key, const char *lbl, const char *cpp, cColor *c) { C.push_back({ grp, key, lbl, cpp, c }); };
    add("textbox", "bgSingle",    axTr("Background (single line)", "Zemin (tek satır)"),    "textcfg.bgColorSingle",        &textcfg.bgColorSingle);
    add("textbox", "bgMulti",     axTr("Background (multi-line)", "Zemin (çok satır)"),    "textcfg.bgColorMulti",         &textcfg.bgColorMulti);
    add("textbox", "border",      axTr("Border", "Çerçeve"),              "textcfg.borderColor",          &textcfg.borderColor);
    add("textbox", "borderFocus", axTr("Border (focused)", "Çerçeve (odakta)"),     "textcfg.borderColorFocused",   &textcfg.borderColorFocused);
    add("textbox", "text",        axTr("Text", "Yazı"),                 "textcfg.textColor",            &textcfg.textColor);
    add("textbox", "placeholder", axTr("Placeholder", "Yer tutucu"),           "textcfg.placeholderColor",     &textcfg.placeholderColor);
    add("textbox", "selection",   axTr("Selection", "Seçim"),                "textcfg.selectionColor",       &textcfg.selectionColor);
    add("textbox", "caret",       axTr("Caret", "İmleç"),                "textcfg.cursorColor",          &textcfg.cursorColor);
    add("textbox", "lineNumber",  axTr("Line number", "Satır numarası"),       "textcfg.lineNumberColor",      &textcfg.lineNumberColor);
    add("textbox", "currentLine", axTr("Current line", "Geçerli satır"),        "textcfg.currentLineHighlight", &textcfg.currentLineHighlight);
    add("syntax", "keyword",      axTr("Keyword", "Anahtar sözcük"),       "syntaxColors.keyword",         &syntaxColors.keyword);
    add("syntax", "type",         axTr("Type", "Tür"),                  "syntaxColors.type",            &syntaxColors.type);
    add("syntax", "string",       axTr("String", "Dizgi"),                "syntaxColors.string",          &syntaxColors.string);
    add("syntax", "comment",      axTr("Comment", "Yorum"),                "syntaxColors.comment",         &syntaxColors.comment);
    add("syntax", "number",       axTr("Number", "Sayı"),                 "syntaxColors.number",          &syntaxColors.number);
    add("syntax", "function",     axTr("Function", "Fonksiyon"),            "syntaxColors.function",        &syntaxColors.function);
    add("syntax", "preprocessor", axTr("Preprocessor", "Önişlemci"),            "syntaxColors.preprocessor",    &syntaxColors.preprocessor);
    add("syntax", "operator",     axTr("Operator", "İşleç"),                "syntaxColors.operatorColor",   &syntaxColors.operatorColor);
    add("syntax", "normal",       axTr("Other", "Diğer"),                "syntaxColors.normal",          &syntaxColors.normal);
    add("tabs", "bar",            axTr("Bar background", "Çubuk zemini"),         "gui.tabStyle.barBackground",   &t.barBackground);
    add("tabs", "normal",         axTr("Tab", "Sekme"),                "gui.tabStyle.tabNormal",       &t.tabNormal);
    add("tabs", "hovered",        axTr("Tab (hover)", "Sekme (üstünde)"),      "gui.tabStyle.tabHovered",      &t.tabHovered);
    add("tabs", "active",         axTr("Tab (active)", "Sekme (etkin)"),        "gui.tabStyle.tabActive",       &t.tabActive);
    add("tabs", "textNormal",     axTr("Text", "Yazı"),                 "gui.tabStyle.tabTextNormal",   &t.tabTextNormal);
    add("tabs", "textActive",     axTr("Text (active)", "Yazı (etkin)"),         "gui.tabStyle.tabTextActive",   &t.tabTextActive);
    add("tabs", "underline",      axTr("Underline", "Alt çizgi"),            "gui.tabStyle.underlineColor",  &t.underlineColor);
    add("tabs", "separator",      axTr("Separator", "Ayırıcı"),              "gui.tabStyle.separatorColor",  &t.separatorColor);
    add("tabs", "closeHovered",   axTr("Close (hover)", "Kapat (üstünde)"),      "gui.tabStyle.closeHovered",    &t.closeHovered);
}

using FloatList = std::vector<FloatItem>;
using ColorList = std::vector<ColorItem>;

nlohmann::json toJson(const FloatList &F, const ColorList &C)
{
    nlohmann::json j;
    for (auto &f : F) j["sizes"][f.group][f.key] = *f.v;
    for (auto &c : C) j["colors"][c.group][c.key] = { c.c->r, c.c->g, c.c->b, c.c->a };
    return j;
}

int fromJson(const FloatList &F, const ColorList &C, const nlohmann::json &j)
{
    int n = 0;
    auto find = [](const nlohmann::json &root, const char *a, const char *b, const char *c) -> const nlohmann::json * {
        auto i = root.find(a); if (i == root.end() || !i->is_object()) return nullptr;
        auto k = i->find(b);   if (k == i->end() || !k->is_object()) return nullptr;
        auto m = k->find(c);   return m == k->end() ? nullptr : &*m;
    };
    for (auto &f : F)
        if (auto v = find(j, "sizes", f.group, f.key); v && v->is_number()) { *f.v = fminf(f.mx, fmaxf(f.mn, v->get<float>())); n++; }
    for (auto &c : C)
        if (auto v = find(j, "colors", c.group, c.key); v && v->is_array() && v->size() >= 3) {
            auto ch = [&](size_t i, int d) { return i < v->size() && (*v)[i].is_number() ? std::max(0, std::min(255, (*v)[i].get<int>())) : d; };
            *c.c = cColor(ch(0, 0), ch(1, 0), ch(2, 0), ch(3, 255)); n++;
        }
    return n;
}

// Style window's own look: a fixed copy taken on first open (or by resetStyleWindowLook), so it is unaffected
// by the style being edited. The window is drawn with it; edited values are kept in a shadow copy
// (g_sf / g_sc) and written to the application after the window.
nlohmann::json g_look;
bool g_lookInit = false;
std::vector<float>  g_sf;       // shadow: sizes (same order as F)
std::vector<cColor> g_sc;       // shadow: colors (same order as C); the color picker writes to these addresses, so they must stay fixed

nlohmann::json g_ref;           // "Save reference" / "Undo"
bool g_refInit = false;
std::string g_msg;

void bindShadow(const FloatList &F, const ColorList &C, FloatList &SF, ColorList &SC)
{
    if (g_sf.size() != F.size()) g_sf.assign(F.size(), 0.0f);
    if (g_sc.size() != C.size()) g_sc.assign(C.size(), cColor(0));
    SF = F; SC = C;
    for (size_t i = 0; i < F.size(); i++) SF[i].v = &g_sf[i];
    for (size_t i = 0; i < C.size(); i++) SC[i].c = &g_sc[i];
}

void copyValues(const FloatList &from, const ColorList &fromC, const FloatList &to, const ColorList &toC)
{
    for (size_t i = 0; i < from.size(); i++) *to[i].v = *from[i].v;
    for (size_t i = 0; i < fromC.size(); i++) *toC[i].c = *fromC[i].c;
}

} // namespace

bool axGUI::saveStyle(const char *path)
{
    FloatList F; ColorList C; collect(*this, F, C);
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << toJson(F, C).dump(2);
    return (bool)f;
}

bool axGUI::loadStyle(const char *path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    nlohmann::json j = nlohmann::json::parse(f, nullptr, false);
    if (j.is_discarded()) return false;
    FloatList F; ColorList C; collect(*this, F, C);
    int n = fromJson(F, C, j);
    setFontsize(fontSize);
    return n > 0;
}

std::string axGUI::styleToCpp()
{
    FloatList F; ColorList C; collect(*this, F, C);
    std::string s = "// axGUI stili\n";
    char buf[160];
    for (auto &f : F) { snprintf(buf, sizeof buf, "%s = %g;\n", f.cpp, *f.v); s += buf; }
    for (auto &c : C) {
        if (c.c->a == 255) snprintf(buf, sizeof buf, "%s = cColor(%d, %d, %d);\n", c.cpp.c_str(), c.c->r, c.c->g, c.c->b);
        else snprintf(buf, sizeof buf, "%s = cColor(%d, %d, %d, %d);\n", c.cpp.c_str(), c.c->r, c.c->g, c.c->b, c.c->a);
        s += buf;
    }
    return s;
}

void axGUI::resetStyleWindowLook()
{
    FloatList F; ColorList C; collect(*this, F, C);
    g_look = toJson(F, C);
    g_lookInit = true;
}

// F / C: values being edited (live style or shadow copy)
static void styleEditorBody(axGUI &g, const FloatList &F, const ColorList &C)
{
    if (!g_refInit) { g_ref = toJson(F, C); g_refInit = true; }

    const float W = g.getContainerWidth();
    const float rowH = fmaxf(24.0f, roundf(g.fontSize * 1.75f));
    const float labW = fminf(190.0f, roundf(W * 0.40f));

    if (g.button(axTr("Save reference###ref", "Referansı kaydet###ref"), vec2(0, rowH), 0, axTr("Revert returns here", "Geri al bu duruma döner"))) { g_ref = toJson(F, C); g_msg = axTr("Reference saved", "Referans kaydedildi"); }
    if (g.button(axTr("Revert###rev", "Geri al###rev"), vec2(0, rowH), 0, axTr("Back to the last reference", "Son referansa dön"))) { fromJson(F, C, g_ref); g_msg = axTr("Reverted to reference", "Referansa dönüldü"); }
    g.newline();
    const std::string &file = g.styleFile;
    if (g.button(axTr("Save", "Kaydet"), vec2(0, rowH), 0, file.c_str())) {
        std::ofstream f(file, std::ios::binary);
        bool ok = (bool)f && (f << toJson(F, C).dump(2));
        g_msg = (ok ? axTr("Saved: ", "Kaydedildi: ") : axTr("Could not save: ", "Kaydedilemedi: ")) + file;
    }
    if (g.button(axTr("Load", "Yükle"), vec2(0, rowH), 0, file.c_str())) {
        std::ifstream f(file, std::ios::binary);
        nlohmann::json j = f ? nlohmann::json::parse(f, nullptr, false) : nlohmann::json();
        bool ok = f && !j.is_discarded() && fromJson(F, C, j) > 0;
        g_msg = (ok ? axTr("Loaded: ", "Yüklendi: ") : axTr("Could not load: ", "Yüklenemedi: ")) + file;
    }
    if (g.button(axTr("Copy C++", "C++ kopyala"), vec2(0, rowH), 0, axTr("Lines that set up this style, to the clipboard", "Bu stili kuran satırlar panoya"))) {
        std::string s = "// axGUI stili\n"; char buf[160];
        for (auto &f : F) { snprintf(buf, sizeof buf, "%s = %g;\n", f.cpp, *f.v); s += buf; }
        for (auto &c : C) {
            if (c.c->a == 255) snprintf(buf, sizeof buf, "%s = cColor(%d, %d, %d);\n", c.cpp.c_str(), c.c->r, c.c->g, c.c->b);
            else snprintf(buf, sizeof buf, "%s = cColor(%d, %d, %d, %d);\n", c.cpp.c_str(), c.c->r, c.c->g, c.c->b, c.c->a);
            s += buf;
        }
        SetClipboardText(s.c_str()); g_msg = axTr("C++ lines copied to clipboard", "C++ satırları panoda");
    }
    g.newline();
    if (!g_msg.empty()) { g.label(g_msg.c_str(), vec2(0, rowH)); g.newline(); }

    g.beginTabBar("##axgui_style");
    if (g.beginTabItem(axTr("Sizes###sizes", "Boyutlar###sizes"), nullptr)) {
        for (auto &grp : kFloatGroups) {
            // groups are collapsible sections; ID is language-independent (###)
            std::string sec = std::string(grp.title()) + "###sz_" + grp.key;
            if (!g.beginSection(sec.c_str())) continue;
            for (auto &f : F) {
                if (strcmp(f.group, grp.key) != 0) continue;
                g.label(f.label, vec2(labW, rowH));
                g.dragFloat(vec2(fminf(220.0f, W - labW - g.itemSpaceX), rowH), f.mn, f.mx, f.v, *f.v, f.speed, f.fmt, false, f.cpp);
                g.newline();
            }
            g.endSection();
        }
        g.endTabItem();
    }
    if (g.beginTabItem(axTr("Colors###colors", "Renkler###colors"), nullptr)) {
        const float aW = 52;
        // dragColor has 3 fields: width + one item spacing; leave room for the scrollbar on the right
        const float cW = fmaxf(120.0f, fminf(210.0f, W - labW - rowH - aW - 6 * g.itemSpaceX - 12));
        for (auto &grp : kColorGroups) {
            std::string sec = std::string(grp.title()) + "###cl_" + grp.key;
            if (!g.beginSection(sec.c_str())) continue;
            for (auto &c : C) {
                if (strcmp(c.group, grp.key) != 0) continue;
                g.label(c.label, vec2(labW, rowH));
                g.colorButton(vec2(rowH, rowH), vec2(rowH * 0.78f, rowH * 0.78f), c.c, 1, g.colors[GUI_WIN_BORDERCLR], c.cpp.c_str());
                g.dragColor(vec2(cW, rowH), *c.c, 1.0f, "%.0f");
                g.dragInt(vec2(aW, rowH), 0, 255, &c.c->a, 1.0f, false, axTr("Opacity (A)", "Saydamlık (A)"));
                g.newline();
            }
            g.endSection();
        }
        g.endTabItem();
    }
    g.endTabBar();
}

void axGUI::styleEditor()
{
    FloatList F; ColorList C; collect(*this, F, C);
    const float fontBefore = fontSize;
    styleEditorBody(*this, F, C);
    if (fontSize != fontBefore) setFontsize(fontSize);
}

void axGUI::showStyleWindow(bool *open)
{
    if (open && !*open) return;
    FloatList F; ColorList C; collect(*this, F, C);       // live style
    FloatList SF; ColorList SC; bindShadow(F, C, SF, SC);  // shadow copy: the editor writes to it
    copyValues(F, C, SF, SC);                              // live -> shadow
    if (!g_lookInit) { g_look = toJson(F, C); g_lookInit = true; }
    fromJson(F, C, g_look);                                // draw the window with the fixed look
    setFontsize(fontSize);

    vec2 &pos = styleWinPos, &size = styleWinSize;
    if (pos.x < 0) pos = vec2(fmaxf(16.0f, GetScreenWidth() - size.x - 24.0f), 64);
    beginWindow(axTr("Style###axgui_style", "Stil###axgui_style"), pos, size, WinFlags(WIN_TITLE_BAR | WIN_SCROLLBAR | WIN_RESIZABLE));
    if (open) {
        if (button(axTr("Close", "Kapat"), vec2(0, fmaxf(24.0f, roundf(fontSize * 1.75f))))) *open = false;
        newline();
    }
    styleEditorBody(*this, SF, SC);
    endWindow("###axgui_style");

    copyValues(SF, SC, F, C);                              // shadow -> live (apply edits)
    setFontsize(fontSize);
}
