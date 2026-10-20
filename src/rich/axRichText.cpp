// axRichText.cpp - ported from NotePen richtext.cpp (collapse animation and heading link lines removed;
// theme is RtTheme, fonts RtFonts, image decoding via raylib LoadImageFromMemory)
#include "axRichText.h"
#include "axRichFonts.h"
#include <cstring>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <cmath>
#include <map>
#include <cctype>
#include <cstdio>

// =================================================================== theme / small helpers
static inline Color rtArgb(uint32_t v)
{
    return Color{ (unsigned char)((v >> 16) & 255), (unsigned char)((v >> 8) & 255), (unsigned char)(v & 255), (unsigned char)((v >> 24) & 255) };
}

static RtTheme g_rtTheme = [] { RtTheme t; t.apply(false); t.version = 0; return t; }();
RtTheme &rtTheme() { return g_rtTheme; }

void RtTheme::apply(bool d)
{
    dark = d;
    auto C = [&](Color l, Color k) { return d ? k : l; };
    page           = C({ 255, 255, 255, 255 }, { 30, 31, 34, 255 });
    ink            = C({ 0, 0, 0, 221 },       { 232, 233, 237, 255 });
    inkStrong      = C({ 0, 0, 0, 255 },       { 245, 246, 248, 255 });
    inkPlaceholder = C({ 0, 0, 0, 80 },        { 255, 255, 255, 80 });
    link           = C({ 25, 118, 210, 255 },  { 100, 170, 245, 255 });
    codeBg         = C({ 242, 242, 242, 255 }, { 44, 45, 50, 255 });
    inlineCodeBg   = C({ 0, 0, 0, 14 },        { 255, 255, 255, 24 });
    quoteBar       = C({ 200, 200, 200, 255 }, { 90, 92, 98, 255 });
    checkbox       = C({ 110, 110, 110, 255 }, { 160, 163, 170, 255 });
    checkboxOn     = C({ 66, 133, 244, 255 },  { 66, 150, 245, 255 });
    tableLine      = C({ 180, 180, 180, 255 }, { 85, 87, 93, 255 });
    caret          = C({ 20, 20, 20, 255 },    { 235, 236, 240, 255 });
    selection      = C({ 33, 150, 243, 60 },   { 66, 165, 245, 80 });
    guide          = C({ 0, 0, 0, 60 },        { 255, 255, 255, 60 });
    version++;
}

Color RtTheme::adapt(Color c) const
{
    if (!dark) return c;
    int mx = std::max({ c.r, c.g, c.b }), mn = std::min({ c.r, c.g, c.b });
    float l = 0.299f * c.r + 0.587f * c.g + 0.114f * c.b;
    if (l < 70 && mx - mn < 40) return Color{ ink.r, ink.g, ink.b, c.a };   // black / dark gray ink -> light
    return c;
}

std::string rtQuillColor(Color c)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "#%02x%02x%02x%02x", c.a, c.r, c.g, c.b);
    return buf;
}

std::string rtBase64Encode(const unsigned char *d, size_t n)
{
    static const char *T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string o;
    o.reserve((n + 2) / 3 * 4);
    size_t i = 0;
    for (; i + 2 < n; i += 3) {
        unsigned v = (d[i] << 16) | (d[i + 1] << 8) | d[i + 2];
        o += T[(v >> 18) & 63]; o += T[(v >> 12) & 63]; o += T[(v >> 6) & 63]; o += T[v & 63];
    }
    if (i < n) {
        unsigned v = d[i] << 16;
        if (i + 1 < n) v |= d[i + 1] << 8;
        o += T[(v >> 18) & 63]; o += T[(v >> 12) & 63];
        o += (i + 1 < n) ? T[(v >> 6) & 63] : '=';
        o += '=';
    }
    return o;
}

std::vector<unsigned char> rtBase64Decode(const std::string &s)
{
    static int map[256];
    static bool init = false;
    if (!init) {
        for (int &m : map) m = -1;
        const char *T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int k = 0; k < 64; k++) map[(unsigned char)T[k]] = k;
        map[(unsigned char)'-'] = 62; map[(unsigned char)'_'] = 63;   // base64url
        init = true;
    }
    std::vector<unsigned char> out;
    out.reserve(s.size() * 3 / 4);
    unsigned v = 0;
    int bits = 0;
    for (unsigned char c : s) {
        int m = map[c];
        if (m < 0) continue;   // '=', space, end of line
        v = (v << 6) | (unsigned)m;
        bits += 6;
        if (bits >= 8) { bits -= 8; out.push_back((unsigned char)((v >> bits) & 255)); }
    }
    return out;
}



// =================================================================== helpers
bool rtParseQuillColor(const std::string &s0, Color &out)
{
    std::string s = s0;
    if (s.empty()) return false;
    static const std::map<std::string, uint32_t> named = {
        {"black", 0xFF000000}, {"white", 0xFFFFFFFF}, {"red", 0xFFFF0000}, {"green", 0xFF008000},
        {"blue", 0xFF0000FF}, {"yellow", 0xFFFFFF00}, {"orange", 0xFFFFA500}, {"purple", 0xFF800080},
        {"gray", 0xFF808080}, {"grey", 0xFF808080}, {"transparent", 0x00000000},
    };
    auto it = named.find(s);
    if (it != named.end()) { out = rtArgb(it->second); return true; }
    if (s[0] == '#') s = s.substr(1);
    else if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0) s = s.substr(2);
    if (s.empty() || s.size() > 8) return false;
    for (char c : s) if (!isxdigit((unsigned char)c)) return false;
    uint32_t v = (uint32_t)std::stoul(s, nullptr, 16);
    if (s.size() == 6) { out = rtArgb(0xFF000000u | v); return true; }
    if (s.size() == 8) { out = rtArgb(v); return true; } // flutter_quill: #AARRGGBB
    if (s.size() == 3) {
        uint32_t r = (v >> 8) & 0xF, g = (v >> 4) & 0xF, b = v & 0xF;
        out = Color{ (unsigned char)(r * 17), (unsigned char)(g * 17), (unsigned char)(b * 17), 255 };
        return true;
    }
    return false;
}

static RtStyle styleFromAttrs(const RtJson &a)
{
    RtStyle st;
    if (!a.is_object()) return st;
    auto b = [&](const char *k) { return a.contains(k) && a[k].is_boolean() && a[k].get<bool>(); };
    st.bold = b("bold");
    st.italic = b("italic");
    st.underline = b("underline");
    st.strike = b("strike");
    st.code = b("code");
    if (a.contains("font") && a["font"].is_string()) st.font = a["font"].get<std::string>();
    if (a.contains("link")) { st.link = true; st.underline = true; st.hasColor = true; st.color = Color{ 25, 118, 210, 255 }; }
    if (a.contains("color") && a["color"].is_string()) st.hasColor = rtParseQuillColor(a["color"].get<std::string>(), st.color) || st.hasColor;
    if (a.contains("background") && a["background"].is_string()) st.hasBg = rtParseQuillColor(a["background"].get<std::string>(), st.bg);
    if (a.contains("size")) {
        const RtJson &s = a["size"];
        if (s.is_number()) st.size = s.get<float>();
        else if (s.is_string()) {
            std::string v = s.get<std::string>();
            if (v == "small") st.size = 16;
            else if (v == "large") st.size = 24;
            else if (v == "huge") st.size = 32;
            else { try { st.size = std::stof(v); } catch (...) { st.size = 0; } }
        }
    }
    return st;
}

static void appendUtf8(std::string &out, int cp)
{
    if (cp < 0x80) out += (char)cp;
    else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
    else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
}

static int nextCodepoint(const std::string &s, size_t &i)
{
    unsigned char c = (unsigned char)s[i];
    int cp, extra;
    if (c < 0x80) { i++; return c; }
    if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
    else { i++; return 0xFFFD; }
    i++;
    for (int k = 0; k < extra && i < s.size(); k++, i++) {
        if (((unsigned char)s[i] & 0xC0) != 0x80) return 0xFFFD;
        cp = (cp << 6) | ((unsigned char)s[i] & 0x3F);
    }
    return cp;
}

static bool isWordCp(int cp)
{
    if (cp >= 128) return cp != 0xA0 && cp != 0xFFFC && !(cp >= 0x2000 && cp <= 0x206F);
    return isalnum(cp) || cp == '_';
}

// =================================================================== RichDoc
void RichDoc::clear()
{
    lines.clear();
    lines.emplace_back();
    embeds.clear();
    embedsVersion++;
    attrs_.clear(); styles_.clear(); attrIndex_.clear();
    internAttr(RtJson::object()); // id 0 = empty
}

int RichDoc::internAttr(const RtJson &a)
{
    // key-order-independent identity: dump with sorted (std::map-based) json
    nlohmann::json sorted = nlohmann::json::object();
    if (a.is_object()) for (auto it = a.begin(); it != a.end(); ++it) if (!it.value().is_null()) sorted[it.key()] = it.value();
    std::string key = sorted.dump();
    auto it = attrIndex_.find(key);
    if (it != attrIndex_.end()) return it->second;
    int id = (int)attrs_.size();
    RtJson ord = RtJson::object();
    for (auto jt = sorted.begin(); jt != sorted.end(); ++jt) ord[jt.key()] = jt.value();
    attrs_.push_back(ord);
    styles_.push_back(styleFromAttrs(ord));
    attrIndex_[key] = id;
    return id;
}

void RichDoc::fromDelta(const RtJson &ops)
{
    clear();
    if (!ops.is_array()) return;
    for (auto &op : ops) {
        if (!op.is_object() || !op.contains("insert")) continue;
        const RtJson &ins = op["insert"];
        RtJson attrs = op.contains("attributes") && op["attributes"].is_object() ? op["attributes"] : RtJson::object();
        if (ins.is_string()) {
            const std::string s = ins.get<std::string>();
            int aid = -1;
            size_t i = 0;
            while (i < s.size()) {
                int cp = nextCodepoint(s, i);
                if (cp == '\n') {
                    lines.back().block = attrs;
                    lines.emplace_back();
                } else if (cp != '\r') {
                    if (aid < 0) aid = internAttr(attrs);
                    RtChar c; c.cp = cp; c.attr = aid;
                    lines.back().chars.push_back(c);
                }
            }
        } else if (ins.is_object()) {
            embeds.push_back(ins); embedsVersion++;
            RtChar c; c.cp = 0xFFFC; c.attr = internAttr(attrs); c.embed = (int)embeds.size() - 1;
            lines.back().chars.push_back(c);
        }
    }
    // a Quill document ends with '\n'; the final empty line is virtual
    if (lines.size() > 1 && lines.back().chars.empty() && lines.back().block.empty()) lines.pop_back();
}

RtJson RichDoc::toDelta() const
{
    RtJson ops = RtJson::array();
    auto pushStr = [&](const std::string &text, int aid, const RtJson *explicitAttrs) {
        const RtJson &a = explicitAttrs ? *explicitAttrs : attrs_[aid];
        bool hasA = a.is_object() && !a.empty();
        if (!ops.empty()) {
            RtJson &last = ops.back();
            if (last["insert"].is_string()) {
                bool lastHas = last.contains("attributes");
                if ((!hasA && !lastHas) || (hasA && lastHas && last["attributes"] == a)) {
                    last["insert"] = last["insert"].get<std::string>() + text;
                    return;
                }
            }
        }
        RtJson op = RtJson::object();
        op["insert"] = text;
        if (hasA) op["attributes"] = a;
        ops.push_back(op);
    };
    for (auto &L : lines) {
        std::string run;
        int runAttr = -1;
        for (auto &c : L.chars) {
            if (c.embed >= 0) {
                if (!run.empty()) { pushStr(run, runAttr, nullptr); run.clear(); }
                runAttr = -1;
                RtJson op = RtJson::object();
                op["insert"] = c.embed < (int)embeds.size() ? embeds[c.embed] : RtJson::object();
                if (!attrs_[c.attr].empty()) op["attributes"] = attrs_[c.attr];
                ops.push_back(op);
                continue;
            }
            if (c.attr != runAttr && !run.empty()) { pushStr(run, runAttr, nullptr); run.clear(); }
            runAttr = c.attr;
            appendUtf8(run, c.cp);
        }
        if (!run.empty()) pushStr(run, runAttr, nullptr);
        pushStr("\n", 0, &L.block);
    }
    return ops;
}

bool RichDoc::isEmpty() const
{
    for (auto &l : lines) if (!l.chars.empty()) return false;
    return lines.size() <= 1 && (lines.empty() || lines[0].block.empty());
}

std::string RichDoc::plainText(RtPos a, RtPos b) const
{
    if (b < a) std::swap(a, b);
    a = clamp(a); b = clamp(b);
    std::string out;
    for (int l = a.line; l <= b.line; l++) {
        const auto &ch = lines[l].chars;
        int s = (l == a.line) ? a.col : 0;
        int e = (l == b.line) ? b.col : (int)ch.size();
        for (int i = s; i < e; i++) if (ch[i].embed < 0) appendUtf8(out, ch[i].cp);
        if (l != b.line) out += '\n';
    }
    return out;
}

std::string RichDoc::plainText() const { return plainText({ 0, 0 }, endPos()); }

RtPos RichDoc::clamp(RtPos p) const
{
    p.line = std::clamp(p.line, 0, (int)lines.size() - 1);
    p.col = std::clamp(p.col, 0, (int)lines[p.line].chars.size());
    return p;
}

RtPos RichDoc::splitLine(RtPos p)
{
    p = clamp(p);
    RtLine &L = lines[p.line];
    RtLine nl;
    nl.chars.assign(L.chars.begin() + p.col, L.chars.end());
    L.chars.erase(L.chars.begin() + p.col, L.chars.end());
    nl.block = L.block;
    lines.insert(lines.begin() + p.line + 1, nl);
    return { p.line + 1, 0 };
}

RtPos RichDoc::insertText(RtPos p, const std::string &s, int attrId)
{
    p = clamp(p);
    size_t i = 0;
    while (i < s.size()) {
        int cp = nextCodepoint(s, i);
        if (cp == '\r') continue;
        if (cp == '\n') { p = splitLine(p); continue; }
        if (cp == '\t') cp = ' ';
        RtChar c; c.cp = cp; c.attr = attrId;
        auto &ch = lines[p.line].chars;
        ch.insert(ch.begin() + p.col, c);
        p.col++;
    }
    return p;
}

std::vector<RtLine> RichDoc::extract(RtPos a, RtPos b) const
{
    if (b < a) std::swap(a, b);
    a = clamp(a); b = clamp(b);
    std::vector<RtLine> out;
    for (int l = a.line; l <= b.line; l++) {
        RtLine x;
        const auto &ch = lines[l].chars;
        int s = (l == a.line) ? a.col : 0;
        int e = (l == b.line) ? b.col : (int)ch.size();
        x.chars.assign(ch.begin() + s, ch.begin() + e);
        if (l != b.line) x.block = lines[l].block;
        out.push_back(x);
    }
    return out;
}

RtPos RichDoc::insertLines(RtPos p, const std::vector<RtLine> &frag)
{
    p = clamp(p);
    if (frag.empty()) return p;
    RtLine &L = lines[p.line];
    if (frag.size() == 1) {
        L.chars.insert(L.chars.begin() + p.col, frag[0].chars.begin(), frag[0].chars.end());
        return { p.line, p.col + (int)frag[0].chars.size() };
    }
    std::vector<RtChar> tail(L.chars.begin() + p.col, L.chars.end());
    RtJson origBlock = L.block;
    L.chars.erase(L.chars.begin() + p.col, L.chars.end());
    L.chars.insert(L.chars.end(), frag[0].chars.begin(), frag[0].chars.end());
    L.block = frag[0].block;
    int at = p.line + 1;
    for (size_t i = 1; i + 1 < frag.size(); i++) lines.insert(lines.begin() + at++, frag[i]);
    RtLine last;
    last.chars = frag.back().chars;
    int col = (int)last.chars.size();
    last.chars.insert(last.chars.end(), tail.begin(), tail.end());
    last.block = origBlock;
    lines.insert(lines.begin() + at, last);
    return { at, col };
}

RtPos RichDoc::erase(RtPos a, RtPos b)
{
    if (b < a) std::swap(a, b);
    a = clamp(a); b = clamp(b);
    if (a == b) return a;
    if (a.line == b.line) {
        auto &ch = lines[a.line].chars;
        ch.erase(ch.begin() + a.col, ch.begin() + b.col);
        return a;
    }
    RtLine &La = lines[a.line];
    const RtLine &Lb = lines[b.line];
    La.chars.erase(La.chars.begin() + a.col, La.chars.end());
    La.chars.insert(La.chars.end(), Lb.chars.begin() + b.col, Lb.chars.end());
    lines.erase(lines.begin() + a.line + 1, lines.begin() + b.line + 1);
    return a;
}

int RichDoc::attrWith(int base, const std::string &key, const RtJson &value)
{
    RtJson j = attrs_[base];
    if (value.is_null() || (value.is_boolean() && !value.get<bool>())) j.erase(key);
    else j[key] = value;
    return internAttr(j);
}

void RichDoc::formatInline(RtPos a, RtPos b, const std::string &key, const RtJson &value)
{
    if (b < a) std::swap(a, b);
    a = clamp(a); b = clamp(b);
    for (int l = a.line; l <= b.line; l++) {
        auto &ch = lines[l].chars;
        int s = (l == a.line) ? a.col : 0;
        int e = (l == b.line) ? b.col : (int)ch.size();
        for (int i = s; i < e; i++) ch[i].attr = attrWith(ch[i].attr, key, value);
    }
}

void RichDoc::formatBlock(int l0, int l1, const std::string &key, const RtJson &value)
{
    if (l1 < l0) std::swap(l0, l1);
    l0 = std::clamp(l0, 0, (int)lines.size() - 1);
    l1 = std::clamp(l1, 0, (int)lines.size() - 1);
    for (int l = l0; l <= l1; l++) {
        RtJson &b = lines[l].block;
        if (value.is_null() || (value.is_boolean() && !value.get<bool>())) b.erase(key);
        else b[key] = value;
    }
}

int RichDoc::attrAt(RtPos p) const
{
    p = clamp(p);
    const auto &ch = lines[p.line].chars;
    int a = 0;
    if (p.col > 0) a = ch[p.col - 1].attr;
    else if (!ch.empty()) a = ch[0].attr;
    return a;
}

RtPos RichDoc::prevPos(RtPos p) const
{
    p = clamp(p);
    if (p.col > 0) return { p.line, p.col - 1 };
    if (p.line > 0) return { p.line - 1, (int)lines[p.line - 1].chars.size() };
    return p;
}

RtPos RichDoc::nextPos(RtPos p) const
{
    p = clamp(p);
    if (p.col < (int)lines[p.line].chars.size()) return { p.line, p.col + 1 };
    if (p.line + 1 < (int)lines.size()) return { p.line + 1, 0 };
    return p;
}

RtPos RichDoc::wordStart(RtPos p) const
{
    p = clamp(p);
    const auto &ch = lines[p.line].chars;
    int c = p.col;
    while (c > 0 && !isWordCp(ch[c - 1].cp)) c--;
    while (c > 0 && isWordCp(ch[c - 1].cp)) c--;
    if (c == p.col && p.col == 0 && p.line > 0) return prevPos(p);
    return { p.line, c };
}

RtPos RichDoc::wordEnd(RtPos p) const
{
    p = clamp(p);
    const auto &ch = lines[p.line].chars;
    int c = p.col, n = (int)ch.size();
    while (c < n && !isWordCp(ch[c].cp)) c++;
    while (c < n && isWordCp(ch[c].cp)) c++;
    if (c == p.col && c == n && p.line + 1 < (int)lines.size()) return nextPos(p);
    return { p.line, c };
}

// =================================================================== layout
struct BlockMetrics { float fs, lh, top, bottom; bool bold; Color color; };

// flutter_quill "line-height" block attribute (1.0 / 1.15 / 1.5 / 2.0); 0 if absent
float rtLineHeightAttr(const RtJson &b)
{
    if (!b.contains("line-height")) return 0;
    const RtJson &v = b["line-height"];
    float f = 0;
    if (v.is_number()) f = v.get<float>();
    else if (v.is_string()) f = (float)atof(v.get<std::string>().c_str());
    return f > 0 ? std::clamp(f, 0.5f, 4.0f) : 0;
}

// ---- note style
RtStyleSheet::RtStyleSheet()
{
    b[H1] = { 32, 1.3f, 0, 0 };   // no extra top / bottom spacing on headings (can be set from the note style panel)
    b[H2] = { 24, 1.3f, 0, 0 };
    b[H3] = { 20, 1.3f, 0, 0 };
    b[P] = { 16, 1.5f, 0, 0 };
    b[CODE] = { 14, 1.4f, 0, 0 };
}

const char *RtStyleSheet::key(int k)
{
    static const char *keys[] = { "h1", "h2", "h3", "p", "code" };
    return k >= 0 && k < COUNT ? keys[k] : "";
}

RtStyleSheet RtStyleSheet::fromJson(const RtJson &j)
{
    RtStyleSheet s;
    if (!j.is_object()) return s;
    for (int k = 0; k < COUNT; k++) {
        if (!j.contains(key(k)) || !j[key(k)].is_object()) continue;
        const RtJson &o = j[key(k)];
        auto num = [&](const char *n, float &v, float lo, float hi) {
            if (o.contains(n) && o[n].is_number()) v = std::clamp(o[n].get<float>(), lo, hi);
        };
        num("size", s.b[k].fs, 6, 96);
        num("lineHeight", s.b[k].lh, 0.8f, 4.0f);
        num("top", s.b[k].top, 0, 96);
        num("bottom", s.b[k].bottom, 0, 96);
    }
    return s;
}

RtJson RtStyleSheet::toJson() const
{
    RtStyleSheet d;
    RtJson j = RtJson::object();
    auto r2 = [](float v) { return std::round((double)v * 100.0) / 100.0; };   // 1.3f -> 1.3 (not 1.2999...)
    for (int k = 0; k < COUNT; k++) {
        RtJson o = RtJson::object();
        if (fabsf(b[k].fs - d.b[k].fs) > 0.001f) o["size"] = r2(b[k].fs);
        if (fabsf(b[k].lh - d.b[k].lh) > 0.001f) o["lineHeight"] = r2(b[k].lh);
        if (fabsf(b[k].top - d.b[k].top) > 0.001f) o["top"] = r2(b[k].top);
        if (fabsf(b[k].bottom - d.b[k].bottom) > 0.001f) o["bottom"] = r2(b[k].bottom);
        if (!o.empty()) j[key(k)] = o;
    }
    return j.empty() ? RtJson() : j;
}

bool RtStyleSheet::operator==(const RtStyleSheet &o) const
{
    for (int k = 0; k < COUNT; k++)
        if (fabsf(b[k].fs - o.b[k].fs) > 0.001f || fabsf(b[k].lh - o.b[k].lh) > 0.001f ||
            fabsf(b[k].top - o.b[k].top) > 0.001f || fabsf(b[k].bottom - o.b[k].bottom) > 0.001f) return false;
    return true;
}

static BlockMetrics blockMetricsBase(const RtJson &b, const RtStyleSheet &st);
static BlockMetrics blockMetrics(const RtJson &b, const RtStyleSheet &st)
{
    BlockMetrics m = blockMetricsBase(b, st);
    if (float lh = rtLineHeightAttr(b)) {
        m.lh = lh;   // same as flutter_quill: line height = font size × value (paragraph 1.5 when the attribute is absent)
        // spacing between blocks also scales with line spacing: 0 at 1.0 (lines touch), full at 1.5 and above
        float k = std::clamp((lh - 1.0f) / 0.5f, 0.0f, 1.0f);
        m.top *= k; m.bottom *= k;
    }
    return m;
}

// block type metrics: from the note style (RtStyleSheet; defaults H1 32/1.3/0/0, H2 24/1.3/0/0, H3 20/1.3/0/0,
// paragraph 16/1.5/0/0, code 14/1.4/0/0). Headings 4-6 use paragraph metrics.
static BlockMetrics blockMetricsBase(const RtJson &b, const RtStyleSheet &st)
{
    int header = b.contains("header") && b["header"].is_number() ? b["header"].get<int>() : 0;
    auto mk = [&](int k, bool bold, Color c) {
        const RtBlockStyle &s = st.b[k];
        return BlockMetrics{ s.fs, s.lh, s.top, s.bottom, bold, c };
    };
    switch (header) {
    case 1: return mk(RtStyleSheet::H1, true, rtTheme().inkStrong);
    case 2: return mk(RtStyleSheet::H2, true, rtTheme().ink);
    case 3: return mk(RtStyleSheet::H3, true, rtTheme().ink);
    default: break;
    }
    if (b.contains("code-block")) return mk(RtStyleSheet::CODE, false, rtTheme().ink);
    return mk(RtStyleSheet::P, false, rtTheme().ink);
}

static std::string blockStr(const RtJson &b, const char *k)
{
    return b.contains(k) && b[k].is_string() ? b[k].get<std::string>() : std::string();
}

static bool isImageEmbed(const RichDoc &doc, const RtChar &c)
{
    if (c.embed < 0 || c.embed >= (int)doc.embeds.size()) return false;
    const RtJson &e = doc.embeds[c.embed];
    return e.is_object() && e.contains("image");
}

// on-screen size of an image embed: the embed's "width" attribute (Quill / flutter_quill; "style": "width: N" is also read),
// otherwise natural width; at most maxW. 240×120 placeholder if it fails to load (ok = false)
static bool imageBox(const RichDoc &doc, const RtChar &c, float maxW, float &w, float &h, size_t &key)
{
    const RtJson &e = doc.embeds[c.embed];
    std::string src = e["image"].is_string() ? e["image"].get<std::string>() : std::string();
    key = 0;
    const RtImage &im = rtImage(src, &key);
    if (!im.ok) { w = std::min(maxW, 240.0f); h = 120; return false; }
    float want = (float)im.w;
    const RtJson &ea = doc.attr(c.attr);
    auto num = [](const RtJson &v) -> float {
        if (v.is_number()) return v.get<float>();
        if (v.is_string()) return (float)std::atof(v.get<std::string>().c_str());
        return 0.0f;
    };
    if (ea.contains("width")) { float aw = num(ea["width"]); if (aw > 0) want = aw; }
    else if (ea.contains("style") && ea["style"].is_string()) {
        std::string st = ea["style"].get<std::string>();
        size_t k = st.find("width:");
        if (k != std::string::npos) { float aw = (float)std::atof(st.c_str() + k + 6); if (aw > 0) want = aw; }
    }
    w = std::max(4.0f, std::min(maxW, want));
    h = w * im.h / std::max(1, im.w);
    return true;
}

// =================================================================== collapsible headings
int RichDoc::headerLevel(int li) const
{
    if (li < 0 || li >= (int)lines.size()) return 0;
    const RtJson &b = lines[(size_t)li].block;
    if (!b.contains("header") || !b["header"].is_number()) return 0;
    int h = b["header"].get<int>();
    return h >= 1 && h <= 6 ? h : 0;
}

bool RichDoc::isCollapsed(int li) const
{
    if (!headerLevel(li)) return false;
    const RtJson &b = lines[(size_t)li].block;
    return b.contains("np-collapsed") && b["np-collapsed"].is_boolean() && b["np-collapsed"].get<bool>();
}

int RichDoc::sectionEnd(int li) const
{
    int h = headerLevel(li);
    if (!h) return li;
    int j = li + 1;
    while (j < (int)lines.size()) {
        int hj = headerLevel(j);
        if (hj && hj <= h) break;
        j++;
    }
    return j - 1;
}

std::vector<int> RichDoc::hiddenOwners() const
{
    std::vector<int> own(lines.size(), -1);
    int level = 0, owner = -1;   // level / line of the heading that hides it
    for (int li = 0; li < (int)lines.size(); li++) {
        int h = headerLevel(li);
        if (level) {
            if (h && h <= level) { level = 0; owner = -1; }
            else { own[(size_t)li] = owner; continue; }
        }
        if (isCollapsed(li)) { level = h; owner = li; }
    }
    return own;
}

void RichDoc::setCollapsed(int li, bool on)
{
    if (!headerLevel(li)) return;
    if (on) lines[(size_t)li].block["np-collapsed"] = true;
    else lines[(size_t)li].block.erase("np-collapsed");
}

std::string RichDoc::lineText(int li) const
{
    if (li < 0 || li >= (int)lines.size()) return std::string();
    return plainText(RtPos{ li, 0 }, RtPos{ li, (int)lines[(size_t)li].chars.size() });
}

// link id from heading text: lowercase, non-letter/digit -> '-', leading / trailing '-' trimmed
std::string rtHeadingSlug(const std::string &text)
{
    std::string out;
    bool dash = false;
    for (unsigned char c : text) {
        bool word = std::isalnum(c) || c >= 0x80;   // UTF-8 multi-byte letters kept as is
        if (word) { out += (char)(c < 0x80 ? std::tolower(c) : c); dash = false; }
        else if (!dash && !out.empty()) { out += '-'; dash = true; }
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    return out;
}

int RichDoc::anchorLine(const std::string &idIn) const
{
    std::string id = idIn;
    if (!id.empty() && id[0] == '#') id.erase(0, 1);
    if (id.empty()) return -1;
    for (int li = 0; li < (int)lines.size(); li++) {
        const RtJson &b = lines[(size_t)li].block;
        if (b.contains("np-id") && b["np-id"].is_string() && b["np-id"].get<std::string>() == id) return li;
    }
    std::string want = rtHeadingSlug(id);
    for (int li = 0; li < (int)lines.size(); li++)
        if (headerLevel(li) && rtHeadingSlug(lineText(li)) == want) return li;
    return -1;
}

bool RichDoc::reveal(int li)
{
    bool changed = false;
    for (int guard = 0; guard < 16; guard++) {   // nested collapsed headings: outer to inner
        std::vector<int> own = hiddenOwners();
        if (li < 0 || li >= (int)own.size() || own[(size_t)li] < 0) break;
        setCollapsed(own[(size_t)li], false);
        changed = true;
    }
    return changed;
}

void RichTextView::layout(float docWidth, const std::function<float(int)> &anchorHeight)
{
    if (themeVer_ != rtTheme().version) { themeVer_ = rtTheme().version; dirty_ = true; }   // default colors come from the theme
    if (fontGen_ != rtFonts().generation()) { fontGen_ = rtFonts().generation(); dirty_ = true; }   // font family
    if (!dirty_ && fabsf(docWidth - layoutWidth_) < 0.5f) return;
    layoutWidth_ = docWidth;
    dirty_ = false;
    items_.clear();
    vlines_.clear();

    const float pad = padding;
    float y = pad;
    // baseline position within the glyph box (Roboto ≈ 0.79); used to align different sizes and seat images
    const float ascR = std::clamp(rtFonts().ascent(100.0f, RFV_REGULAR) / 100.0f, 0.6f, 0.95f);
    std::map<int, int> orderedCounters;
    std::string prevList;
    std::vector<float> xsTmp;

    const std::vector<int> hiddenBy = doc.hiddenOwners();
    // heading hierarchy: a section body aligns with the heading's text, subheadings start at the parent heading's text indent
    struct SecLvl { int level; float textOff; };
    std::vector<SecLvl> secStack;
    for (int li = 0; li < (int)doc.lines.size(); li++) {
        const RtLine &L = doc.lines[li];
        const RtJson &B = L.block;
        BlockMetrics bm = blockMetrics(B, style_);
        const int hdr = collapsibleHeaders ? doc.headerLevel(li) : 0;
        if (hdr) while (!secStack.empty() && secStack.back().level >= hdr) secStack.pop_back();
        const float secOff = secStack.empty() ? 0.0f : secStack.back().textOff;
        const float toggleW = hdr ? roundf(bm.fs * 0.75f) : 0.0f;
        if (hdr) secStack.push_back({ hdr, secOff + toggleW });
        if (collapsibleHeaders && hiddenBy[(size_t)li] >= 0) continue;   // under a collapsed heading: no layout (cursor, hit testing, drawing skip it)
        y += bm.top;
        int indent = B.contains("indent") && B["indent"].is_number() ? B["indent"].get<int>() : 0;
        std::string list = blockStr(B, "list");
        std::string align = blockStr(B, "align");
        bool code = B.contains("code-block"), quote = B.contains("blockquote");

        const float secX = pad + secOff;   // section hierarchy indent
        float indentX = secX + indent * 32.0f + (quote ? 16.0f : 0.0f);
        float x0 = indentX + (list.empty() ? 0.0f : 32.0f);
        // heading: fold arrow on the left, text starts right of the arrow; the body starts at this text indent (secStack)
        const bool hasSection = hdr && (doc.isCollapsed(li) || doc.sectionEnd(li) > li);
        x0 += toggleW;
        float maxW = std::max(40.0f, docWidth - pad - x0);

        if (list == "ordered") {
            if (prevList != "ordered") orderedCounters.clear();
            orderedCounters[indent]++;
            for (auto it = orderedCounters.begin(); it != orderedCounters.end();)
                if (it->first > indent) it = orderedCounters.erase(it); else ++it;
        } else orderedCounters.clear();
        prevList = list;

        // ---- block embed line (when the line holds only a single table / anchor embed; images flow inline)
        if (L.chars.size() == 1 && L.chars[0].embed >= 0 && !isImageEmbed(doc, L.chars[0])) {
            const RtJson &e = doc.embeds[L.chars[0].embed];
            Item it; it.x = x0; it.y = y; it.w = maxW; it.line = li;
            if (e.contains("flex-inline-table")) {
                RtJson d = e["flex-inline-table"];
                if (d.is_string()) { try { d = RtJson::parse(d.get<std::string>()); } catch (...) { d = RtJson(); } }
                int r = d.is_object() ? d.value("r", 1) : 1;
                it.kind = Item::INLINE_TABLE; it.h = std::max(1, r) * 32.0f; it.data = d;
            } else if (e.contains("flex-table")) {
                int id = 0;
                const RtJson &d = e["flex-table"];
                if (d.is_string()) { try { id = std::stoi(d.get<std::string>()); } catch (...) {} }
                else if (d.is_number()) id = d.get<int>();
                float h = anchorHeight ? anchorHeight(id) : 24.0f;
                it.kind = Item::OTHER_PH; it.h = h > 0 ? h : 24.0f;
            } else {
                it.kind = Item::OTHER_PH; it.h = 24; it.text = "[embed]";
            }
            items_.push_back(it);
            VLine vl; vl.line = li; vl.c0 = 0; vl.c1 = 1; vl.y = y; vl.h = it.h; vl.textY = y; vl.embedLine = true;
            vl.caretY = y; vl.caretH = it.h;
            vl.xs = { x0, x0 + std::min(it.w, 24.0f) };
            vlines_.push_back(vl);
            y += it.h + bm.bottom;
            continue;
        }

        // ---- character widths (consecutive characters with identical attributes are measured as one run)
        const int n = (int)L.chars.size();
        std::vector<float> w(n, 0.0f), fsz(n, bm.fs), imgH(n, 0.0f);
        std::vector<size_t> imgKey(n, 0);
        std::vector<char> imgOk(n, 0);
        {
            int i = 0;
            while (i < n) {
                int j = i;
                if (L.chars[i].embed >= 0) {
                    if (isImageEmbed(doc, L.chars[i])) {
                        float iw, ih; size_t key;
                        imgOk[i] = imageBox(doc, L.chars[i], maxW, iw, ih, key);
                        w[i] = iw; imgH[i] = ih; imgKey[i] = key;
                    } else w[i] = 18;
                    fsz[i] = bm.fs; i++; continue;
                }
                while (j < n && L.chars[j].embed < 0 && L.chars[j].attr == L.chars[i].attr) j++;
                const RtStyle &st = doc.style(L.chars[i].attr);
                float fs = st.size > 0 ? st.size : bm.fs;
                std::string s;
                for (int k = i; k < j; k++) appendUtf8(s, L.chars[k].cp);
                RtFontFamilyGuard fg(st.font);
                float total = rtFonts().advances(s, fs, rtFontVariant(st.bold || bm.bold, st.italic, st.code || code), xsTmp);
                for (int k = i; k < j; k++) {
                    int idx = k - i;
                    float next = (idx + 1 < (int)xsTmp.size()) ? xsTmp[idx + 1] : total;
                    w[k] = next - xsTmp[idx];
                    fsz[k] = fs;
                }
                i = j;
            }
        }

        // ---- line breaking (word-based)
        struct Seg { int c0, c1; };
        std::vector<Seg> segs;
        {
            int start = 0;
            float x = 0;
            int i = 0;
            while (i < n) {
                // word: non-spaces + the spaces that follow
                int j = i;
                if (L.chars[i].embed >= 0) j = i + 1;   // an embed (image) is a "word" by itself
                else while (j < n && L.chars[j].cp != ' ' && L.chars[j].embed < 0) j++;
                int k = j;
                while (k < n && L.chars[k].cp == ' ') k++;
                float wordW = 0; for (int q = i; q < j; q++) wordW += w[q];
                float spaceW = 0; for (int q = j; q < k; q++) spaceW += w[q];
                if (x + wordW > maxW && i > start) {
                    segs.push_back({ start, i });
                    start = i; x = 0;
                }
                if (wordW > maxW) {
                    // a single word does not fit on the line: break character by character
                    for (int q = i; q < j; q++) {
                        if (x + w[q] > maxW && q > start) { segs.push_back({ start, q }); start = q; x = 0; }
                        x += w[q];
                    }
                    x += spaceW;
                } else x += wordW + spaceW;
                i = k;
            }
            segs.push_back({ start, n });
        }

        float blockTop = y;
        for (size_t si = 0; si < segs.size(); si++) {
            Seg sg = segs[si];
            float fsLine = 0;
            float maxImgH = 0;
            bool hasText = false;
            for (int q = sg.c0; q < sg.c1; q++) {
                if (L.chars[q].embed >= 0) maxImgH = std::max(maxImgH, imgH[q]);
                else { fsLine = std::max(fsLine, fsz[q]); hasText = true; }
            }
            if (fsLine <= 0) fsLine = bm.fs;
            // vertical layout: the font renderer draws glyphs into a box of exactly fsLine height (ascent + descent
            // = fsLine, baseline ascR * fsLine below the box top). This box is centered in the line
            // (extra line height is split equally above and below, like LibreOffice / CSS "half-leading").
            float textH = fsLine * bm.lh;
            float lineH = textH;
            float textY = y + (lineH - fsLine) * 0.5f;
            float baseY = 0;   // bottom edge of images
            if (maxImgH > 0) {
                if (hasText) {
                    // on the same line as text: image bottom edge sits on the text baseline (as in flutter_quill)
                    float below = (textH - fsLine) * 0.5f + fsLine * (1.0f - ascR);   // below the baseline
                    lineH = std::max(textH, maxImgH + below);
                    textY = y + lineH - textH + (textH - fsLine) * 0.5f;
                    baseY = textY + fsLine * ascR;
                } else {
                    lineH = std::max(maxImgH, fsLine * 1.2f);
                    textY = y + lineH - fsLine;
                    baseY = y + lineH;
                }
            }
            float lineW = 0;
            int lastNonSpace = sg.c1;
            while (lastNonSpace > sg.c0 && L.chars[lastNonSpace - 1].cp == ' ') lastNonSpace--;
            for (int q = sg.c0; q < lastNonSpace; q++) lineW += w[q];
            float x = x0;
            if (align == "center") x = x0 + (maxW - lineW) * 0.5f;
            else if (align == "right") x = x0 + (maxW - lineW);

            VLine vl;
            vl.line = li; vl.c0 = sg.c0; vl.c1 = sg.c1; vl.y = y; vl.h = lineH;
            vl.textY = textY;
            if (maxImgH > 0 && !hasText) { vl.caretY = y; vl.caretH = lineH; }   // image only: cursor spans the image height
            else { vl.caretY = textY; vl.caretH = fsLine; }   // cursor spans the glyph box (ascent ... descent)
            vl.xs.push_back(x);
            float cx = x;
            for (int q = sg.c0; q < sg.c1; q++) { cx += w[q]; vl.xs.push_back(cx); }
            vlines_.push_back(vl);

            if (si == 0 && hasSection) {
                Item t; t.kind = Item::TOGGLE; t.x = x0 - toggleW; t.y = vl.caretY; t.w = toggleW; t.h = vl.caretH; t.rowY = vl.y; t.rowH = vl.h;
                t.fs = bm.fs; t.st.color = bm.color; t.line = li; t.checked = doc.isCollapsed(li);
                items_.push_back(t);
            }
            if (si == 0 && !list.empty()) {
                Item m; m.y = y; m.h = lineH; m.fs = bm.fs; m.st.color = bm.color; m.x = indentX; m.line = li;
                if (list == "bullet") m.kind = Item::BULLET;
                else if (list == "ordered") { m.kind = Item::NUMBER; m.text = std::to_string(orderedCounters[indent]) + "."; }
                else if (list == "checked" || list == "unchecked") { m.kind = Item::CHECK; m.checked = (list == "checked"); }
                items_.push_back(m);
            }
            // text runs (identical attributes)
            int q = sg.c0;
            while (q < sg.c1) {
                int r = q;
                if (L.chars[q].embed >= 0) {
                    Item it; it.x = vl.xs[q - sg.c0]; it.line = li; it.col = q;
                    if (isImageEmbed(doc, L.chars[q])) {
                        it.kind = Item::IMAGE_PH; it.w = w[q]; it.h = imgH[q]; it.y = baseY - imgH[q];
                        if (imgOk[q]) it.imgKey = imgKey[q]; else it.text = imagePlaceholder;
                    } else { it.kind = Item::OTHER_PH; it.y = vl.textY; it.w = 18; it.h = fsLine; it.text = "▪"; }
                    items_.push_back(it);
                    q++;
                    continue;
                }
                while (r < sg.c1 && L.chars[r].embed < 0 && L.chars[r].attr == L.chars[q].attr) r++;
                RtStyle st = doc.style(L.chars[q].attr);
                if (bm.bold) st.bold = true;
                if (!st.hasColor) st.color = bm.color;
                if (code) { st.code = true; st.codeBlock = true; }
                if (list == "checked") { st.strike = true; st.color.a = (unsigned char)(st.color.a * 0.6f); }
                Item it;
                it.kind = Item::TEXT; it.st = st; it.fs = fsz[q]; it.line = li;
                for (int k = q; k < r; k++) appendUtf8(it.text, L.chars[k].cp);
                it.x = vl.xs[q - sg.c0];
                it.w = vl.xs[r - sg.c0] - it.x;
                it.y = textY + (fsLine - it.fs) * ascR;   // different sizes share the same baseline
                it.h = it.fs;
                if (st.link) { const RtJson &aj = doc.attr(L.chars[q].attr); if (aj.contains("link") && aj["link"].is_string()) it.link = aj["link"].get<std::string>(); }
                items_.push_back(it);
                q = r;
            }
            y += lineH;
        }
        if (code) { Item bg; bg.kind = Item::CODE_BG; bg.x = secX; bg.y = blockTop; bg.w = docWidth - pad - secX; bg.h = y - blockTop; items_.push_back(bg); }
        if (quote) { Item qb; qb.kind = Item::QUOTE_BAR; qb.x = secX + indent * 32.0f; qb.y = blockTop; qb.w = 4; qb.h = y - blockTop; items_.push_back(qb); }
        y += bm.bottom;
    }
    std::stable_partition(items_.begin(), items_.end(), [](const Item &i) { return i.kind == Item::CODE_BG; });
    contentH_ = y + pad;

}

// =================================================================== hit testing / cursor
// =================================================================== hit testing / cursor
int RichTextView::vlineIndexOf(RtPos p) const
{
    int best = -1;
    for (int i = 0; i < (int)vlines_.size(); i++) {
        const VLine &v = vlines_[i];
        if (v.line != p.line) { if (v.line > p.line) break; continue; }
        if (p.col >= v.c0 && p.col <= v.c1) {
            best = i;
            // wrapped line end: the start of the next image line is preferred
            bool nextSame = i + 1 < (int)vlines_.size() && vlines_[i + 1].line == p.line;
            if (p.col < v.c1 || !nextSame) return i;
        }
    }
    return best >= 0 ? best : (vlines_.empty() ? -1 : (int)vlines_.size() - 1);
}

Rectangle RichTextView::caretRect(RtPos p) const
{
    p = doc.clamp(p);
    int vi = vlineIndexOf(p);
    if (vi < 0) return { padding, padding, 2, 20 };
    const VLine &v = vlines_[vi];
    int idx = std::clamp(p.col - v.c0, 0, (int)v.xs.size() - 1);
    float x = v.xs[idx];
    const float h = std::max(12.0f, v.caretH);   // at least 12 for very small text, centered on the glyph box
    return { x, v.caretY - (h - v.caretH) * 0.5f, 2, h };
}

RtPos RichTextView::hitTest(Vector2 p) const
{
    if (vlines_.empty()) return { 0, 0 };
    int vi = (int)vlines_.size() - 1;
    for (int i = 0; i < (int)vlines_.size(); i++) {
        const VLine &v = vlines_[i];
        if (p.y < v.y + v.h) { vi = i; break; }
    }
    const VLine &v = vlines_[vi];
    int best = 0;
    float bd = 1e9f;
    for (int i = 0; i < (int)v.xs.size(); i++) {
        float d = fabsf(v.xs[i] - p.x);
        if (d < bd) { bd = d; best = i; }
    }
    int col = v.c0 + best;
    // clicking the whitespace at the end of a wrapped line keeps the cursor on that line
    bool nextSame = vi + 1 < (int)vlines_.size() && vlines_[vi + 1].line == v.line;
    if (nextSame && col == v.c1 && col > v.c0) col--;
    return { v.line, col };
}

RtPos RichTextView::moveVertical(RtPos p, int dir, float &prefX) const
{
    int vi = vlineIndexOf(p);
    if (vi < 0) return p;
    if (prefX < 0) prefX = caretRect(p).x;
    int ti = vi + dir;
    if (ti < 0) return { 0, 0 };
    if (ti >= (int)vlines_.size()) return doc.endPos();
    const VLine &v = vlines_[ti];
    int best = 0; float bd = 1e9f;
    for (int i = 0; i < (int)v.xs.size(); i++) { float d = fabsf(v.xs[i] - prefX); if (d < bd) { bd = d; best = i; } }
    int col = v.c0 + best;
    bool nextSame = ti + 1 < (int)vlines_.size() && vlines_[ti + 1].line == v.line;
    if (nextSame && col == v.c1 && col > v.c0) col--;
    return { v.line, col };
}

RtPos RichTextView::lineHome(RtPos p) const
{
    int vi = vlineIndexOf(p);
    if (vi < 0) return p;
    return { vlines_[vi].line, vlines_[vi].c0 };
}

RtPos RichTextView::lineEnd(RtPos p) const
{
    int vi = vlineIndexOf(p);
    if (vi < 0) return p;
    const VLine &v = vlines_[vi];
    bool nextSame = vi + 1 < (int)vlines_.size() && vlines_[vi + 1].line == v.line;
    int col = v.c1;
    if (nextSame && col > v.c0) col--;
    return { v.line, col };
}

int RichTextView::toggleAt(Vector2 p, float slop) const
{
    // Vertically the heading's LINE BAND decides (including line spacing; bands of consecutive lines don't overlap):
    // the arrow of the heading whose line contains the touch. Padded (slop) boxes would overlap and trigger the neighbouring
    // heading. Padding applies only horizontally (wide on the left, not enough to spill onto text on the right); in the
    // gap between bands (paragraph spacing) the nearest band is chosen.
    int best = -1;
    float bestD = 1e30f;
    for (auto &it : items_) {
        if (it.kind != Item::TOGGLE) continue;
        if (p.x < it.x - slop || p.x > it.x + it.w - slop * 0.5f) continue;   // don't spill onto text on the right
        float y0 = it.rowH > 0 ? it.rowY : it.y, y1 = y0 + (it.rowH > 0 ? it.rowH : it.h);
        if (p.y >= y0 && p.y < y1) return it.line;
        float d = p.y < y0 ? y0 - p.y : p.y - y1;
        if (d <= slop && d < bestD) { bestD = d; best = it.line; }
    }
    return best;
}

int RichTextView::checkboxAt(Vector2 p) const
{
    for (auto &it : items_) {
        if (it.kind != Item::CHECK) continue;
        float s = it.fs * 0.9f;
        Rectangle b{ it.x, it.y + (it.h - s) * 0.5f - 3, s + 12, s + 6 };
        if (CheckCollisionPointRec(p, b)) return it.line;
    }
    return -1;
}

// =================================================================== drawing
bool RichTextView::rangeRect(const RtRange &g, Rectangle &r) const
{
    bool found = false;
    for (const VLine &v : vlines_) {
        if (v.line != g.line) { if (found || v.line > g.line) break; continue; }
        if (g.c1 <= v.c0 && !(g.c0 == g.c1 && g.c0 == v.c0)) continue;
        if (g.c0 > v.c1) continue;
        int s = std::max(g.c0, v.c0), e = std::min(g.c1, v.c1);
        float x0 = v.xs[std::clamp(s - v.c0, 0, (int)v.xs.size() - 1)];
        float x1 = v.xs[std::clamp(e - v.c0, 0, (int)v.xs.size() - 1)];
        Rectangle q{ x0, v.caretY, std::max(2.0f, x1 - x0), v.caretH };
        if (!found) { r = q; found = true; }
        else {   // match spilling onto a wrapped line: widen the box
            float rx1 = std::max(r.x + r.width, q.x + q.width), ry1 = std::max(r.y + r.height, q.y + q.height);
            r.x = std::min(r.x, q.x); r.y = std::min(r.y, q.y); r.width = rx1 - r.x; r.height = ry1 - r.y;
        }
    }
    return found;
}


void RichTextView::draw(Vector2 o, Rectangle vis, const DrawOpts &opt) const
{
    const RtTheme &th = rtTheme();
    auto visibleY = [&](float y, float h) { return !(y > vis.y + vis.height || y + h < vis.y); };
    // search matches: glyph box height; current match orange, others yellow
    if (opt.marks && !opt.marks->empty()) {
        size_t mi = 0;
        const std::vector<RtRange> &M = *opt.marks;
        for (const VLine &v : vlines_) {
            if (!visibleY(v.y, v.h)) continue;
            while (mi < M.size() && M[mi].line < v.line) mi++;
            for (size_t k = mi; k < M.size() && M[k].line == v.line; k++) {
                const RtRange &g = M[k];
                if (g.c1 <= v.c0 || g.c0 >= v.c1) continue;
                int s = std::max(g.c0, v.c0), e = std::min(g.c1, v.c1);
                float x0 = v.xs[std::clamp(s - v.c0, 0, (int)v.xs.size() - 1)];
                float x1 = v.xs[std::clamp(e - v.c0, 0, (int)v.xs.size() - 1)];
                bool cur = (int)k == opt.markCur;
                DrawRectangleRec({ o.x + x0, o.y + v.caretY, x1 - x0, v.caretH },
                                 cur ? Color{ 255, 140, 0, 150 } : Color{ 255, 213, 0, 110 });
            }
        }
    }
    // selection highlight
    if (opt.showCaret && opt.caret != opt.anchor) {
        RtPos a = opt.anchor, b = opt.caret;
        if (b < a) std::swap(a, b);
        for (int i = 0; i < (int)vlines_.size(); i++) {
            const VLine &v = vlines_[i];
            if (v.line < a.line || v.line > b.line) continue;
            if (!visibleY(v.y, v.h)) continue;
            int s = (v.line == a.line) ? std::max(a.col, v.c0) : v.c0;
            int e = (v.line == b.line) ? std::min(b.col, v.c1) : v.c1;
            if (v.line == a.line && a.col > v.c1) continue;
            if (v.line == b.line && b.col < v.c0) continue;
            if (e < s) continue;
            float x0 = v.xs[std::clamp(s - v.c0, 0, (int)v.xs.size() - 1)];
            float x1 = v.xs[std::clamp(e - v.c0, 0, (int)v.xs.size() - 1)];
            bool lastOfLine = !(i + 1 < (int)vlines_.size() && vlines_[i + 1].line == v.line);
            if (v.line < b.line && lastOfLine) x1 += 6;   // line end (newline) selected
            DrawRectangleRec({ o.x + x0, o.y + v.y, x1 - x0, v.h }, th.selection);
        }
    }

    const float ascR = std::clamp(rtFonts().ascent(100.0f, RFV_REGULAR) / 100.0f, 0.6f, 0.95f);
    for (auto &it : items_) {
        Rectangle r{ o.x + it.x, o.y + it.y, it.w, it.h };
        if (it.kind != Item::QUOTE_BAR && it.kind != Item::CODE_BG) {
            if (!visibleY(it.y, std::max(it.h, it.fs * 1.5f))) continue;
        } else if (!visibleY(it.y, it.h)) continue;
        switch (it.kind) {
        case Item::TEXT: {
            if (it.st.hasBg) DrawRectangleRec(r, it.st.bg);
            if (it.st.code && !it.st.hasBg && !it.st.codeBlock) DrawRectangleRec(r, th.inlineCodeBg);
            Color tc = it.st.link && !it.st.hasColor ? th.link : th.adapt(it.st.color);   // in the dark theme black ink stays visible
            if (it.st.link && it.st.hasColor) {
                // default link color (styleFromAttrs) follows the theme
                Color dl{ 25, 118, 210, 255 };
                if (it.st.color.r == dl.r && it.st.color.g == dl.g && it.st.color.b == dl.b) tc = th.link;
            }
            if (it.st.hasBg && it.st.bg.a > 100) {
                // keep text readable on a light highlight background (also in the dark theme)
                auto lum = [](Color c) { return 0.299f * c.r + 0.587f * c.g + 0.114f * c.b; };
                if (lum(it.st.bg) > 150 && lum(tc) > 150) tc = it.st.hasColor ? it.st.color : Color{ 0, 0, 0, 221 };
            }
            {
                RtFontFamilyGuard fg(it.st.font);
                rtDrawText(it.text, { r.x, r.y }, it.fs, tc, it.st.bold, it.st.italic, it.st.code);
            }
            float tk = std::max(1.0f, it.fs / 14.0f);
            const float ulY = r.y + it.fs * (ascR + 0.09f), stY = r.y + it.fs * (ascR - 0.26f);
            if (it.st.underline) DrawLineEx({ r.x, ulY }, { r.x + r.width, ulY }, tk, tc);
            if (it.st.strike) DrawLineEx({ r.x, stY }, { r.x + r.width, stY }, tk, tc);
            break;
        }
        case Item::BULLET:
            DrawCircleV({ r.x + 12, r.y + it.h * 0.5f }, it.fs * 0.17f, th.adapt(it.st.color));
            break;
        case Item::NUMBER: {
            float w = rtMeasureText(it.text, it.fs);
            rtDrawText(it.text, { r.x + 26 - w, r.y + (it.h - it.fs) * 0.5f }, it.fs, th.adapt(it.st.color));
            break;
        }
        case Item::CHECK: {
            float s = it.fs * 0.9f;
            Rectangle b{ r.x + 4, r.y + (it.h - s) * 0.5f, s, s };
            if (it.checked) {
                DrawRectangleRounded(b, 0.2f, 4, th.checkboxOn);
                DrawLineEx({ b.x + s * 0.2f, b.y + s * 0.55f }, { b.x + s * 0.42f, b.y + s * 0.77f }, 2, WHITE);
                DrawLineEx({ b.x + s * 0.42f, b.y + s * 0.77f }, { b.x + s * 0.82f, b.y + s * 0.25f }, 2, WHITE);
            } else {
                DrawRectangleRoundedLinesEx(b, 0.2f, 4, 1.5f, th.checkbox);
            }
            break;
        }
        case Item::TOGGLE: {
            // collapsed: ▸, expanded: ▾ (text color, dimmed)
            Color c = th.adapt(it.st.color);
            c.a = (unsigned char)(c.a * (it.checked ? 0.85f : 0.45f));
            float cx = r.x + r.width * 0.45f, cy = r.y + r.height * 0.5f, s = std::max(3.0f, it.fs * 0.17f);
            Vector2 a, b, t;
            if (it.checked) { a = { cx - s * 0.6f, cy - s }; b = { cx - s * 0.6f, cy + s }; t = { cx + s * 0.75f, cy }; }
            else { a = { cx - s, cy - s * 0.6f }; b = { cx + s, cy - s * 0.6f }; t = { cx, cy + s * 0.75f }; }
            DrawTriangle(a, t, b, c); DrawTriangle(a, b, t, c);   // wrap direction does not matter
            break;
        }
        case Item::QUOTE_BAR: DrawRectangleRec(r, th.quoteBar); break;
        case Item::CODE_BG: DrawRectangleRounded(r, 0.02f, 4, th.codeBg); break;
        case Item::INLINE_TABLE: {
            int rows = 1, cols = 1;
            std::vector<std::string> cells;
            if (it.data.is_object()) {
                rows = std::max(1, it.data.value("r", 1));
                cols = std::max(1, it.data.value("c", 1));
                if (it.data.contains("v") && it.data["v"].is_array())
                    for (auto &v : it.data["v"]) cells.push_back(v.is_string() ? v.get<std::string>() : "");
            }
            float cw = r.width / cols, rh = r.height / rows;
            for (int rr = 0; rr < rows; rr++)
                for (int cc = 0; cc < cols; cc++) {
                    Rectangle cr{ r.x + cc * cw, r.y + rr * rh, cw, rh };
                    DrawRectangleLinesEx(cr, 1, th.tableLine);
                    size_t idx = (size_t)(rr * cols + cc);
                    if (idx < cells.size() && !cells[idx].empty())
                        rtDrawText(cells[idx], { cr.x + 6, cr.y + (rh - 14) * 0.5f }, 14, th.ink);
                }
            break;
        }
        case Item::IMAGE_PH:
            if (const RtImage *im = it.imgKey ? rtImageByKey(it.imgKey) : nullptr) {
                if (im->ok) {
                    DrawTexturePro(im->tex, { 0, 0, (float)im->w, (float)im->h }, r, { 0, 0 }, 0, WHITE);
                    break;
                }
            }
            DrawRectangleRec(r, Color{ 0, 0, 0, 12 });
            DrawRectangleLinesEx(r, 1, Color{ 190, 190, 190, 255 });
            rtDrawText(it.text, { r.x + 10, r.y + 10 }, 14, GRAY, false, true);
            break;
        case Item::OTHER_PH:
            if (!it.text.empty()) rtDrawText(it.text, { r.x, r.y + 4 }, 14, GRAY, false, true);
            break;
        }
    }

    if (opt.placeholder && opt.placeholder[0] && doc.isEmpty()) {
        float fs = style_.b[RtStyleSheet::P].fs;
        rtDrawText(opt.placeholder, { o.x + padding, o.y + padding + (fs * style_.b[RtStyleSheet::P].lh - fs) * 0.5f },
                   fs, th.inkPlaceholder, false, true);
    }

    if (opt.showCaret && !opt.hideCaret) {
        double t = GetTime();
        if (fmod(t - opt.blinkStart, 1.0) < 0.6) {
            Rectangle c = caretRect(opt.caret);
            DrawRectangleRec({ o.x + c.x - 0.5f, o.y + c.y, 1.6f, c.height }, th.caret);
        }
    }
}

std::string RichTextView::linkAt(Vector2 p) const
{
    for (auto &it : items_)
        if (it.kind == Item::TEXT && !it.link.empty() && CheckCollisionPointRec(p, { it.x, it.y, it.w, it.h })) return it.link;
    return std::string();
}

float RichTextView::lineTop(int line) const
{
    for (const VLine &v : vlines_) if (v.line == line) return v.y;
    return -1;
}

RtPos RichTextView::imageAt(Vector2 p) const
{
    for (auto &it : items_)
        if (it.kind == Item::IMAGE_PH && CheckCollisionPointRec(p, { it.x, it.y, it.w, it.h })) return { it.line, it.col };
    return { -1, 0 };
}

bool RichTextView::imageRect(RtPos at, Rectangle &r) const
{
    for (auto &it : items_)
        if (it.kind == Item::IMAGE_PH && it.line == at.line && it.col == at.col && it.imgKey) { r = { it.x, it.y, it.w, it.h }; return true; }
    return false;
}

// =================================================================== text / string helpers
bool RichDoc::fromDeltaString(const std::string &js)
{
    RtJson j = RtJson::parse(js, nullptr, false);
    if (j.is_discarded()) { clear(); return false; }
    if (j.is_object() && j.contains("ops")) j = j["ops"];   // {"ops": [...]} (Quill.js format)
    fromDelta(j);
    return j.is_array();
}

std::string RichDoc::toDeltaString(int indent) const
{
    return toDelta().dump(indent, ' ', false, nlohmann::ordered_json::error_handler_t::replace);
}

void RichDoc::setPlainText(const std::string &utf8)
{
    clear();
    insertText({ 0, 0 }, utf8, 0);
}

void RtStyleSheet::scale(float k)
{
    for (auto &x : b) x.fs = std::max(6.0f, x.fs * k);
}

// =================================================================== image embed cache
static std::unordered_map<size_t, RtImage> &imgCache()
{
    static std::unordered_map<size_t, RtImage> c;
    return c;
}

void rtImageCacheClear()
{
    for (auto &kv : imgCache()) if (kv.second.tex.id) UnloadTexture(kv.second.tex);
    imgCache().clear();
}

const RtImage *rtImageByKey(size_t key)
{
    auto &c = imgCache();
    auto it = c.find(key);
    return it == c.end() ? nullptr : &it->second;
}

// extension from the content signature (raylib LoadImageFromMemory looks at the extension)
static std::string sniffExt(const unsigned char *b, size_t n, const std::string &fallback)
{
    if (n > 3 && b[0] == 0xFF && b[1] == 0xD8) return ".jpg";
    if (n > 8 && b[0] == 0x89 && b[1] == 'P') return ".png";
    if (n > 4 && b[0] == 'G' && b[1] == 'I' && b[2] == 'F') return ".gif";
    if (n > 2 && b[0] == 'B' && b[1] == 'M') return ".bmp";
    if (n > 4 && b[0] == 'q' && b[1] == 'o' && b[2] == 'i' && b[3] == 'f') return ".qoi";
    return fallback;
}

static Image loadImageBytes(const unsigned char *data, size_t size, const std::string &extIn)
{
    std::string ext = sniffExt(data, size, extIn);
    return LoadImageFromMemory(ext.c_str(), data, (int)size);
}

const RtImage &rtImage(const std::string &src, size_t *keyOut)
{
    size_t key = std::hash<std::string>()(src) | 1;   // 0 = none
    if (keyOut) *keyOut = key;
    auto &c = imgCache();
    auto it = c.find(key);
    if (it != c.end()) return it->second;
    RtImage &e = c[key];
    if (src.empty()) return e;
    Image img{};
    if (src.rfind("data:", 0) == 0) {
        size_t comma = src.find(','), semi = src.find(';');
        if (comma != std::string::npos && semi != std::string::npos && src.compare(semi, 7, ";base64") == 0) {
            std::string mime = src.substr(5, semi - 5);
            std::string ext = ".png";
            if (mime == "image/jpeg" || mime == "image/jpg") ext = ".jpg";
            else if (mime == "image/gif") ext = ".gif";
            else if (mime == "image/bmp") ext = ".bmp";
            std::vector<unsigned char> bytes = rtBase64Decode(src.substr(comma + 1));
            if (!bytes.empty()) img = loadImageBytes(bytes.data(), bytes.size(), ext);
        }
    } else if (src.rfind("http", 0) != 0 && FileExists(src.c_str())) {
        img = LoadImage(src.c_str());
    }
    if (!img.data) return e;
    e.w = img.width; e.h = img.height;
    e.tex = LoadTextureFromImage(img);
    UnloadImage(img);
    if (e.tex.id) {
        GenTextureMipmaps(&e.tex);
        SetTextureFilter(e.tex, TEXTURE_FILTER_TRILINEAR);
        e.ok = true;
    }
    return e;
}

static std::string mimeOfExt(std::string ext)
{
    for (auto &ch : ext) ch = (char)tolower(ch);
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    return "image/png";
}

std::string rtImageDataUri(const Image &src, int maxPx)
{
    Image img = ImageCopy(src);
    if (!img.data) return "";
    if (std::max(img.width, img.height) > maxPx) {
        float k = (float)maxPx / std::max(img.width, img.height);
        ImageResize(&img, std::max(1, (int)lroundf(img.width * k)), std::max(1, (int)lroundf(img.height * k)));
    }
    int sz = 0;
    unsigned char *png = ExportImageToMemory(img, ".png", &sz);
    UnloadImage(img);
    if (!png) return "";
    std::string out = "data:image/png;base64," + rtBase64Encode(png, (size_t)sz);
    MemFree(png);
    return out;
}

std::string rtImageDataUri(const unsigned char *data, size_t size, const std::string &extIn, int maxPx)
{
    std::string ext = extIn;
    for (auto &ch : ext) ch = (char)tolower(ch);
    ext = sniffExt(data, size, ext);
    Image img = LoadImageFromMemory(ext.c_str(), data, (int)size);
    if (!img.data) return "";
    // PNG / JPG stored as is; others converted to PNG (so they open on every platform)
    bool keep = (ext == ".png" || ext == ".jpg" || ext == ".jpeg") && std::max(img.width, img.height) <= maxPx;
    std::string out = keep ? "data:" + mimeOfExt(ext) + ";base64," + rtBase64Encode(data, size) : rtImageDataUri(img, maxPx);
    UnloadImage(img);
    return out;
}

std::string rtImageFileDataUri(const std::string &path, int maxPx)
{
    int n = 0;
    unsigned char *d = LoadFileData(path.c_str(), &n);
    if (!d || n <= 0) { if (d) UnloadFileData(d); return ""; }
    std::string out = rtImageDataUri(d, (size_t)n, GetFileExtension(path.c_str()) ? GetFileExtension(path.c_str()) : ".png", maxPx);
    UnloadFileData(d);
    return out;
}
