#ifndef CSS_SAVE_H
#define CSS_SAVE_H

class DataSrc;

#ifdef SAVE_SUPPORT

class URL;
class SavedUrlCache;
class UnicodeFileOutputStream;
class Head;
class Window;

OP_STATUS SaveCSSWithInline(URL& url, const uni_char* fname, const char* force_encoding, const char* parent_encoding, SavedUrlCache* su_cache, UnicodeFileOutputStream* out, DataSrc* src_head, Window* window, BOOL main_page = FALSE);

void SaveCSSWithInlineL(URL& url, const uni_char* fname, const char* force_encoding, const char* parent_encoding, SavedUrlCache* su_cache, UnicodeFileOutputStream* out, DataSrc* src_head, Window* window, BOOL main_page = FALSE);

#endif // SAVE_SUPPORT

#if defined STYLE_EXTRACT_URLS
class DataSrc;
/// Build up a list of all URLs referenced from a CSS file
void ExtractURLsL(URL& base_url, URL& css_url, AutoDeleteHead* link_list);

OP_STATUS ExtractURLs(URL& base_url, URL& css_url, AutoDeleteHead* link_list);

/// Build up a list of all URLs referenced from a CSS buffer
void ExtractURLsL(URL& url, DataSrc* src_head, AutoDeleteHead* link_list);

OP_STATUS ExtractURLs(URL& url, DataSrc* src_head, AutoDeleteHead* link_list);
#endif // STYLE_EXTRACT_URLS

#endif // CSS_SAVE_H

