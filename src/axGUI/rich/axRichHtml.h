// axRichHtml.h - HTML <-> rich-text lines
//
// Import (copies from browser, Word, Gmail, Google Docs):
//   inline: bold, italic, underline, strikethrough, inline code, link, text color, highlight (background),
//           font size (only clearly smaller/larger: 13-17 px counts as "normal")
//   line:   heading 1-3 (h4-h6 -> 3), bullet / numbered list (nested -> indent), quote, code block, alignment
//   table:  each row becomes a line, cells joined with " | ";  images: data: URI (http(s) only via fetch)
//   Text colors very close to black/white and white / very dark backgrounds are dropped (so theme-dependent colors stay readable).
// Export: headings h1-h3, lists ul / ol, to-dos ☐ / ☑, quote, code block, alignment, colors.
#pragma once
#include "axRichText.h"
#include <string>
#include <vector>
#include <functional>

// http(s) image downloader (optional): bytes + extension (".png", ".jpg"...); false on failure
using RtFetchImageFn = std::function<bool(const std::string &url, std::vector<unsigned char> &bytes, std::string &ext)>;

// HTML -> lines. Attributes / embeds are added to the doc's tables. Empty if there is no text.
// With a single line and no line format the result is 1 line (inserted inline into the cursor's line); otherwise an empty
// line is appended at the end (RichDoc::insertLines gives the last line the cursor line's format).
std::vector<RtLine> rtHtmlToLines(const std::string &html, RichDoc &doc, const RtFetchImageFn &fetch = nullptr);

// Lines -> HTML fragment (attrs: JSON of the RtChar::attr indices used in the lines)
std::string rtLinesToHtml(const std::vector<RtLine> &lines, const std::vector<RtJson> &attrs,
                          const std::vector<RtJson> &embeds, const RtStyleSheet &ss);

struct RichDocHtml
{
    static bool import(RichDoc &doc, const std::string &html, const RtFetchImageFn &fetch = nullptr);   // modifies the document
    static std::string exportHtml(const RichDoc &doc, const RtStyleSheet &ss = RtStyleSheet());
};
