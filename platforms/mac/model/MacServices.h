#ifndef _MAC_SERVICES_HANDLER_H_
#define _MAC_SERVICES_HANDLER_H_

#ifdef SUPPORT_OSX_SERVICES

#ifndef NO_CARBON
const CFStringRef kOperaOpenURLString = CFSTR("openURL");
const CFStringRef kOperaMailToString = CFSTR("mailTo");
const CFStringRef kOperaSendFilesString = CFSTR("sendFiles");
const CFStringRef kOperaOpenFilesString = CFSTR("openFiles");
#endif // NO_CARBON

class MacOpServices
{
public:
	static void Free();
	static void InstallServicesHandler();

private:
	MacOpServices();

#ifndef NO_CARBON
    enum OperaServiceProviderKind
    {
    	kOperaServiceKindUnknown = 0,
    	kOperaServiceKindOpenURL = 1,
    	kOperaServiceKindMailTo = 2,
    	kOperaServiceKindSendFiles = 3,
    	kOperaServiceKindOpenFiles = 4
    };

	static OperaServiceProviderKind GetServiceKind(CFStringRef cfstr);

	static pascal OSStatus ServicesHandler(EventHandlerCallRef nextHandler, EventRef inEvent, void *inUserData);

	static EventHandlerRef sServicesHandler;
	static EventHandlerUPP sServicesUPP;
#endif // NO_CARBON
};

#endif // SUPPORT_OSX_SERVICES
#endif // _MAC_SERVICES_HANDLER_H_
