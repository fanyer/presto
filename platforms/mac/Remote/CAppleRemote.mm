#import "platforms/mac/Remote/AppleRemote.h"
#include "platforms/mac/Remote/CAppleRemote.h"
#include "platforms/mac/util/systemcapabilities.h"

extern long gVendorDataID;

@interface RemoteListener : NSObject {
	CAppleRemoteListener* m_callback;
}
@end
@implementation RemoteListener

- (void) setCallback: (CAppleRemoteListener*) callback
{
	m_callback = callback;
}

// delegate methods for AppleRemote
- (void) appleRemoteButton: (AppleRemoteCookieIdentifier)buttonIdentifier pressedDown: (BOOL) pressedDown
{
	if (m_callback)
	{
		m_callback->RemoteButtonClick((AppleRemoteButton)buttonIdentifier, pressedDown);
	}
}

@end

CAppleRemoteListener::CAppleRemoteListener()
	: _private(NULL)
{
	NSAutoreleasePool * pool = [NSAutoreleasePool new];
	RemoteListener* delegate = [RemoteListener new];
	[delegate setCallback:this];
	_private = delegate;
	[pool release];
}

CAppleRemoteListener::~CAppleRemoteListener()
{
	NSObject * delegate = (NSObject *) _private;
	if (delegate)
	{
		NSAutoreleasePool * pool = [NSAutoreleasePool new];
		[delegate release];
		[pool release];
	}
}


CAppleRemote::CAppleRemote()
	: _private(NULL), m_listener(NULL)
{
	NSAutoreleasePool * pool = [NSAutoreleasePool new];
	if ((GetOSVersion() >= 0x1040) && (gVendorDataID == 'OPRA'))
	{
		_private = [AppleRemote new];
	}
	[pool release];
}

CAppleRemote::~CAppleRemote()
{
	AppleRemote * remote = (AppleRemote *) _private;
	if (remote)
	{
		NSAutoreleasePool * pool = [NSAutoreleasePool new];
		[remote release];
		[pool release];
	}
}

Boolean CAppleRemote::IsRemoteAvailable()
{
	AppleRemote * remote = (AppleRemote *) _private;
	Boolean result = false;
	if (remote)
	{
		NSAutoreleasePool * pool = [NSAutoreleasePool new];
		result = [remote isRemoteAvailable];
		[pool release];
	}
	return result;
}

Boolean CAppleRemote::IsListeningToRemote()
{
	AppleRemote * remote = (AppleRemote *) _private;
	Boolean result = false;
	if (remote)
	{
		NSAutoreleasePool * pool = [NSAutoreleasePool new];
		result = [remote isListeningToRemote];
		[pool release];
	}
	return result;
}

void CAppleRemote::SetListeningToRemote(Boolean value)
{
	AppleRemote * remote = (AppleRemote *) _private;
	if (remote)
	{
		NSAutoreleasePool * pool = [NSAutoreleasePool new];
		[remote setListeningToRemote:value];
		[pool release];
	}
}


Boolean CAppleRemote::IsOpenInExclusiveMode()
{
	AppleRemote * remote = (AppleRemote *) _private;
	Boolean result = false;
	if (remote)
	{
		NSAutoreleasePool * pool = [NSAutoreleasePool new];
		result = [remote isOpenInExclusiveMode];
		[pool release];
	}
	return result;
}

void CAppleRemote::SetOpenInExclusiveMode(Boolean value)
{
	AppleRemote * remote = (AppleRemote *) _private;
	if (remote)
	{
		NSAutoreleasePool * pool = [NSAutoreleasePool new];
		[remote setOpenInExclusiveMode:value];
		[pool release];
	}
}


CAppleRemoteListener * CAppleRemote::GetListener()
{
	return m_listener;
}

void CAppleRemote::SetListener(CAppleRemoteListener * listener)
{
	AppleRemote * remote = (AppleRemote *) _private;
	if (remote && listener && listener->_private)
	{
		NSAutoreleasePool * pool = [NSAutoreleasePool new];
		[remote setDelegate:(id)(listener->_private)];
		[pool release];
	}
	m_listener = listener;
}
