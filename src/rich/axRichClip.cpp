// axRichClip.cpp - system clipboard (HTML). Ported from NotePen platform.cpp.
// NOTE: does not include raylib.h (Rectangle / CloseWindow / DrawText clash with windows.h); separate translation unit.
#include "axRichClip.h"
#include <cstdlib>
#include <cstdio>
#include <string>

extern "C" {
void *GetWindowHandle(void);                 // raylib
void SetClipboardText(const char *text);     // raylib
}

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

void rtSetClipboardHtml(const std::string &html, const std::string &plain)
{
#ifdef _WIN32
    static UINT fmt = RegisterClipboardFormatW(L"HTML Format");
    // CF_HTML header: offsets are in bytes, written fixed-width (10 digits)
    const char *hdrFmt = "Version:0.9\r\nStartHTML:%010d\r\nEndHTML:%010d\r\nStartFragment:%010d\r\nEndFragment:%010d\r\n";
    const std::string pre = "<html><head><meta charset=\"utf-8\"></head><body>\r\n<!--StartFragment-->";
    const std::string post = "<!--EndFragment-->\r\n</body></html>";
    char hdr[160];
    int hlen = snprintf(hdr, sizeof(hdr), hdrFmt, 0, 0, 0, 0);
    int startHtml = hlen, startFrag = startHtml + (int)pre.size(), endFrag = startFrag + (int)html.size();
    int endHtml = endFrag + (int)post.size();
    snprintf(hdr, sizeof(hdr), hdrFmt, startHtml, endHtml, startFrag, endFrag);
    std::string cf = std::string(hdr) + pre + html + post;
    int wn = MultiByteToWideChar(CP_UTF8, 0, plain.c_str(), (int)plain.size(), nullptr, 0);
    // an owner window is required: SetClipboardData fails after OpenClipboard(NULL) + EmptyClipboard
    if (!fmt || !OpenClipboard((HWND)GetWindowHandle())) { SetClipboardText(plain.c_str()); return; }
    EmptyClipboard();
    if (HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, cf.size() + 1)) {
        if (char *p = (char *)GlobalLock(h)) { memcpy(p, cf.c_str(), cf.size() + 1); GlobalUnlock(h); SetClipboardData(fmt, h); }
        else GlobalFree(h);
    }
    if (HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (size_t)(wn + 1) * sizeof(wchar_t))) {
        if (wchar_t *p = (wchar_t *)GlobalLock(h)) {
            MultiByteToWideChar(CP_UTF8, 0, plain.c_str(), (int)plain.size(), p, wn);
            p[wn] = 0;
            GlobalUnlock(h);
            SetClipboardData(CF_UNICODETEXT, h);
        } else GlobalFree(h);
    }
    CloseClipboard();
#else
    (void)html;
    SetClipboardText(plain.c_str());
#endif
}

std::string rtClipboardHtml()
{
#ifdef _WIN32
    static UINT fmt = RegisterClipboardFormatW(L"HTML Format");
    if (!fmt || !IsClipboardFormatAvailable(fmt)) return "";
    if (!OpenClipboard(nullptr)) return "";
    std::string data;
    if (HANDLE h = GetClipboardData(fmt)) {
        if (const char *p = (const char *)GlobalLock(h)) {
            size_t n = GlobalSize(h);
            data.assign(p, strnlen(p, n));
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    auto num = [&](const char *key) -> long {
        size_t k = data.find(key);
        if (k == std::string::npos || k > 1024) return -1;
        return std::strtol(data.c_str() + k + strlen(key), nullptr, 10);
    };
    // source page ("SourceURL:"): the HTML converter treats "#" links to the same page as in-document links
    std::string src;
    {
        size_t k = data.find("SourceURL:");
        if (k != std::string::npos && k < 1024) {
            size_t e = data.find_first_of("\r\n", k);
            src = data.substr(k + 10, e == std::string::npos ? std::string::npos : e - k - 10);
            if (src.find("-->") != std::string::npos) src.clear();
        }
    }
    const std::string head = src.empty() ? std::string() : "<!--np-source:" + src + "-->";
    long a = num("StartFragment:"), b = num("EndFragment:");
    if (a >= 0 && b > a && (size_t)b <= data.size()) return head + data.substr((size_t)a, (size_t)(b - a));
    a = num("StartHTML:"); b = num("EndHTML:");
    if (a >= 0 && b > a && (size_t)b <= data.size()) return head + data.substr((size_t)a, (size_t)(b - a));
    return "";
#elif defined(__linux__) && !defined(__ANDROID__)
    // X11: xclip, Wayland: wl-paste (empty if missing / no HTML on the clipboard). Target list first.
    const char *cmds[][2] = { { "wl-paste -l 2>/dev/null", "wl-paste -n -t text/html 2>/dev/null" },
                              { "xclip -selection clipboard -t TARGETS -o 2>/dev/null", "xclip -selection clipboard -t text/html -o 2>/dev/null" } };
    for (auto &pair : cmds) {
        const char *cmd = pair[1];
        if (std::strncmp(cmd, "wl", 2) == 0 && !std::getenv("WAYLAND_DISPLAY")) continue;
        if (std::strncmp(cmd, "xclip", 5) == 0 && !std::getenv("DISPLAY")) continue;
        {
            FILE *lf = popen(pair[0], "r");
            if (!lf) continue;
            std::string types;
            char tb[1024];
            size_t tr;
            while ((tr = fread(tb, 1, sizeof(tb), lf)) > 0) types.append(tb, tr);
            pclose(lf);
            if (types.find("text/html") == std::string::npos) continue;
        }
        FILE *f = popen(cmd, "r");
        if (!f) continue;
        std::string out;
        char buf[4096];
        size_t r;
        while ((r = fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, r);
        int st = pclose(f);
        if (st == 0 && !out.empty()) return out;
    }
    return "";
#else
    return "";
#endif
}

bool rtClipboardHasText()
{
#ifdef _WIN32
    return IsClipboardFormatAvailable(CF_UNICODETEXT) || IsClipboardFormatAvailable(CF_TEXT);
#else
    return true;
#endif
}
