/*
	File:	WidgetApp.c (Based on BasicRAEL from CarbonSDK)

	Demonstrate getting Opera 7.2 up and running as a Widget
	from a CFM-based host application.
*/

#ifdef __MACH__
#include <Carbon/Carbon.h>
#else
#include <Carbon.h>
#endif

#include "WidgetApp.h"
#include "OperaControl.h"
#include "WidgetAppEvents.h"

// Globals
WidgetAppEvents *gWidgetAppEvents = NULL;
extern EmBrowserRef gWidgetPointer;
extern EmBrowserInitParameterBlock * params;
Boolean	gQuitFlag;

int main(int argc, const char** argv)
{
	Initialize();
	MakeMenu();

    gWidgetAppEvents = new WidgetAppEvents;

    if (gWidgetAppEvents)
    {
	    gWidgetAppEvents->InstallCarbonEventHandlers();
		gWidgetAppEvents->InstallPeriodicEventHandler(10);
	}

    RunApplicationEventLoop();

	if (gWidgetAppEvents)
	{
	    gWidgetAppEvents->RemoveCarbonEventHandlers();
	    gWidgetAppEvents->RemovePeriodicEventHandler();
	}

	// If widget was not destroyed yet, do it now.
	if (gWidgetPointer && params->destroyInstance)
		DestroyWidget();

	/**********************************
	** STEP 6:  SHUTDOWN THE LIBRARY **
	***********************************/
	if (params && params->shutdown)
		params->shutdown();

	return 0;
}

void Initialize()
{
	OSErr	err;

	InitCursor();

    err = AEInstallEventHandler(kCoreEventClass, kAEQuitApplication, NewAEEventHandlerUPP(QuitAppleEventHandler), 0, false);
	if (err != noErr)
		ExitToShell();
}

void MakeWindow(WindowRef *outRef)
{	Rect		wRect;
	WindowRef	window;
	OSStatus	err;
    EventHandlerRef	ref;

	SetRect(&wRect,50,50,600,600);

	err = CreateNewWindow(kDocumentWindowClass, kWindowStandardDocumentAttributes,&wRect, &window);

	InstallStandardEventHandler(GetWindowEventTarget(window));
    gWidgetAppEvents->InstallCarbonWindowHandler(window,&ref);

    ShowWindow(window);
    *outRef = window;
}

void MakeMenu(void)
{
	Handle		menuBar;
	MenuRef 	menu;
	long 		response;
	OSStatus	err = noErr;

	menuBar = GetNewMBar(128);
	if (menuBar)
	{
		SetMenuBar(menuBar);

		// see if we should modify quit in accordance with the Aqua HI guidelines
		err = Gestalt(gestaltMenuMgrAttr, &response);
		if ((err == noErr) && (response & gestaltMenuMgrAquaLayoutMask))
		{
			menu = GetMenuHandle(mFile);
			DeleteMenuItem(menu, iQuit);
			DeleteMenuItem(menu, iQuitSeperator);
		}
		DrawMenuBar();
	}
	else
		DebugStr("\p MakeMenu failed");

}

static pascal OSErr QuitAppleEventHandler(const AppleEvent *appleEvt, AppleEvent* reply, long refcon)
{
#pragma unused (appleEvt, reply, refcon)
	QuitApplicationEventLoop();

	return noErr;
}

void DoAboutBox(void)
{
	Alert(kAboutBox, nil);  // simple alert dialog box
}

