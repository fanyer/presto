#ifndef _DELAYEDACTION_H_
#define _DELAYEDACTION_H_

#include "modules/hardcore/timer/optimer.h"
#include "modules/inputmanager/inputmanager.h"

/***********************************************************************************
 **
 ** OpDelayedAction
 **
 ***********************************************************************************/

class OpDelayedAction : OpTimerListener
{
public:
	OpDelayedAction(UINT32 timeout_ms = 0)
	{
#ifdef HC_CAP_OPERA_RUN
		m_timer = new OpTimer();
#else
		g_systemFactory->CreateOpTimer(&m_timer);
#endif
		if (m_timer)
		{
			m_timer->SetTimerListener(this);
			m_timer->Start(timeout_ms);
		}
	}
	virtual ~OpDelayedAction() { delete m_timer; }
	virtual void DoAction() = 0;

private:
	virtual void OnTimeOut(OpTimer* timer)
	{
		DoAction();
		delete this;
	}
	OpTimer *m_timer;
};


/***********************************************************************************
 **
 ** OpDelayedInputAction
 **
 ***********************************************************************************/

class OpDelayedInputAction : public OpDelayedAction
{
private:
	OpInputAction* m_input_action;
	
public:
	OpDelayedInputAction(OpInputAction* input_action, UINT32 delay)
	: OpDelayedAction(delay),
	  m_input_action(input_action)
	{
	}
	
	virtual void DoAction()
	{
		g_input_manager->InvokeAction(m_input_action);
		delete m_input_action;
	}
};

#endif // _DELAYEDACTION_H_
