// axGUIRichText.cpp - axGUI::richTextBox: connects RichTextView + RichEditor to axGUI layout, input and clipping
#include "axGuiSystem.h"
#include "axRichFonts.h"
#include "axGUITextBox.h"     // textcfg (box colors)
#include "axGUIRichText.h"
#include <algorithm>
#include <cmath>

extern TextBoxConfig textcfg;   // axGUITextBox.cpp

void RichTextState::scrollToLine(int line)
{
    if (view.doc.reveal(line)) { view.invalidate(); changed = true; editor.dirty = true; }
    pendingScrollLine = line;
}

static bool isImageFile(const char *path)
{
    return IsFileExtension(path, ".png;.jpg;.jpeg;.bmp;.gif;.qoi;.PNG;.JPG;.JPEG;.BMP;.GIF");
}

bool axGUI::richTextBox(RichTextState &st, vec2 size)
{
    genID();
    const int id = ID;
    st.changed = false;
    st.editor.changedThisFrame = false;
    st.editor.readOnly = st.readOnly;
    st.hoverLink.clear();

    const bool autoW = size.x <= 0;
    if (size.x <= 0) size.x = getContainerWidth();
    if (size.y <= 0) size.y = 200;
    const vec2 pos = advanceCursor(size, autoW);
    if (!st.focused && outOfLayoutArea(pos, size)) return false;

    // font system: load on first use; DPI / atlas maintenance once per frame
    if (!rtFonts().loaded()) rtFonts().load(std::string(GetApplicationDirectory()) + "resources/fonts");
    {
        static long fontFrame = -1;
        if (fontFrame != frameCnt) { fontFrame = frameCnt; rtFonts().beginFrame(); }
    }

    const float sbW = 8.0f;                         // scrollbar strip
    RichTextView &view = st.view;
    RichEditor &ed = st.editor;

    view.layout(size.x - sbW);
    auto clampScroll = [&]() {
        st.contentH = view.contentHeight();
        float maxS = std::max(0.0f, st.contentH - size.y);
        st.scrollY = std::clamp(st.scrollY, 0.0f, maxS);
    };
    clampScroll();
    st.viewH = size.y;

    const bool hovered = buttonHovered(pos, size);
    const vec2 local = ms - winScroll - pos;        // inside the box
    auto docPoint = [&]() { return Vector2{ local.x, local.y + st.scrollY }; };
    const bool hasSB = st.contentH > size.y + 0.5f;
    const float thumbH = hasSB ? std::max(24.0f, size.y * size.y / st.contentH) : 0.0f;
    const float maxScroll = std::max(1.0f, st.contentH - size.y);
    const float thumbY = hasSB ? (size.y - thumbH) * (st.scrollY / maxScroll) : 0.0f;
    const bool overSB = hasSB && local.x >= size.x - sbW - 2;

    // ---- mouse
    if (msLeftPressed) {
        if (hovered) {
            focusID = id;
            proc = true;
            if (!st.focused) ed.blinkStart = GetTime();
            st.focused = true;
            if (overSB) {
                st.thumbDrag = true;
                st.thumbGrab = (local.y >= thumbY && local.y <= thumbY + thumbH) ? local.y - thumbY : thumbH * 0.5f;
            } else {
                double t = GetTime();
                bool near = fabsf(local.x - st.lastClickX) < 6 && fabsf(local.y - st.lastClickY) < 6;
                st.clickCount = (t - st.lastClickTime < textcfg.clickTimeWindow && near) ? std::min(3, st.clickCount + 1) : 1;
                st.lastClickTime = t;
                st.lastClickX = local.x; st.lastClickY = local.y;
                Vector2 dp = docPoint();
                int tog = view.toggleAt(dp, 4);
                int cb = tog < 0 ? view.checkboxAt(dp) : -1;
                std::string link = (tog < 0 && cb < 0) ? view.linkAt(dp) : std::string();
                if (tog >= 0) {
                    view.doc.setCollapsed(tog, !view.doc.isCollapsed(tog));
                    view.invalidate();
                    ed.dirty = true;
                    st.changed = true;
                    // if the cursor is in a collapsed section, move to the end of its heading
                    std::vector<int> own = view.doc.hiddenOwners();
                    if (ed.caret.line < (int)own.size() && own[(size_t)ed.caret.line] >= 0) {
                        int h = own[(size_t)ed.caret.line];
                        ed.caret = ed.anchor = RtPos{ h, (int)view.doc.lines[(size_t)h].chars.size() };
                    }
                } else if (cb >= 0 && !st.readOnly) {
                    ed.toggleChecked(cb);
                } else if (!link.empty() && (st.readOnly || ctrl || IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))) {
                    if (st.onLinkClick) st.onLinkClick(link);
                    else if (link[0] == '#') { int l = view.doc.anchorLine(link); if (l >= 0) st.scrollToLine(l); }
                    else if (link.rfind("http://", 0) == 0 || link.rfind("https://", 0) == 0 || link.rfind("mailto:", 0) == 0) OpenURL(link.c_str());
                } else {
                    bool sh = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
                    ed.mousePress(dp, sh, st.clickCount);
                    st.selecting = true;
                }
            }
        } else if (st.focused) {
            st.focused = false;
            st.selecting = false;
            ed.mouseRelease();
        }
    }
    if (st.thumbDrag) {
        if (msLeftDown && hasSB) {
            float t = (local.y - st.thumbGrab) / std::max(1.0f, size.y - thumbH);
            st.scrollY = std::clamp(t, 0.0f, 1.0f) * maxScroll;
            proc = true;
        } else st.thumbDrag = false;
    }
    if (st.selecting) {
        if (msLeftDown) {
            // auto-scroll when dragging outside the box
            float dt = GetFrameTime() * 60.0f;
            if (local.y < 0) st.scrollY -= std::min(40.0f, -local.y) * 0.5f * dt;
            else if (local.y > size.y) st.scrollY += std::min(40.0f, local.y - size.y) * 0.5f * dt;
            clampScroll();
            ed.mouseDrag(docPoint());
            proc = true;
        } else {
            st.selecting = false;
            ed.mouseRelease();
        }
    }
    if (hovered && msWheel != 0 && hasSB) {
        st.wheelScroll.add(st.scrollY, -msWheel * 48.0f);
        msWheel = 0;   // keep the window from scrolling too
    }
    if (st.wheelScroll.step(st.scrollY, 0.0f, std::max(0.0f, st.contentH - size.y), wheelSmooth, GetFrameTime()))
        clampScroll();

    // ---- image file drag and drop
    if (st.acceptDrops && !st.readOnly && hovered && IsFileDropped()) {
        FilePathList files = LoadDroppedFiles();
        bool any = false;
        for (unsigned i = 0; i < files.count; i++) {
            if (!isImageFile(files.paths[i])) continue;
            std::string uri = rtImageFileDataUri(files.paths[i], st.dropImageMaxPx);
            if (uri.empty()) continue;
            if (!any) { ed.mousePress(docPoint(), false, 1); ed.mouseRelease(); }
            ed.insertImageEmbed(uri);
            any = true;
        }
        UnloadDroppedFiles(files);
        if (any) { st.focused = true; focusID = id; }
    }

    // ---- keyboard
    if (st.focused) {
        ed.handleKeys();
        kbCaptureNext = true;
        if (IsKeyPressed(KEY_ESCAPE)) { ed.anchor = ed.caret; }
    }

    // ---- keep layout and cursor visible after a change / cursor movement
    if (ed.changedThisFrame || ed.caretMovedByKey) {
        view.layout(size.x - sbW);
        clampScroll();
        Rectangle c = view.caretRect(ed.caret);
        const float m = 8;
        if (c.y - m < st.scrollY) st.scrollY = c.y - m;
        else if (c.y + c.height + m > st.scrollY + size.y) st.scrollY = c.y + c.height + m - size.y;
        clampScroll();
    }
    if (st.pendingScrollLine >= 0) {
        view.layout(size.x - sbW);
        float y = view.lineTop(st.pendingScrollLine);
        if (y >= 0) st.scrollY = y - 8;
        st.pendingScrollLine = -1;
        clampScroll();
    }

    // ---- cursor type / tooltip
    if (hovered && !overSB) {
        Vector2 dp = docPoint();
        std::string link = view.linkAt(dp);
        st.hoverLink = link;
        if (view.toggleAt(dp, 4) >= 0 || (!st.readOnly && view.checkboxAt(dp) >= 0) ||
            (!link.empty() && (st.readOnly || IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))))
            cursorRequest = MOUSE_CURSOR_POINTING_HAND;
        else cursorRequest = MOUSE_CURSOR_IBEAM;
    }

    // ---- drawing
    const float bw = st.borderWidth;
    bot->roundedFRect(pos.x + bw, pos.y + bw, size.x - 2 * bw, size.y - 2 * bw, st.radius, textcfg.bgColorMulti, st.borderWidth,
                      st.focused ? textcfg.borderColorFocused : textcfg.borderColor);
    bot->save();
    bot->intersectScissorWorld(pos + vec2(st.borderWidth, st.borderWidth), size - vec2(2 * st.borderWidth, 2 * st.borderWidth));
    {
        RichTextView::DrawOpts o;
        o.showCaret = st.focused && !st.readOnly;
        o.caret = ed.caret; o.anchor = ed.anchor;
        o.blinkStart = ed.blinkStart;
        o.placeholder = st.placeholder;
        if (st.readOnly && st.focused && ed.hasSelection()) { o.showCaret = true; o.hideCaret = true; }   // show the selection, not the cursor
        // draws with raylib directly: in retained mode this runs in the window's layer
        Vector2 dp = { pos.x, pos.y - st.scrollY };
        Rectangle clip = { 0, st.scrollY, size.x, size.y };
        bot->deferred([&view, dp, clip, o]() { view.draw(dp, clip, o); });
    }
    bot->restore();
    if (hasSB) {
        float ty = (size.y - thumbH) * (st.scrollY / maxScroll);
        cColor tc = textcfg.textColor; tc.a = (st.thumbDrag || overSB) ? 110 : 60;
        bot->roundedFRect(pos.x + size.x - sbW + 1, pos.y + ty + 2, sbW - 4, thumbH - 4, (sbW - 4) * 0.5f, tc);
    }

    ItemState &item = itemStates[id];
    item.set("richText", pos + winScroll, size, hovered, st.focused, st.radius);

    st.changed = st.changed || ed.changedThisFrame;
    return st.changed;
}
