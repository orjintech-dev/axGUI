#include "axGuiSystem.h"
#include "axUTF8.h"
#include "axGUITextBox.h"


// ============================================================================
// Text Manipulation Helpers
// ============================================================================
TextBoxConfig textcfg;

static void stripNewlines(std::string& text) {
    text.erase(std::remove(text.begin(), text.end(), '\n'), text.end());
    text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
}

static int clampi(int val, int min, int max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

static int getLineIndent(const std::string& line) {
    int indent = 0;
    for (char c : line) {
        if (c == ' ') indent++;
        else if (c == '\t') indent += 4;
        else break;
    }
    return indent;
}

// ============================================================================
// Rendering Helpers
// ============================================================================
// active while textBox runs: for custom measuring (TextBoxState::measureText)
static const TextBoxState *g_tbState = nullptr;
static float measureW(axDrawBot *bot, const std::string &s)
{
    if (g_tbState && g_tbState->measureText) return g_tbState->measureText(s);
    return bot->textBounds(s).x;
}

static float textBounds(axDrawBot *bot, float x, float y, const cString &str, const char* end, float* bounds)
{
    float ek;
    vec2 bb = bot->textBounds(str);
    bb.x = measureW(bot, str);
    bounds[2] = x+bb.x;
    bounds[3] = y+bb.y;
    bounds[0] = x;
    bounds[1] = y;
    return 0;

    bot->textBounds(".", 0, 0, bounds);
    ek = bounds[2]-bounds[0];

    bot->textBounds((str+".").c_str(), x, y, bounds);
    bounds[2] -= ek;
    return 0;
}


static float caretXFromIndex(axDrawBot *bot, const std::string& text, int idx, float x, float y)
{
    if (idx <= 0) return x;

    if (idx >= (int)text.size()) {
        float bounds[4];
        textBounds(bot, x, y, text.c_str(), nullptr, bounds);
        return bounds[2];
    }

    float bounds[4];
    std::string substr = text.substr(0, idx);
    textBounds(bot, x, y, substr.c_str(), nullptr, bounds);
    return bounds[2];
}

static int indexFromX(axDrawBot *bot, const std::string& text, float textX, float textY, float clickX)
{
    if (text.empty()) return 0;

    int bestIdx = 0;
    float bestDist = std::abs(clickX - textX);

    int pos = 0;
    while (pos <= (int)text.size()) {
        float bounds[4];
        textBounds(bot, textX, textY, text.substr(0, pos).c_str(), nullptr, bounds);
        float x = bounds[2];
        x = textX + measureW(bot, text.substr(0, pos));
        float dist = std::abs(clickX - x);

        if (dist < bestDist) {
            bestDist = dist;
            bestIdx = pos;
        }

        if (pos >= (int)text.size()) break;
        pos = utf8_next(text, pos);
    }

    return bestIdx;
}

// multi-line centering (centerH / centerV): horizontal shift of the line, vertical shift of the line block (px)
static float lineOffsetX(axDrawBot *bot, const TextBoxState& st, int line, float textAreaW)
{
    if (!st.multiline || !(st.centerH || st.alignRight)) return 0.0f;
    float bounds[4];
    std::string lt = st.getLineText(line);
    textBounds(bot, 0, 0, lt.c_str(), nullptr, bounds);
    float lw = bounds[2] - bounds[0];
    return std::max(0.0f, st.alignRight ? (textAreaW - lw) : (textAreaW - lw) * 0.5f);
}

static float blockOffsetY(const TextBoxState& st, float textAreaH, float pad)
{
    if (!st.multiline || !st.centerV) return 0.0f;
    float bh = (float)std::max<size_t>(1, st.lineStarts.size()) * st.lineHeight;
    // stays centered if inside the box even when it does not fit the inner area (may spill into the padding); top-aligned if larger than the box
    return std::max(-pad, (textAreaH - bh) * 0.5f);
}

static void cursorFromMouse(axDrawBot *bot, TextBoxState& st, vec2 pos, vec2 size, vec2 mp, float pad)
{
    float lineNumOffset = (textcfg.showLineNumbers && st.multiline) ? textcfg.lineNumberWidth : 0.0f;
    const float areaW = size.x - 2 * pad - lineNumOffset, areaH = size.y - 2 * pad;
    float xText = pos.x + pad + lineNumOffset - st.scrollX;
    float yText = pos.y + pad - st.scrollY + blockOffsetY(st, areaH, pad);

    int line = (int)floorf((mp.y - yText) / st.lineHeight);
    line = clampi(line, 0, (int)st.lineStarts.size() - 1);

    std::string lineText = st.getLineText(line);
    float lineY = yText + line * st.lineHeight + st.lineHeight * 0.5f;
    int idx = indexFromX(bot, lineText, xText + lineOffsetX(bot, st, line, areaW), lineY, mp.x);

    st.cursor = st.lineStarts[line] + idx;
    st.cursor = clampi(st.cursor, 0, (int)st.text.size());
    st.updateCursorLineCol();
}

// ============================================================================
// Main TextBox Function
// ============================================================================

bool axGUI::textBox(TextBoxState& st, vec2 size)
{
    bool submitted = false;
    genID();
    // skip drawing if fully outside the visible area (like other widgets); keep processing input if focused
    if (!st.focused && outOfLayoutArea(size)) { advanceCursor(size); return false; }
    // NVGcontext* vg = bot->vg;
    const float w = size.x, h = size.y;
    const vec2 pos = curPos;
    const float pad = st.padding;
    const float strk = textcfg.borderWidth;
    int txAlign = BOT_ALIGN_LEFT | BOT_ALIGN_MIDDLE;

    if (st.multiline)  txAlign = BOT_ALIGN_LEFT | BOT_ALIGN_TOP;

    float asc, desc, lineh;
    bot->textMetrics(&asc, &desc, &lineh);
    st.lineHeight = st.lineHeightOverride > 0 ? st.lineHeightOverride : lineh;
    struct StateGuard { StateGuard(const TextBoxState *s) { g_tbState = s; } ~StateGuard() { g_tbState = nullptr; } } stateGuard(&st);

    if (st.multiline && (st.lineStarts.empty() || st.lineStarts[0] != 0)) {
        st.rebuildLineIndex();
        st.updateCursorLineCol();
    }

    // ========================================================================
    // Input State
    // ========================================================================

    const vec2 mp = ms;
    // bool hovered = rect(pos + winScroll, size).isPointInRec(mp) && winClip.isPointInRec(mp);
    const bool mousePressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    const bool mouseDown = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    const bool alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);

    bool hovered = buttonHovered(pos, vec2(w,h));

    if (mousePressed) {
        if (hovered) {
            if (!st.focused) st.blinkStart = GetTime();
            st.focused = true;
            st.dragging = true;

            double t = GetTime();
            if (t - st.lastClickTime < textcfg.clickTimeWindow) {
                st.clickCount++;
            } else {
                st.clickCount = 1;
            }
            st.lastClickTime = t;
            focusID = ID;
            proc = true;
        } else {
            st.focused = false;
            st.dragging = false;
            st.clickCount = 0;
        }
    }
    if (!mouseDown) st.dragging = false;
    if (hovered) hoverID = ID;
    if (st.focused) kbCaptureNext = true;

    // ========================================================================
    // Rendering - Background
    // ========================================================================
    bot->roundedFRect(pos.x + strk, pos.y + strk, w - 2*strk, h - 2*strk, textcfg.cornerRadius,
                     st.multiline ? textcfg.bgColorMulti : textcfg.bgColorSingle,
                     strk, st.focused ? textcfg.borderColorFocused : textcfg.borderColor);

    float lineNumOffset = (textcfg.showLineNumbers && st.multiline) ? textcfg.lineNumberWidth : 0.0f;
    const float textAreaW = w - 2*pad - lineNumOffset;
    const float textAreaH = h - 2*pad;
    auto offX = [&](int line) { return lineOffsetX(bot, st, line, textAreaW); };   // centerH
    auto offY = [&]() { return blockOffsetY(st, textAreaH, pad); };                 // centerV

    // ========================================================================
    // Scrolling
    // ========================================================================
    if (hovered && st.focused) {
        float wheel = msWheel;   // msWheel: consume it so the window does not scroll
        if (wheel != 0.0f) {
            msWheel = 0;
            if (st.multiline) {
                if (shift) {
                    st.scrollX = std::max(0.0f, st.scrollX - wheel * textcfg.wheelScrollSpeed);
                } else {
                    st.scrollY = std::max(0.0f, st.scrollY - wheel * st.lineHeight * textcfg.wheelScrollLines);
                }
            } else {
                st.scrollX = std::max(0.0f, st.scrollX - wheel * textcfg.wheelScrollSpeed);
            }
        }
    }

    // ========================================================================
    // Mouse Selection
    // ========================================================================
    if (st.focused && (st.dragging || (mousePressed && hovered))) {
        if (mousePressed) {
            if (st.multiline) {
                cursorFromMouse(bot, st, pos, size, mp, pad);
            } else {
                float xText = pos.x + pad - st.scrollX;
                float yMid = pos.y + h * 0.5f;
                int idx = indexFromX(bot, st.text, xText, yMid, mp.x);
                st.cursor = clampi(idx, 0, (int)st.text.size());
            }

            if (!shift) {
                if (st.clickCount == 2) {
                    int wordStart = st.cursor;
                    int wordEnd = st.cursor;

                    while (wordStart > 0 && isWordChar(st.text[wordStart - 1])) {
                        wordStart--;
                    }

                    while (wordEnd < (int)st.text.size() && isWordChar(st.text[wordEnd])) {
                        wordEnd++;
                    }

                    if (wordStart == wordEnd && st.cursor < (int)st.text.size()) {
                        wordStart = st.cursor;
                        wordEnd = st.cursor + 1;
                        while (wordEnd < (int)st.text.size() &&
                               !isWordChar(st.text[wordEnd]) &&
                               !std::isspace((unsigned char)st.text[wordEnd])) {
                            wordEnd++;
                        }
                    }

                    st.sel_anchor = wordStart;
                    st.cursor = wordEnd;
                } else if (st.clickCount >= 3) {
                    if (st.multiline) {
                        st.updateCursorLineCol();
                        int lineStart = st.lineStarts[st.cursorLine];
                        int lineEnd = (st.cursorLine + 1 < (int)st.lineStarts.size())
                                          ? st.lineStarts[st.cursorLine + 1]
                                          : st.text.size();
                        st.sel_anchor = lineStart;
                        st.cursor = lineEnd;
                    } else {
                        st.sel_anchor = 0;
                        st.cursor = (int)st.text.size();
                    }
                } else {
                    st.sel_anchor = st.cursor;
                }
            }
            st.blinkStart = GetTime();
        }
        else if (mouseDown && st.clickCount == 1) {
            if (st.multiline) {
                cursorFromMouse(bot, st, pos, size, mp, pad);
            } else {
                float xText = pos.x + pad - st.scrollX;
                float yMid = pos.y + h * 0.5f;
                int idx = indexFromX(bot, st.text, xText, yMid, mp.x);
                st.cursor = clampi(idx, 0, (int)st.text.size());
            }
        }
    }

    // ========================================================================
    // Keyboard Input
    // ========================================================================
    if (st.focused)
    {
        bool textChanged = false;

        int cp;
        while ((cp = GetCharPressed()) != 0) {
            if (cp >= 32 && cp != 127) {
                if (!st.multiline && (cp == '\n' || cp == '\r')) continue;

                char buf[5] = {0};
                utf8_encode(cp, buf);
                st.deleteSelection();
                st.text.insert(st.cursor, buf);
                st.cursor += (int)std::strlen(buf);
                st.sel_anchor = st.cursor;
                st.blinkStart = GetTime();
                textChanged = true;
            }
        }
        // if(textChanged) axLog("STR: "+st.text);

        if (st.multiline && IsKeyPressed(KEY_TAB) && !ctrl)
        {
            int a = std::min(st.cursor, st.sel_anchor);
            int b = std::max(st.cursor, st.sel_anchor);

            if (a != b || 1)
            {
                st.rebuildLineIndex();

                int lineA = 0, lineB = 0;
                for (int i = 0; i < (int)st.lineStarts.size(); i++) {
                    if (a >= st.lineStarts[i]) lineA = i;
                    if (b > st.lineStarts[i]) lineB = i;
                }

                if (lineB >= (int)st.lineStarts.size()) {
                    lineB = st.lineStarts.size() - 1;
                }
                if(lineB<lineA) lineB=lineA;

                if (shift) {
                    int cursorAdjust = 0;
                    int anchorAdjust = 0;
                    for (int line = lineA; line <= lineB; line++) {
                        st.rebuildLineIndex();

                        if (line >= (int)st.lineStarts.size()) break;

                        int lineStart = st.lineStarts[line];
                        std::string lineText = st.getLineText(line);

                        int removeCount = 0;
                        for (int i = 0; i < st.tabSize && i < (int)lineText.size(); i++) {
                            if (lineText[i] == ' ') {
                                removeCount++;
                            } else if (lineText[i] == '\t') {
                                removeCount = 1;
                                break;
                            } else {
                                break;
                            }
                        }
                        if (removeCount > 0) {
                            st.text.erase(lineStart, removeCount);

                            if (st.cursor > lineStart) {
                                cursorAdjust += removeCount;
                            }
                            if (st.sel_anchor > lineStart) {
                                anchorAdjust += removeCount;
                            }
                        }
                    }

                    st.cursor -= cursorAdjust;
                    st.sel_anchor -= anchorAdjust;

                    if (cursorAdjust > 0 || anchorAdjust > 0) {
                        textChanged = true;
                    }
                } else {
                    std::string spaces(st.tabSize, ' ');
                    int totalAdded = 0;

                    for (int line = lineA; line <= lineB; line++) {
                        int lineStart = st.lineStarts[line] + totalAdded;
                        st.text.insert(lineStart, spaces);
                        totalAdded += st.tabSize;

                        if (st.cursor >= lineStart) {
                            st.cursor += st.tabSize;
                        }
                        if (st.sel_anchor >= lineStart) {
                            st.sel_anchor += st.tabSize;
                        }
                    }
                    textChanged = true;
                }

                st.blinkStart = GetTime();
            } else {
                st.deleteSelection();
                std::string spaces(st.tabSize, ' ');
                st.text.insert(st.cursor, spaces);
                st.cursor += st.tabSize;
                st.sel_anchor = st.cursor;
                textChanged = true;
            }
        }
        const bool enterPressed = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
        if (enterPressed && (!st.multiline || ctrl)) submitted = true;   // confirmed: drawing continues
        else if (enterPressed && st.multiline) {
            st.deleteSelection();

            std::string currentLine = st.getLineText(st.cursorLine);
            int indent = getLineIndent(currentLine);

            std::string newLine = "\n" + std::string(indent, ' ');
            st.text.insert(st.cursor, newLine);
            st.cursor += newLine.size();
            st.sel_anchor = st.cursor;
            st.blinkStart = GetTime();
            textChanged = true;
        }

        if (ctrl && IsKeyPressed(KEY_A)) {
            st.sel_anchor = 0;
            st.cursor = (int)st.text.size();
        }
        if (ctrl && IsKeyPressed(KEY_C)) {
            int a = std::min(st.cursor, st.sel_anchor);
            int b = std::max(st.cursor, st.sel_anchor);
            if (a < b) SetClipboardText(st.text.substr(a, b - a).c_str());
        }
        if (ctrl && IsKeyPressed(KEY_X)) {
            int a = std::min(st.cursor, st.sel_anchor);
            int b = std::max(st.cursor, st.sel_anchor);
            if (a < b) {
                SetClipboardText(st.text.substr(a, b - a).c_str());
                st.deleteSelection();
                textChanged = true;
            }
        }
        if (ctrl && IsKeyPressed(KEY_V)) {
            const char* clip = GetClipboardText();
            if (clip) {
                std::string sclip = clip;
                if(!st.multiline) stripNewlines(sclip);
                sclip.erase(std::remove(sclip.begin(), sclip.end(), '\r'), sclip.end());

                st.deleteSelection();
                st.text.insert(st.cursor, sclip);
                st.cursor += (int)sclip.size();
                st.sel_anchor = st.cursor;
                st.blinkStart = GetTime();
                textChanged = true;
            }
        }

        if (st.multiline && ctrl && IsKeyPressed(KEY_D)) {
            std::string lineText = st.getLineText(st.cursorLine);
            int lineStart = st.lineStarts[st.cursorLine];
            st.text.insert(lineStart, lineText + "\n");
            st.cursor += lineText.size() + 1;
            st.sel_anchor = st.cursor;
            textChanged = true;
        }

        if (st.multiline && ctrl && shift && IsKeyPressed(KEY_K)) {
            int lineStart = st.lineStarts[st.cursorLine];
            int lineEnd = (st.cursorLine + 1 < (int)st.lineStarts.size())
                              ? st.lineStarts[st.cursorLine + 1]
                              : st.text.size();
            st.text.erase(lineStart, lineEnd - lineStart);
            st.cursor = lineStart;
            st.sel_anchor = st.cursor;
            textChanged = true;
        }

        if (st.shouldKeyTrigger(KEY_BACKSPACE)) {
            if (!st.deleteSelection()) {
                if (ctrl) {
                    int newPos = prevWord(st.text, st.cursor);
                    st.text.erase(newPos, st.cursor - newPos);
                    st.cursor = newPos;
                } else {
                    erase_cp_left(st.text, st.cursor);
                }
            }
            st.sel_anchor = st.cursor;
            st.blinkStart = GetTime();
            textChanged = true;
        }
        if (st.shouldKeyTrigger(KEY_DELETE)) {
            if (!st.deleteSelection()) {
                if (ctrl) {
                    int newPos = nextWord(st.text, st.cursor);
                    st.text.erase(st.cursor, newPos - st.cursor);
                } else {
                    erase_cp_right(st.text, st.cursor);
                }
            }
            st.sel_anchor = st.cursor;
            st.blinkStart = GetTime();
            textChanged = true;
        }

        if (st.multiline)
        {
            if (alt && IsKeyPressed(KEY_UP) && st.cursorLine > 0)
            {
                std::string currentLine = st.getLineText(st.cursorLine);
                std::string prevLine = st.getLineText(st.cursorLine - 1);

                int prevStart = st.lineStarts[st.cursorLine - 1];
                int currentStart = st.lineStarts[st.cursorLine];
                int currentEnd = (st.cursorLine + 1 < (int)st.lineStarts.size())
                                     ? st.lineStarts[st.cursorLine + 1] - 1
                                     : st.text.size();

                // Delete both lines
                st.text.erase(prevStart, currentEnd - prevStart + 1);
                // Insert in reverse order
                st.text.insert(prevStart, currentLine + "\n" + prevLine + "\n");

                st.cursor = prevStart + (st.cursor - currentStart);
                st.sel_anchor = st.cursor;
                textChanged = true;
            }
            else if (alt && IsKeyPressed(KEY_DOWN) && st.cursorLine < (int)st.lineStarts.size() - 1)
            {
                std::string currentLine = st.getLineText(st.cursorLine);
                std::string nextLine = st.getLineText(st.cursorLine + 1);

                int currentStart = st.lineStarts[st.cursorLine];
                int nextStart = st.lineStarts[st.cursorLine + 1];
                int nextEnd = (st.cursorLine + 2 < (int)st.lineStarts.size())
                                  ? st.lineStarts[st.cursorLine + 2] - 1
                                  : st.text.size();

                st.text.erase(currentStart, nextEnd - currentStart + 1);
                st.text.insert(currentStart, nextLine + "\n" + currentLine + "\n");

                st.cursor = currentStart + nextLine.size() + 1 + (st.cursor - currentStart);
                st.sel_anchor = st.cursor;
                textChanged = true;
            }
            else if (st.shouldKeyTrigger(KEY_UP)) {
                if (st.cursorLine > 0) {
                    st.cursor = st.posFromLineCol(st.cursorLine - 1, st.cursorCol);
                    st.updateCursorLineCol();
                } else {
                    st.cursor = 0;
                }
                if (!shift) st.sel_anchor = st.cursor;
                st.blinkStart = GetTime();
            }
            else if (st.shouldKeyTrigger(KEY_DOWN)) {
                if (st.cursorLine < (int)st.lineStarts.size() - 1) {
                    st.cursor = st.posFromLineCol(st.cursorLine + 1, st.cursorCol);
                    st.updateCursorLineCol();
                } else {
                    st.cursor = st.text.size();
                }
                if (!shift) st.sel_anchor = st.cursor;
                st.blinkStart = GetTime();
            }

            int linesPerPage = (int)(textAreaH / st.lineHeight);
            if (st.shouldKeyTrigger(KEY_PAGE_UP)) {
                int newLine = std::max(0, st.cursorLine - linesPerPage);
                st.cursor = st.posFromLineCol(newLine, st.cursorCol);
                st.updateCursorLineCol();
                if (!shift) st.sel_anchor = st.cursor;
                st.blinkStart = GetTime();
            }
            if (st.shouldKeyTrigger(KEY_PAGE_DOWN)) {
                int newLine = std::min((int)st.lineStarts.size() - 1, st.cursorLine + linesPerPage);
                st.cursor = st.posFromLineCol(newLine, st.cursorCol);
                st.updateCursorLineCol();
                if (!shift) st.sel_anchor = st.cursor;
                st.blinkStart = GetTime();
            }
        }

        if (IsKeyPressed(KEY_LEFT)) {
            st.cursor = ctrl ? prevWord(st.text, st.cursor) : utf8_prev(st.text, st.cursor);
            if (!shift) st.sel_anchor = st.cursor;
            if (st.multiline) st.updateCursorLineCol();
            st.blinkStart = GetTime();
        }
        if (IsKeyPressed(KEY_RIGHT)) {
            int oldPos = st.cursor;
            st.cursor = ctrl ? nextWord(st.text, st.cursor) : utf8_next(st.text, st.cursor);
            if (ctrl) {
                printf("Ctrl+Right: %d -> %d, char at old: '%c'\n",
                       oldPos, st.cursor,
                       oldPos < (int)st.text.size() ? st.text[oldPos] : '?');
            }
            if (!shift) st.sel_anchor = st.cursor;
            if (st.multiline) st.updateCursorLineCol();
            st.blinkStart = GetTime();
        }
        if (st.shouldKeyTrigger(KEY_HOME)) {
            if (st.multiline) {
                if (ctrl) {
                    st.cursor = 0;
                } else {
                    int lineStart = st.lineStarts[st.cursorLine];
                    std::string lineText = st.getLineText(st.cursorLine);
                    int firstNonSpace = lineStart;
                    for (size_t i = 0; i < lineText.size(); i++) {
                        if (!std::isspace((unsigned char)lineText[i])) {
                            firstNonSpace = lineStart + i;
                            break;
                        }
                    }
                    st.cursor = (st.cursor == firstNonSpace) ? lineStart : firstNonSpace;
                }
                st.updateCursorLineCol();
            } else {
                st.cursor = 0;
            }
            if (!shift) st.sel_anchor = st.cursor;
            st.blinkStart = GetTime();
        }
        if (st.shouldKeyTrigger(KEY_END)) {
            if (st.multiline) {
                if (ctrl) {
                    st.cursor = st.text.size();
                } else {
                    int lineEnd = (st.cursorLine + 1 < (int)st.lineStarts.size())
                    ? st.lineStarts[st.cursorLine + 1] - 1
                    : st.text.size();
                    st.cursor = lineEnd;
                }
                st.updateCursorLineCol();
            } else {
                st.cursor = st.text.size();
            }
            if (!shift) st.sel_anchor = st.cursor;
            st.blinkStart = GetTime();
        }

        st.changed = false;
        if (textChanged) {
            if (st.multiline) {
                st.rebuildLineIndex();
                st.updateCursorLineCol();
            }
            st.changed = true;
        }
    }

    // ========================================================================
    // Auto-scroll to keep cursor visible
    // ========================================================================
    if (st.multiline) {
        float cursorPixelY = st.cursorLine * st.lineHeight;
        if (cursorPixelY < st.scrollY) {
            st.scrollY = cursorPixelY;
        }
        if (cursorPixelY + st.lineHeight > st.scrollY + textAreaH) {
            st.scrollY = cursorPixelY + st.lineHeight - textAreaH;
        }
        // no vertical scroll if the centered block fits in the box (blockOffsetY uses the padding)
        if (st.centerV && (float)st.lineStarts.size() * st.lineHeight <= h - 2 * strk) st.scrollY = 0;

        std::string lineText = st.getLineText(st.cursorLine);
        int colPos = st.cursor - st.lineStarts[st.cursorLine];
        float bounds[4];
        textBounds(bot, 0, 0, lineText.substr(0, colPos).c_str(), nullptr, bounds);
        float cursorPixelX = bounds[2] - bounds[0];

        if (cursorPixelX < st.scrollX) {
            st.scrollX = cursorPixelX;
        }
        if (cursorPixelX > st.scrollX + textAreaW) {
            st.scrollX = cursorPixelX - textAreaW;
        }
        if (st.scrollX < 0) st.scrollX = 0;
    } else {
        float bounds[4];
        textBounds(bot, 0, 0, st.text.substr(0, st.cursor).c_str(), nullptr, bounds);
        float cursorPixelX = bounds[2] - bounds[0];

        if (cursorPixelX < st.scrollX) {
            st.scrollX = cursorPixelX;
        }
        if (cursorPixelX > st.scrollX + textAreaW) {
            st.scrollX = cursorPixelX - textAreaW;
        }
        if (st.scrollX < 0) st.scrollX = 0;

        textBounds(bot, 0, 0, st.text.c_str(), nullptr, bounds);
        float textW = bounds[2] - bounds[0];
        float maxScroll = std::max(0.0f, textW - textAreaW);
        if (st.scrollX > maxScroll) st.scrollX = maxScroll;
    }

    // ========================================================================
    // Rendering - Content
    // ========================================================================

    bot->save();
    // intersect with the window clip, otherwise the text is drawn outside the window when the box
    // moves past its edge
    // always clip to the border vertically (to the padding horizontally), so accents on capital letters in the first line
    // and descenders of the last line are not cut off
    bot->intersectScissorWorld(vec2(pos.x+pad, pos.y+strk), vec2(w-2*pad, h-2*strk));

    if (textcfg.showLineNumbers && st.multiline)
    {
        int firstLine = (int)(st.scrollY / st.lineHeight);
        int lastLine = (int)((st.scrollY + textAreaH) / st.lineHeight) + 1;
        firstLine = clampi(firstLine, 0, (int)st.lineStarts.size() - 1);
        lastLine = clampi(lastLine, 0, (int)st.lineStarts.size() - 1);

        txAlign = BOT_ALIGN_RIGHT | BOT_ALIGN_TOP;

        for (int line = firstLine; line <= lastLine; line++)
        {
            float y = pos.y + pad - st.scrollY + line * st.lineHeight;
            cColor clr;
            if (line == st.cursorLine && st.focused) {
                clr = textcfg.borderColorFocused;
            } else {
                clr = textcfg.lineNumberColor;
            }

            char lineNum[16];
            snprintf(lineNum, sizeof(lineNum), "%d", line + 1);
            bot->text(lineNum, pos.x + pad + textcfg.lineNumberWidth - textcfg.lineNumberPadding, y, 0, clr, txAlign);
        }

        txAlign = BOT_ALIGN_LEFT | BOT_ALIGN_TOP;
        bot->drawLine( vec2(pos.x+pad+textcfg.lineNumberWidth, pos.y + pad),
                     vec2(pos.x+pad+textcfg.lineNumberWidth, pos.y+h-pad),
                     textcfg.borderColor,
                     1.0f
                     );
        // nvgStroke(vg);
    }

    if (st.multiline && st.focused)
    {
        float y = pos.y + pad - st.scrollY + offY() + st.cursorLine * st.lineHeight;
        bot->roundedFRect(pos.x + pad + lineNumOffset, y, textAreaW, st.lineHeight, 2.0f, textcfg.currentLineHighlight);
    }

    int a = std::min(st.cursor, st.sel_anchor);
    int b = std::max(st.cursor, st.sel_anchor);
    if (st.focused && a != b) {
        if (st.multiline) {
            int lineA = 0, lineB = 0;
            for (int i = 0; i < (int)st.lineStarts.size(); i++)
            {
                if (a >= st.lineStarts[i]) lineA = i;
                if (b >= st.lineStarts[i]) lineB = i;
            }

            // nvgFillColor(vg, toNVG(textcfg.selectionColor));

            for (int line = lineA; line <= lineB; line++)
            {
                int lineStart = st.lineStarts[line];
                int lineEnd = (line + 1 < (int)st.lineStarts.size())
                                  ? st.lineStarts[line + 1] - 1
                                  : st.text.size();

                int selStart = std::max(a, lineStart);
                int selEnd = std::min(b, lineEnd);

                if (selStart < selEnd)
                {
                    std::string lineText = st.getLineText(line);
                    int colStart = selStart - lineStart;
                    int colEnd = selEnd - lineStart;

                    float bounds[4];
                    float x1, x2;
                    textBounds(bot, 0, 0, lineText.substr(0, colStart).c_str(), nullptr, bounds);
                    x1 = bounds[2] - bounds[0];
                    textBounds(bot, 0, 0, lineText.substr(0, colEnd).c_str(), nullptr, bounds);
                    x2 = bounds[2] - bounds[0];

                    float y = pos.y + pad - st.scrollY + offY() + line * st.lineHeight;
                    bot->roundedFRect(pos.x+pad+lineNumOffset+offX(line)+x1-st.scrollX, y, x2-x1, st.lineHeight, 0, textcfg.selectionColor);
                }
            }
        }
        else {
            float ax = caretXFromIndex(bot, st.text, a, pos.x + pad - st.scrollX, pos.y + h * 0.5f);
            float bx = caretXFromIndex(bot, st.text, b, pos.x + pad - st.scrollX, pos.y + h * 0.5f);
            bot->roundedFRect(ax, pos.y + 4, bx - ax, h - 8, 0, textcfg.selectionColor);
        }
    }

    if (st.text.empty() && st.placeholder && !st.focused)
    {
        if (st.multiline) {
            bot->text(st.placeholder, pos.x + pad + lineNumOffset - st.scrollX, pos.y + pad - st.scrollY, 0, textcfg.placeholderColor, txAlign);
        } else {
            bot->text(st.placeholder, pos.x + pad - st.scrollX, pos.y + h * 0.5f, 0, textcfg.placeholderColor, txAlign);
        }
    }
    else
    {
        if (st.multiline)
        {
            int firstLine = (int)(st.scrollY / st.lineHeight);
            int lastLine = (int)((st.scrollY + textAreaH) / st.lineHeight) + 1;
            firstLine = clampi(firstLine, 0, (int)st.lineStarts.size() - 1);
            lastLine = clampi(lastLine, 0, (int)st.lineStarts.size() - 1);

            for (int line = firstLine; line <= lastLine; line++)
            {
                std::string lineText = st.getLineText(line);
                float x = pos.x + pad + lineNumOffset - st.scrollX + offX(line);
                float y = pos.y + pad - st.scrollY + offY() + line * st.lineHeight;

                if (st.syntaxHighlight && !st.syntaxLanguage.empty() && !lineText.empty())
                {
                    std::vector<Token> tokens = tokenizeLine(lineText, st.syntaxLanguage);

                    std::vector<cColor> charColors(lineText.size(), textcfg.textColor);
                    for (const Token& token : tokens) {
                        cColor color = getTokenColor(token.type);
                        for (int i = 0; i < token.length && token.start + i < (int)lineText.size(); i++) {
                            charColors[token.start + i] = color;
                        }
                    }

                    size_t i = 0;
                    while (i < lineText.size())
                    {
                        cColor currentColor = charColors[i];
                        size_t segmentStart = i;

                        while (i < lineText.size() &&
                               charColors[i].r == currentColor.r &&
                               charColors[i].g == currentColor.g &&
                               charColors[i].b == currentColor.b) {
                            i++;
                        }

                        if (segmentStart > 0) {
                            std::string beforeSegment = lineText.substr(0, segmentStart);
                            float bounds[4];
                            textBounds(bot, x, y, beforeSegment.c_str(), nullptr, bounds);

                            std::string segment = lineText.substr(segmentStart, i - segmentStart);
                            bot->text(segment, bounds[2], y, 0, currentColor, txAlign);
                        } else {
                            std::string segment = lineText.substr(segmentStart, i - segmentStart);
                            bot->text(segment, x, y, 0, currentColor, txAlign);
                        }
                    }
                }
                else if (st.drawText) {
                    st.drawText(lineText, x, y, textcfg.textColor);
                }
                else {
                    bot->text(lineText, x, y, 0, textcfg.textColor, txAlign);
                }
            }
        }
        else {
            bot->text(st.text, pos.x + pad - st.scrollX, pos.y + h * 0.5f, 0, textcfg.textColor, txAlign);
        }
    }

    static cString sub="Sub text";

    if (st.focused) {
        bool on = fmod(GetTime() - st.blinkStart, textcfg.blinkInterval) < (textcfg.blinkInterval * textcfg.blinkOnRatio);
        if (on) {
            if (st.multiline) {
                std::string lineText = st.getLineText(st.cursorLine);
                int colPos = st.cursor - st.lineStarts[st.cursorLine];

                if (colPos < 0) colPos = 0;
                if (colPos > (int)lineText.size()) colPos = lineText.size();

                float baseX = pos.x + pad + lineNumOffset - st.scrollX + offX(st.cursorLine);
                float cy = pos.y + pad - st.scrollY + offY() + st.cursorLine * st.lineHeight;

                float cx = baseX;
                if (colPos > 0 && !lineText.empty()) {
                    float bounds[4], ek=0;
                    textBounds(bot, 0, 0, lineText.substr(0, colPos).c_str(), nullptr, bounds);
                    cx = baseX + (bounds[2] - bounds[0]) - ek;
                }

                bot->roundedFRect(cx, cy, 2.0f, st.lineHeight, 1.0f, textcfg.cursorColor);
            }
            else {
                float baseX = pos.x + pad - st.scrollX;
                float cy = pos.y + 6;

                int cursorPos = st.cursor;
                if (cursorPos < 0) cursorPos = 0;
                if (cursorPos > (int)st.text.size()) cursorPos = st.text.size();

                float cx = baseX;
                if (cursorPos > 0) {
                    float bounds[4], ek=0;
                    textBounds(bot, 0, 0, st.text.substr(0, cursorPos).c_str(), nullptr, bounds);
                    cx = baseX + (bounds[2] - bounds[0]) - ek;
                }

                bot->roundedFRect(cx, cy, 2.0f, h - 12, 1.0f, textcfg.cursorColor);

            }
        }
    }
    bot->restore();

    if (st.showDebugInfo) {
        st.renderDebugInfo(bot, pos, size);
    }

    advanceCursor(size);
    return submitted;
}
