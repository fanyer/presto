// ===========================================================================
//	WidgetAppEvents.h
// ===========================================================================

#ifndef _H_WidgetAppEvents
#define _H_WidgetAppEvents


class WidgetAppEvents
{

public:

	WidgetAppEvents();
	~WidgetAppEvents();

	//***************************************************************************************
	// Installing The Handlers
	//***************************************************************************************

	/** Installs all carbon event handlers (except custom and window-related)
	  * on the PowerPlant application of type LDocApplication.
	  */
	OSStatus InstallCarbonEventHandlers();

	/** Removes all carbon event handlers (except custom and window-related)
	  * on the PowerPlant application of type LDocApplication.
	  */
	OSStatus RemoveCarbonEventHandlers();
	/** Installs Carbon window event handler for a new window or dialog.
	  */

	OSStatus InstallCarbonWindowHandler(	WindowRef newWindow,
											EventHandlerRef *newWindowHandler);

	/** Removes Carbon window event handler when the user closes a window.
	  */
	OSStatus RemoveCarbonWindowHandler(		WindowRef			*oldWindow,
											EventHandlerRef *oldWindowHandler);

	/** Installs a Periodic Event Handler, to handle timer based processing.
	 */
	OSStatus InstallPeriodicEventHandler(int periodMS);

	/** Remove the Opera Periodic Event Handler.
	  */
	OSStatus RemovePeriodicEventHandler();

	//***************************************************************************************
	// Handler Callback Functions
	//***************************************************************************************

	/**
	* Handle application-level events (launch, quit, etc.).
	*/
    static pascal OSStatus CarbonApplicationHandler(EventHandlerCallRef nextHandler, EventRef inEvent, void *inUserData);

	/**
	 * Handle events related to the mouse (mouse down/up/moved).
	 */
	static pascal OSStatus CarbonMouseHandler(EventHandlerCallRef nextHandler, EventRef inEvent, void *inUserData);

	/**
	* Handle command events (HICommands).
	*/
    static pascal OSStatus CarbonCommandHandler(EventHandlerCallRef nextHandler, EventRef inEvent, void *inUserData);

	/**
	* Handle window-related events.
	*/
    static pascal OSStatus CarbonWindowHandler(EventHandlerCallRef nextHandler, EventRef inEvent, void *inUserData);
    static pascal OSStatus CarbonRootControlHandler(EventHandlerCallRef nextHandler, EventRef inEvent, void *inUserData);

	/**
	* Handle events related to the keyboard.
	*/
    static pascal OSStatus CarbonKeyboardHandler(EventHandlerCallRef nextHandler, EventRef inEvent, void *inUserData);

	/**
	* Handle apple Events.
	*/
    static pascal OSStatus CarbonAEHandler(EventHandlerCallRef nextHandler, EventRef inEvent, void *inUserData);

	/**
	* Handler for periodic events.
	*/
	static pascal void CarbonPeriodicEventHandler(EventLoopTimerRef theTimer, void* userData);

	/**
	* Handle custom defined browser notification events.
	*/
	static pascal OSStatus NotificationHandler(EventHandlerCallRef nextHandler, EventRef inEvent, void *inUserData);

protected:

private:

	EventHandlerRef 	mCommandHandler;
	EventHandlerRef 	mApplicationHandler;
	EventHandlerRef 	mMouseHandler;
	EventHandlerRef 	mKeyboardHandler;
	EventHandlerRef 	mAEHandler;
	EventHandlerRef		mNotificationHandler;
	// Only one in this app...
	// This usually hangs on a C++ object with window attributes.
	EventHandlerRef 	mWindowHandler;
	EventHandlerRef		mControlHandler;
	EventLoopTimerRef 	mTimerHandlerRef;

};

#endif