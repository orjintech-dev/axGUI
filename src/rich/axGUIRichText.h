// axGUIRichText.h - axGUI rich text box (axGUI::richTextBox) and its state
//
// Usage:
//   static RichTextState rt;                       // persistent state (non-copyable)
//   rt.loadDelta(json);                            // or setHtml / setPlainText
//   if (gui.richTextBox(rt, vec2(0, 300))) { ... } // true if the document changed this frame
//   rt.editor.toggleInline("bold");                // format buttons go through the editor
//   std::string saved = rt.deltaString();
//
// Mouse: click = cursor, drag = select, double click = word, triple click = line, Shift+click extends the selection; click on a checkbox /
// heading arrow; Ctrl+click opens a link (plain click when read-only). Wheel scrolls. Dropping an image file
// on the box inserts it.
// Keyboard: typing, Enter, Backspace / Delete (Ctrl: word), arrows / Home / End / PgUp / PgDn (Shift: select,
// Ctrl: word / document), Tab (indent in lists), Ctrl+A / C / X / V (Shift+V: plain), Ctrl+Z / Y (Shift+Z),
// Ctrl+B / I / U, Ctrl+Shift+X strikethrough, Ctrl+Shift+1..3 heading (0: paragraph), Ctrl+Shift+7 / 8 / 9
// numbered / bullet / to-do list, Ctrl+Shift+E code block.
#pragma once
#include "axRichText.h"
#include "axRichEdit.h"
#include "axRichHtml.h"
#include <functional>
#include <string>

struct RichTextState
{
    RichTextView view;
    RichEditor editor{ view };

    // --- settings
    bool readOnly = false;
    const char *placeholder = nullptr;      // placeholder text when the document is empty
    bool acceptDrops = true;                // image file drag and drop
    int dropImageMaxPx = 2048;              // maximum long edge of a dropped / pasted image
    float radius = 6, borderWidth = 1.5f;   // box (colors come from textcfg: bgColorMulti, borderColor...)
    // on link click (Ctrl+click; plain click when read-only). If empty: "#id" jumps to that heading in the document,
    // http(s) / mailto are opened with OpenURL.
    std::function<void(const std::string &)> onLinkClick;

    // --- state (managed by the widget)
    bool focused = false;
    bool changed = false;                   // document changed this frame
    float scrollY = 0;
    SmoothScroll wheelScroll;               // wheel scrolling (gui.wheelSmooth)
    std::string hoverLink;                  // link under the mouse (for the tooltip)

    RichTextState() { editor.reset(); }
    RichTextState(const RichTextState &) = delete;
    RichTextState &operator=(const RichTextState &) = delete;

    // --- content
    void loadDelta(const RtJson &ops) { view.setDelta(ops); editor.reset(); scrollY = 0; }
    bool loadDeltaString(const std::string &json) { bool ok = view.doc.fromDeltaString(json); view.invalidate(); editor.reset(); scrollY = 0; return ok; }
    RtJson delta() const { return view.doc.toDelta(); }
    std::string deltaString(int indent = -1) const { return view.doc.toDeltaString(indent); }
    bool setHtml(const std::string &html) { bool ok = RichDocHtml::import(view.doc, html, editor.fetchImage); view.invalidate(); editor.reset(); scrollY = 0; return ok; }
    std::string html() const { return RichDocHtml::exportHtml(view.doc, view.style()); }
    void setPlainText(const std::string &t) { view.doc.setPlainText(t); view.invalidate(); editor.reset(); scrollY = 0; }
    std::string plainText() const { return view.doc.plainText(); }
    void scrollToLine(int line);            // make the line visible (expands it if under a collapsed heading)

    // --- internal
    double lastClickTime = 0;
    int clickCount = 0;
    float lastClickX = 0, lastClickY = 0;
    bool selecting = false, thumbDrag = false;
    float thumbGrab = 0, viewH = 0, contentH = 0;
    int pendingScrollLine = -1;
};
