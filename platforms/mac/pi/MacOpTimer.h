#if !defined(MACOPTIMER_H)
#define MACOPTIMER_H

#include "modules/hardcore/timer/optimer.h"

class MacOpTimer : public OpTimer
{
public:
	MacOpTimer();
	virtual ~MacOpTimer();

	OP_STATUS Init();

	void Start(UINT32 ms);
	UINT32 Stop();
	void SetTimerListener(OpTimerListener *listener);

	UINT32 GetId();

	void opTimerEvent(EventLoopTimerRef timerRef);
	static pascal void sOpTimerEvent(EventLoopTimerRef timerRef, void *opTimer);

private:
	static UINT32 timer_id;
	static EventLoopTimerUPP timerUPP;
	OpTimerListener *m_listener;
	UINT32 m_id;
	UINT32 m_startTime;
	UINT32 m_interval;
	EventLoopTimerRef m_timerRef;
};

#endif
