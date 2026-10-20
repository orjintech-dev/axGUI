// axRichClip.h - HTML on the system clipboard (formatted copy / paste). Does not include raylib.h (windows.h on Windows).
#pragma once
#include <string>
#include <cstring>

// HTML fragment on the clipboard (UTF-8). Empty if absent / unsupported.
//  Windows: "HTML Format" (CF_HTML), between StartFragment..EndFragment
//  Linux:   text/html via xclip / wl-paste
//  others:  empty
std::string rtClipboardHtml();

// Writes formatted (HTML) + plain text to the clipboard together (Google Docs, LibreOffice, Word, email accept the formatting).
//  Windows: CF_HTML + CF_UNICODETEXT;  others: plain text only (SetClipboardText)
void rtSetClipboardHtml(const std::string &html, const std::string &plain);

// Whether the clipboard has text (on Windows GetClipboardText() prints a GLFW warning when only an image is present).
bool rtClipboardHasText();

// Whether the "text" read from the clipboard is actually binary data (on Linux, when only an image is present GLFW may return
// the image bytes as text): known signatures, invalid UTF-8 or control characters.
inline bool rtClipboardTextLooksBinary(const std::string &s)
{
    for (const char *sig : { "\x89PNG", "?PNG", "\xC2\x89PNG", "\xFF\xD8\xFF", "\xC3\xBF\xC3\x98\xC3\xBF", "GIF8", "BM6", "RIFF" })
        if (s.compare(0, strlen(sig), sig) == 0) return true;
    const unsigned char *p = (const unsigned char *)s.data(), *e = p + s.size();
    while (p < e) {
        unsigned c = *p;
        if (c < 0x80) { if (c < 0x20 && c != '\n' && c != '\r' && c != '\t') return true; p++; continue; }
        int extra = (c & 0xE0) == 0xC0 ? 1 : (c & 0xF0) == 0xE0 ? 2 : (c & 0xF8) == 0xF0 ? 3 : -1;
        if (extra < 0 || e - p < extra + 1) return true;
        for (int k = 1; k <= extra; k++) if ((p[k] & 0xC0) != 0x80) return true;
        p += extra + 1;
    }
    return false;
}
