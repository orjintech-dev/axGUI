// axRichEdit.h - rich text editor: cursor, selection, keyboard, formatting, clipboard, undo / redo
// (ported from NotePen richedit.h; adds undo / redo and read-only mode)
#pragma once
#include "axRichText.h"
#include <functional>
#include <string>
#include <vector>
#include <memory>

class RichEditor
{
public:
    explicit RichEditor(RichTextView &v) : view_(v) {}

    RtPos caret{}, anchor{};
    bool dirty = false;           // document changed since the last save (the app resets it)
    bool changedThisFrame = false; // document changed this frame (richTextBox resets it every frame)
    bool caretMovedByKey = false; // keyboard cursor movement this frame (for auto-scroll)
    bool readOnly = false;        // navigation, selection and copy only
    double lastEdit = 0;
    double blinkStart = 0;        // cursor blink resets on typing / movement

    void reset();                 // when a new document is loaded (history is reset too)
    void clampCaret();
    bool hasSelection() const { return caret != anchor; }
    RtPos selStart() const { return anchor < caret ? anchor : caret; }
    RtPos selEnd() const { return anchor < caret ? caret : anchor; }
    std::string selectedText() const;

    // mouse (document coordinates)
    void mousePress(Vector2 docPt, bool shift, int clickCount);
    void mouseDrag(Vector2 docPt);
    void mouseRelease() { dragging_ = false; }
    bool dragging() const { return dragging_; }

    // keyboard (every frame while focused)
    void handleKeys();

    // formatting (on the selection; with no selection, inline formatting applies to the next typed text)
    bool inlineActive(const std::string &key) const;
    RtJson inlineValue(const std::string &key) const;
    void toggleInline(const std::string &key);
    void setInline(const std::string &key, const RtJson &value);   // null = remove
    int headerLevel() const;
    void setHeader(int level);                       // 0 = paragraf
    std::string listType() const;                    // "", "bullet", "ordered", "checked", "unchecked"
    void toggleList(const std::string &type);
    std::string align() const;                       // "", "center", "right", "justify"
    void setAlign(const std::string &a);
    bool blockActive(const std::string &key) const;  // "blockquote", "code-block"
    void toggleBlock(const std::string &key);
    // link: extent and address of the link at the cursor; applyLink applies to the selection or the link at the cursor
    // (empty: removes it); with no selection / link, inserts the address as linked text
    bool linkRange(RtPos &a, RtPos &b, std::string &url) const;
    std::string currentLink() const;
    void applyLink(const std::string &url);
    float lineHeight() const;
    void setLineHeight(float v);
    void clearFormatting();
    void indent(int delta);
    void toggleChecked(int line);                    // toggle / clear a to-do item

    // clipboard
    void copy(bool cut);
    void paste();          // formatted if there is external HTML, otherwise plain text; image if no text
    void pastePlain();     // Ctrl+Shift+V: plain (with the cursor's format)
    void insertText(const std::string &utf8) { insertTyped(utf8); }
    // insert an image embed line at the cursor (Quill {"image": uri}); cursor moves to the line after the image
    void insertImageEmbed(const std::string &uri, int width = 0);
    void selectAll();

    // undo / redo (Ctrl+Z, Ctrl+Y / Ctrl+Shift+Z)
    bool canUndo() const { return hIndex_ > 0; }
    bool canRedo() const { return hIndex_ + 1 < (int)history_.size(); }
    void undo();
    void redo();
    int historyLimit = 200;

    // called when the clipboard has no text; default: raylib GetClipboardImage -> image embed
    std::function<bool()> onPasteImage;
    // downloader for http(s) images (HTML paste); if unset, these images are dropped
    std::function<bool(const std::string &url, std::vector<unsigned char> &bytes, std::string &ext)> fetchImage;

private:
    RichTextView &view_;
    RichDoc &doc() { return view_.doc; }
    const RichDoc &doc() const { return view_.doc; }
    bool dragging_ = false;
    int dragMode_ = 1;           // 1 character, 2 word, 3 line
    RtPos dragOrigin0_{}, dragOrigin1_{};
    float prefX_ = -1;
    int pendingAttr_ = -1;       // format change without a selection (applies to the next typed text)

    struct Snapshot {
        std::vector<RtLine> lines;
        std::shared_ptr<const std::vector<RtJson>> embeds;
        int embedsVersion = -1;
        RtPos caret{}, anchor{};
        int kind = 0;            // 1: typing (consecutive typing is merged)
        double time = 0;
    };
    std::vector<Snapshot> history_;
    int hIndex_ = -1;
    int editKind_ = 0;           // set before changed()
    void pushHistory(int kind);
    void restore(const Snapshot &s);

    void changed();
    void deleteSelection();
    void insertTyped(const std::string &utf8);
    void newline();
    void backspace(bool word);
    void del(bool word);
    void moveTo(RtPos p, bool extend, bool keepPrefX = false);
};
