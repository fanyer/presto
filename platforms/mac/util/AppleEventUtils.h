#ifndef APPLE_EVENT_UTILS_H
#define APPLE_EVENT_UTILS_H

class CWidgetWindow;
class DesktopWindow;
class BrowserDesktopWindow;

void CloseEveryWindow();

unsigned int GetExportIDForDesktopWindow(DesktopWindow* win, Boolean toplevel);
DesktopWindow* GetDesktopWindowForExportID(unsigned int export_id, Boolean toplevel);
void InformDesktopWindowCreated(DesktopWindow* new_win);
void InformDesktopWindowRemoved(DesktopWindow* dead_win);

// These functions are used to create export descriptors for internal items.
// All you should provide them with is index numbers of some sort. DO NOT PASS POINTERS.
OSErr CreateAEDescFromID(unsigned int id_num, DescType object_type, AEDesc* item_desc);
OSErr AddIDToAEList(unsigned int id_num, long index, DescType object_type, AEDescList* list);
OSErr CreateAEListFromIDs(unsigned int* id_nums, long count, DescType object_type, AEDescList* list);

// No window counting logic should be outside these functions
int GetWindowCount(Boolean toplevel, BrowserDesktopWindow* container);
int GetTopLevelWindowNumber(DesktopWindow* window);
Boolean SetTopLevelWindowNumber(DesktopWindow* window, int number);
DesktopWindow* GetNumberedWindow(int number, Boolean toplevel, BrowserDesktopWindow* container);
DesktopWindow* GetOrdinalWindow(DescType ordinal, Boolean toplevel, BrowserDesktopWindow* container);
DesktopWindow* GetNamedWindow(const uni_char* name, Boolean toplevel, BrowserDesktopWindow* container);
DesktopWindow* GetWindowFromAEDesc(const AEDesc *inDesc);

// Ready-to-serve AEDescs.
OSErr CreateDescForWindow(DesktopWindow* win, Boolean toplevel, AEDesc* item_desc);
OSErr CreateDescForNumberedWindow(int number, Boolean toplevel, BrowserDesktopWindow* container, AEDesc* item_desc);
OSErr CreateDescForOrdinalWindow(DescType ordinal, Boolean toplevel, BrowserDesktopWindow* container, AEDesc* item_desc);
OSErr CreateDescForNamedWindow(const uni_char* name, Boolean toplevel, BrowserDesktopWindow* container, AEDesc* item_desc);

uni_char* GetNewStringFromObjectAndKey(const AEDescList* desc, AEKeyword keyword);
DesktopWindow* GetWindowFromObjectAndKey(const AEDescList* desc, AEKeyword keyword);
Boolean GetFileFromObjectAndKey(const AEDescList* desc, AEKeyword keyword, FSRef* file, Boolean create);
uni_char* GetNewStringFromListAndIndex(const AEDescList* desc, long index);

#ifdef _MAC_DEBUG
#define FCC_TO_CHARS(a) ((char)((a) >> 24) & 0xFF), ((char)((a) >> 16) & 0xFF), ((char)((a) >> 8) & 0xFF), ((char)(a) & 0xFF)
void ObjectTraverse(const AEDescList *object);
#endif

#endif // APPLE_EVENT_UTILS_H
