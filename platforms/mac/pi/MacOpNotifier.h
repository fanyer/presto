#ifndef MACOPNOTIFIER_H
#define MACOPNOTIFIER_H

#include "adjunct/desktop_pi/desktop_notifier.h"
#include "modules/hardcore/timer/optimer.h"

#define BOOL NSBOOL
#include "platforms/mac/notifications/Headers/Growl.h"
#undef BOOL

#if __objc__
@class NSString;
#else
class NSString;
#endif

class MacOpNotifier : public DesktopNotifier, public OpTimerListener
{
public:
	MacOpNotifier();
	virtual	~MacOpNotifier();

	virtual OP_STATUS Init(DesktopNotifier::DesktopNotifierType notification_group, const OpStringC& text, const OpStringC8& image, OpInputAction* action, BOOL managed = FALSE, BOOL wrapping = FALSE);
	virtual OP_STATUS Init(DesktopNotifier::DesktopNotifierType notification_group, const OpStringC& title, Image& image, const OpStringC& text, OpInputAction* action, OpInputAction* cancel_action, BOOL managed = FALSE, BOOL wrapping = FALSE);
	
	/** Adds a button to the notifier below the title
	  * @param text Text on button
	  * @param action Action to execute when this button is clicked
	  */
	virtual OP_STATUS AddButton(const OpStringC& text, OpInputAction* action);

	virtual void StartShow();

	// Setting this to false will leave it up to Growl to manage a notification flood :)
	virtual BOOL IsManaged() const { return FALSE; }

	virtual void AddListener(DesktopNotifierListener* listener) { if (!m_listener) m_listener = listener; }
	virtual void RemoveListener(DesktopNotifierListener* listener) { if(m_listener == listener) m_listener = NULL; }

	void InvokeAction();

	void SetReceiveTime(time_t time) { m_timestamp = time; }

	// OpTimerListener
	void OnTimeOut(OpTimer* timer);

private:
	
	bool SetNotificationTitleAndName(OpString& title, NSString*& name);
	
	OpString				m_text;
	OpString				m_title;
	OpInputAction			*m_action;
	void					*m_image;  // NSImage
	DesktopNotifierListener	*m_listener;
	time_t					m_timestamp;
	int						m_notifier_id;
	OpTimer*				m_timer;

	DesktopNotifier::DesktopNotifierType	m_notification_group;
};

#endif // MACOPNOTIFIER_H
