// axRichHtml.cpp - HTML <-> rich-text lines. Ported from NotePen htmlpaste.cpp (import) and richedit.cpp
// (export). Small dependency-free, error-tolerant parser: tag stack + inline format
// stack + line (block) context. In malformed / incomplete HTML, unclosed tags are closed at the end,
// and closing tags without a match are ignored.
#include "axRichHtml.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <map>
#include <set>
#include <unordered_map>

namespace {

struct Fmt
{
    bool bold = false, italic = false, underline = false, strike = false, code = false;
    std::string color, bg, link;
    int size = 0;
};

struct Frame
{
    std::string tag;
    Fmt fmt;
    bool block = false;       // added line context (the line ends when it closes)
    RtJson blockAttrs;          // what this element contributes to the line (header, list, align...)
    bool list = false;        // ul / ol
    bool pre = false;
    bool skip = false;        // script / style ...: content is discarded
    bool flexRow = false;     // display:flex (no wrap, horizontal): inner blocks are not split into separate lines, they flow on the same line
};

std::string lower(std::string s) { for (auto &c : s) c = (char)std::tolower((unsigned char)c); return s; }
std::string trim(const std::string &s)
{
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

void appendUtf8(std::string &o, unsigned c)
{
    if (c < 0x80) o += (char)c;
    else if (c < 0x800) { o += (char)(0xC0 | (c >> 6)); o += (char)(0x80 | (c & 0x3F)); }
    else if (c < 0x10000) { o += (char)(0xE0 | (c >> 12)); o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
    else { o += (char)(0xF0 | (c >> 18)); o += (char)(0x80 | ((c >> 12) & 0x3F)); o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
}

// &amp; &#233; &#xE9; &nbsp; ... -> UTF-8
std::string decodeEntities(const std::string &s)
{
    static const std::map<std::string, unsigned> named = {
        { "amp", '&' }, { "lt", '<' }, { "gt", '>' }, { "quot", '"' }, { "apos", '\'' }, { "nbsp", 0xA0 },
        { "ndash", 0x2013 }, { "mdash", 0x2014 }, { "hellip", 0x2026 }, { "lsquo", 0x2018 }, { "rsquo", 0x2019 },
        { "ldquo", 0x201C }, { "rdquo", 0x201D }, { "bull", 0x2022 }, { "middot", 0xB7 }, { "copy", 0xA9 },
        { "reg", 0xAE }, { "trade", 0x2122 }, { "euro", 0x20AC }, { "times", 0xD7 }, { "deg", 0xB0 },
        { "laquo", 0xAB }, { "raquo", 0xBB }, { "shy", 0xAD }, { "zwnj", 0x200C }, { "zwj", 0x200D },
    };
    std::string o;
    o.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] != '&') { o += s[i]; continue; }
        size_t semi = s.find(';', i);
        if (semi == std::string::npos || semi - i > 12) { o += '&'; continue; }
        std::string e = s.substr(i + 1, semi - i - 1);
        unsigned cp = 0;
        bool ok = false;
        if (!e.empty() && e[0] == '#') {
            char *end = nullptr;
            unsigned long v = (e.size() > 1 && (e[1] == 'x' || e[1] == 'X')) ? std::strtoul(e.c_str() + 2, &end, 16) : std::strtoul(e.c_str() + 1, &end, 10);
            if (end && *end == 0 && v > 0 && v < 0x110000) { cp = (unsigned)v; ok = true; }
        } else {
            auto it = named.find(e);
            if (it != named.end()) { cp = it->second; ok = true; }
        }
        if (!ok) { o += '&'; continue; }
        appendUtf8(o, cp);
        i = semi;
    }
    return o;
}

// "a:b; c:d" -> {a:b, c:d} (keys lowercased)
std::map<std::string, std::string> parseStyle(const std::string &st)
{
    std::map<std::string, std::string> m;
    size_t i = 0;
    while (i < st.size()) {
        size_t semi = st.find(';', i);
        if (semi == std::string::npos) semi = st.size();
        std::string decl = st.substr(i, semi - i);
        size_t c = decl.find(':');
        if (c != std::string::npos) {
            std::string k = lower(trim(decl.substr(0, c))), v = trim(decl.substr(c + 1));
            size_t imp = lower(v).find("!important");
            if (imp != std::string::npos) v = trim(v.substr(0, imp));
            if (!k.empty()) m[k] = v;
        }
        i = semi + 1;
    }
    return m;
}

// CSS color -> r,g,b (false if a == 0)
bool parseCssColor(std::string v, int &r, int &g, int &b)
{
    v = lower(trim(v));
    static const std::map<std::string, unsigned> named = {
        { "black", 0x000000 }, { "white", 0xFFFFFF }, { "red", 0xFF0000 }, { "green", 0x008000 }, { "blue", 0x0000FF },
        { "yellow", 0xFFFF00 }, { "orange", 0xFFA500 }, { "purple", 0x800080 }, { "gray", 0x808080 }, { "grey", 0x808080 },
        { "silver", 0xC0C0C0 }, { "maroon", 0x800000 }, { "navy", 0x000080 }, { "teal", 0x008080 }, { "olive", 0x808000 },
        { "lime", 0x00FF00 }, { "aqua", 0x00FFFF }, { "cyan", 0x00FFFF }, { "fuchsia", 0xFF00FF }, { "magenta", 0xFF00FF },
        { "brown", 0xA52A2A }, { "pink", 0xFFC0CB }, { "darkred", 0x8B0000 }, { "darkgreen", 0x006400 }, { "darkblue", 0x00008B },
    };
    auto it = named.find(v);
    if (it != named.end()) { r = (it->second >> 16) & 255; g = (it->second >> 8) & 255; b = it->second & 255; return true; }
    if (!v.empty() && v[0] == '#') {
        std::string h = v.substr(1);
        for (char c : h) if (!std::isxdigit((unsigned char)c)) return false;
        if (h.size() == 3 || h.size() == 4) {
            if (h.size() == 4 && h[3] == '0') return false;
            r = std::stoi(std::string(2, h[0]), nullptr, 16); g = std::stoi(std::string(2, h[1]), nullptr, 16); b = std::stoi(std::string(2, h[2]), nullptr, 16);
            return true;
        }
        if (h.size() == 6 || h.size() == 8) {
            if (h.size() == 8 && h.substr(6) == "00") return false;
            r = std::stoi(h.substr(0, 2), nullptr, 16); g = std::stoi(h.substr(2, 2), nullptr, 16); b = std::stoi(h.substr(4, 2), nullptr, 16);
            return true;
        }
        return false;
    }
    if (v.rfind("rgb", 0) == 0) {
        size_t a = v.find('('), z = v.find(')');
        if (a == std::string::npos || z == std::string::npos) return false;
        std::string in = v.substr(a + 1, z - a - 1);
        for (auto &c : in) if (c == ',' || c == '/') c = ' ';
        float f[4] = { 0, 0, 0, 1 };
        int n = 0;
        size_t p = 0;
        while (n < 4 && p < in.size()) {
            while (p < in.size() && in[p] == ' ') p++;
            if (p >= in.size()) break;
            char *end = nullptr;
            float x = std::strtof(in.c_str() + p, &end);
            if (!end || end == in.c_str() + p) break;
            p = (size_t)(end - in.c_str());
            if (p < in.size() && in[p] == '%') { x = n < 3 ? x * 2.55f : x / 100; p++; }
            f[n++] = x;
        }
        if (n < 3 || f[3] <= 0.05f) return false;
        r = (int)std::clamp(f[0], 0.0f, 255.0f); g = (int)std::clamp(f[1], 0.0f, 255.0f); b = (int)std::clamp(f[2], 0.0f, 255.0f);
        return true;
    }
    return false;
}

float luma(int r, int g, int b) { return 0.299f * r + 0.587f * g + 0.114f * b; }

std::string quillColor(int r, int g, int b)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "#ff%02x%02x%02x", r, g, b);   // flutter_quill: #AARRGGBB
    return buf;
}

// font size in px (px, pt, em/rem ~16, keywords); 0 if unknown
float parseFontSize(std::string v)
{
    v = lower(trim(v));
    static const std::map<std::string, float> kw = { { "xx-small", 9 }, { "x-small", 10 }, { "small", 13 }, { "medium", 16 },
                                                     { "large", 18 }, { "x-large", 24 }, { "xx-large", 32 }, { "xxx-large", 48 } };
    auto it = kw.find(v);
    if (it != kw.end()) return it->second;
    char *end = nullptr;
    float x = std::strtof(v.c_str(), &end);
    if (!end || end == v.c_str() || x <= 0) return 0;
    std::string unit = trim(std::string(end));
    if (unit == "px" || unit.empty()) return x;
    if (unit == "pt") return x * 4.0f / 3.0f;
    if (unit == "em" || unit == "rem") return x * 16;
    return 0;
}

class Converter
{
public:
    explicit Converter(RichDoc &d) : doc_(d) {}

    std::vector<RtLine> run(const std::string &html)
    {
        // source page (clipboardHtml prepends "<!--np-source:URL-->"): "#" links pointing to the same page become in-note links
        {
            size_t k = html.find("<!--np-source:");
            if (k != std::string::npos && k < 64) {
                size_t e = html.find("-->", k);
                if (e != std::string::npos) srcBase_ = stripFragment(trim(html.substr(k + 14, e - k - 14)));
            }
        }
        size_t i = 0, n = html.size();
        while (i < n) {
            if (html[i] == '<' && i + 1 < n && !(std::isalpha((unsigned char)html[i + 1]) || html[i + 1] == '/' || html[i + 1] == '!' || html[i + 1] == '?')) {
                text("<");   // stray '<' in text (a < b)
                i++;
                continue;
            }
            if (html[i] == '<') {
                if (html.compare(i, 4, "<!--") == 0) {   // comment (including Word conditional comments)
                    size_t e = html.find("-->", i + 4);
                    i = e == std::string::npos ? n : e + 3;
                    continue;
                }
                if (i + 1 < n && (html[i + 1] == '!' || html[i + 1] == '?')) {   // <!DOCTYPE>, <![if ...]>, <?xml?>
                    size_t e = html.find('>', i);
                    i = e == std::string::npos ? n : e + 1;
                    continue;
                }
                size_t e = tagEnd(html, i);
                if (e == std::string::npos) { text(html.substr(i)); break; }
                tag(html.substr(i + 1, e - i - 1));
                i = e + 1;
                continue;
            }
            size_t e = html.find('<', i);
            if (e == std::string::npos) e = n;
            text(html.substr(i, e - i));
            i = e;
        }
        flush(false);
        resolveInternalLinks();
        // leading / trailing empty lines
        while (!out_.empty() && out_.front().chars.empty() && out_.front().block.empty()) out_.erase(out_.begin());
        while (!out_.empty() && out_.back().chars.empty() && out_.back().block.empty()) out_.pop_back();
        if (out_.empty()) return out_;
        bool anyText = false;
        for (auto &l : out_) if (!l.chars.empty()) { anyText = true; break; }
        if (!anyText) return {};
        if (out_.size() == 1 && out_[0].block.empty() && images_ == 0) return out_;   // inline paste
        // so the image line doesn't merge into the cursor's line: leading empty line (an empty line is added to the cursor's line)
        if (!out_.front().chars.empty() && out_.front().chars[0].embed >= 0) out_.insert(out_.begin(), RtLine());
        RtLine tail;   // preserve the last line's format (insertLines gives the last line the cursor line's format)
        out_.push_back(tail);
        return out_;
    }

private:
    RichDoc &doc_;
    std::vector<Frame> stack_;
    std::vector<RtLine> out_;
    RtLine cur_;
    bool curTouched_ = false;    // line opened by <br> / a block (written even if empty)
    int images_ = 0;
    std::map<std::string, int> attrCache_;
    std::string srcBase_;                 // source page address (with '#' and what follows stripped)
    std::vector<std::string> pendingIds_; // ids of opened elements: go to the next non-empty line (the first becomes "np-id")
    std::set<std::string> ids_;           // target ids in the pasted content
    std::map<std::string, std::string> idAlias_;   // other ids falling on the same line -> the line's np-id

    static std::string stripFragment(const std::string &u)
    {
        size_t h = u.find('#');
        std::string b = h == std::string::npos ? u : u.substr(0, h);
        while (!b.empty() && b.back() == '/') b.pop_back();
        return b;
    }
    // if the target of a "#id" link is not among the pasted content: with a known source page it becomes the full URL
    // (opens on the web); otherwise "#id" stays (it may match a same-named heading in the note)
    void resolveInternalLinks()
    {
        auto fix = [&](RtLine &L) {
            for (RtChar &c : L.chars) {
                if (!c.attr) continue;
                const RtJson &a = doc_.attr(c.attr);
                if (!a.contains("link") || !a["link"].is_string()) continue;
                std::string l = a["link"].get<std::string>();
                if (l.size() < 2 || l[0] != '#' || ids_.count(l.substr(1))) continue;
                auto al = idAlias_.find(l.substr(1));
                if (al != idAlias_.end()) { c.attr = doc_.attrWith(c.attr, "link", RtJson("#" + al->second)); continue; }
                if (srcBase_.empty()) continue;
                c.attr = doc_.attrWith(c.attr, "link", RtJson(srcBase_ + l));
            }
        };
        for (RtLine &L : out_) fix(L);
        fix(cur_);
    }

    static size_t tagEnd(const std::string &h, size_t i)
    {
        char q = 0;
        for (size_t k = i + 1; k < h.size(); k++) {
            char c = h[k];
            if (q) { if (c == q) q = 0; continue; }
            if (c == '"' || c == '\'') q = c;
            else if (c == '>') return k;
        }
        return std::string::npos;
    }

    Fmt fmt() const { return stack_.empty() ? Fmt() : stack_.back().fmt; }
    bool inFlexRow() const { for (auto &f : stack_) if (f.flexRow && !f.skip) return true; return false; }
    // block boundary inside a flex row: the line does not end, a single space is inserted
    void flexGap() { if (!cur_.chars.empty() && cur_.chars.back().cp != ' ') putChar(' ', 0); }
    // display:flex, direction row (default), no wrap -> side-by-side content such as cards / media + text.
    // Wrapping grids (flex-wrap: wrap, e.g. Bootstrap .row page layouts) do not count.
    static bool isFlexRowStyle(const std::map<std::string, std::string> &st)
    {
        auto get = [&](const char *k) -> std::string { auto it = st.find(k); return it == st.end() ? std::string() : lower(trim(it->second)); };
        std::string d = get("display");
        if (d != "flex" && d != "inline-flex") return false;
        std::string dir = get("flex-direction");
        if (!dir.empty() && dir.rfind("row", 0) != 0) return false;
        std::string fw = get("flex-wrap"), ff = get("flex-flow");
        if (fw.find("wrap") != std::string::npos && fw.find("nowrap") == std::string::npos) return false;
        if (ff.find("column") != std::string::npos) return false;
        if (ff.find("wrap") != std::string::npos && ff.find("nowrap") == std::string::npos) return false;
        return true;
    }
    // CSS / attribute length (px) -> px; percent / auto / calc -> 0
    static int cssPx(const std::string &v)
    {
        std::string t = lower(trim(v));
        if (t.empty() || t.find('%') != std::string::npos || t.find("calc") != std::string::npos || t == "auto") return 0;
        float x = (float)std::atof(t.c_str());
        if (t.find("rem") != std::string::npos || t.find("em") != std::string::npos) x *= 16;
        return x > 0 ? (int)std::lround(x) : 0;
    }
    bool inPre() const { for (auto &f : stack_) if (f.pre) return true; return false; }
    bool skipping() const { for (auto &f : stack_) if (f.skip) return true; return false; }
    int listDepth() const { int d = 0; for (auto &f : stack_) if (f.list) d++; return d; }
    std::string listType() const
    {
        for (auto it = stack_.rbegin(); it != stack_.rend(); ++it) if (it->list) return it->tag == "ol" ? "ordered" : "bullet";
        return "bullet";
    }
    bool inHeader() const
    {
        for (auto &f : stack_) if (f.blockAttrs.is_object() && f.blockAttrs.contains("header")) return true;
        return false;
    }

    RtJson effectiveBlock() const
    {
        RtJson b = RtJson::object();
        for (auto &f : stack_)
            if (f.blockAttrs.is_object())
                for (auto it = f.blockAttrs.begin(); it != f.blockAttrs.end(); ++it) b[it.key()] = it.value();
        return b;
    }

    int attrId(const Fmt &f)
    {
        RtJson a = RtJson::object();
        if (f.bold) a["bold"] = true;
        if (f.italic) a["italic"] = true;
        if (f.underline) a["underline"] = true;
        if (f.strike) a["strike"] = true;
        if (f.code) a["code"] = true;
        if (!f.link.empty()) a["link"] = f.link;
        if (!f.color.empty()) a["color"] = f.color;
        if (!f.bg.empty()) a["background"] = f.bg;
        if (f.size > 0) a["size"] = std::to_string(f.size);
        if (a.empty()) return 0;
        std::string k = a.dump();
        auto it = attrCache_.find(k);
        if (it != attrCache_.end()) return it->second;
        int id = doc_.internAttr(a);
        attrCache_[k] = id;
        return id;
    }

    // end the line. force: write it even if empty (<br>, empty paragraph)
    void flush(bool force)
    {
        while (!cur_.chars.empty() && cur_.chars.back().cp == ' ' && !inPre()) cur_.chars.pop_back();
        if (cur_.chars.empty() && !force && !curTouched_) return;
        cur_.block = effectiveBlock();
        if (!pendingIds_.empty() && !cur_.chars.empty()) {   // in-note link target
            const std::string &main = pendingIds_.front();
            cur_.block["np-id"] = main;
            ids_.insert(main);
            for (size_t k = 1; k < pendingIds_.size(); k++) idAlias_[pendingIds_[k]] = main;
            pendingIds_.clear();
        }
        out_.push_back(cur_);
        cur_ = RtLine();
        curTouched_ = false;
    }

    void putChar(unsigned cp, int attr)
    {
        RtChar c;
        c.cp = (int)cp;
        c.attr = attr;
        cur_.chars.push_back(c);
    }

    void text(const std::string &raw)
    {
        if (raw.empty() || skipping()) return;
        std::string s = decodeEntities(raw);
        int attr = attrId(fmt());
        bool pre = inPre();
        const unsigned char *p = (const unsigned char *)s.c_str(), *end = p + s.size();
        while (p < end) {
            unsigned c = *p, extra = 0;
            if (c < 0x80) extra = 0;
            else if ((c & 0xE0) == 0xC0) { c &= 0x1F; extra = 1; }
            else if ((c & 0xF0) == 0xE0) { c &= 0x0F; extra = 2; }
            else if ((c & 0xF8) == 0xF0) { c &= 0x07; extra = 3; }
            else { c = 0xFFFD; }
            p++;
            for (unsigned k = 0; k < extra && p < end; k++, p++) c = (c << 6) | (*p & 0x3F);
            if (pre) {
                if (c == '\r') continue;
                if (c == '\n') { flush(true); continue; }
                if (c == '\t') { for (int k = 0; k < 4; k++) putChar(' ', attr); continue; }
                putChar(c == 0xA0 ? ' ' : c, attr);
                continue;
            }
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f') {
                // run of whitespace becomes a single space; dropped at line start
                if (cur_.chars.empty() || cur_.chars.back().cp == ' ') continue;
                putChar(' ', attr);
                continue;
            }
            if (c == 0xAD || c == 0x200B || c == 0xFEFF) continue;   // soft hyphen, zero width
            putChar(c == 0xA0 ? ' ' : c, attr);
        }
    }

    static std::map<std::string, std::string> parseAttrs(const std::string &s, size_t from)
    {
        std::map<std::string, std::string> m;
        size_t i = from, n = s.size();
        while (i < n) {
            while (i < n && (std::isspace((unsigned char)s[i]) || s[i] == '/')) i++;
            size_t ks = i;
            while (i < n && !std::isspace((unsigned char)s[i]) && s[i] != '=' && s[i] != '/') i++;
            std::string key = lower(s.substr(ks, i - ks));
            while (i < n && std::isspace((unsigned char)s[i])) i++;
            std::string val;
            if (i < n && s[i] == '=') {
                i++;
                while (i < n && std::isspace((unsigned char)s[i])) i++;
                if (i < n && (s[i] == '"' || s[i] == '\'')) {
                    char q = s[i++];
                    size_t ve = s.find(q, i);
                    if (ve == std::string::npos) ve = n;
                    val = s.substr(i, ve - i);
                    i = ve + 1;
                } else {
                    size_t vs = i;
                    while (i < n && !std::isspace((unsigned char)s[i])) i++;
                    val = s.substr(vs, i - vs);
                }
            }
            if (!key.empty()) m[key] = decodeEntities(val);
        }
        return m;
    }

    static bool isBlockTag(const std::string &t)
    {
        static const char *b[] = { "p", "div", "h1", "h2", "h3", "h4", "h5", "h6", "li", "blockquote", "pre", "ul", "ol",
                                   "tr", "table", "section", "article", "header", "footer", "main", "nav", "aside", "dl", "dt", "dd",
                                   "figure", "figcaption", "address", "center", "form", "fieldset", "details", "summary", "tbody", "thead" };
        for (const char *x : b) if (t == x) return true;
        return false;
    }
    static bool isVoid(const std::string &t)
    {
        static const char *v[] = { "br", "img", "hr", "meta", "link", "input", "wbr", "col", "area", "base", "source", "embed", "param", "track" };
        for (const char *x : v) if (t == x) return true;
        return false;
    }

    void applyStyle(Frame &f, const std::map<std::string, std::string> &st, bool block)
    {
        int r, g, b;
        auto get = [&](const char *k) -> std::string { auto it = st.find(k); return it == st.end() ? std::string() : lower(it->second); };
        std::string v;
        if (!(v = get("font-weight")).empty()) {
            if (v == "bold" || v == "bolder") f.fmt.bold = true;
            else if (v == "normal" || v == "lighter") f.fmt.bold = false;
            else { int w = std::atoi(v.c_str()); if (w > 0) f.fmt.bold = w >= 600; }
        }
        if (!(v = get("font-style")).empty()) f.fmt.italic = v.find("italic") != std::string::npos || v.find("oblique") != std::string::npos;
        std::string td = get("text-decoration");
        if (td.empty()) td = get("text-decoration-line");
        if (!td.empty()) {
            if (td.find("none") != std::string::npos) f.fmt.underline = f.fmt.strike = false;
            if (td.find("underline") != std::string::npos) f.fmt.underline = true;
            if (td.find("line-through") != std::string::npos) f.fmt.strike = true;
        }
        if (!(v = get("color")).empty() && parseCssColor(v, r, g, b)) {
            float l = luma(r, g, b);
            // the page theme's black / white text is dropped (so the note keeps its default color)
            bool gray = std::abs(r - g) < 24 && std::abs(g - b) < 24 && std::abs(r - b) < 24;
            if (l < 70 || l > 232 || (gray && l < 110)) f.fmt.color.clear();
            else f.fmt.color = quillColor(r, g, b);
        }
        std::string bgv = get("background-color");
        if (bgv.empty()) { std::string bg = get("background"); if (!bg.empty() && bg.find("url(") == std::string::npos) bgv = bg; }
        if (!block && !bgv.empty()) {
            if (bgv == "transparent" || bgv == "none" || bgv == "initial" || bgv == "inherit") f.fmt.bg.clear();
            else if (parseCssColor(bgv, r, g, b)) {
                float l = luma(r, g, b);
                if (l > 235 || l < 60) f.fmt.bg.clear();
                else f.fmt.bg = quillColor(r, g, b);
            }
        }
        // light text on a dark background (badge / label such as "Historical sites"): restored if the light color was dropped above,
        // otherwise the background stays and the text is unreadable against the note's dark color
        if (!f.fmt.bg.empty() && f.fmt.color.empty() && !(v = get("color")).empty() && parseCssColor(v, r, g, b) && luma(r, g, b) > 200) {
            // background: this element's CSS value, else the inherited Quill color ("#AARRGGBB" -> last 6 digits)
            std::string bs = bgv;
            if (bs.empty() || bs == "transparent") { bs = f.fmt.bg; if (bs.size() == 9 && bs[0] == '#') bs = "#" + bs.substr(3); }
            int br, bgc, bb;
            if (parseCssColor(bs, br, bgc, bb) && luma(br, bgc, bb) < 150) f.fmt.color = quillColor(r, g, b);
        }
        if (!(v = get("font-size")).empty()) {
            float px = parseFontSize(v);
            if (px > 0) {
                int sz = (int)std::lround(std::clamp(px, 6.0f, 60.0f));
                f.fmt.size = (sz >= 13 && sz <= 17) ? 0 : sz;   // body sizes are "normal"
            }
        }
        if (block && !(v = get("text-align")).empty()) {
            if (v == "center" || v == "right" || v == "justify") { if (!f.blockAttrs.is_object()) f.blockAttrs = RtJson::object(); f.blockAttrs["align"] = v; }
        }
        std::string ff = get("font-family");
        if (!ff.empty() && (ff.find("mono") != std::string::npos || ff.find("courier") != std::string::npos || ff.find("consolas") != std::string::npos)) f.fmt.code = true;
    }

    void tag(const std::string &body)
    {
        if (body.empty()) return;
        bool closing = body[0] == '/';
        size_t s = closing ? 1 : 0;
        size_t e = s;
        while (e < body.size() && !std::isspace((unsigned char)body[e]) && body[e] != '/' && body[e] != '>') e++;
        std::string name = lower(body.substr(s, e - s));
        if (name.empty()) return;
        size_t colon = name.find(':');
        if (colon != std::string::npos) name = name.substr(colon + 1);   // Word: <o:p>, <w:...>

        if (closing) { close(name); return; }
        bool selfClose = !body.empty() && body.back() == '/';

        if (skipping()) {
            if (!isVoid(name) && !selfClose) { Frame f; f.tag = name; f.skip = true; stack_.push_back(f); }
            return;
        }
        auto attrs = parseAttrs(body, e);
        {
            // target id: id (or legacy <a name>); the outer element takes precedence (written to the first non-empty line)
            auto ia = attrs.find("id");
            if (ia == attrs.end() && name == "a") ia = attrs.find("name");
            if (ia != attrs.end() && !skipping() && pendingIds_.size() < 8) {
                std::string id = trim(ia->second);
                if (!id.empty() && id.size() < 200) pendingIds_.push_back(id);
            }
        }

        if (name == "br") { flush(true); return; }
        if (name == "hr") { flush(false); return; }
        if (name == "img") {
            // image: Quill image embed on its own line (http URLs are downloaded in htmlToRtLines)
            auto sit = attrs.find("src");
            std::string src = sit == attrs.end() ? std::string() : trim(sit->second);
            std::string sl = lower(src);
            auto num = [&](const char *k) { auto a = attrs.find(k); return a == attrs.end() ? -1 : std::atoi(a->second.c_str()); };
            int w = num("width"), h = num("height");
            bool tiny = (w >= 0 && w <= 2) || (h >= 0 && h <= 2);   // tracking pixels
            // displayed width: inline style (browser copies write the computed style: "width: 120px") or the
            // width attribute. The note image is inserted at this width (the embed character's "width" attribute).
            int dispW = 0;
            { auto st = attrs.find("style"); if (st != attrs.end()) { auto sm = parseStyle(st->second); auto wi = sm.find("width"); if (wi != sm.end()) dispW = cssPx(wi->second); } }
            if (dispW <= 0 && w > 2) dispW = w;
            if (!tiny && (sl.rfind("data:image/", 0) == 0 || sl.rfind("http://", 0) == 0 || sl.rfind("https://", 0) == 0) && images_ < 30) {
                const bool inl = inFlexRow();   // side-by-side content (card): image on the same line as the text
                if (!inl) flush(false);
                else flexGap();
                RtJson emb = RtJson::object();
                emb["image"] = src;
                RtChar c; c.cp = 0xFFFC; c.embed = doc_.addEmbed(emb);
                if (dispW > 0) { RtJson wa = RtJson::object(); wa["width"] = std::to_string(std::min(dispW, 2048)); c.attr = doc_.internAttr(wa); }
                cur_.chars.push_back(c);
                if (!inl) {
                    RtLine l = cur_;
                    l.block = RtJson::object();   // image line has no formatting (separate line even inside a list/heading)
                    out_.push_back(l);
                    cur_ = RtLine();
                } else putChar(' ', 0);
                images_++;
            }
            return;
        }
        if (isVoid(name)) return;

        Frame f;
        f.tag = name;
        f.fmt = fmt();
        if (name == "script" || name == "style" || name == "head" || name == "title" || name == "noscript" ||
            name == "template" || name == "svg" || name == "math" || name == "button" || name == "select" || name == "textarea") {
            f.skip = true;
            if (!selfClose) stack_.push_back(f);
            return;
        }
        bool block = isBlockTag(name);
        std::map<std::string, std::string> styleMap;
        { auto it = attrs.find("style"); if (it != attrs.end()) styleMap = parseStyle(it->second); }
        const bool parentFlex = inFlexRow();
        const bool flexHere = isFlexRowStyle(styleMap);
        if (parentFlex) {
            if (block || flexHere) flexGap();   // side-by-side content: the line does not end
        } else if (block || flexHere) flush(false);
        f.flexRow = flexHere;

        if (name == "b" || name == "strong") f.fmt.bold = true;
        else if (name == "i" || name == "em" || name == "cite" || name == "var" || name == "dfn") f.fmt.italic = true;
        else if (name == "u" || name == "ins") f.fmt.underline = true;
        else if (name == "s" || name == "strike" || name == "del") f.fmt.strike = true;
        else if (name == "code" || name == "kbd" || name == "samp" || name == "tt") f.fmt.code = true;
        else if (name == "mark") f.fmt.bg = quillColor(255, 241, 118);
        else if (name == "a") {
            auto it = attrs.find("href");
            if (it != attrs.end()) {
                std::string h = trim(it->second);
                std::string hl = lower(h);
                if (h.size() > 1 && h[0] == '#') f.fmt.link = h;   // in-page link -> in-note
                else if (hl.rfind("http", 0) == 0 && !srcBase_.empty() && h.find('#') != std::string::npos &&
                         h.find('#') + 1 < h.size() && stripFragment(h) == srcBase_)
                    f.fmt.link = h.substr(h.find('#'));                 // section of the same page -> in-note
                else if (hl.rfind("http", 0) == 0 || hl.rfind("mailto:", 0) == 0) f.fmt.link = h;
            }
        } else if (name == "font") {
            int r, g, b;
            auto it = attrs.find("color");
            if (it != attrs.end() && parseCssColor(it->second, r, g, b) && luma(r, g, b) >= 70 && luma(r, g, b) <= 232) f.fmt.color = quillColor(r, g, b);
        } else if (name.size() == 2 && name[0] == 'h' && name[1] >= '1' && name[1] <= '6') {
            if (parentFlex) f.fmt.bold = true;   // in side-by-side content (card title): bold text, not a line heading
            else {
                f.blockAttrs = RtJson::object();
                f.blockAttrs["header"] = std::min(3, name[1] - '0');
                f.fmt.size = 0;
            }
        } else if (name == "ul" || name == "ol") {
            f.list = true;
        } else if (name == "li") {
            f.blockAttrs = RtJson::object();
            f.blockAttrs["list"] = listType();
            int depth = listDepth();
            if (depth > 1) f.blockAttrs["indent"] = std::min(8, depth - 1);
        } else if (name == "blockquote") {
            f.blockAttrs = RtJson::object();
            f.blockAttrs["blockquote"] = true;
        } else if (name == "pre") {
            f.blockAttrs = RtJson::object();
            f.blockAttrs["code-block"] = true;
            f.pre = true;
        } else if (name == "td" || name == "th") {
            if (!cur_.chars.empty()) for (unsigned c : { ' ', '|', ' ' }) putChar(c, 0);
            if (name == "th") f.fmt.bold = true;
        } else if (name == "center") {
            f.blockAttrs = RtJson::object();
            f.blockAttrs["align"] = "center";
        }
        {
            if (!styleMap.empty()) applyStyle(f, styleMap, block);
            auto al = attrs.find("align");
            if (block && al != attrs.end()) {
                std::string a = lower(al->second);
                if (a == "center" || a == "right" || a == "justify") { if (!f.blockAttrs.is_object()) f.blockAttrs = RtJson::object(); f.blockAttrs["align"] = a; }
            }
        }
        if (inHeader() || (f.blockAttrs.is_object() && f.blockAttrs.contains("header"))) f.fmt.size = 0;   // heading size comes from the line
        if (f.pre || inPre()) f.fmt.code = false;   // code block line is already monospaced; the inline-code background would draw a separate box on every line
        if (f.pre || inPre()) {   // code block: syntax colors follow the page theme (dark theme) and are dropped
            f.fmt.color.clear(); f.fmt.bg.clear();
        }
        f.block = block || flexHere;
        if (!selfClose) stack_.push_back(f);
    }

    void close(const std::string &name)
    {
        int k = (int)stack_.size() - 1;
        while (k >= 0 && stack_[k].tag != name) k--;
        if (k < 0) return;   // no match
        // unclosed tags in between are closed too; if there is a block, the line ends before it is popped from the stack
        bool anyBlock = false;
        for (int j = k; j < (int)stack_.size(); j++) if (stack_[j].block && !stack_[j].skip) anyBlock = true;
        bool flexOuter = false;   // is there still side-by-side content outside the closed elements
        for (int j = 0; j < k; j++) if (stack_[j].flexRow && !stack_[j].skip) flexOuter = true;
        if (anyBlock && !skipping()) { if (flexOuter) flexGap(); else flush(false); }
        stack_.resize(k);
    }
};

} // namespace


// =================================================================== export (lines -> HTML)
static void putUtf8(std::string &o, unsigned c)
{
    if (c < 0x80) o += (char)c;
    else if (c < 0x800) { o += (char)(0xC0 | (c >> 6)); o += (char)(0x80 | (c & 0x3F)); }
    else if (c < 0x10000) { o += (char)(0xE0 | (c >> 12)); o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
    else { o += (char)(0xF0 | (c >> 18)); o += (char)(0x80 | ((c >> 12) & 0x3F)); o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
}

// ------------------------------------------------------------------ internal copy -> HTML (pasting into another app)
static void htmlEsc(std::string &o, const std::string &s)
{
    for (char c : s) {
        if (c == '&') o += "&amp;"; else if (c == '<') o += "&lt;"; else if (c == '>') o += "&gt;"; else if (c == '"') o += "&quot;";
        else o += c;
    }
}

// flutter_quill colors "#AARRGGBB" (sometimes "#RRGGBB"): CSS rgba()
static std::string cssColor(const RtJson &v)
{
    if (!v.is_string()) return "";
    std::string s = v.get<std::string>();
    if (!s.empty() && s[0] == '#') s.erase(0, 1);
    unsigned long x = 0;
    try { x = std::stoul(s, nullptr, 16); } catch (...) { return ""; }
    int a = 255, r, g, b;
    if (s.size() == 8) { a = (int)(x >> 24) & 255; r = (int)(x >> 16) & 255; g = (int)(x >> 8) & 255; b = (int)x & 255; }
    else if (s.size() == 6) { r = (int)(x >> 16) & 255; g = (int)(x >> 8) & 255; b = (int)x & 255; }
    else return "";
    char buf[64];
    if (a >= 255) snprintf(buf, sizeof(buf), "#%02x%02x%02x", r, g, b);
    else snprintf(buf, sizeof(buf), "rgba(%d,%d,%d,%.2f)", r, g, b, a / 255.0);
    return buf;
}

static bool jtrue(const RtJson &a, const char *k) { auto it = a.find(k); return it != a.end() && it->is_boolean() && it->get<bool>(); }
static std::string jstr(const RtJson &a, const char *k)
{
    auto it = a.find(k);
    if (it == a.end()) return "";
    if (it->is_string()) return it->get<std::string>();
    if (it->is_number()) { char b[32]; snprintf(b, sizeof(b), "%g", it->get<double>()); return b; }
    return "";
}

// Writes inline characters to HTML (runs with identical attributes become span / b / i ...)
static void htmlInline(std::string &o, const RtLine &line, const std::vector<RtJson> &attrs, const std::vector<RtJson> &embeds)
{
    const RtJson empty = RtJson::object();
    size_t i = 0, n = line.chars.size();
    while (i < n) {
        int aid = line.chars[i].attr;
        const RtJson &a = aid >= 0 && aid < (int)attrs.size() && attrs[(size_t)aid].is_object() ? attrs[(size_t)aid] : empty;
        if (line.chars[i].embed >= 0) {
            const RtJson &em = line.chars[i].embed < (int)embeds.size() ? embeds[(size_t)line.chars[i].embed] : empty;
            if (em.is_object() && em.contains("image") && em["image"].is_string()) {
                o += "<img src=\"";
                htmlEsc(o, em["image"].get<std::string>());
                o += "\"";
                std::string w = jstr(a, "width");
                if (!w.empty()) o += " width=\"" + w + "\"";
                o += ">";
            }
            i++;
            continue;
        }
        size_t j = i;
        std::string txt;
        while (j < n && line.chars[j].attr == aid && line.chars[j].embed < 0) { putUtf8(txt, (unsigned)line.chars[j].cp); j++; }
        std::string css;
        std::string c = cssColor(a.contains("color") ? a["color"] : RtJson()), bg = cssColor(a.contains("background") ? a["background"] : RtJson());
        if (!c.empty()) css += "color:" + c + ";";
        if (!bg.empty()) css += "background-color:" + bg + ";";
        std::string sz = jstr(a, "size");
        if (!sz.empty() && (isdigit((unsigned char)sz[0]))) css += "font-size:" + sz + "px;";
        std::string ff = jstr(a, "font");
        if (!ff.empty()) css += "font-family:'" + ff + "';";
        std::string open, close;
        std::string link = jstr(a, "link");
        if (!link.empty()) { open += "<a href=\""; htmlEsc(open, link); open += "\">"; close = "</a>" + close; }
        if (jtrue(a, "bold")) { open += "<b>"; close = "</b>" + close; }
        if (jtrue(a, "italic")) { open += "<i>"; close = "</i>" + close; }
        if (jtrue(a, "underline")) { open += "<u>"; close = "</u>" + close; }
        if (jtrue(a, "strike")) { open += "<s>"; close = "</s>" + close; }
        if (jtrue(a, "code")) { open += "<code>"; close = "</code>" + close; }
        if (!css.empty()) { open += "<span style=\"" + css + "\">"; close = "</span>" + close; }
        o += open;
        htmlEsc(o, txt);
        o += close;
        i = j;
    }
}

// Lines -> HTML: headings h1-h3, lists (nested by indent) ul / ol, to-dos ☐ / ☑, quote, code block, alignment
std::string rtLinesToHtml(const std::vector<RtLine> &lines, const std::vector<RtJson> &attrs, const std::vector<RtJson> &embeds,
                               const RtStyleSheet &ns)
{
    std::string o;
    // the note's font (the app's Roboto) and style metrics (font size, line height, spacing) are written
    // to every block; otherwise the target app uses its own default (Docs: Arial 11)
    auto blockCss = [&](int k) {
        const RtBlockStyle &bs = ns.b[k];
        char buf[256];
        snprintf(buf, sizeof(buf), "font-family:%s;font-size:%gpx;line-height:%g;margin-top:%gpx;margin-bottom:%gpx;",
                 k == RtStyleSheet::CODE ? "'Roboto Mono',monospace" : "Roboto,Arial,sans-serif",
                 roundf(bs.fs * 10) / 10, roundf(bs.lh * 100) / 100, roundf(bs.top), roundf(bs.bottom));
        return std::string(buf);
    };
    std::vector<std::string> lists;   // open list tags ("ul" / "ol"), depth = indent + 1
    auto closeTo = [&](size_t depth) { while (lists.size() > depth) { o += "</" + lists.back() + ">"; lists.pop_back(); } };
    bool inPre = false;
    for (const RtLine &L : lines) {
        const RtJson &b = L.block;
        std::string list = jstr(b, "list");
        int indent = 0;
        { std::string s = jstr(b, "indent"); if (!s.empty()) indent = std::max(0, atoi(s.c_str())); }
        bool code = jtrue(b, "code-block") || b.contains("code-block");
        if (inPre && !code) { o += "</pre>"; inPre = false; }
        if (list.empty()) closeTo(0);
        else {
            std::string tag = list == "ordered" ? "ol" : "ul";
            size_t depth = (size_t)indent + 1;
            closeTo(depth);
            if (lists.size() == depth && lists.back() != tag) closeTo(depth - 1);
            while (lists.size() < depth) { o += "<" + tag + ">"; lists.push_back(tag); }
        }
        std::string h = jstr(b, "header");
        int kind = code ? RtStyleSheet::CODE : h == "1" ? RtStyleSheet::H1 : h == "2" ? RtStyleSheet::H2 : h == "3" ? RtStyleSheet::H3 : RtStyleSheet::P;
        std::string style = code ? std::string() : blockCss(kind);
        std::string al = jstr(b, "align");
        if (!al.empty()) style += "text-align:" + al + ";";
        if (list.empty() && indent > 0) style += "margin-left:" + std::to_string(indent * 32) + "px;";
        std::string sa = style.empty() ? std::string() : " style=\"" + style + "\"";
        std::string inner;
        if (list == "checked") inner += "\xE2\x98\x91 ";        // ☑
        else if (list == "unchecked") inner += "\xE2\x98\x90 ";  // ☐
        htmlInline(inner, L, attrs, embeds);
        if (code) {
            if (!inPre) { o += "<pre style=\"" + blockCss(RtStyleSheet::CODE) + "\">"; inPre = true; }
            o += inner + "\n";
        } else if (!list.empty()) o += "<li" + sa + ">" + inner + "</li>";
        else if (h == "1" || h == "2" || h == "3") o += "<h" + h + sa + ">" + inner + "</h" + h + ">";
        else if (b.contains("blockquote")) o += "<blockquote" + sa + ">" + inner + "</blockquote>";
        else o += "<p" + sa + ">" + (inner.empty() ? std::string("<br>") : inner) + "</p>";
    }
    if (inPre) o += "</pre>";
    closeTo(0);
    return o;
}


std::vector<RtLine> rtHtmlToLines(const std::string &html, RichDoc &doc, const RtFetchImageFn &fetch)
{
    if (html.find('<') == std::string::npos) return {};
    size_t embeds0 = doc.embeds.size();
    Converter c(doc);
    std::vector<RtLine> out = c.run(html);
    if (out.empty()) { doc.embeds.resize(embeds0); return out; }
    // images: data: URIs as is (shrunk if too large); http(s) is downloaded only if fetch is provided.
    // An image that cannot be resolved is dropped: the line if it is on its own line, the character if inline.
    std::vector<bool> drop(out.size(), false);
    auto resolve = [&](RtJson &emb) -> bool {
        std::string src = emb.value("image", std::string());
        std::string sl = lower(src);
        std::string uri;
        if (sl.rfind("http", 0) == 0) {
            std::vector<unsigned char> bytes; std::string ext;
            if (fetch && fetch(src, bytes, ext) && !bytes.empty()) uri = rtImageDataUri(bytes.data(), bytes.size(), ext);
        } else if (sl.rfind("data:image/", 0) == 0) {
            const RtImage &im = rtImage(src);   // can it be resolved
            if (im.ok && std::max(im.w, im.h) <= 2048) uri = src;
            else if (im.ok) {
                size_t comma = src.find(',');
                std::vector<unsigned char> b = rtBase64Decode(src.substr(comma + 1));
                if (!b.empty()) uri = rtImageDataUri(b.data(), b.size(), ".png");
            }
        }
        if (uri.empty()) return false;
        emb["image"] = uri;
        return true;
    };
    for (size_t i = 0; i < out.size(); i++) {
        RtLine &L = out[i];
        if (L.chars.size() == 1 && L.chars[0].embed >= 0) {
            if (!resolve(doc.embeds[(size_t)L.chars[0].embed])) drop[i] = true;
            continue;
        }
        for (size_t k = 0; k < L.chars.size();) {
            if (L.chars[k].embed >= 0 && doc.embeds[(size_t)L.chars[k].embed].contains("image") &&
                !resolve(doc.embeds[(size_t)L.chars[k].embed])) { L.chars.erase(L.chars.begin() + (long)k); continue; }
            k++;
        }
    }
    doc.embedsVersion++;
    std::vector<RtLine> kept;
    for (size_t i = 0; i < out.size(); i++) if (!drop[i]) kept.push_back(out[i]);
    bool anyText = false;
    for (auto &l : kept) if (!l.chars.empty()) { anyText = true; break; }
    if (!anyText) return {};
    return kept;
}

bool RichDocHtml::import(RichDoc &doc, const std::string &html, const RtFetchImageFn &fetch)
{
    doc.clear();
    std::vector<RtLine> frag = rtHtmlToLines(html, doc, fetch);
    if (frag.empty()) return false;
    doc.insertLines({ 0, 0 }, frag);
    while (doc.lines.size() > 1 && doc.lines.back().chars.empty() && doc.lines.back().block.empty()) doc.lines.pop_back();
    return true;
}

std::string RichDocHtml::exportHtml(const RichDoc &doc, const RtStyleSheet &ss)
{
    std::vector<RtJson> attrs;
    for (int i = 0; i < doc.attrCount(); i++) attrs.push_back(doc.attr(i));
    return rtLinesToHtml(doc.lines, attrs, doc.embeds, ss);
}
