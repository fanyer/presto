/*
	File:		WidgetAppEvents.c

	WidgetApp Event Handling.
*/

#ifdef __MACH__
#include <Carbon/Carbon.h>
#else
#include <Carbon.h>
#endif

#include "WidgetApp.h"
#include "OperaControl.h"
#include "WidgetAppEvents.h"
#include <stdio.h>

extern WidgetAppEvents *gWidgetAppEvents;
extern EmBrowserRef gWidgetPointer;
extern EmBrowserMethods * widgetMethods;
extern EmBrowserInitParameterBlock * params;
extern WindowRef gWindow;
extern Boolean gLibraryInitialized;
extern Boolean gUseWackyCursors;

WidgetAppEvents::WidgetAppEvents()
{
	mCommandHandler = NULL;
	mWindowHandler = NULL;
}

WidgetAppEvents::~WidgetAppEvents()
{
}

#pragma mark -
OSStatus WidgetAppEvents::InstallCarbonEventHandlers()
{	OSStatus			retVal = noErr;
	EventTargetRef 		currentTarget;
	EventHandlerUPP 	handlerUPP;
	Str255				itemString;

	//*********************************************************************
	// Install command event handler
	//*********************************************************************
	EventTypeSpec commandEventList[] =
	{
		{kEventClassCommand, kEventCommandProcess},
	};

	handlerUPP = NewEventHandlerUPP(CarbonCommandHandler);
	retVal = InstallEventHandler(GetApplicationEventTarget(),
						handlerUPP,
						GetEventTypeCount(commandEventList),
						commandEventList,
						NULL,
						&mCommandHandler);

	return retVal;
}

OSStatus WidgetAppEvents::RemoveCarbonEventHandlers()
{
	OSStatus retVal = noErr;

	if (mCommandHandler != NULL)
		retVal |= RemoveEventHandler(mCommandHandler);

	if (mWindowHandler != NULL)
		retVal |= RemoveEventHandler(mWindowHandler);

	return retVal;
}

#pragma mark -

OSStatus WidgetAppEvents::InstallCarbonWindowHandler(WindowRef			newWindow,
													EventHandlerRef *newWindowHandler)
{
	OSStatus			retVal = noErr;
	EventTargetRef 		windowTarget;
	EventTargetRef 		controlTarget;
	EventHandlerUPP 	handlerUPP;
	ControlRef			rootControl;

	EventTypeSpec		windowEventList[] =
	{
		{kEventClassWindow, kEventWindowClose},
		{kEventClassWindow, kEventWindowBoundsChanged}
	};

	// Install Custom Window Handler
	windowTarget = GetWindowEventTarget(newWindow);
	handlerUPP = NewEventHandlerUPP(CarbonWindowHandler);
	retVal = InstallEventHandler(windowTarget,
						handlerUPP,
						GetEventTypeCount(windowEventList),
						windowEventList,
						NULL,
						&mWindowHandler);

	return retVal;
}


OSStatus WidgetAppEvents::RemoveCarbonWindowHandler(WindowRef			*oldWindow,
													EventHandlerRef *oldWindowHandler)
{
	OSStatus			retVal = noErr;

	retVal = RemoveEventHandler(mWindowHandler);

    DisposeWindow(*oldWindow);

	return retVal;
}

#pragma mark -

OSStatus WidgetAppEvents::InstallPeriodicEventHandler(int periodMS)
{
	OSStatus			retVal = noErr;
	EventTimerInterval 	eventInterval = kEventDurationMillisecond*periodMS;
	EventLoopRef		mainLoop = GetMainEventLoop();
	EventLoopTimerUPP	timerUPP = NewEventLoopTimerUPP(CarbonPeriodicEventHandler);

	retVal = InstallEventLoopTimer(	mainLoop,
									0,
									eventInterval,
									timerUPP,
									NULL,
									&mTimerHandlerRef);
	return retVal;
}

OSStatus WidgetAppEvents::RemovePeriodicEventHandler()
{
	return RemoveEventLoopTimer(mTimerHandlerRef);
}

#pragma mark -

pascal void WidgetAppEvents::CarbonPeriodicEventHandler(EventLoopTimerRef theTimer,
																		void* userData)
{
	OSErr err = noErr;
	MenuRef theMenuRef;
	MenuItemIndex theIndex;
	EmBrowserMessage inMessage = 0;

TEST_PORT

	// Enable the Widget menu
	err = GetIndMenuItemWithCommandID(NULL, kHICommandInit, 1, &theMenuRef, &theIndex);
	if (err == noErr)
	{
		if (!gLibraryInitialized)
			EnableMenuItem(theMenuRef,theIndex);
		else
			DisableMenuItem(theMenuRef,theIndex);
	}

	err = GetIndMenuItemWithCommandID(NULL, kHICommandCreate, 1, &theMenuRef, &theIndex);
	if (err == noErr)
	{
		if (gLibraryInitialized  && !gWidgetPointer)
			EnableMenuItem(theMenuRef,theIndex);
		else
			DisableMenuItem(theMenuRef,theIndex);
	}

	err = GetIndMenuItemWithCommandID(NULL, kHICommandLoadURL, 1, &theMenuRef, &theIndex);
	if (err == noErr)
	{
		if (gLibraryInitialized && gWidgetPointer && widgetMethods)
			EnableMenuItem(theMenuRef,theIndex);
		else
			DisableMenuItem(theMenuRef,theIndex);
	}

	err = GetIndMenuItemWithCommandID(NULL, kHICommandDestroy, 1, &theMenuRef, &theIndex);
	if (err == noErr)
	{
		if (gLibraryInitialized && gWidgetPointer && widgetMethods)
			EnableMenuItem(theMenuRef,theIndex);
		else
			DisableMenuItem(theMenuRef,theIndex);
	}


	/**********************************************************************************
	** STEP 4B:  ASKING OPERA WHICH COMMANDS ARE AVAILABLE AT A GIVEN POINT IN TIME  **
	***********************************************************************************/
	err = GetIndMenuItemWithCommandID(NULL, kHICommandCopy, 1, &theMenuRef, &theIndex);
	if (err == noErr)
	{
		if (gWidgetPointer && widgetMethods)
		{
			inMessage = emBrowser_msg_copy_cmd;
			if (widgetMethods->canHandleMessage(gWidgetPointer, inMessage))
				EnableMenuItem(theMenuRef,theIndex);
			else
				DisableMenuItem(theMenuRef,theIndex);
		}
	}

	err = GetIndMenuItemWithCommandID(NULL, kHICommandPaste, 1, &theMenuRef, &theIndex);
	if (err == noErr)
	{
		if (gWidgetPointer && widgetMethods)
		{
			inMessage = emBrowser_msg_paste_cmd;
			if (widgetMethods->canHandleMessage(gWidgetPointer, inMessage))
				EnableMenuItem(theMenuRef,theIndex);
			else
				DisableMenuItem(theMenuRef,theIndex);
		}
	}

	err = GetIndMenuItemWithCommandID(NULL, kHICommandCut, 1, &theMenuRef, &theIndex);
	if (err == noErr)
	{
		if (gWidgetPointer && widgetMethods)
		{
			inMessage = emBrowser_msg_cut_cmd;
			if (widgetMethods->canHandleMessage(gWidgetPointer, inMessage))
				EnableMenuItem(theMenuRef,theIndex);
			else
				DisableMenuItem(theMenuRef,theIndex);
		}
	}

	err = GetIndMenuItemWithCommandID(NULL, kHICommandUndo, 1, &theMenuRef, &theIndex);
	if (err == noErr)
	{
		if (gWidgetPointer && widgetMethods)
		{
			inMessage = emBrowser_msg_undo_cmd;
			if (widgetMethods->canHandleMessage(gWidgetPointer, inMessage))
				EnableMenuItem(theMenuRef,theIndex);
			else
				DisableMenuItem(theMenuRef,theIndex);
		}
	}

	err = GetIndMenuItemWithCommandID(NULL, kHICommandClear, 1, &theMenuRef, &theIndex);
	if (err == noErr)
	{
		if (gWidgetPointer && widgetMethods)
		{
			inMessage = emBrowser_msg_clear_cmd;
			if (widgetMethods->canHandleMessage(gWidgetPointer, inMessage))
				EnableMenuItem(theMenuRef,theIndex);
			else
				DisableMenuItem(theMenuRef,theIndex);
		}
	}

	// Enable other API testing menus
	err = GetIndMenuItemWithCommandID(NULL, kHICommandStop, 1, &theMenuRef, &theIndex);
	if (err == noErr)
	{
		if (gWidgetPointer && widgetMethods)
		{
			inMessage = emBrowser_msg_stop_loading;
			if (widgetMethods->canHandleMessage(gWidgetPointer, inMessage))
				EnableMenuItem(theMenuRef,theIndex);
			else
				DisableMenuItem(theMenuRef,theIndex);
		}
		else
			DisableMenuItem(theMenuRef,theIndex);
	}

	err = GetIndMenuItemWithCommandID(NULL, kHICommandReload, 1, &theMenuRef, &theIndex);
	if (err == noErr)
	{
		if (gWidgetPointer && widgetMethods)
		{
			inMessage = emBrowser_msg_reload;
			if (widgetMethods->canHandleMessage(gWidgetPointer, inMessage))
				EnableMenuItem(theMenuRef,theIndex);
			else
				DisableMenuItem(theMenuRef,theIndex);
		}
		else
			DisableMenuItem(theMenuRef,theIndex);
	}

	err = GetIndMenuItemWithCommandID(NULL, kHICommandForward, 1, &theMenuRef, &theIndex);
	if (err == noErr)
	{
		if (gWidgetPointer && widgetMethods)
		{
			inMessage = emBrowser_msg_forward;
			if (widgetMethods->canHandleMessage(gWidgetPointer, inMessage))
				EnableMenuItem(theMenuRef,theIndex);
			else
				DisableMenuItem(theMenuRef,theIndex);
		}
		else
			DisableMenuItem(theMenuRef,theIndex);
	}

	err = GetIndMenuItemWithCommandID(NULL, kHICommandBack, 1, &theMenuRef, &theIndex);
	if (err == noErr)
	{
		if (gWidgetPointer && widgetMethods)
		{
			inMessage = emBrowser_msg_back;
			if (widgetMethods->canHandleMessage(gWidgetPointer, inMessage))
				EnableMenuItem(theMenuRef,theIndex);
			else
				DisableMenuItem(theMenuRef,theIndex);
		}
		else
			DisableMenuItem(theMenuRef,theIndex);
	}

	err = GetIndMenuItemWithCommandID(NULL, kHICommandHome, 1, &theMenuRef, &theIndex);
	if (err == noErr)
	{
		if (gWidgetPointer && widgetMethods)
		{
			inMessage = emBrowser_msg_homepage;
			if (widgetMethods->canHandleMessage(gWidgetPointer, inMessage))
				EnableMenuItem(theMenuRef,theIndex);
			else
				DisableMenuItem(theMenuRef,theIndex);
		}
		else
			DisableMenuItem(theMenuRef,theIndex);
	}

	err = GetIndMenuItemWithCommandID(NULL, kHICommandSearch, 1, &theMenuRef, &theIndex);
	if (err == noErr)
	{
		if (gWidgetPointer && widgetMethods)
		{
			inMessage = emBrowser_msg_searchpage;
			if (widgetMethods->canHandleMessage(gWidgetPointer, inMessage))
				EnableMenuItem(theMenuRef,theIndex);
			else
				DisableMenuItem(theMenuRef,theIndex);
		}
		else
			DisableMenuItem(theMenuRef,theIndex);
	}
	/******************
	** STEP 4B:  END **
	*******************/

	err = GetIndMenuItemWithCommandID(NULL, kHICommandAPI1, 1, &theMenuRef, &theIndex);
	if (err == noErr)
	{
		if (gLibraryInitialized && gWidgetPointer && widgetMethods)
			EnableMenuItem(theMenuRef,theIndex);
		else
			DisableMenuItem(theMenuRef,theIndex);
	}

	err = GetIndMenuItemWithCommandID(NULL, kHICommandAPI2, 1, &theMenuRef, &theIndex);
	if (err == noErr)
	{
		if (gLibraryInitialized && gWidgetPointer && widgetMethods)
			EnableMenuItem(theMenuRef,theIndex);
		else
			DisableMenuItem(theMenuRef,theIndex);
	}

	err = GetIndMenuItemWithCommandID(NULL, kHICommandAPI3, 1, &theMenuRef, &theIndex);
	if (err == noErr)
	{
		if (gLibraryInitialized && gWidgetPointer && widgetMethods)
			EnableMenuItem(theMenuRef,theIndex);
		else
			DisableMenuItem(theMenuRef,theIndex);
	}

	err = GetIndMenuItemWithCommandID(NULL, 'if16', 1, &theMenuRef, &theIndex);
	if (err == noErr)
	{
		if (gLibraryInitialized && gWidgetPointer && widgetMethods)
			EnableMenuItem(theMenuRef,theIndex);
		else
			DisableMenuItem(theMenuRef,theIndex);
	}

	err = GetIndMenuItemWithCommandID(NULL, 'if17', 1, &theMenuRef, &theIndex);
	if (err == noErr)
	{
		CheckMenuItem(theMenuRef,theIndex,gUseWackyCursors);
	}

TEST_PORT
}


#pragma mark -

pascal OSStatus WidgetAppEvents::CarbonCommandHandler(EventHandlerCallRef nextHandler,
													EventRef 				inEvent,
													void 					*inUserData)
{
	bool		eventHandled 	= false;
	UInt32 		carbonEventKind = GetEventKind(inEvent);
	HICommand	aCommand;
	OSStatus	result;
	long		retVal = 0;
	EmBrowserMessage inMessage = 0;
	static EmBrowserChar strBuffer[256];
	static RgnHandle updateRgn = NULL;
	long boolVal = 0;

	if (!updateRgn)
	{
		updateRgn = NewRgn();
	}

TEST_PORT

	GetEventParameter(inEvent, kEventParamDirectObject, typeHICommand, NULL, sizeof(HICommand), NULL, &aCommand);

	// Handle Carbon Events by in WidgetApp
   	switch (aCommand.commandID)
	{
		case 'abou':
			DoAboutBox();
			eventHandled = true;
			break;

		case 'inli':
			InitLibrary();
			eventHandled = true;
			break;
		case 'crwi':
			CreateWidget();
			eventHandled = true;
			break;
		case 'lurl':
			LoadURL();
			eventHandled = true;
			break;
		case 'desw':
			DestroyWidget();
			eventHandled = true;
			break;

		/***********************************************************************************
		** STEP 4C:  PROCESSING AND INTERACTION WITH OPERA BASED ON MESSAGES _TO_ OPERA **
		***********************************************************************************/
		case 'stop':
			inMessage = emBrowser_msg_stop_loading;
			if (gWidgetPointer && widgetMethods)
				retVal = widgetMethods->handleMessage(gWidgetPointer, inMessage);
			eventHandled = true;
			break;

		case 'relo':
			inMessage = emBrowser_msg_reload;
			if (gWidgetPointer && widgetMethods)
				retVal = widgetMethods->handleMessage(gWidgetPointer, inMessage);
			eventHandled = true;
			break;

		case 'forw':
			inMessage = emBrowser_msg_forward;
			if (gWidgetPointer && widgetMethods)
				retVal = widgetMethods->handleMessage(gWidgetPointer, inMessage);
			eventHandled = true;
			break;

		case 'back':
			inMessage = emBrowser_msg_back;
			if (gWidgetPointer && widgetMethods)
				retVal = widgetMethods->handleMessage(gWidgetPointer, inMessage);
			eventHandled = true;
			break;

		case 'if05':
			inMessage = emBrowser_msg_homepage;
			if (gWidgetPointer && widgetMethods)
				retVal = widgetMethods->handleMessage(gWidgetPointer, inMessage);
			eventHandled = true;
			break;

		case 'if06':
			inMessage = emBrowser_msg_searchpage;
			if (gWidgetPointer && widgetMethods)
				retVal = widgetMethods->handleMessage(gWidgetPointer, inMessage);
			eventHandled = true;
			break;
		/******************
		** STEP 4C:  END **
		*******************/

		case 'if07':
			TestAPI1();
			eventHandled = true;
			break;

		case 'if08':
			TestAPI2();
			eventHandled = true;
			break;

		case 'if09':
			TestAPI3();
			eventHandled = true;
			break;

		case 'if10':
			TestSuite();
			eventHandled = true;
			break;

		/*
		case 'if11':
			inMessage = emBrowser_msg_select_all_cmd;
			if (gWidgetPointer && widgetMethods)
				retVal = widgetMethods->handleMessage(gWidgetPointer, inMessage);
			eventHandled = true;
			break;

		case 'if12':
			if (gWidgetPointer && widgetMethods && widgetMethods->setVisible)
				retVal = widgetMethods->setVisible(gWidgetPointer, true);
			eventHandled = true;
			break;

		case 'if13':
			if (gWidgetPointer && widgetMethods && widgetMethods->getVisible)
				boolVal = widgetMethods->getVisible(gWidgetPointer);
			eventHandled = true;
			break;

		case 'if14':
			/***** Not used.
			long start = 0;
			long end = 0;
			if (gWidgetPointer && widgetMethods && widgetMethods->getSelection)
				boolVal = widgetMethods->getSelection(gWidgetPointer,&start,&end);
			if (start < end)
				eventHandled = true;
			*****/
			/*break;

		case 'if15':
			if (gWidgetPointer && widgetMethods && widgetMethods->scrollToSelection)
				boolVal = widgetMethods->scrollToSelection(gWidgetPointer);
			eventHandled = true;
			break;*/

		case 'if16':
			ShowThumbnail();
			eventHandled = true;
			break;

		case 'if17':
			gUseWackyCursors = !gUseWackyCursors;
			eventHandled = true;
			break;

		case kHICommandCopy:
			inMessage = emBrowser_msg_copy_cmd;
			if (gWidgetPointer && widgetMethods)
				retVal = widgetMethods->handleMessage(gWidgetPointer, inMessage);
			eventHandled = true;
			break;

		case kHICommandPaste:
			inMessage = emBrowser_msg_paste_cmd;
			if (gWidgetPointer && widgetMethods)
				retVal = widgetMethods->handleMessage(gWidgetPointer, inMessage);
			eventHandled = true;
			break;

		case kHICommandCut:
			inMessage = emBrowser_msg_cut_cmd;
			if (gWidgetPointer && widgetMethods)
				retVal = widgetMethods->handleMessage(gWidgetPointer, inMessage);
			eventHandled = true;
			break;

		case kHICommandUndo:
			inMessage = emBrowser_msg_undo_cmd;
			if (gWidgetPointer && widgetMethods)
				retVal = widgetMethods->handleMessage(gWidgetPointer, inMessage);
			eventHandled = true;
			break;

		case kHICommandClear:
			inMessage = emBrowser_msg_clear_cmd;
			if (gWidgetPointer && widgetMethods)
				retVal = widgetMethods->handleMessage(gWidgetPointer, inMessage);
			eventHandled = true;
			break;


		case kHICommandQuit:
			DestroyWidget();
			QuitApplicationEventLoop();
			eventHandled = true;
			break;

		default:
			break;
	}
    HiliteMenu(0);

TEST_PORT

	if (eventHandled)
		return noErr;
	else
		return eventNotHandledErr;

}



pascal OSStatus WidgetAppEvents::CarbonWindowHandler(EventHandlerCallRef nextHandler,
													EventRef 				inEvent,
													void 					*inUserData)
{
	bool		eventHandled 	= false;
	UInt32 		carbonEventKind = GetEventKind(inEvent);
    WindowRef	window;
    Rect		bounds;
	OSStatus	retVal = noErr;
	UInt32		_keyModifiers = 0;
	HIPoint		mouseLocation;


    GetEventParameter(inEvent, kEventParamDirectObject, typeWindowRef, NULL, sizeof(window), NULL, &window);

	Rect sizeRect;

TEST_PORT

	// Handle Carbon Events by in WidgetApp
	switch (carbonEventKind)
	{
		case kEventWindowBoundsChanged:
			// IMPORTANT:	When window bounds change, tell Opera where the browser
			//				rendering area is.
			{
				CGrafPtr port = GetWindowPort(window);
				SetPort(port);
				SetOrigin(0,0);
				GetWindowPortBounds(window, &bounds);
				if (gWidgetPointer && widgetMethods->setLocation)
				{
					EmBrowserRect widgetPosRect;
					widgetPosRect.top = bounds.top;
					widgetPosRect.left = bounds.left;
					widgetPosRect.bottom = bounds.bottom;
					widgetPosRect.right = bounds.right;

					widgetMethods->setLocation(gWidgetPointer, &widgetPosRect,gWindow);
				}
			}
			eventHandled = true;
			break;

		case kEventWindowClose:
			DestroyWidget();
	    	gWidgetAppEvents->RemoveCarbonWindowHandler(&window,NULL);
			eventHandled = true;
			break;

		default:
			break;
	}

TEST_PORT

	if (eventHandled)
		return noErr;
	else
		return eventNotHandledErr;
}