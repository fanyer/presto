#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/state/WidgetStateModifier.h"


/***********************************************************************************
**  WidgetStateModifier::WidgetStateModifier
**
************************************************************************************/
WidgetStateModifier::WidgetStateModifier() : m_is_inited(FALSE)
{
}

/***********************************************************************************
**  WidgetStateModifier::WidgetStateModifier
**
************************************************************************************/
WidgetStateModifier::~WidgetStateModifier()
{
	WidgetStateListener* listener = NULL;
	for(OpListenersIterator iterator(m_state_listeners); m_state_listeners.HasNext(iterator);)
	{
		listener = m_state_listeners.GetNext(iterator);
		OP_ASSERT(listener);
		if (listener)
		{
			listener->SetModifier(NULL);
		}
	}
}

/***********************************************************************************
**  WidgetStateModifier::Init
**  @return
**
************************************************************************************/
OP_STATUS WidgetStateModifier::Init()
{
	SetIsInited(TRUE);
	return OpStatus::OK;
}


/***********************************************************************************
**  WidgetStateModifier::AddWidgetStateListener
**  @param listener	The widget that listenes to state changes.
**  @return	If adding was successful
**
************************************************************************************/
OP_STATUS WidgetStateModifier::AddWidgetStateListener(WidgetStateListener* listener)
{
	listener->OnStateChanged(GetCurrentWidgetState());
	return m_state_listeners.Add(listener);
}


/***********************************************************************************
**  WidgetStateModifier::RemoveWidgetStateListener
**  @param listener The widget that has been listening to state changes.
**  @return If removing was successful
**
************************************************************************************/
OP_STATUS WidgetStateModifier::RemoveWidgetStateListener(WidgetStateListener* listener)
{
	return m_state_listeners.Remove(listener);
}


/***********************************************************************************
**  Inform all listeners about state changes
**
**  WidgetStateModifier::GenerateOnStateChanged
**  @param state	Current widget state
************************************************************************************/
void WidgetStateModifier::GenerateOnStateChanged(WidgetState* state)
{
	WidgetStateListener* listener = NULL;
	for(OpListenersIterator iterator(m_state_listeners); m_state_listeners.HasNext(iterator);)
	{
		listener = m_state_listeners.GetNext(iterator);
		OP_ASSERT(listener);
		if (listener)
		{
			listener->OnStateChanged(state);
		}
	}
}


/***********************************************************************************
**  WidgetStateModifier::IsInited
**  @return
************************************************************************************/
BOOL WidgetStateModifier::IsInited() const
{
	return m_is_inited;
}


/***********************************************************************************
**  WidgetStateModifier::SetIsInited
**  @param is_inited
************************************************************************************/
void WidgetStateModifier::SetIsInited(BOOL is_inited)
{
	m_is_inited = is_inited;
}
