// axGUITable.cpp - axGUI::table (see axGUITable.h)
#include "axGuiSystem.h"
#include "axGUITable.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

// ----------------------------------------------------------------------------- TableState
bool TableState::isSelected(int row) const
{
    if (row < 0) return false;
    if (row == selRow) return true;
    return std::find(selected.begin(), selected.end(), row) != selected.end();
}

void TableState::select(int row)
{
    selRow = row;
    selected.clear();
    if (row >= 0) selected.push_back(row);
    ensureVisibleRow = row;
}

// ----------------------------------------------------------------------------- helpers
namespace {

// texts like "12", "-3,5", "1.250,00" compare as numbers; others case-insensitively
bool parseNumber(const std::string &s, double &out)
{
    std::string t;
    for (char c : s) {
        if (c == ' ' || c == '\t') continue;
        t += c;
    }
    if (t.empty()) return false;
    // thousands separator: if both '.' and ',' are present, the last one is the decimal separator
    size_t lastDot = t.rfind('.'), lastComma = t.rfind(',');
    std::string n;
    for (size_t i = 0; i < t.size(); i++) {
        char c = t[i];
        if (c == '.' || c == ',') {
            bool isDecimal = (lastDot != std::string::npos && lastComma != std::string::npos)
                                 ? i == std::max(lastDot, lastComma)
                                 : i == (c == '.' ? lastDot : lastComma);
            if (isDecimal) n += '.';
            continue;
        }
        n += c;
    }
    char *end = nullptr;
    out = strtod(n.c_str(), &end);
    return end && *end == 0 && end != n.c_str();
}

// UTF-8 -> code point
unsigned nextCp(const std::string &s, size_t &i)
{
    unsigned char c = (unsigned char)s[i];
    unsigned cp = c; int n = 0;
    if (c >= 0xF0) { cp = c & 0x07; n = 3; } else if (c >= 0xE0) { cp = c & 0x0F; n = 2; } else if (c >= 0xC0) { cp = c & 0x1F; n = 1; }
    i++;
    while (n-- > 0 && i < s.size() && ((unsigned char)s[i] & 0xC0) == 0x80) cp = (cp << 6) | ((unsigned char)s[i++] & 0x3F);
    return cp;
}

// letter weight: case-insensitive, Turkish alphabet order (c < c-cedilla < d, g < g-breve < h, dotless i < i, o < o-umlaut < p, s < s-cedilla < t, u < u-umlaut < v)
unsigned letterWeight(unsigned cp)
{
    switch (cp) {
    case 0xE7: case 0xC7: return ('c' << 2) + 1;          // c-cedilla (upper/lower)
    case 0x11F: case 0x11E: return ('g' << 2) + 1;        // g-breve
    case 0x131: return ('i' << 2) - 1;                    // dotless i (before i; ASCII I counts as i)
    case 0x130: return 'i' << 2;                          // dotted capital I
    case 0xF6: case 0xD6: return ('o' << 2) + 1;          // o-umlaut
    case 0x15F: case 0x15E: return ('s' << 2) + 1;        // s-cedilla
    case 0xFC: case 0xDC: return ('u' << 2) + 1;          // u-umlaut
    case 0xE2: case 0xC2: return 'a' << 2;                // â
    case 0xEE: case 0xCE: return 'i' << 2;                // î
    case 0xFB: case 0xDB: return 'u' << 2;                // û
    }
    if (cp >= 'A' && cp <= 'Z') cp += 32;
    if (cp < 128) return cp << 2;
    return 0x10000 + cp;                                  // others come after letters, in code point order
}

// natural sort: digit runs compare as numbers ("Pear 2" < "Pear 10")
int naturalCompare(const std::string &a, const std::string &b)
{
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        bool da = a[i] >= '0' && a[i] <= '9', db = b[j] >= '0' && b[j] <= '9';
        if (da && db) {
            size_t si = i, sj = j;
            while (si < a.size() && a[si] == '0') si++;
            while (sj < b.size() && b[sj] == '0') sj++;
            size_t ei = si, ej = sj;
            while (ei < a.size() && a[ei] >= '0' && a[ei] <= '9') ei++;
            while (ej < b.size() && b[ej] >= '0' && b[ej] <= '9') ej++;
            if (ei - si != ej - sj) return (ei - si) < (ej - sj) ? -1 : 1;
            int c = a.compare(si, ei - si, b, sj, ej - sj);
            if (c) return c < 0 ? -1 : 1;
            i = ei; j = ej;
            continue;
        }
        unsigned wa = letterWeight(nextCp(a, i)), wb = letterWeight(nextCp(b, j));
        if (wa != wb) return wa < wb ? -1 : 1;
    }
    if (i < a.size()) return 1;
    if (j < b.size()) return -1;
    return 0;
}

bool cellLess(const std::string &a, const std::string &b)
{
    double x, y;
    bool nx = parseNumber(a, x), ny = parseNumber(b, y);
    if (nx && ny) return x < y;
    if (nx != ny) return nx;            // numbers before text
    return naturalCompare(a, b) < 0;
}

cColor pick(const cColor &c, const cColor &def) { return c.a ? c : def; }
cColor withA(cColor c, int a) { c.a = a; return c; }
cColor clampC(cColor c)
{
    auto cl = [](int v) { return v < 0 ? 0 : v > 255 ? 255 : v; };
    return cColor(cl(c.r), cl(c.g), cl(c.b), cl(c.a));
}

// truncate at a UTF-8 boundary, append "…"
std::string fitText(axDrawBot *bot, const std::string &s, float maxW, float fsz)
{
    if (maxW <= 0) return std::string();
    if (bot->textBounds(s, fsz).x <= maxW) return s;
    static const std::string ell = "\xE2\x80\xA6";
    float ellW = bot->textBounds(ell, fsz).x;
    if (ellW > maxW) return std::string();
    std::vector<size_t> cuts;   // character boundaries
    for (size_t i = 1; i < s.size(); i++)
        if (((unsigned char)s[i] & 0xC0) != 0x80) cuts.push_back(i);
    int lo = 0, hi = (int)cuts.size() - 1, best = -1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (bot->textBounds(s.substr(0, cuts[(size_t)mid]), fsz).x + ellW <= maxW) { best = mid; lo = mid + 1; }
        else hi = mid - 1;
    }
    return best < 0 ? ell : s.substr(0, cuts[(size_t)best]) + ell;
}

} // namespace

// ----------------------------------------------------------------------------- table
bool axGUI::table(TableState &st, vec2 size, const std::vector<TableColumn> &cols, int rows,
                      const TableCellFn &cell, int flags, const TableDrawFn &drawCell, const TableStyle &style)
{
    genID();
    const int myID = ID;
    st.clickedRow = st.activatedRow = st.rightClickedRow = -1;
    st.selectionChanged = st.sortChanged = false;
    st.hoverRow = st.hoverCol = -1;
    const int ncol = (int)cols.size();
    if (ncol == 0) return false;
    rows = std::max(rows, 0);
    const bool touch = pointerType == PTR_TOUCH;

    // --- metrics
    const float fsz = style.fontSize > 0 ? style.fontSize : fontSize;
    const float rowH = style.rowH > 0 ? style.rowH : std::round(fsz * 1.9f);
    const bool hasHeader = flags & TBL_HEADER;
    const float headH = hasHeader ? (style.headerH > 0 ? style.headerH : rowH) : 0;
    const float padX = style.padX;
    if (size.x <= 0) size.x = std::max(60.0f, getContainerWidth() - (curPos.x - layoutArea.left()));
    if (size.y <= 0) size.y = headH + std::max(rows, 1) * rowH + 2;   // everything visible

    vec2 pos = curPos;
    if (outOfLayoutArea(size)) { advanceCursor(size); return false; }
    const vec2 spos = pos + winScroll;                 // screen position
    const float viewW = size.x, bodyH = size.y - headH;

    // --- colors
    const cColor cText = pick(style.text, colors[GUI_TEXT_COLOR]);
    const cColor cBorder = pick(style.border, colors[GUI_WIN_BORDERCLR]);
    const cColor cHead = pick(style.header, colors[GUI_IDLE_COLOR]);
    const cColor cHeadText = pick(style.headerText, colors[GUI_TEXT_HL_COLOR]);
    const cColor cStripe = pick(style.stripe, withA(cText, 10));
    const cColor cHover = pick(style.hover, withA(colors[GUI_HOVER_COLOR], 110));
    const cColor cSel = pick(style.selected, colors[GUI_BTN_SEL_COLOR]);
    const cColor cGrid = pick(style.grid, withA(cBorder, 90));

    // --- column widths: user setting > column value > share of remaining space
    if ((int)st.widths.size() != ncol) st.widths.assign((size_t)ncol, 0.0f);
    std::vector<float> w((size_t)ncol, 0.0f);
    float fixedSum = 0;
    int stretch = 0;
    for (int c = 0; c < ncol; c++) {
        float v = st.widths[(size_t)c] > 0 ? st.widths[(size_t)c] : cols[(size_t)c].width;
        if (v > 0) { w[(size_t)c] = std::max(v, cols[(size_t)c].minWidth); fixedSum += w[(size_t)c]; }
        else stretch++;
    }
    const float sbW = style.scrollbarW;
    float contentH = rows * rowH;
    bool vbar = contentH > bodyH + 0.5f;
    float avail = viewW - (vbar ? sbW + 2 : 0);
    if (stretch) {
        float each = std::max(0.0f, avail - fixedSum) / stretch;
        for (int c = 0; c < ncol; c++)
            if (w[(size_t)c] <= 0) w[(size_t)c] = std::max(each, cols[(size_t)c].minWidth);
    }
    float totalW = 0;
    for (float v : w) totalW += v;
    bool hbar = totalW > avail + 0.5f;
    float viewH = bodyH - (hbar ? sbW + 2 : 0);
    if (!vbar && contentH > viewH + 0.5f) { vbar = true; }

    // --- display order (sorting)
    if (st.lastRows != rows || st.lastSortCol != st.sortCol || st.lastSortAsc != st.sortAsc || st.orderDirty ||
        (int)st.order.size() != rows) {
        st.order.resize((size_t)rows);
        for (int i = 0; i < rows; i++) st.order[(size_t)i] = i;
        if (st.sortCol >= 0 && st.sortCol < ncol && cell) {
            std::vector<std::string> keys((size_t)rows);
            for (int i = 0; i < rows; i++) keys[(size_t)i] = cell(i, st.sortCol);
            bool asc = st.sortAsc;
            std::stable_sort(st.order.begin(), st.order.end(), [&](int a, int b) {
                return asc ? cellLess(keys[(size_t)a], keys[(size_t)b]) : cellLess(keys[(size_t)b], keys[(size_t)a]);
            });
        }
        // clear the selection if the data rows shrank
        if (st.selRow >= rows) st.selRow = -1;
        st.selected.erase(std::remove_if(st.selected.begin(), st.selected.end(), [rows](int r) { return r >= rows; }), st.selected.end());
        st.lastRows = rows; st.lastSortCol = st.sortCol; st.lastSortAsc = st.sortAsc; st.orderDirty = false;
    }
    auto dispOf = [&](int dataRow) -> int {
        for (int i = 0; i < rows; i++) if (st.order[(size_t)i] == dataRow) return i;
        return -1;
    };

    auto clampScroll = [&]() {
        st.scroll.x = std::clamp(st.scroll.x, 0.0f, std::max(0.0f, totalW - avail));
        st.scroll.y = std::clamp(st.scroll.y, 0.0f, std::max(0.0f, contentH - viewH));
    };
    auto ensureVisibleDisp = [&](int d) {
        if (d < 0) return;
        float top = d * rowH, bot = top + rowH;
        if (top < st.scroll.y) st.scroll.y = top;
        else if (bot > st.scroll.y + viewH) st.scroll.y = bot - viewH;
    };
    if (st.ensureVisibleRow >= 0) { ensureVisibleDisp(dispOf(st.ensureVisibleRow)); st.ensureVisibleRow = -1; }

    // --- regions (screen)
    rect rAll(spos, size);
    rect rHead(spos, vec2(avail, headH));
    rect rBody(spos + vec2(0, headH), vec2(avail, viewH));
    rect rVBar(spos + vec2(viewW - sbW - 1, headH + 1), vec2(sbW, viewH - 2));
    rect rHBar(spos + vec2(1, size.y - sbW - 1), vec2(avail - 2, sbW));
    float vThumbH = vbar ? std::max(24.0f, rVBar.size.y * viewH / contentH) : 0;
    float vThumbY = vbar ? rVBar.pos.y + (rVBar.size.y - vThumbH) * (st.scroll.y / std::max(1.0f, contentH - viewH)) : 0;
    float hThumbW = hbar ? std::max(24.0f, rHBar.size.x * avail / totalW) : 0;
    float hThumbX = hbar ? rHBar.pos.x + (rHBar.size.x - hThumbW) * (st.scroll.x / std::max(1.0f, totalW - avail)) : 0;

    bool hovered = useInput && winHovered && rAll.isPointInRec(ms) && winClip.isPointInRec(ms);
    if (hovered) { overGUI = true; if (!msLeftAction && !msrightAction) hoverID = myID; }

    // column edge (in header, screen x) - resize handle
    const float grabTol = touch ? 12.0f : 5.0f;
    auto edgeAt = [&](vec2 p) -> int {
        if (!(flags & TBL_RESIZABLE) || !hasHeader || !rHead.isPointInRec(p)) return -1;
        float x = spos.x - st.scroll.x;
        for (int c = 0; c < ncol; c++) {
            x += w[(size_t)c];
            if (std::fabs(p.x - x) <= grabTol) return c;
        }
        return -1;
    };
    auto colAt = [&](float sx) -> int {
        float x = spos.x - st.scroll.x;
        for (int c = 0; c < ncol; c++) { x += w[(size_t)c]; if (sx < x) return c; }
        return -1;
    };
    auto dispAt = [&](float sy) -> int {
        int d = (int)std::floor((sy - rBody.pos.y + st.scroll.y) / rowH);
        return (d >= 0 && d < rows) ? d : -1;
    };

    if (hovered && rBody.isPointInRec(ms)) {
        int d = dispAt(ms.y);
        if (d >= 0) { st.hoverRow = st.order[(size_t)d]; st.hoverCol = colAt(ms.x); }
    }
    if ((hovered && edgeAt(ms) >= 0) || st.resizeCol >= 0) cursorRequest = MOUSE_CURSOR_RESIZE_EW;

    // --- selection helpers
    const bool canSelect = !(flags & TBL_NOSELECT);
    const bool multi = flags & TBL_MULTISELECT;
    auto selectDisp = [&](int d, bool additive, bool range) {
        if (!canSelect || d < 0) return;
        int row = st.order[(size_t)d];
        if (multi && range && st.anchorDisp >= 0) {
            int a = std::min(st.anchorDisp, d), b = std::max(st.anchorDisp, d);
            if (!additive) st.selected.clear();
            for (int i = a; i <= b; i++) {
                int r = st.order[(size_t)i];
                if (std::find(st.selected.begin(), st.selected.end(), r) == st.selected.end()) st.selected.push_back(r);
            }
            st.selRow = row;
        } else if (multi && additive) {
            auto it = std::find(st.selected.begin(), st.selected.end(), row);
            if (it != st.selected.end()) st.selected.erase(it); else st.selected.push_back(row);
            st.selRow = row;
            st.anchorDisp = d;
        } else {
            if (st.selRow == row && st.selected.size() == 1) { ensureVisibleDisp(d); return; }
            st.selected.assign(1, row);
            st.selRow = row;
            st.anchorDisp = d;
        }
        st.selectionChanged = true;
        ensureVisibleDisp(d);
    };

    // --- mouse / touch
    if (hovered && msLeftPressed) {
        focusID = myID;
        proc = true;
        st.pressed = true;
        st.dragScroll = st.thumbDragV = st.thumbDragH = false;
        st.speed = vec2(0, 0);
        st.pressRow = st.pressCol = -1;
        int edge = edgeAt(ms);
        if (edge >= 0) {
            st.pressZone = 3;
            st.resizeCol = edge;
            st.resizeStartW = w[(size_t)edge];
        } else if (vbar && !touch && rect(rVBar.pos - vec2(3, 0), rVBar.size + vec2(6, 0)).isPointInRec(ms)) {
            st.pressZone = 4;
            st.thumbDragV = true;
            st.thumbGrab = (ms.y >= vThumbY && ms.y <= vThumbY + vThumbH) ? ms.y - vThumbY : vThumbH / 2;
        } else if (hbar && !touch && rect(rHBar.pos - vec2(0, 3), rHBar.size + vec2(0, 6)).isPointInRec(ms)) {
            st.pressZone = 5;
            st.thumbDragH = true;
            st.thumbGrab = (ms.x >= hThumbX && ms.x <= hThumbX + hThumbW) ? ms.x - hThumbX : hThumbW / 2;
        } else if (hasHeader && rHead.isPointInRec(ms)) {
            st.pressZone = 1;
            st.pressCol = colAt(ms.x);
        } else if (rBody.isPointInRec(ms)) {
            st.pressZone = 2;
            int d = dispAt(ms.y);
            st.pressRow = d >= 0 ? st.order[(size_t)d] : -1;
            // select on mouse press (desktop habit); on touch, on release without scrolling
            if (!touch && d >= 0) selectDisp(d, ctrl, shift);
        } else st.pressZone = 0;
    }
    if (hovered && msRightPressed && rBody.isPointInRec(ms)) {
        int d = dispAt(ms.y);
        if (d >= 0) {
            int row = st.order[(size_t)d];
            if (!st.isSelected(row)) selectDisp(d, false, false);
            st.rightClickedRow = row;
            focusID = myID;
            proc = true;
        }
    }
    if (st.pressed) {
        proc = true;
        if (msLeftDown) {
            if (st.pressZone == 3 && st.resizeCol >= 0) {
                float nw = std::max(cols[(size_t)st.resizeCol].minWidth, st.resizeStartW + (ms.x - msDown.x));
                st.widths[(size_t)st.resizeCol] = nw;
            } else if (st.thumbDragV) {
                float t = (ms.y - st.thumbGrab - rVBar.pos.y) / std::max(1.0f, rVBar.size.y - vThumbH);
                st.scroll.y = t * (contentH - viewH);
            } else if (st.thumbDragH) {
                float t = (ms.x - st.thumbGrab - rHBar.pos.x) / std::max(1.0f, rHBar.size.x - hThumbW);
                st.scroll.x = t * (totalW - avail);
            } else if ((st.pressZone == 2 || st.pressZone == 1) && msDragging) {
                // drag to scroll (always for touch; works with mouse too)
                st.dragScroll = true;
                vec2 d = ms - msPre;
                st.scroll.y -= st.pressZone == 2 ? d.y : 0;
                st.scroll.x -= d.x;
                st.speed = st.speed * 0.5f + d * 0.5f;
            }
        }
        if (msLeftReleased) {
            if (!st.dragScroll && !msDragging && hovered) {
                if (st.pressZone == 1 && st.pressCol >= 0 && (flags & TBL_SORTABLE) && cols[(size_t)st.pressCol].sortable) {
                    // ascending -> descending -> unsorted
                    if (st.sortCol != st.pressCol) { st.sortCol = st.pressCol; st.sortAsc = true; }
                    else if (st.sortAsc) st.sortAsc = false;
                    else st.sortCol = -1;
                    st.sortChanged = true;
                } else if (st.pressZone == 2) {
                    int d = dispAt(ms.y);
                    if (d >= 0 && st.order[(size_t)d] == st.pressRow) {
                        if (touch) selectDisp(d, false, false);
                        st.clickedRow = st.pressRow;
                        double now = GetTime();
                        float tol = touch ? 30.0f : 6.0f;
                        if (st.lastClickRow == st.pressRow && now - st.lastClickTime < 0.4 && (ms - msDown).length() < tol) {
                            st.activatedRow = st.pressRow;
                            st.lastClickTime = 0;
                        } else st.lastClickTime = now;
                        st.lastClickRow = st.pressRow;
                    }
                }
            }
            if (!st.dragScroll) st.speed = vec2(0, 0);
            st.pressed = st.dragScroll = st.thumbDragV = st.thumbDragH = false;
            st.resizeCol = -1;
            st.pressZone = 0;
        }
    } else if (st.speed.x != 0 || st.speed.y != 0) {
        // inertia (only after drag scrolling)
        st.scroll = st.scroll - st.speed;
        st.speed = st.speed * 0.92f;
        if (st.speed.length() < 0.3f) st.speed = vec2(0, 0);
    }
    if (hovered && msWheel != 0) {
        if (shift || (totalW > avail && contentH <= viewH)) st.wheelX.add(st.scroll.x, -msWheel * rowH * 2);
        else st.wheelY.add(st.scroll.y, -msWheel * rowH * 3);
        msWheel = 0;   // so the window scroll doesn't move as well
        proc = true;
        st.speed = vec2(0, 0);
    }
    {
        const float dt = GetFrameTime();
        bool animating = st.wheelX.step(st.scroll.x, 0.0f, std::max(0.0f, totalW - avail), wheelSmooth, dt);
        animating |= st.wheelY.step(st.scroll.y, 0.0f, std::max(0.0f, contentH - viewH), wheelSmooth, dt);
        if (animating) bot->textSnap = false;   // rows are sliding: keep text moving with them
    }

    // --- keyboard (when focused)
    if (focusID == myID && rows > 0 && !wantCaptureKeyboard) {
        int cur = st.selRow >= 0 ? dispOf(st.selRow) : -1;
        int page = std::max(1, (int)(viewH / rowH) - 1);
        int to = -2;
        if (shouldKeyTrigger(KEY_DOWN)) to = cur < 0 ? 0 : std::min(rows - 1, cur + 1);
        else if (shouldKeyTrigger(KEY_UP)) to = cur < 0 ? 0 : std::max(0, cur - 1);
        else if (shouldKeyTrigger(KEY_PAGE_DOWN)) to = std::min(rows - 1, std::max(cur, 0) + page);
        else if (shouldKeyTrigger(KEY_PAGE_UP)) to = std::max(0, cur - page);
        else if (IsKeyPressed(KEY_HOME)) to = 0;
        else if (IsKeyPressed(KEY_END)) to = rows - 1;
        if (to >= 0) selectDisp(to, false, shift && multi);
        if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) && st.selRow >= 0) st.activatedRow = st.selRow;
        if (multi && ctrl && IsKeyPressed(KEY_A)) {
            st.selected = st.order;
            st.selectionChanged = true;
        }
    }

    // widths may have changed: recompute the total
    totalW = 0;
    for (int c = 0; c < ncol; c++) {
        if (st.widths[(size_t)c] > 0) w[(size_t)c] = std::max(st.widths[(size_t)c], cols[(size_t)c].minWidth);
        totalW += w[(size_t)c];
    }
    clampScroll();

    // ------------------------------------------------------------------ drawing (layout coordinates)
    const float rad = style.radius;
    if (style.bg.a) bot->roundedFRect(pos.x, pos.y, size.x, size.y, rad, style.bg);
    rect clip = rect(spos, size).getIntersection(winClip);
    if (clip.size.x > 0 && clip.size.y > 0) {
        bot->save();
        bot->scissorScreen(clip.pos.x, clip.pos.y, clip.size.x, clip.size.y);

        const float x0 = pos.x - st.scroll.x;
        const float bodyY = pos.y + headH;
        // body: visible rows only
        rect bclip = rect(spos + vec2(0, headH), vec2(avail, viewH)).getIntersection(clip);
        if (bclip.size.x > 0 && bclip.size.y > 0 && rows > 0) {
            bot->save();
            bot->scissorScreen(bclip.pos.x, bclip.pos.y, bclip.size.x, bclip.size.y);
            int first = std::max(0, (int)std::floor(st.scroll.y / rowH));
            int last = std::min(rows - 1, (int)std::floor((st.scroll.y + viewH) / rowH));
            for (int d = first; d <= last; d++) {
                int row = st.order[(size_t)d];
                float y = bodyY + d * rowH - st.scroll.y;
                bool sel = st.isSelected(row);
                if (sel) bot->roundedFRect(pos.x, y, avail, rowH, 0, cSel);
                else if (st.hoverRow == row && canHover()) bot->roundedFRect(pos.x, y, avail, rowH, 0, cHover);
                else if ((flags & TBL_STRIPED) && (d & 1)) bot->roundedFRect(pos.x, y, avail, rowH, 0, cStripe);
                float x = x0;
                for (int c = 0; c < ncol; c++) {
                    float cw = w[(size_t)c];
                    if (x + cw >= pos.x && x <= pos.x + avail) {
                        rect cr(vec2(x, y), vec2(cw, rowH));
                        bool custom = drawCell && drawCell(row, c, cr, sel);
                        if (!custom && cell) {
                            std::string s = fitText(bot, cell(row, c), cw - 2 * padX, fsz);
                            int al = cols[(size_t)c].align;
                            float tx = (al & BOT_ALIGN_RIGHT) ? x + cw - padX : (al & BOT_ALIGN_CENTER) ? x + cw / 2 : x + padX;
                            int ha = (al & BOT_ALIGN_RIGHT) ? BOT_ALIGN_RIGHT : (al & BOT_ALIGN_CENTER) ? BOT_ALIGN_CENTER : BOT_ALIGN_LEFT;
                            bot->text(s, vec2(tx, y + rowH / 2), fsz, cText, ha | BOT_ALIGN_MIDDLE);
                        }
                    }
                    x += cw;
                    if ((flags & TBL_GRID) && c < ncol - 1) bot->drawLine(x, y, x, y + rowH, cGrid, 1);
                }
                bot->drawLine(pos.x, y + rowH, pos.x + avail, y + rowH, withA(cGrid, cGrid.a / 2), 1);
            }
            bot->restore();
        }
        if (rows == 0 && style.emptyText)
            bot->text(style.emptyText, vec2(pos.x + avail / 2, bodyY + viewH / 2), fsz, withA(cText, 140), BOT_ALIGN_CENTER | BOT_ALIGN_MIDDLE);

        // header
        if (hasHeader) {
            bot->roundedFRect(pos.x, pos.y, size.x, headH, 0, cHead);
            float x = x0;
            for (int c = 0; c < ncol; c++) {
                float cw = w[(size_t)c];
                if (x + cw >= pos.x && x <= pos.x + avail) {
                    bool sorted = st.sortCol == c;
                    float arrowW = sorted ? 12 : 0;
                    std::string s = fitText(bot, cols[(size_t)c].title, cw - 2 * padX - arrowW, fsz);
                    int al = cols[(size_t)c].align;
                    float tx = (al & BOT_ALIGN_RIGHT) ? x + cw - padX - arrowW : (al & BOT_ALIGN_CENTER) ? x + cw / 2 : x + padX;
                    int ha = (al & BOT_ALIGN_RIGHT) ? BOT_ALIGN_RIGHT : (al & BOT_ALIGN_CENTER) ? BOT_ALIGN_CENTER : BOT_ALIGN_LEFT;
                    if (hovered && st.pressZone != 3 && rHead.isPointInRec(ms) && colAt(ms.x) == c && edgeAt(ms) < 0 &&
                        (flags & TBL_SORTABLE) && cols[(size_t)c].sortable && canHover())
                        bot->roundedFRect(x, pos.y, cw, headH, 0, withA(cText, 14));
                    bot->text(s, vec2(tx, pos.y + headH / 2), fsz, cHeadText, ha | BOT_ALIGN_MIDDLE);
                    if (sorted) {
                        float ax = x + cw - padX - 4, ay = pos.y + headH / 2, a = 4;
                        if (st.sortAsc) bot->fillTriangle(vec2(ax - a, ay + a * 0.6f), vec2(ax + a, ay + a * 0.6f), vec2(ax, ay - a * 0.7f), cHeadText);
                        else bot->fillTriangle(vec2(ax + a, ay - a * 0.6f), vec2(ax - a, ay - a * 0.6f), vec2(ax, ay + a * 0.7f), cHeadText);
                    }
                }
                x += cw;
                if (c < ncol - 1 || (flags & TBL_RESIZABLE)) {
                    bool hot = st.resizeCol == c || (hovered && edgeAt(ms) == c);
                    bot->drawLine(x, pos.y + (hot ? 2 : 6), x, pos.y + headH - (hot ? 2 : 6),
                                  hot ? colors[GUI_BTN_TOGGLE_COLOR] : withA(cBorder, 160), hot ? 2.0f : 1.0f);
                }
            }
            bot->drawLine(pos.x, pos.y + headH, pos.x + size.x, pos.y + headH, cBorder, 1);
        }

        // scrollbars
        auto lp = [&](const rect &r) { return r.pos - winScroll; };
        if (vbar) {
            vec2 p = lp(rVBar);
            bot->roundedFRect(p.x, p.y, sbW, rVBar.size.y, sbW / 2, withA(cText, 18));
            bool hot = st.thumbDragV || (hovered && !touch && rect(rVBar.pos - vec2(3, 0), rVBar.size + vec2(6, 0)).isPointInRec(ms));
            bot->roundedFRect(p.x, vThumbY - winScroll.y, sbW, vThumbH, sbW / 2, withA(cText, hot ? 150 : 90));
        }
        if (hbar) {
            vec2 p = lp(rHBar);
            bot->roundedFRect(p.x, p.y, rHBar.size.x, sbW, sbW / 2, withA(cText, 18));
            bool hot = st.thumbDragH || (hovered && !touch && rect(rHBar.pos - vec2(0, 3), rHBar.size + vec2(0, 6)).isPointInRec(ms));
            bot->roundedFRect(hThumbX - winScroll.x, p.y, hThumbW, sbW, sbW / 2, withA(cText, hot ? 150 : 90));
        }
        bot->restore();
    }
    // frame (highlighted when focused)
    bot->roundedRect(pos.x, pos.y, size.x, size.y, rad, focusID == myID ? 1.5f : 1.0f,
                     focusID == myID ? clampC(colors[GUI_BTN_TOGGLE_COLOR]) : cBorder);

    ItemState &item = itemStates[myID];
    item.set("table", spos, size, hovered, focusID == myID, rad);
    advanceCursor(size);
    return st.selectionChanged || st.activatedRow >= 0 || st.sortChanged || st.clickedRow >= 0 || st.rightClickedRow >= 0;
}
