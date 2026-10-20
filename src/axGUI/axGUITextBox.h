#ifndef AXGUITEXTBOX_H
#define AXGUITEXTBOX_H
#pragma once
#include "axGuiSystem.h"
#include "axDrawBot.h"
#include "axUTF8.h"
#include <functional>

// ============================================================================
// Configuration & Constants
// ============================================================================

struct TextBoxConfig {
    // Visual

    float borderWidth = 2.0f;
    float cornerRadius = 4.0f;

    // Colors
    cColor bgColorSingle = cColor(32, 32, 32);
    cColor bgColorMulti = cColor(32, 32, 32);
    cColor borderColor = cColor(20, 20, 20);
    cColor borderColorFocused = cColor(80, 120, 255);
    cColor textColor = cColor(220, 220, 220);
    cColor placeholderColor = cColor(150, 150, 150);
    cColor selectionColor = cColor(29, 89, 94, 200);
    cColor cursorColor = cColor(240, 240, 240);
    cColor lineNumberColor = cColor(120, 120, 120);
    cColor currentLineHighlight = cColor(230, 240, 255, 40);

    // Behavior
    double clickTimeWindow = 0.3;
    double blinkInterval = 1.0;
    double blinkOnRatio = 0.5;
    float wheelScrollSpeed = 30.0f;
    float wheelScrollLines = 3.0f;

    // Line numbers
    bool showLineNumbers = false;
    float lineNumberWidth = 40.0f;
    float lineNumberPadding = 8.0f;

    // Debug
    // bool showDebugInfo = true;
};

struct TextBoxState {
    std::string text;
    int cursor = 0;
    int sel_anchor = 0;
    bool focused = false;
    bool multiline = false;
    const char* placeholder = nullptr;
    bool dragging = false;
    double blinkStart = 0.0;
    double lastClickTime = 0.0;
    int clickCount = 0;

    // Scrolling
    float scrollX = 0.0f;
    float scrollY = 0.0f;

    // Multi-line specific
    std::vector<int> lineStarts;
    int cursorLine = 0;
    int cursorCol = 0;
    float lineHeight = 20.0f;
    bool showDebugInfo = false;

    // Options
    int tabSize = 4;
    float padding = 8.0f;
    bool wrapLines = false;
    bool centerH = false;   // multi-line: each line horizontally centered (a line that does not fit is left-aligned and scrolls)
    bool centerV = false;   // multi-line: line block vertically centered (top-aligned if it does not fit)
    bool alignRight = false;   // multi-line: each line right-aligned (a line that does not fit is left-aligned and scrolls)
    // Custom text measuring / drawing (e.g. the app's own font, with kerning): if set, textBox uses these instead of bot
    // (multi-line text, caret, selection, clicking). In drawText, y is the top of the line.
    std::function<float(const std::string&)> measureText;
    std::function<void(const std::string&, float x, float y, cColor c)> drawText;
    float lineHeightOverride = 0.0f;   // > 0: line height (px)
    bool syntaxHighlight = false;
    bool changed=false;
    std::string syntaxLanguage = ""; // "cpp", "java", "angelscript", "javascript", "python", etc.
    KeyRepeat keyRepeat;

    // // Key repeat timing
    // struct KeyRepeat {
    //     int key = -1;
    //     double pressTime = 0.0;
    //     double lastRepeatTime = 0.0;
    //     double initialDelay = 0.4;
    //     double repeatInterval = 0.033;
    // } keyRepeat;

    void rebuildLineIndex()
    {
        lineStarts.clear();
        lineStarts.push_back(0);

        for (size_t i = 0; i < text.size(); i++) {
            if (text[i] == '\n') {
                lineStarts.push_back(i + 1);
            }
        }
    }

    void updateCursorLineCol()
    {
        cursorLine = 0;
        for (size_t i = 0; i < lineStarts.size(); i++) {
            if (cursor < lineStarts[i]) {
                cursorLine = i - 1;
                break;
            }
            cursorLine = i;
        }

        int lineStart = lineStarts[cursorLine];
        cursorCol = utf8_count_codepoints(text.substr(lineStart, cursor - lineStart));
    }

    int posFromLineCol(int line, int col) const
    {
        if (line < 0) line = 0;
        if (line >= (int)lineStarts.size()) line = lineStarts.size() - 1;

        int lineStart = lineStarts[line];
        int lineEnd = (line + 1 < (int)lineStarts.size())
                          ? lineStarts[line + 1] - 1
                          : text.size();

        int pos = lineStart;
        int curCol = 0;
        while (pos < lineEnd && curCol < col) {
            pos = utf8_next(text, pos);
            curCol++;
        }
        return pos;
    }

    std::string getLineText(int line) const
    {
        if (line < 0 || line >= (int)lineStarts.size()) return "";

        int start = lineStarts[line];
        int end = (line + 1 < (int)lineStarts.size())
                      ? lineStarts[line + 1] - 1
                      : text.size();

        if (start >= (int)text.size() || end < start) return "";

        return text.substr(start, end - start);
    }

    bool deleteSelection()
    {
        int a = std::min(cursor, sel_anchor);
        int b = std::max(cursor, sel_anchor);
        if (a == b) return false;

        text.erase(a, b - a);
        cursor = a;
        sel_anchor = a;
        return true;
    }

    bool shouldKeyTrigger(int key) {
        double currentTime = GetTime();

        if (IsKeyPressed(key)) {
            keyRepeat.key = key;
            keyRepeat.pressTime = currentTime;
            keyRepeat.lastRepeatTime = currentTime;
            return true;
        }

        if (IsKeyDown(key) && keyRepeat.key == key) {
            double timeSincePress = currentTime - keyRepeat.pressTime;
            double timeSinceRepeat = currentTime - keyRepeat.lastRepeatTime;

            if (timeSincePress >= keyRepeat.initialDelay &&
                timeSinceRepeat >= keyRepeat.repeatInterval) {
                keyRepeat.lastRepeatTime = currentTime;
                return true;
            }
        }

        if (!IsKeyDown(key) && keyRepeat.key == key) {
            keyRepeat.key = -1;
        }

        return false;
    }

    void setText(const std::string& newText)
    {
        text = newText;
        cursor = 0;
        sel_anchor = 0;
        if (multiline) {
            rebuildLineIndex();
            updateCursorLineCol();
        }
    }

    cString getText()
    {
        return text;
    }

    void renderDebugInfo(axDrawBot *bot, vec2 pos, vec2 size, float yOffset = 0.0f)
    {
        float infoY = pos.y + size.y + yOffset + 5.0f;

        char info[256];
        int selStart = std::min(cursor, sel_anchor);
        int selEnd = std::max(cursor, sel_anchor);
        int selLen = selEnd - selStart;

        if (multiline) {
            snprintf(info, sizeof(info),
                     "Cursor: %d | Line: %d, Col: %d | Selection: %d-%d (%d chars) | Lines: %d | ScrollY: %.1f",
                     cursor, cursorLine + 1, cursorCol, selStart, selEnd, selLen,
                     (int)lineStarts.size(), scrollY);
        }
        else {
            snprintf(info, sizeof(info),
                     "Cursor: %d | Selection: %d-%d (%d chars) | Length: %d | ScrollX: %.1f",
                     cursor, selStart, selEnd, selLen, (int)text.size(), scrollX);
        }
        // nvgText(vg, pos.x, infoY, info, nullptr);
        bot->text(info, pos.x, infoY, 12.0f, cColor(220, 200, 200));
    }
};

// ============================================================================
// Word Navigation
// ============================================================================

static bool isWordChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

static int prevWord(const std::string& text, int pos) {
    if (pos <= 0) return 0;

    pos = utf8_prev(text, pos);

    while (pos > 0 && std::isspace((unsigned char)text[pos])) {
        pos = utf8_prev(text, pos);
    }

    while (pos > 0) {
        int prev = utf8_prev(text, pos);
        if (!isWordChar(text[prev])) break;
        pos = prev;
    }

    return pos;
}

static int nextWord(const std::string& text, int pos) {
    if (pos >= (int)text.size()) return text.size();

    // Check the current character first
    if (pos < (int)text.size()) {
        char current = text[pos];

        // If in a word, go to the end of the word
        if (isWordChar(current)) {
            while (pos < (int)text.size() && isWordChar(text[pos])) {
                pos = utf8_next(text, pos);
            }
        }
        // If on whitespace, skip the whitespace
        else if (std::isspace((unsigned char)current)) {
            while (pos < (int)text.size() && std::isspace((unsigned char)text[pos])) {
                pos = utf8_next(text, pos);
            }
        }
        // If an operator or special character, move to the next character
        else {
            pos = utf8_next(text, pos);
        }
    }

    // Finally skip trailing whitespace
    while (pos < (int)text.size() && std::isspace((unsigned char)text[pos])) {
        pos = utf8_next(text, pos);
    }

    return pos;
}
// ============================================================================
// Syntax Highlighting
// ============================================================================

struct SyntaxColors {
    cColor keyword = cColor(69, 200, 220);
    cColor type = cColor(78, 201, 176);
    cColor string = cColor(206, 145, 120);
    cColor comment = cColor(106, 153, 85);
    cColor number = cColor(181, 206, 168);
    cColor function = cColor(220, 220, 170);
    cColor preprocessor = cColor(189, 147, 249);
    cColor operatorColor = cColor(212, 212, 212);
    cColor normal = cColor(214, 207, 154);
};

inline SyntaxColors syntaxColors;   // inline instead of static, so the app / theme / style editor can modify it (a static copy would be per-.cpp)

enum class Tokentype {
    NORMAL,
    KEYWORD,
    TYPE,
    STRING,
    COMMENT,
    NUMBER,
    FUNCTION,
    PREPROCESSOR,
    OPERATOR
};

struct Token {
    Tokentype type;
    int start;
    int length;
};

const std::vector<std::string> cppKeywords = {
    "if", "else", "while", "for", "do", "switch", "case", "break", "continue",
    "return", "goto", "default", "class", "struct", "union", "enum",
    "public", "private", "protected", "virtual", "override", "final",
    "static", "const", "constexpr", "volatile", "mutable", "inline",
    "new", "delete", "this", "nullptr", "true", "false",
    "try", "catch", "throw", "template", "typename", "namespace", "using",
    "friend", "extern", "typedef", "sizeof", "operator", "explicit"
};

const std::vector<std::string> cppTypes = {
    "void", "int", "float", "double", "char", "bool", "long", "short",
    "unsigned", "signed", "size_t", "int8_t", "int16_t", "int32_t", "int64_t",
    "uint8_t", "uint16_t", "uint32_t", "uint64_t", "auto", "string", "vector",
    "map", "set", "list", "array", "std", "shared_ptr", "unique_ptr", "weak_ptr"
};

const std::vector<std::string> angelScriptKeywords = {
    "if", "else", "while", "for", "do", "switch", "case", "break", "continue",
    "return", "class", "interface", "mixin", "private", "protected",
    "const", "true", "false", "null", "funcdef", "cast", "is", "auto",
    "shared", "external", "get", "set", "property", "import", "from", "enum"
};

const std::vector<std::string> angelScriptTypes = {
    "void", "int", "int8", "int16", "int32", "int64",
    "uint", "uint8", "uint16", "uint32", "uint64",
    "float", "double", "bool", "string", "array", "dictionary", "any", "ref"
};

const std::vector<std::string> javaKeywords = {
    "if", "else", "while", "for", "do", "switch", "case", "break", "continue",
    "return", "class", "interface", "extends", "implements", "package", "import",
    "public", "private", "protected", "static", "final", "abstract", "synchronized",
    "native", "transient", "volatile", "strictfp", "new", "this", "super",
    "try", "catch", "finally", "throw", "throws", "assert", "instanceof",
    "true", "false", "null", "default", "enum"
};

const std::vector<std::string> javaTypes = {
    "void", "int", "float", "double", "char", "boolean", "byte", "short", "long",
    "String", "Integer", "Float", "Double", "Boolean", "Character", "Object",
    "List", "ArrayList", "Map", "HashMap", "Set", "HashSet"
};

const std::vector<std::string> markdownKeywords = {
    "TODO", "NOTE", "FIXME", "WARNING", "IMPORTANT"  // for custom highlights
};

static bool isIdentifierChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

static bool isOperatorChar(char c) {
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '%' ||
           c == '=' || c == '!' || c == '<' || c == '>' || c == '&' ||
           c == '|' || c == '^' || c == '~' || c == '?' || c == ':' ||
           c == '.' || c == ',' || c == ';';
}

static std::vector<Token> tokenizeMarkdown(const std::string& line) {
    std::vector<Token> tokens;
    int i = 0;
    int len = line.size();

    while (i < len) {
        // Skip spaces
        if (std::isspace((unsigned char)line[i])) {
            i++;
            continue;
        }

        // Headings: # ## ### #### ##### ######
        if (line[i] == '#' && (i == 0 || std::isspace((unsigned char)line[i-1]))) {
            int start = i;
            while (i < len && line[i] == '#') i++;

            // Continue if the heading is followed by a space
            if (i < len && std::isspace((unsigned char)line[i])) {
                // Mark the # characters as KEYWORD
                tokens.push_back({Tokentype::KEYWORD, start, i - start});

                // Mark the heading text as FUNCTION (yellow)
                while (i < len && std::isspace((unsigned char)line[i])) i++;
                int textStart = i;
                while (i < len) i++;
                if (i > textStart) {
                    tokens.push_back({Tokentype::FUNCTION, textStart, i - textStart});
                }
                break;
            }
            continue;
        }

        // Bold: **text** or __text__
        if (i + 1 < len &&
            ((line[i] == '*' && line[i+1] == '*') ||
             (line[i] == '_' && line[i+1] == '_'))) {
            tokens.push_back({Tokentype::KEYWORD, i, 2});
            i += 2;
            continue;
        }

        // Italic: *text* or _text_
        if (line[i] == '*' || line[i] == '_') {
            tokens.push_back({Tokentype::TYPE, i, 1});
            i++;
            continue;
        }

        // Bullet points: * - +
        if ((line[i] == '*' || line[i] == '-' || line[i] == '+') &&
            (i == 0 || std::isspace((unsigned char)line[i-1]))) {
            tokens.push_back({Tokentype::OPERATOR, i, 1});
            i++;
            continue;
        }

        // Numbered list: 1. 2. 3.
        if (std::isdigit((unsigned char)line[i])) {
            int start = i;
            while (i < len && std::isdigit((unsigned char)line[i])) i++;
            if (i < len && line[i] == '.') {
                tokens.push_back({Tokentype::NUMBER, start, i - start + 1});
                i++;
                continue;
            }
            // A number but not a list item, continue normally
            i = start + 1;
            continue;
        }

        // Kod: `code` veya ```code```
        if (line[i] == '`') {
            int start = i;
            i++;

            // Triple backtick?
            if (i + 1 < len && line[i] == '`' && line[i+1] == '`') {
                i += 2;
                // Mark as STRING until the end of the line
                tokens.push_back({Tokentype::STRING, start, len - start});
                break;
            }

            // Single backtick - find the closing one
            while (i < len && line[i] != '`') i++;
            if (i < len) i++; // Include the closing backtick

            tokens.push_back({Tokentype::STRING, start, i - start});
            continue;
        }

        // Link: [text](url) or [text]
        if (line[i] == '[') {
            int start = i;
            i++;

            // Find the ] character
            while (i < len && line[i] != ']') i++;
            if (i < len) i++; // Include ]

            // Followed by (?
            if (i < len && line[i] == '(') {
                while (i < len && line[i] != ')') i++;
                if (i < len) i++; // Include )
            }

            tokens.push_back({Tokentype::FUNCTION, start, i - start});
            continue;
        }

        // Special words: TODO, NOTE, etc.
        if (isIdentifierChar(line[i]) && !std::isdigit((unsigned char)line[i])) {
            int start = i;
            while (i < len && isIdentifierChar(line[i])) i++;

            std::string word = line.substr(start, i - start);

            // Convert to upper case and check
            std::string upperWord = word;
            std::transform(upperWord.begin(), upperWord.end(), upperWord.begin(), ::toupper);

            if (std::find(markdownKeywords.begin(), markdownKeywords.end(), upperWord) != markdownKeywords.end()) {
                tokens.push_back({Tokentype::PREPROCESSOR, start, i - start});
                continue;
            }
        }
        i++;
    }

    return tokens;
}

static std::vector<Token> tokenizeLine(const std::string& line, const std::string& language)
{
    std::vector<Token> tokens;

    const std::vector<std::string>* keywords = &cppKeywords;
    const std::vector<std::string>* types = &cppTypes;

    // Custom tokenizer for Markdown
    if (language == "markdown" || language == "md") {
        return tokenizeMarkdown(line);
    }

    if (language == "angelscript") {
        keywords = &angelScriptKeywords;
        types = &angelScriptTypes;
    } else if (language == "java") {
        keywords = &javaKeywords;
        types = &javaTypes;
    } else if (language == "javascript" || language == "js") {
        keywords = &cppKeywords;
        types = &cppTypes;
    }

    int i = 0;
    int len = line.size();

    while (i < len) {
        if (std::isspace((unsigned char)line[i])) {
            i++;
            continue;
        }

        if (i + 1 < len && line[i] == '/' && line[i+1] == '/') {
            tokens.push_back({Tokentype::COMMENT, i, len - i});
            break;
        }

        if (line[i] == '#' && (language == "cpp" || language == "c")) {
            tokens.push_back({Tokentype::PREPROCESSOR, i, len - i});
            break;
        }

        if (line[i] == '"' || line[i] == '\'') {
            char quote = line[i];
            int start = i++;
            while (i < len && line[i] != quote) {
                if (line[i] == '\\' && i + 1 < len) i += 2;
                else i++;
            }
            if (i < len) i++;
            tokens.push_back({Tokentype::STRING, start, i - start});
            continue;
        }

        if (std::isdigit((unsigned char)line[i]) ||
            (line[i] == '.' && i + 1 < len && std::isdigit((unsigned char)line[i+1]))) {
            int start = i;
            while (i < len && (std::isdigit((unsigned char)line[i]) ||
                               line[i] == '.' || line[i] == 'f' || line[i] == 'x' ||
                               (line[i] >= 'a' && line[i] <= 'f') ||
                               (line[i] >= 'A' && line[i] <= 'F'))) {
                i++;
            }
            tokens.push_back({Tokentype::NUMBER, start, i - start});
            continue;
        }

        if (isIdentifierChar(line[i]) && !std::isdigit((unsigned char)line[i])) {
            int start = i;
            while (i < len && isIdentifierChar(line[i])) {
                i++;
            }

            std::string word = line.substr(start, i - start);

            int j = i;
            while (j < len && std::isspace((unsigned char)line[j])) j++;
            bool isFunc = (j < len && line[j] == '(');

            Tokentype type = Tokentype::NORMAL;

            if (std::find(keywords->begin(), keywords->end(), word) != keywords->end()) {
                type = Tokentype::KEYWORD;
            }
            else if (std::find(types->begin(), types->end(), word) != types->end()) {
                type = Tokentype::TYPE;
            }
            else if (isFunc) {
                type = Tokentype::FUNCTION;
            }

            tokens.push_back({type, start, i - start});
            continue;
        }

        if (isOperatorChar(line[i])) {
            int start = i;
            while (i < len && isOperatorChar(line[i])) {
                i++;
            }
            tokens.push_back({Tokentype::OPERATOR, start, i - start});
            continue;
        }

        i++;
    }

    return tokens;
}

static cColor getTokenColor(Tokentype type) {
    switch (type) {
    case Tokentype::KEYWORD: return syntaxColors.keyword;
    case Tokentype::TYPE: return syntaxColors.type;
    case Tokentype::STRING: return syntaxColors.string;
    case Tokentype::COMMENT: return syntaxColors.comment;
    case Tokentype::NUMBER: return syntaxColors.number;
    case Tokentype::FUNCTION: return syntaxColors.function;
    case Tokentype::PREPROCESSOR: return syntaxColors.preprocessor;
    case Tokentype::OPERATOR: return syntaxColors.operatorColor;
    default: return syntaxColors.normal;
    }
}


#endif // AXGUITEXTBOX_H
