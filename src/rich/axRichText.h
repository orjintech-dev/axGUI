// axRichText.h - axGUI rich text: Quill Delta document model, layout and drawing
// (ported from NotePen richtext.h; NotePen-specific dependencies removed)
//
// Model: lines (RtLine) and characters (RtChar). Each character's inline attributes are kept as JSON in a shared
// table (attr id); line (block) attributes are the line's own JSON object. This way unknown Quill attributes
// are preserved losslessly and written back to the delta.
//
// Supported
//   inline: bold, italic, underline, strike, code, link, color, background, size, font
//   line:   header 1-3, list (bullet / ordered / checked / unchecked), indent, align, blockquote,
//           code-block, line-height; collapsible heading ("np-collapsed"), in-note link target ("np-id")
//   embed:  image ({"image": "data:image/png;base64,..." or file path}), flex-inline-table
#pragma once
#include "raylib.h"
#include <json.hpp>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <memory>

using RtJson = nlohmann::ordered_json;

// ------------------------------------------------------------------ theme
// Colors of rich text. Changed via rtTheme(); call bump() after changing (re-layout).
struct RtTheme
{
    bool dark = false;
    Color page, ink, inkStrong, inkPlaceholder, link, codeBg, inlineCodeBg, quoteBar, checkbox, checkboxOn,
          tableLine, caret, selection, guide;
    int version = 0;
    void apply(bool darkTheme);        // built-in light / dark palette
    void bump() { version++; }
    Color adapt(Color c) const;        // in the dark theme near-black gray ink becomes light
};
RtTheme &rtTheme();

struct RtStyle
{
    bool bold = false, italic = false, underline = false, strike = false, code = false, link = false;
    bool hasColor = false, hasBg = false;
    bool codeBlock = false;   // code block line: monospaced but no inline-code background (the block has its own)
    Color color{ 0, 0, 0, 221 };
    Color bg{ 0, 0, 0, 0 };
    float size = 0;           // 0 = line default
    std::string font;         // font family (Quill "font"; RtFonts::addFamily), "" = default
};

struct RtChar
{
    int cp = 0;        // Unicode code point (0xFFFC for an embed)
    int attr = 0;      // inline attribute id (0 = empty)
    int embed = -1;    // embed index (RichDoc::embeds)
};

// Inline range (e.g. search highlight): characters [c0, c1) on line `line`
struct RtRange { int line = 0, c0 = 0, c1 = 0; };

struct RtLine
{
    std::vector<RtChar> chars;
    RtJson block = RtJson::object();   // header, list, align, indent, code-block, blockquote ...
};

struct RtPos
{
    int line = 0, col = 0;
    bool operator==(const RtPos &o) const { return line == o.line && col == o.col; }
    bool operator!=(const RtPos &o) const { return !(*this == o); }
    bool operator<(const RtPos &o) const { return line < o.line || (line == o.line && col < o.col); }
};

class RichDoc
{
public:
    std::vector<RtLine> lines;
    std::vector<RtJson> embeds;                  // embed insert objects
    int embedsVersion = 0;                       // increases when embeds change (undo snapshots share it)

    RichDoc() { clear(); }
    void clear();
    void fromDelta(const RtJson &ops);
    RtJson toDelta() const;
    bool fromDeltaString(const std::string &json);   // false if invalid (empty document)
    std::string toDeltaString(int indent = -1) const;
    bool isEmpty() const;
    std::string plainText(RtPos a, RtPos b) const;
    std::string plainText() const;
    void setPlainText(const std::string &utf8);

    // attribute table (grows only: ids of older rows stay valid)
    int internAttr(const RtJson &attrs);
    const RtJson &attr(int id) const { return attrs_[id]; }
    const RtStyle &style(int id) const { return styles_[id]; }
    int attrCount() const { return (int)attrs_.size(); }

    int addEmbed(const RtJson &e) { embeds.push_back(e); embedsVersion++; return (int)embeds.size() - 1; }

    // --- editing ---
    RtPos clamp(RtPos p) const;
    RtPos endPos() const { return { (int)lines.size() - 1, (int)lines.back().chars.size() }; }
    RtPos insertText(RtPos p, const std::string &utf8, int attrId);        // '\n' splits lines
    RtPos insertLines(RtPos p, const std::vector<RtLine> &frag);           // rich paste
    std::vector<RtLine> extract(RtPos a, RtPos b) const;                   // rich copy
    RtPos erase(RtPos a, RtPos b);
    RtPos splitLine(RtPos p);                                              // Enter
    void formatInline(RtPos a, RtPos b, const std::string &key, const RtJson &value); // null = remove
    void formatBlock(int l0, int l1, const std::string &key, const RtJson &value);
    int attrAt(RtPos p) const;                                             // typing attributes
    int attrWith(int base, const std::string &key, const RtJson &value);   // base + delta

    RtPos prevPos(RtPos p) const;
    RtPos nextPos(RtPos p) const;

    // --- collapsible headings: if a heading line has "np-collapsed": true, the lines up to the next heading of the same or higher level
    // are hidden (may be nested)
    int headerLevel(int line) const;                  // 0: not a heading
    bool isCollapsed(int line) const;
    int sectionEnd(int line) const;                   // last line of the heading's section
    std::vector<int> hiddenOwners() const;            // per line, the outermost collapsed heading hiding it (-1: visible)
    void setCollapsed(int line, bool on);
    bool reveal(int line);                            // expands the headings hiding the line; true if anything changed
    // --- in-document links: link value is "#id". The target line carries "np-id": "id"; otherwise the heading
    // slug (rtHeadingSlug) is matched.
    int anchorLine(const std::string &id) const;      // -1: none
    std::string lineText(int line) const;
    RtPos wordStart(RtPos p) const;
    RtPos wordEnd(RtPos p) const;

private:
    std::vector<RtJson> attrs_;
    std::vector<RtStyle> styles_;
    std::unordered_map<std::string, int> attrIndex_;
};

std::string rtHeadingSlug(const std::string &text);

// Per block type: font size, line-height multiplier, top / bottom spacing
struct RtBlockStyle { float fs = 16, lh = 1.5f, top = 6, bottom = 0; };
struct RtStyleSheet
{
    enum Kind { H1, H2, H3, P, CODE, COUNT };
    RtBlockStyle b[COUNT];
    RtStyleSheet();                                  // built-in defaults
    static const char *key(int k);                   // "h1", "h2", "h3", "p", "code"
    static RtStyleSheet fromJson(const RtJson &j);   // missing / invalid fields fall back to defaults
    RtJson toJson() const;                           // only the differing fields; null if all are defaults
    bool operator==(const RtStyleSheet &o) const;
    bool operator!=(const RtStyleSheet &o) const { return !(*this == o); }
    void scale(float k);                             // scales all font sizes (e.g. 0.875 for UI)
};

// Layout + drawing + hit testing. Coordinates are document coordinates (0,0 = top-left of the document).
class RichTextView
{
public:
    RichDoc doc;
    float padding = 16.0f;
    bool collapsibleHeaders = true;     // collapse arrow left of headings
    std::string imagePlaceholder = "[image]";

    void setDelta(const RtJson &ops) { doc.fromDelta(ops); invalidate(); }
    void invalidate() { dirty_ = true; }
    void setStyle(const RtStyleSheet &s) { if (s != style_) { style_ = s; dirty_ = true; } }
    const RtStyleSheet &style() const { return style_; }
    bool isEmpty() const { return doc.isEmpty(); }

    // anchorHeight: height for "flex-table" embeds (optional)
    void layout(float docWidth, const std::function<float(int)> &anchorHeight = nullptr);
    float contentHeight() const { return contentH_; }
    float layoutWidth() const { return layoutWidth_; }

    struct DrawOpts {
        bool showCaret = false;          // caret + selection
        bool hideCaret = false;          // selection only (read-only)
        RtPos caret{}, anchor{};
        double blinkStart = 0;
        const char *placeholder = nullptr;               // placeholder text when the document is empty
        const std::vector<RtRange> *marks = nullptr;     // search matches (yellow); markCur is orange
        int markCur = -1;
    };
    // origin: draw position of the document's (0,0); visible: visible area (document coordinates)
    void draw(Vector2 origin, Rectangle visible, const DrawOpts &o) const;

    RtPos hitTest(Vector2 p) const;                    // document coordinates -> position
    Rectangle caretRect(RtPos p) const;                // document coordinates
    RtPos moveVertical(RtPos p, int dir, float &prefX) const;
    RtPos lineHome(RtPos p) const;                     // start of the visual line
    RtPos lineEnd(RtPos p) const;
    int checkboxAt(Vector2 p) const;                   // line of the clicked checkbox (-1 if none)
    int toggleAt(Vector2 p, float slop = 0) const;     // heading line of the clicked collapse arrow (-1 if none)
    RtPos imageAt(Vector2 p) const;                    // position of the visual embed at the point (line = -1 if none)
    bool imageRect(RtPos at, Rectangle &r) const;      // box of the visual embed
    bool rangeRect(const RtRange &g, Rectangle &r) const;
    std::string linkAt(Vector2 p) const;               // link target at the point ("" if none)
    float lineTop(int line) const;                     // top of the line's first visual line (-1 if none)

private:
    struct VLine {
        int line = 0, c0 = 0, c1 = 0;      // characters [c0, c1)
        float y = 0, h = 0, textY = 0;
        float caretY = 0, caretH = 0;
        std::vector<float> xs;            // x of the c0..c1 boundaries (c1-c0+1 entries)
        bool embedLine = false;
    };
    struct Item {
        enum Kind { TEXT, BULLET, NUMBER, CHECK, QUOTE_BAR, CODE_BG, INLINE_TABLE, IMAGE_PH, OTHER_PH, TOGGLE } kind = TEXT;
        float x = 0, y = 0, w = 0, h = 0;
        float fs = 16;
        std::string text;
        RtStyle st;
        bool checked = false;
        int line = -1, col = -1;          // IMAGE_PH: position of the embed character
        RtJson data;
        size_t imgKey = 0;                // IMAGE_PH: image cache key (0 = failed to load)
        float rowY = 0, rowH = 0;         // TOGGLE: line band of the heading
        std::string link;                 // TEXT: link target
    };
    std::vector<VLine> vlines_;
    std::vector<Item> items_;
    float layoutWidth_ = -1;
    float contentH_ = 0;
    RtStyleSheet style_;
    bool dirty_ = true;
    int themeVer_ = -1;
    int fontGen_ = -1;
    int vlineIndexOf(RtPos p) const;
};

bool rtParseQuillColor(const std::string &s, Color &out);
std::string rtQuillColor(Color c);                  // "#AARRGGBB" (flutter_quill)
float rtLineHeightAttr(const RtJson &block);        // "line-height" block attribute, 0 if absent

// Texture cache for image embeds ({"image": "data:image/png;base64,..."} or file path).
struct RtImage { Texture2D tex{}; int w = 0, h = 0; bool ok = false; };
const RtImage &rtImage(const std::string &src, size_t *key = nullptr);
const RtImage *rtImageByKey(size_t key);
void rtImageCacheClear();
// Builds a data: URI (png/jpg bytes as-is; others are converted to PNG). Downscaled if the long edge exceeds maxPx.
std::string rtImageDataUri(const unsigned char *data, size_t size, const std::string &ext, int maxPx = 2048);
std::string rtImageDataUri(const Image &img, int maxPx = 2048);
std::string rtImageFileDataUri(const std::string &path, int maxPx = 2048);   // from file

std::string rtBase64Encode(const unsigned char *data, size_t n);
std::vector<unsigned char> rtBase64Decode(const std::string &s);
