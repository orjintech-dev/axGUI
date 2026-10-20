// axRichEdit.cpp - ported from NotePen richedit.cpp; adds undo / redo and read-only mode
#include "axRichEdit.h"
#include "axRichHtml.h"
#include "axRichClip.h"
#include <unordered_map>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cmath>

// in-app rich clipboard (pasted with formatting if it matches the plain text on the system clipboard)
static std::vector<RtLine> g_richClip;
static std::vector<RtJson> g_richClipEmbeds;
static std::vector<RtJson> g_richClipAttrs;   // RtChar::attr in the internal copy indexes this table (not the source document's id)
static std::string g_richClipText;

static bool ctrlDown() { return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER); }
static bool shiftDown() { return IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT); }
static bool pressed(int k) { return IsKeyPressed(k) || IsKeyPressedRepeat(k); }

static void encodeUtf8(int cp, std::string &out)
{
    if (cp < 0x80) out += (char)cp;
    else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
    else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
}

void RichEditor::reset()
{
    caret = anchor = { 0, 0 };
    dirty = false;
    dragging_ = false;
    prefX_ = -1;
    pendingAttr_ = -1;
    history_.clear();
    hIndex_ = -1;
    pushHistory(0);
}

// ------------------------------------------------------------------ undo / redo
void RichEditor::pushHistory(int kind)
{
    double now = GetTime();
    // consecutive typing (within 1 s) is a single step
    if (kind == 1 && hIndex_ >= 0 && hIndex_ == (int)history_.size() - 1 && hIndex_ > 0 &&
        history_[(size_t)hIndex_].kind == 1 && now - history_[(size_t)hIndex_].time < 1.0) {
        history_.pop_back();
        hIndex_--;
    }
    if (hIndex_ + 1 < (int)history_.size()) history_.resize((size_t)hIndex_ + 1);
    Snapshot s;
    s.lines = doc().lines;
    if (!history_.empty() && history_.back().embedsVersion == doc().embedsVersion) s.embeds = history_.back().embeds;
    else s.embeds = std::make_shared<const std::vector<RtJson>>(doc().embeds);
    s.embedsVersion = doc().embedsVersion;
    s.caret = caret; s.anchor = anchor;
    s.kind = kind; s.time = now;
    history_.push_back(std::move(s));
    if ((int)history_.size() > std::max(2, historyLimit)) history_.erase(history_.begin());
    hIndex_ = (int)history_.size() - 1;
}

void RichEditor::restore(const Snapshot &s)
{
    doc().lines = s.lines;
    if (doc().embedsVersion != s.embedsVersion) { doc().embeds = *s.embeds; doc().embedsVersion = s.embedsVersion; }
    caret = s.caret; anchor = s.anchor;
    pendingAttr_ = -1;
    prefX_ = -1;
    dirty = true;
    changedThisFrame = true;
    caretMovedByKey = true;
    lastEdit = blinkStart = GetTime();
    view_.invalidate();
    clampCaret();
}

void RichEditor::undo()
{
    if (readOnly || !canUndo()) return;
    hIndex_--;
    restore(history_[(size_t)hIndex_]);
}

void RichEditor::redo()
{
    if (readOnly || !canRedo()) return;
    hIndex_++;
    restore(history_[(size_t)hIndex_]);
}

std::string RichEditor::selectedText() const
{
    return hasSelection() ? doc().plainText(selStart(), selEnd()) : std::string();
}

void RichEditor::clampCaret()
{
    caret = doc().clamp(caret);
    anchor = doc().clamp(anchor);
}

void RichEditor::changed()
{
    dirty = true;
    changedThisFrame = true;
    lastEdit = GetTime();
    blinkStart = lastEdit;
    view_.invalidate();
    clampCaret();
    pushHistory(editKind_);
    editKind_ = 0;
}

void RichEditor::moveTo(RtPos p, bool extend, bool keepPrefX)
{
    caret = doc().clamp(p);
    if (!extend) anchor = caret;
    if (!keepPrefX) prefX_ = -1;
    pendingAttr_ = -1;
    caretMovedByKey = true;
    blinkStart = GetTime();
}

// ------------------------------------------------------------------ mouse
void RichEditor::mousePress(Vector2 pt, bool shift, int clicks)
{
    RtPos p = view_.hitTest(pt);
    pendingAttr_ = -1;
    prefX_ = -1;
    blinkStart = GetTime();
    dragging_ = true;
    dragMode_ = std::clamp(clicks, 1, 3);
    if (dragMode_ == 1) {
        caret = p;
        if (!shift) anchor = p;
        dragOrigin0_ = dragOrigin1_ = anchor;
    } else if (dragMode_ == 2) {
        RtPos a = doc().wordStart(doc().nextPos(p)), b = doc().wordEnd(p);
        if (a.line != p.line) a = { p.line, 0 };
        if (b.line != p.line) b = { p.line, (int)doc().lines[p.line].chars.size() };
        if (p.col == 0) a = p;
        anchor = a; caret = b;
        dragOrigin0_ = a; dragOrigin1_ = b;
    } else {
        anchor = { p.line, 0 };
        caret = doc().nextPos({ p.line, (int)doc().lines[p.line].chars.size() });
        if (caret.line == p.line) caret = { p.line, (int)doc().lines[p.line].chars.size() };
        dragOrigin0_ = anchor; dragOrigin1_ = caret;
    }
}

void RichEditor::mouseDrag(Vector2 pt)
{
    if (!dragging_) return;
    RtPos p = view_.hitTest(pt);
    if (dragMode_ == 1) { caret = p; return; }
    // extend the selection in word/line mode
    if (p < dragOrigin0_) { anchor = dragOrigin1_; caret = dragMode_ == 2 ? doc().wordStart(doc().nextPos(p)) : RtPos{ p.line, 0 }; }
    else { anchor = dragOrigin0_; caret = dragMode_ == 2 ? doc().wordEnd(p) : RtPos{ p.line, (int)doc().lines[p.line].chars.size() }; }
}

// ------------------------------------------------------------------ editing
void RichEditor::deleteSelection()
{
    if (readOnly) return;
    if (!hasSelection()) return;
    RtPos a = selStart(), b = selEnd();
    caret = anchor = doc().erase(a, b);
    changed();
}

void RichEditor::insertTyped(const std::string &s)
{
    if (readOnly) return;
    // single-character typing is merged (undo is one step)
    int attr = pendingAttr_ >= 0 ? pendingAttr_ : doc().attrAt(hasSelection() ? selStart() : caret);
    if (pendingAttr_ < 0 && !hasSelection() && doc().attr(attr).contains("link")) {
        // text typed at the end of a link does not join the link (like Quill); in the middle it does
        const auto &ch = doc().lines[caret.line].chars;
        bool inside = caret.col < (int)ch.size() && ch[caret.col].attr == attr;
        if (!inside) attr = doc().attrWith(attr, "link", RtJson());
    }
    bool hadSel = hasSelection();
    deleteSelection();
    caret = anchor = doc().insertText(caret, s, attr);
    prefX_ = -1;
    editKind_ = (!hadSel && s.find('\n') == std::string::npos && s.size() <= 8) ? 1 : 0;
    changed();
}

void RichEditor::newline()
{
    if (readOnly) return;
    deleteSelection();
    RtLine &L = doc().lines[caret.line];
    std::string list = L.block.contains("list") && L.block["list"].is_string() ? L.block["list"].get<std::string>() : "";
    // Enter on an empty list item: leave the list
    if (!list.empty() && L.chars.empty()) {
        doc().formatBlock(caret.line, caret.line, "list", nullptr);
        changed();
        return;
    }
    bool atEnd = caret.col == (int)L.chars.size();
    bool wasHeader = L.block.contains("header");
    // Enter at the end of a collapsed heading: the new paragraph goes after the section, not inside the hidden part
    if (wasHeader && atEnd && doc().isCollapsed(caret.line)) {
        int e = doc().sectionEnd(caret.line);
        doc().lines.insert(doc().lines.begin() + e + 1, RtLine());
        caret = anchor = RtPos{ e + 1, 0 };
        prefX_ = -1;
        changed();
        return;
    }
    RtPos np = doc().splitLine(caret);
    RtLine &N = doc().lines[np.line];
    if (wasHeader && atEnd) N.block.erase("header");           // normal paragraph after a heading
    if (list == "checked") N.block["list"] = "unchecked";       // new item without a bullet
    caret = anchor = np;
    prefX_ = -1;
    changed();
}

void RichEditor::backspace(bool word)
{
    if (readOnly) return;
    if (hasSelection()) { deleteSelection(); return; }
    if (caret.col == 0) {
        RtLine &L = doc().lines[caret.line];
        // at line start: remove the line format first (Quill behavior)
        if (L.block.contains("list")) { L.block.erase("list"); changed(); return; }
        if (L.block.contains("indent")) {
            int ind = L.block["indent"].is_number() ? L.block["indent"].get<int>() : 0;
            if (ind > 1) L.block["indent"] = ind - 1; else L.block.erase("indent");
            changed(); return;
        }
        if (L.block.contains("blockquote") || L.block.contains("code-block") || L.block.contains("header")) {
            L.block.erase("blockquote"); L.block.erase("code-block"); L.block.erase("header");
            changed(); return;
        }
        if (caret.line == 0) return;
    }
    RtPos from = word ? doc().wordStart(caret) : doc().prevPos(caret);
    caret = anchor = doc().erase(from, caret);
    prefX_ = -1;
    changed();
}

void RichEditor::del(bool word)
{
    if (readOnly) return;
    if (hasSelection()) { deleteSelection(); return; }
    RtPos to = word ? doc().wordEnd(caret) : doc().nextPos(caret);
    if (to == caret) return;
    caret = anchor = doc().erase(caret, to);
    changed();
}

void RichEditor::selectAll()
{
    anchor = { 0, 0 };
    caret = doc().endPos();
}

static void putUtf8(std::string &o, unsigned c) { encodeUtf8((int)c, o); }

void RichEditor::copy(bool cut)
{
    if (!hasSelection()) return;
    g_richClip = doc().extract(selStart(), selEnd());
    g_richClipEmbeds = doc().embeds;
    // attribute ids are per-document: keep them with their JSON, re-registered in the target document on paste
    {
        g_richClipAttrs.clear();
        std::unordered_map<int, int> remap;
        for (auto &l : g_richClip)
            for (auto &ch : l.chars) {
                auto it = remap.find(ch.attr);
                if (it == remap.end()) {
                    int idx = (int)g_richClipAttrs.size();
                    g_richClipAttrs.push_back(doc().attr(ch.attr));
                    it = remap.emplace(ch.attr, idx).first;
                }
                ch.attr = it->second;
            }
    }
    // plain text to the clipboard: images as "[image]" (so the clipboard is not empty when only an image is selected;
    // when pasting, if this text matches the internal copy, the image itself is pasted)
    {
        RtPos a = doc().clamp(selStart()), b = doc().clamp(selEnd());
        std::string out;
        for (int l = a.line; l <= b.line; l++) {
            const auto &ch = doc().lines[l].chars;
            int s = (l == a.line) ? a.col : 0, e = (l == b.line) ? b.col : (int)ch.size();
            for (int i = s; i < e; i++) {
                if (ch[i].embed < 0) putUtf8(out, (unsigned)ch[i].cp);
                else {
                    const RtJson &em = ch[i].embed < (int)doc().embeds.size() ? doc().embeds[ch[i].embed] : RtJson();
                    out += em.is_object() && em.contains("image") ? view_.imagePlaceholder : std::string("[embed]");
                }
            }
            if (l != b.line) out += '\n';
        }
        g_richClipText = out;
    }
    // plain text + formatted HTML to the clipboard (Google Docs, LibreOffice, Word, email accept the formatting)
    rtSetClipboardHtml(rtLinesToHtml(g_richClip, g_richClipAttrs, g_richClipEmbeds, view_.style()), g_richClipText);
    if (cut && !readOnly) deleteSelection();
}

void RichEditor::paste()
{
    if (readOnly) return;
    const char *c = rtClipboardHasText() ? GetClipboardText() : nullptr;
    std::string text = c ? c : "";
    if (rtClipboardTextLooksBinary(text)) text.clear();
    text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
    if (!text.empty() && !g_richClip.empty() && text == g_richClipText) {
        deleteSelection();
        // move the attribute/embed ids of the rich fragment into this document
        std::vector<RtLine> frag = g_richClip;
        for (auto &l : frag)
            for (auto &ch : l.chars) {
                ch.attr = ch.attr >= 0 && ch.attr < (int)g_richClipAttrs.size() ? doc().internAttr(g_richClipAttrs[(size_t)ch.attr]) : 0;
                if (ch.embed >= 0) {
                    RtJson e = ch.embed < (int)g_richClipEmbeds.size() ? g_richClipEmbeds[ch.embed] : RtJson::object();
                    ch.embed = doc().addEmbed(e);
                }
            }
        caret = anchor = doc().insertLines(caret, frag);
        changed();
        return;
    }
    // clipboard has a single link + image ("Copy image" in a browser): prefer the image
    {
        bool loneUrl = (text.compare(0, 7, "http://") == 0 || text.compare(0, 8, "https://") == 0) &&
                       text.find_first_of(" \n\t") == std::string::npos;
        if (loneUrl && onPasteImage && onPasteImage()) return;
    }
    // formatted copy from outside (browser, Word, Gmail...): convert the HTML to lines
    std::string html = rtClipboardHtml();
    if (!html.empty() && !rtClipboardTextLooksBinary(html)) {
        std::vector<RtLine> frag = rtHtmlToLines(html, doc(), fetchImage);
        if (!frag.empty()) {
            deleteSelection();
            // the first piece is inserted into the cursor's line: if it is a plain paragraph, keep that line's format (list, heading);
            // if it is a formatted line (heading, bullet...) and the cursor is mid-line, start on a new line
            RtPos at = doc().clamp(caret);
            const RtJson &curBlock = doc().lines[at.line].block;
            if (frag.size() > 1 && !frag[0].block.empty() && at.col > 0) {
                RtLine head;
                head.block = curBlock;
                frag.insert(frag.begin(), head);
            } else if (frag.size() > 1 && frag[0].block.empty()) frag[0].block = curBlock;
            caret = anchor = doc().insertLines(caret, frag);
            prefX_ = -1;
            changed();
            return;
        }
    }
    if (!text.empty()) insertTyped(text);
    else if (onPasteImage) onPasteImage();   // image only (screenshot, "Copy image")
    else {
        // default: raylib reads the clipboard image (Windows); inserted as a PNG data: URI
        Image img = GetClipboardImage();
        if (img.data) {
            std::string uri = rtImageDataUri(img);
            UnloadImage(img);
            if (!uri.empty()) insertImageEmbed(uri);
        }
    }
}

void RichEditor::insertImageEmbed(const std::string &uri, int width)
{
    if (readOnly || uri.empty()) return;
    deleteSelection();
    RtJson emb = RtJson::object();
    emb["image"] = uri;
    RtLine img;
    RtChar c; c.cp = 0xFFFC; c.embed = doc().addEmbed(emb);
    if (width > 0) { RtJson wa = RtJson::object(); wa["width"] = std::to_string(width); c.attr = doc().internAttr(wa); }
    img.chars.push_back(c);
    // [start of cursor's line] [image] [rest of line]; if at line start, do not leave an empty line above
    std::vector<RtLine> frag;
    RtPos at = doc().clamp(caret);
    if (at.col > 0) frag.push_back(RtLine());
    else img.block = RtJson::object();
    frag.push_back(img);
    frag.push_back(RtLine());
    if (at.col > 0) frag[0].block = doc().lines[at.line].block;
    caret = anchor = doc().insertLines(caret, frag);
    prefX_ = -1;
    changed();
}

void RichEditor::pastePlain()
{
    if (readOnly) return;
    const char *c = rtClipboardHasText() ? GetClipboardText() : nullptr;
    std::string text = c ? c : "";
    text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
    if (!text.empty()) insertTyped(text);
}

// ------------------------------------------------------------------ format queries
RtJson RichEditor::inlineValue(const std::string &key) const
{
    int a;
    if (pendingAttr_ >= 0) a = pendingAttr_;
    else if (hasSelection()) {
        RtPos s = selStart();
        const auto &ch = doc().lines[s.line].chars;
        a = s.col < (int)ch.size() ? ch[s.col].attr : doc().attrAt(s);
    } else a = doc().attrAt(caret);
    const RtJson &j = doc().attr(a);
    return j.contains(key) ? j[key] : RtJson();
}

bool RichEditor::inlineActive(const std::string &key) const
{
    RtJson v = inlineValue(key);
    return !v.is_null() && !(v.is_boolean() && !v.get<bool>());
}

void RichEditor::setInline(const std::string &key, const RtJson &value)
{
    if (readOnly) return;
    if (hasSelection()) {
        doc().formatInline(selStart(), selEnd(), key, value);
        changed();
    } else {
        int base = pendingAttr_ >= 0 ? pendingAttr_ : doc().attrAt(caret);
        pendingAttr_ = doc().attrWith(base, key, value);
    }
}

void RichEditor::toggleInline(const std::string &key)
{
    setInline(key, inlineActive(key) ? RtJson() : RtJson(true));
}

int RichEditor::headerLevel() const
{
    const RtJson &b = doc().lines[doc().clamp(caret).line].block;
    return b.contains("header") && b["header"].is_number() ? b["header"].get<int>() : 0;
}

void RichEditor::setHeader(int level)
{
    if (readOnly) return;
    doc().formatBlock(selStart().line, selEnd().line, "header", level > 0 ? RtJson(level) : RtJson());
    changed();
}

std::string RichEditor::listType() const
{
    const RtJson &b = doc().lines[doc().clamp(caret).line].block;
    return b.contains("list") && b["list"].is_string() ? b["list"].get<std::string>() : "";
}

void RichEditor::toggleList(const std::string &type)
{
    if (readOnly) return;
    std::string cur = listType();
    bool isCheck = type == "checked" || type == "unchecked";
    bool curCheck = cur == "checked" || cur == "unchecked";
    bool remove = (cur == type) || (isCheck && curCheck);
    doc().formatBlock(selStart().line, selEnd().line, "list", remove ? RtJson() : RtJson(type));
    changed();
}

std::string RichEditor::align() const
{
    const RtJson &b = doc().lines[doc().clamp(caret).line].block;
    return b.contains("align") && b["align"].is_string() ? b["align"].get<std::string>() : "";
}

void RichEditor::setAlign(const std::string &a)
{
    if (readOnly) return;
    doc().formatBlock(selStart().line, selEnd().line, "align", (a.empty() || a == "left") ? RtJson() : RtJson(a));
    changed();
}

bool RichEditor::blockActive(const std::string &key) const
{
    const RtJson &b = doc().lines[doc().clamp(caret).line].block;
    return b.contains(key) && !b[key].is_null() && !(b[key].is_boolean() && !b[key].get<bool>());
}

void RichEditor::toggleBlock(const std::string &key)
{
    if (readOnly) return;
    bool on = blockActive(key);
    int l0 = selStart().line, l1 = selEnd().line;
    if (!on && key == "code-block") doc().formatBlock(l0, l1, "blockquote", RtJson());   // one or the other
    if (!on && key == "blockquote") doc().formatBlock(l0, l1, "code-block", RtJson());
    doc().formatBlock(l0, l1, key, on ? RtJson() : RtJson(true));
    changed();
}

void RichEditor::toggleChecked(int line)
{
    if (readOnly || line < 0 || line >= (int)doc().lines.size()) return;
    RtJson &b = doc().lines[(size_t)line].block;
    std::string l = b.contains("list") && b["list"].is_string() ? b["list"].get<std::string>() : "";
    if (l != "checked" && l != "unchecked") return;
    b["list"] = l == "checked" ? "unchecked" : "checked";
    changed();
}

bool RichEditor::linkRange(RtPos &a, RtPos &b, std::string &url) const
{
    RtPos p = doc().clamp(caret);
    const auto &ch = doc().lines[p.line].chars;
    auto linkOf = [&](int col) -> std::string {
        if (col < 0 || col >= (int)ch.size()) return std::string();
        const RtJson &j = doc().attr(ch[col].attr);
        return j.contains("link") && j["link"].is_string() ? j["link"].get<std::string>() : std::string();
    };
    int c = p.col;
    std::string u = linkOf(c);
    if (u.empty()) { c = p.col - 1; u = linkOf(c); }
    if (u.empty()) return false;
    int s = c, e = c;
    while (s > 0 && linkOf(s - 1) == u) s--;
    while (e + 1 < (int)ch.size() && linkOf(e + 1) == u) e++;
    a = { p.line, s }; b = { p.line, e + 1 };
    url = u;
    return true;
}

std::string RichEditor::currentLink() const
{
    if (hasSelection()) {
        RtJson v = inlineValue("link");
        return v.is_string() ? v.get<std::string>() : std::string();
    }
    RtPos a, b;
    std::string u;
    return linkRange(a, b, u) ? u : std::string();
}

void RichEditor::applyLink(const std::string &url)
{
    if (readOnly) return;
    RtJson v = url.empty() ? RtJson() : RtJson(url);
    if (hasSelection()) { doc().formatInline(selStart(), selEnd(), "link", v); changed(); return; }
    RtPos a, b;
    std::string u;
    if (linkRange(a, b, u)) { doc().formatInline(a, b, "link", v); changed(); return; }
    if (url.empty()) return;
    int base = doc().attrAt(caret);
    pendingAttr_ = doc().attrWith(base, "link", url);
    insertTyped(url);
    pendingAttr_ = doc().attrWith(base, "link", RtJson());   // no link should follow
}

float RichEditor::lineHeight() const
{
    return rtLineHeightAttr(doc().lines[doc().clamp(caret).line].block);
}

void RichEditor::setLineHeight(float v)
{
    if (readOnly) return;
    doc().formatBlock(selStart().line, selEnd().line, "line-height", v > 0 ? RtJson(v) : RtJson());
    changed();
}

void RichEditor::indent(int delta)
{
    if (readOnly) return;
    for (int l = selStart().line; l <= selEnd().line; l++) {
        RtJson &b = doc().lines[l].block;
        int ind = b.contains("indent") && b["indent"].is_number() ? b["indent"].get<int>() : 0;
        ind = std::clamp(ind + delta, 0, 8);
        if (ind == 0) b.erase("indent"); else b["indent"] = ind;
    }
    changed();
}

void RichEditor::clearFormatting()
{
    if (readOnly) return;
    if (hasSelection()) {
        RtPos a = selStart(), b = selEnd();
        for (int l = a.line; l <= b.line; l++) {
            auto &ch = doc().lines[l].chars;
            int s = (l == a.line) ? a.col : 0;
            int e = (l == b.line) ? b.col : (int)ch.size();
            for (int i = s; i < e; i++) ch[i].attr = 0;
        }
    } else pendingAttr_ = 0;
    for (int l = selStart().line; l <= selEnd().line; l++) doc().lines[l].block = RtJson::object();
    changed();
}

// ------------------------------------------------------------------ keyboard
void RichEditor::handleKeys()
{
    caretMovedByKey = false;
    bool c = ctrlDown(), s = shiftDown();
    if (readOnly) {
        while (GetCharPressed() != 0) {}
        auto mv = [&](RtPos p) { moveTo(p, s); };
        if (pressed(KEY_LEFT)) mv(c ? doc().wordStart(caret) : doc().prevPos(caret));
        if (pressed(KEY_RIGHT)) mv(c ? doc().wordEnd(caret) : doc().nextPos(caret));
        if (pressed(KEY_UP)) { float px = prefX_; RtPos p = view_.moveVertical(caret, -1, px); moveTo(p, s, true); prefX_ = px; }
        if (pressed(KEY_DOWN)) { float px = prefX_; RtPos p = view_.moveVertical(caret, 1, px); moveTo(p, s, true); prefX_ = px; }
        if (pressed(KEY_HOME)) mv(c ? RtPos{ 0, 0 } : view_.lineHome(caret));
        if (pressed(KEY_END)) mv(c ? doc().endPos() : view_.lineEnd(caret));
        if (c && IsKeyPressed(KEY_A)) selectAll();
        if (c && IsKeyPressed(KEY_C)) copy(false);
        return;
    }

    // typed characters
    std::string typed;
    int cp;
    while ((cp = GetCharPressed()) != 0) {
        if (c && !IsKeyDown(KEY_RIGHT_ALT)) continue;   // Ctrl+letter shortcuts insert no text (except AltGr)
        if (cp >= 32 && cp != 127) encodeUtf8(cp, typed);
    }
    if (!typed.empty()) { insertTyped(typed); caretMovedByKey = true; }

    if (pressed(KEY_ENTER) || pressed(KEY_KP_ENTER)) { if (!c) { newline(); caretMovedByKey = true; } }
    if (pressed(KEY_BACKSPACE)) { backspace(c); caretMovedByKey = true; }
    if (pressed(KEY_DELETE)) { del(c); caretMovedByKey = true; }
    if (pressed(KEY_TAB)) {
        if (!listType().empty()) indent(s ? -1 : 1);
        else if (!s) insertTyped("    ");
        caretMovedByKey = true;
    }

    // cursor movement
    if (pressed(KEY_LEFT)) {
        if (hasSelection() && !s) moveTo(selStart(), false);
        else moveTo(c ? doc().wordStart(caret) : doc().prevPos(caret), s);
    }
    if (pressed(KEY_RIGHT)) {
        if (hasSelection() && !s) moveTo(selEnd(), false);
        else moveTo(c ? doc().wordEnd(caret) : doc().nextPos(caret), s);
    }
    if (pressed(KEY_UP)) { float px = prefX_; RtPos p = view_.moveVertical(caret, -1, px); moveTo(p, s, true); prefX_ = px; }
    if (pressed(KEY_DOWN)) { float px = prefX_; RtPos p = view_.moveVertical(caret, 1, px); moveTo(p, s, true); prefX_ = px; }
    if (pressed(KEY_HOME)) moveTo(c ? RtPos{ 0, 0 } : view_.lineHome(caret), s);
    if (pressed(KEY_END)) moveTo(c ? doc().endPos() : view_.lineEnd(caret), s);
    if (pressed(KEY_PAGE_UP)) { float px = prefX_; RtPos p = caret; for (int i = 0; i < 15; i++) p = view_.moveVertical(p, -1, px); moveTo(p, s, true); prefX_ = px; }
    if (pressed(KEY_PAGE_DOWN)) { float px = prefX_; RtPos p = caret; for (int i = 0; i < 15; i++) p = view_.moveVertical(p, 1, px); moveTo(p, s, true); prefX_ = px; }

    if (c) {
        if (IsKeyPressed(KEY_A)) selectAll();
        if (pressed(KEY_Z)) { if (s) redo(); else undo(); }
        if (pressed(KEY_Y)) redo();
        if (IsKeyPressed(KEY_C)) copy(false);
        if (IsKeyPressed(KEY_X) && !s) { copy(true); caretMovedByKey = true; }
        if (IsKeyPressed(KEY_V)) { if (s) pastePlain(); else paste(); caretMovedByKey = true; }
        if (IsKeyPressed(KEY_B)) toggleInline("bold");
        if (IsKeyPressed(KEY_I)) toggleInline("italic");
        if (IsKeyPressed(KEY_U)) toggleInline("underline");
        if (s && IsKeyPressed(KEY_X)) toggleInline("strike");
        if (IsKeyPressed(KEY_E) && s) toggleBlock("code-block");
        if (IsKeyPressed(KEY_SEVEN) && s) toggleList("ordered");
        if (IsKeyPressed(KEY_EIGHT) && s) toggleList("bullet");
        if (IsKeyPressed(KEY_NINE) && s) toggleList("unchecked");
        if (IsKeyPressed(KEY_ONE) && s) setHeader(1);
        if (IsKeyPressed(KEY_TWO) && s) setHeader(2);
        if (IsKeyPressed(KEY_THREE) && s) setHeader(3);
        if (IsKeyPressed(KEY_ZERO) && s) setHeader(0);
    }
}

