#ifndef _DELAYEDACTION_H_
#define _DELAYEDACTION_H_

#include "modules/hardcore/timer/optimer.h"

class DelayedAction : OpTimerListener
{
public:
	DelayedAction(UINT32 timeout_ms = 0);
	virtual ~DelayedAction();
	virtual void DoAction() = 0;

private:
	virtual void OnTimeOut(OpTimer* timer);
	OpTimer *mOpTimer;
};

#endif // _DELAYEDACTION_H_
