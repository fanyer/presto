/** @file EmBrowser_main.h
  *
  * Internal interface to EmBrowser functions.
  *
  * @author Jason Hazlett
  * @author Anders Markussen
  */


#ifndef EMBROWSER_MAIN_H
#define EMBROWSER_MAIN_H

#include "adjunct/embrowser/EmBrowser.h"
class URL;

#ifdef MSWIN	//can be used for other !_MACINTOSH_
struct EmVendorBrowserInitParameterBlock  
{
	long 				majorVersion;
	long 				minorVersion;
	char				registrationCode[64];
};
#endif 

class WindowCommander;
class Window;
class FramesDocument;

WindowCommander* DEPRECATED(MakeNewWidget(BOOL background, int width, int height, BOOL is_client_size, BOOL3 scrollbars, BOOL3 toolbars, BOOL open_in_page, const uni_char* destURL, Window* parent = NULL));
OpWindowCommander* DEPRECATED(GetActiveEmBrowserWindowCommander());
EmBrowserRef DEPRECATED(GetBrowserRefFromWindow(Window* window));
EmBrowserRef DEPRECATED(GetBrowserRefFromDocumentRef(struct OpaqueEmBrowserDocument*));
EmBrowserStatus WidgetShutdownLibrary();

extern EmBrowserAppNotification *gApplicationProcs;
extern long gVendorDataID;
extern class CCarbonEvents* gCarbonEvents;
extern long gEventResponse;

// Return state conversion

#define RETURN_IF_FAILED(x) {OP_STATUS ret = x; if(OpStatus::IsMemoryError(ret)) return emBrowserOutOfMemoryErr; else if(OpStatus::IsError(ret)) return emBrowserGenericErr;}

#endif  //EMBROWSER_MAIN_H
