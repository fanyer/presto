#ifndef MACUTILS_H_
#define MACUTILS_H_

#include "adjunct/desktop_scope/src/scope_desktop_window_manager.h"

Boolean IsCacheFolderOwner();
void RemoveCacheLock();
void CleanupOldCacheFolders();
void GetOperaCacheFolderName(OpString &path, BOOL bOpCache, BOOL bParentOnly = FALSE);
void CheckAndFixCacheFolderName(OpString &path);

char *GetVersionString();
long GetBuildNumber();
int  GetVideoRamSize();
int  GetMaxScreenSize();
int  GetMaxScreenDimension();

Boolean OpenFileOrFolder(FSRefPtr inSpec);

Boolean MacOpenURLInDefaultApplication(const uni_char* url, bool inOpera = false);
Boolean MacOpenURLInApplication(const uni_char* url, const uni_char* appPath, bool new_instance = false);
Boolean MacOpenURLInTrustedApplication(const uni_char *url);
Boolean MacGetDefaultApplicationPath(IN const uni_char* url, OUT OpString& appName, OUT OpString& appPath);
Boolean MacExecuteProgram(const uni_char* program, const uni_char* parameters);

void MacGetBookmarkFileLocation(int format, OpString &location);
const uni_char * GetDefaultLanguage();
Boolean FindUniqueFileName(OpString &path, const OpString &suggested);
BOOL SetOpString(OpString& dest, CFStringRef source);
bool GetSignatureAndVersionFromBundle(CFURLRef bundleURL, OpString *signature, OpString *short_version);

#ifdef HC_CAP_NEWUNISTRLIB
char *StrToLocaleEncoding(const uni_char *str);
uni_char *StrFromLocaleEncoding(const char *str);
#endif // HC_CAP_NEWUNISTRLIB
void ComposeExternalMail(const uni_char* to, const uni_char* cc, const uni_char* bcc, const uni_char* subject, const uni_char* message, const uni_char* raw_address, int mailhandler);

OpPoint ConvertToScreen(const OpPoint &point, class OpWindow* pw, class MDE_OpView* pv); 

BOOL OpenFileFolder(const OpStringC & file_path);
void PostDummyEvent();

OpStringC	GetCFBundleName();
CFStringRef GetCFBundleID();
CFStringRef GetOperaBundleID();
void		*GetMainMenu();
void		*GetActiveMenu();
OP_STATUS	SetQuickExternalMenuInfo(void *menu, OpScopeDesktopWindowManager_SI::QuickMenuInfo& info);
OP_STATUS	SetQuickInternalMenuInfo(void *menu, OpScopeDesktopWindowManager_SI::QuickMenuInfo& info);
OP_STATUS	SetQuickMenuItemInfo(void *menu_item, OpInputAction *action, int row, OpScopeDesktopWindowManager_SI::QuickMenuInfo& info);
void		SelectPopMenuItemWithTitle(void *popmenu, const OpStringC& item_text);


void SendMenuHighlightToScope(CFStringRef item_title, BOOL main_menu, BOOL is_submenu);
void SendMenuTrackingToScope(BOOL tracking);
void SendSetActiveNSMenu(void *nsmenu);
void SendSetActiveNSPopUpButton(void *nspopupbutton);
void PressScopeKey(OpKey::Code key, ShiftKeyState modifier);

/**
 * This function is used in OSX <= 10.5 to setup background image
 * newer versions has [NSWorkspace setDesktopImageURL:forScreen:options:error:]
 */
OP_STATUS SetBackgroundUsingAppleEvents(URL& url);

/**
 * @brief Iterates folder and calls given function.
 * @return Copy of callbcak function (object)
 * @remarks fun's signature should be/support:
 * \code
 void fun(ItemCount count, const FSCatalogInfo* inCatInfo, const FSRef* refs)
 * \endcode
 */
template<typename T>
inline T MacForEachItemInFolder(const FSRef which_folder,
								T fun, 
								const FSCatalogInfoBitmap kCatalogInfoBitmap = 
											kFSCatInfoNodeFlags | kFSCatInfoFinderInfo) {
	const size_t kRequestCountPerIteration = ((4096 * 4) / sizeof(FSCatalogInfo));
    FSIterator iterator;
    FSCatalogInfo * catalogInfoArray;
	
    // Create an iterator
    OSStatus outStatus = FSOpenIterator(&which_folder, kFSIterateFlat, &iterator );
	
	if(outStatus == noErr)
    {
        // Allocate storage for the returned information
        catalogInfoArray = (FSCatalogInfo *)malloc(sizeof(FSCatalogInfo) * kRequestCountPerIteration);
		FSRef* fsrefs = (FSRef*)malloc(sizeof(FSRef) * kRequestCountPerIteration);
		
        if(catalogInfoArray == NULL) {
            outStatus = memFullErr;
        } else {
            // Request information about files in the given directory,
            // until we get a status code back from the File Manager
            do {
                ItemCount actualCount;
				
                outStatus = FSGetCatalogInfoBulk(iterator, kRequestCountPerIteration,
												 &actualCount, NULL, kCatalogInfoBitmap,
												 catalogInfoArray, fsrefs, NULL, NULL );
				
                // Process all items received
                if(outStatus == noErr || outStatus == errFSNoMoreItems)
                    for(UInt32 index = 0; index < actualCount; index += 1) {
                        // Do something interesting with the object found						
						fun(actualCount, &catalogInfoArray[index], &fsrefs[index]);
                    }
            } while(outStatus == noErr);
			
            // errFSNoMoreItems tells us we have successfully processed all
            // items in the directory -- not really an error
            if (outStatus == errFSNoMoreItems)
                outStatus = noErr;
			
            // Free the array memory
            free(catalogInfoArray);
			free(fsrefs);
        }
    }
	
    FSCloseIterator(iterator);
	return fun;
}

#endif //MACUTILS_H_
