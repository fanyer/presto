#ifndef CAPPLEREMOTE_H
#define CAPPLEREMOTE_H

//#define USE_STUPID_OCJ_C_WRAPPER

typedef enum AppleRemoteButton
{
	kAppleRemoteButtonVolume_Plus=0,
	kAppleRemoteButtonVolume_Minus,
	kAppleRemoteButtonMenu,
	kAppleRemoteButtonPlay,
	kAppleRemoteButtonRight,
	kAppleRemoteButtonLeft,
	kAppleRemoteButtonRight_Hold,
	kAppleRemoteButtonLeft_Hold,
	kAppleRemoteButtonPlay_Sleep,
	kAppleRemoteActionCount
} AppleRemoteButton;

class CAppleRemoteListener
{
	friend class CAppleRemote;
public:

	CAppleRemoteListener();
	virtual ~CAppleRemoteListener();
	virtual void RemoteButtonClick(AppleRemoteButton button, Boolean down) = 0;
#ifdef USE_STUPID_OCJ_C_WRAPPER
private:
	void * _private;
#endif
};

#if __LP64__
typedef uint32_t hid_cookie_t;
#else
typedef void* hid_cookie_t;
#endif

class CAppleRemote
{
public:
	CAppleRemote();
	~CAppleRemote();

	Boolean IsRemoteAvailable();

	Boolean IsListeningToRemote();
	void SetListeningToRemote(Boolean value);

	Boolean IsOpenInExclusiveMode();
	void SetOpenInExclusiveMode(Boolean value);

	CAppleRemoteListener * GetListener();
	void SetListener(CAppleRemoteListener * listener);

#ifdef USE_STUPID_OCJ_C_WRAPPER
private:
	void * _private;
#else
	Boolean m_listening;
	Boolean m_exclusive;
	hid_cookie_t* m_cookies;
	struct IOHIDDeviceInterface** m_interface;
	struct IOHIDQueueInterface** m_queue;
private:
#endif
	CAppleRemoteListener * m_listener;
};

#endif //CAPPLEREMOTE_H
