#ifndef APPLE_EVENT_MANAGER_H
#define APPLE_EVENT_MANAGER_H

#include "modules/util/adt/opvector.h"

class CocoaAppleEventHandler;

class AppleEventManager
{
public:
	AppleEventManager();
	~AppleEventManager();

	static pascal OSErr Make(const AppleEvent *msgin, AppleEvent *reply, long inRef);
	static pascal OSErr Close(const AppleEvent *msgin, AppleEvent *reply, long inRef);
	static pascal OSErr GetData(const AppleEvent *msgin, AppleEvent *reply, long inRef);
	static pascal OSErr SetData(const AppleEvent *msgin, AppleEvent *reply, long inRef);
	static pascal OSErr Count(const AppleEvent *msgin, AppleEvent *reply, long inRef);
	static pascal OSErr Exists(const AppleEvent *msgin, AppleEvent *reply, long inRef);
	static pascal OSErr Save(const AppleEvent *msgin, AppleEvent *reply, long inRef);
	static pascal OSErr ReopenApp(const AppleEvent *msgin, AppleEvent *reply, long inRef);
	static pascal OSErr Open(const AppleEvent *msgin, AppleEvent *reply, long inRef);
	static pascal OSErr Print(const AppleEvent *msgin, AppleEvent *reply, long inRef);

	static pascal OSErr OpenURL(const AppleEvent *msgin, AppleEvent *reply, long inRef);
	static pascal OSErr ListWindows(const AppleEvent *msgin, AppleEvent *reply, long inRef);
	static pascal OSErr GetWindowInfo(const AppleEvent *msgin, AppleEvent *reply, long inRef);
	static pascal OSErr CloseWindow(const AppleEvent *msgin, AppleEvent *reply, long inRef);
	static pascal OSErr CloseAllWindows(const AppleEvent *msgin, AppleEvent *reply, long inRef);
	static pascal OSErr RegisterURLEcho(const AppleEvent *msgin, AppleEvent *reply, long inRef);
	static pascal OSErr UnregisterURLEcho(const AppleEvent *msgin, AppleEvent *reply, long inRef);

#ifdef _MAC_DEBUG
	static pascal OSErr DoNothing(const AppleEvent *msgin, AppleEvent *reply, long inRef);
#endif
};

#ifdef SUPPORT_SHARED_MENUS
OSErr InitializeSharedMenusStuff();
OSErr InformApplication(long psn);
extern OpINT32Vector gInformApps;
#endif // SUPPORT_SHARED_MENUS

#endif //APPLE_EVENT_MANAGER_H
