#ifndef AXUTF8_H
#define AXUTF8_H
#include <string>
// #include <iostream>
#include <cstring>
#include <vector>

static int cntByte(unsigned char c) {
    if ((c & 0x80) == 0x00) return 1;
    else if ((c & 0xE0) == 0xC0) return 2;
    else if ((c & 0xF0) == 0xE0) return 3;
    else if ((c & 0xF8) == 0xF0) return 4;
    else return 0;
}

static std::string toUtf8Char(int codepoint)
{
    std::string out;
    if (codepoint < 0x80) {
        out += (char)codepoint;
    } else if (codepoint < 0x800) {
        out += (char)(0xC0 | (codepoint >> 6));
        out += (char)(0x80 | (codepoint & 0x3F));
    } else if (codepoint < 0x10000) {
        out += (char)(0xE0 | (codepoint >> 12));
        out += (char)(0x80 | ((codepoint >> 6) & 0x3F));
        out += (char)(0x80 | (codepoint & 0x3F));
    } else if (codepoint < 0x110000) {
        out += (char)(0xF0 | (codepoint >> 18));
        out += (char)(0x80 | ((codepoint >> 12) & 0x3F));
        out += (char)(0x80 | ((codepoint >> 6) & 0x3F));
        out += (char)(0x80 | (codepoint & 0x3F));
    }
    return out;
}


// ============================================================================
// UTF-8 Helper Functions
// ============================================================================

static void utf8_encode(int cp, char* buf) {
    if (cp < 0x80) {
        buf[0] = cp;
        buf[1] = 0;
    } else if (cp < 0x800) {
        buf[0] = 0xC0 | (cp >> 6);
        buf[1] = 0x80 | (cp & 0x3F);
        buf[2] = 0;
    } else if (cp < 0x10000) {
        buf[0] = 0xE0 | (cp >> 12);
        buf[1] = 0x80 | ((cp >> 6) & 0x3F);
        buf[2] = 0x80 | (cp & 0x3F);
        buf[3] = 0;
    } else {
        buf[0] = 0xF0 | (cp >> 18);
        buf[1] = 0x80 | ((cp >> 12) & 0x3F);
        buf[2] = 0x80 | ((cp >> 6) & 0x3F);
        buf[3] = 0x80 | (cp & 0x3F);
        buf[4] = 0;
    }
}

static int utf8_char_length(const std::string& s, int pos) {
    if (pos >= (int)s.size()) return 0;
    unsigned char c = s[pos];
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

static int utf8_next(const std::string& s, int pos) {
    if (pos >= (int)s.size()) return pos;
    return pos + utf8_char_length(s, pos);
}

static int utf8_prev(const std::string& s, int pos) {
    if (pos <= 0) return 0;
    pos--;
    while (pos > 0 && (s[pos] & 0xC0) == 0x80) pos--;
    return pos;
}

static int utf8_count_codepoints(const std::string& text) {
    int count = 0;
    int pos = 0;
    while (pos < (int)text.size()) {
        pos = utf8_next(text, pos);
        count++;
    }
    return count;
}

static void erase_cp_left(std::string& text, int& cursor) {
    if (cursor <= 0) return;
    int prevPos = utf8_prev(text, cursor);
    text.erase(prevPos, cursor - prevPos);
    cursor = prevPos;
}

static void erase_cp_right(std::string& text, int& cursor) {
    if (cursor >= (int)text.size()) return;
    int nextPos = utf8_next(text, cursor);
    text.erase(cursor, nextPos - cursor);
}

// UTF-8 helper functions
static int utf8_byte_offset(const std::string& text, int codepoint_offset)
{
    int pos = 0;
    int count = 0;
    while (pos < (int)text.size() && count < codepoint_offset) {
        pos = utf8_next(text, pos);
        count++;
    }
    return pos;
}

class UTF8
{
public:
    std::string subStr(const char* cStr, int start, int length)
    {
        int charPos = 0;
        int i = 0;
        std::string result;
        result.reserve(length * 4); // worst-case UTF-8 size

        // Skip until start
        while (cStr[i] != '\0' && charPos < start)
        {
            int b = cntByte((unsigned char)cStr[i]);
            if (b == 0) return "";
            i += b;
            charPos++;
        }

        while (cStr[i] != '\0' && charPos < start + length)
        {
            int b = cntByte((unsigned char)cStr[i]);
            if (b == 0) break;
            result.append(cStr + i, b);
            i += b;
            charPos++;
        }
        return result;
    }

    // Advances by `count` UTF-8 codepoints from byte position `pos`
    int advance(const char* cStr, int pos, int count)
    {
        int i = pos,  moved = 0;

        while (cStr[i] != '\0' && moved < count) {
            int b = cntByte((unsigned char)cStr[i]);
            if (b == 0) break;  // malformed UTF-8
            i += b;
            moved++;
        }
        return i; // new byte position
    }

    int length(const char *cStr)
    {
        int len = 0, i = 0;
        while (cStr[i] != '\0') {
            int b = cntByte((unsigned char)cStr[i]);
            if (b == 0 || cStr[i + b - 1] == '\0') break;
            i += b;
            len++;
        }
        return len;
    }

    int length(const std::string &cStr)    {        return length(cStr.c_str());    }

    std::string charAt(const char *cStr, int index)
    {
        return subStr(cStr, index, 1);
    }

    void insert(std::string &line, int curPos, const std::string &s)
    {
        if(curPos < 0 || curPos > (int)line.length()) return; // Invalid index
        std::string s1 = subStr(line.c_str(), 0, curPos);
        std::string s2 = subStr(line.c_str(), curPos, length(line.c_str()) - curPos);
        line = s1 + s + s2; // Insert substring at position
    }

    void deleteCharAt(std::string& str, int charPos, int len=1)
    {
        int totalChars = length(str.c_str());
        if (charPos < 0 || charPos >= totalChars) return;
        std::string before = subStr(str.c_str(), 0, charPos);
        std::string after;
        int afterLen = totalChars - (charPos + len);
        if(afterLen>0) after = subStr(str.c_str(), charPos + len, afterLen);
        str = before + after;
    }

    int findChar(std::string& str, int pos, std::string ch, bool backward)
    {
        int totalChars = length(str.c_str());
        if(backward)
        {
            for(int i=std::max(pos,totalChars-1); i>=0; i--)
            {
                std::string c = subStr(str.c_str(), i, 1);
                printf("%s",c.c_str());
                if(c == ch) printf("back:%d\n",i);
                if(c == ch) return i+1;
            }
            return 0;
        }
        else
        {
            for(int i=pos; i<totalChars; i++)
            {
                std::string c = subStr(str.c_str(), i, 1);
                printf(" %s",c.c_str());
                if(c == ch)  printf("frwd:%d\n",i);
                if(c == ch) return i;
            }
            return totalChars-1;
        }
        return -1;
    }

    std::vector<std::string> split(const std::string& text, const std::string& delimiter = "\t")
    {
        std::vector<std::string> result;
        std::string current;
        int pos = 0;

        while (pos < (int)text.size())
        {
            int charLen = utf8_char_length(text, pos);
            std::string ch = text.substr(pos, charLen);

            if (ch == delimiter || ch=="\r")
            {
                result.push_back(current);
                current.clear();
            } else {
                current += ch;
            }
            pos += charLen;
        }

        // Add last part
        result.push_back(current);
        return result;
    }

    std::string removeChars(const std::string& text, const std::string& charsToRemove)
    {
        std::string result;
        int pos = 0;

        while (pos < (int)text.size())
        {
            int charLen = utf8_char_length(text, pos);
            std::string ch = text.substr(pos, charLen);

            if (charsToRemove.find(ch) == std::string::npos)
            {
                result += ch;
            }
            pos += charLen;
        }
        return result;
    }

};




#endif // AXUTF8_H
