/*
	File:		OperaControl.h
*/

#ifndef OPERA_CONTROL_H
#define OPERA_CONTROL_H

#include "adjunct/embrowser/embrowser.h"

//************** Local Functions and defines
extern EmBrowserInitLibraryProc gInitLibraryProc;

EmBrowserStatus InitLibrary();
void CreateWidget();
void LoadURL();
void DestroyWidget();
void SetWidgetBounds(const Rect *rect);
void GetDocumentSource(EmDocumentRef DocIn);
void TestDocumentHandling2();
Boolean AnalyzeOperaAPIVersion(CFBundleRef operaBundle);
Boolean DiscoverOpera();

// Notification from Opera to the Host App
void Notify(EmBrowserRef inst, EmBrowserAppMessage msg, long value);
long BeforeNavigation(EmBrowserRef inst, const EmBrowserString destURL, EmBrowserString newURL);
long BeforeNewBrowser(EmBrowserRef caller, EmBrowserOptions browserOptions, EmBrowserWindowOptions windowOptions, EmBrowserRect *bounds, const EmBrowserString destURL, EmBrowserRef* result);
long PositionChangeNotify(EmBrowserRef widget, long x_pos, long y_pos);
long SizeChangeNotify(EmBrowserRef widget, long width, long height);
long WackyCursors(EmMouseCursor cursorKind);

// API Test functions connected to menu items
void TestAPI1();
void TestAPI2();
void TestAPI3();
EmBrowserStatus TestSuite();
EmBrowserStatus ShowThumbnail();

// Utility funtions
size_t EmBrowserStringLen(EmBrowserString inStr);
int EmBrowserStringCompare(EmBrowserString inStr1, EmBrowserString inStr2, size_t inNumChars = -1);
EmBrowserString EmBrowserStringChr(EmBrowserString str, EmBrowserChar c);
Boolean ConvertUTF8ToEmBrowserString(const char* inutf8str, EmBrowserString outBuffer, size_t outBufferLen);
OSStatus LoadFrameworks();

// Import functions in ApplicationServices.framework (for CFM)
typedef CFTypeID (*CGImageGetTypeIDProcPtr)(void);
typedef void (*CGImageReleaseProcPtr)(CGImageRef image);
typedef void (*CGContextReleaseProcPtr)(CGContextRef ctx);
typedef void (*CGContextFlushProcPtr)(CGContextRef ctx);
typedef void (*CGContextDrawImageProcPtr)(CGContextRef ctx, CGRect rect, CGImageRef image);
typedef OSStatus (*SyncCGContextOriginWithPortProcPtr)(CGContextRef inContext, CGrafPtr port);
typedef OSStatus (*CreateCGContextForPortProcPtr)(CGrafPtr inPort, CGContextRef* outContext);
typedef CGImageRef (*CGImageRetainProcPtr)(CGImageRef image);

// Import functions in Carbon.framework (for CFM)
typedef OSStatus (*CreateStandardAlertProcPtr)(AlertType alertType,CFStringRef error, CFStringRef explanation, const AlertStdCFStringAlertParamRec *  param, DialogRef *outAlert);
typedef OSStatus (*RunStandardAlertProcPtr)(DialogRef inAlert, ModalFilterUPP filterProc, DialogItemIndex *outItemHit);
#endif

#ifdef WIDGET_APP_ORIGIN_TEST
extern void TestPort();
#define TEST_PORT TestPort();
#else
#define TEST_PORT
#endif

