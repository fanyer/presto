#include "core/pch.h"

#include "adjunct/quick/state_modifiers/SyncWidgetStateModifier.h"

SyncWidgetState::SyncWidgetState() : m_text(UNI_L(""))
{
	// OpInputAction::CreateInputActionsFromString("Show popup menu, \"New Bookmark Menu\"", m_action);
	m_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_POPUP_MENU, 0, UNI_L("Sync Popup Menu")));
}

SyncWidgetState::~SyncWidgetState()
{
	OP_DELETE(m_action);
	// delete text??
}

SyncWidgetStateModifier::SyncWidgetStateModifier()
{
	m_widget_state = OP_NEW(SyncEnabledWidgetState, ());
}


SyncWidgetStateModifier::~SyncWidgetStateModifier()
{
	OP_DELETE(m_widget_state);
}


WidgetState* SyncWidgetStateModifier::GetCurrentWidgetState()
{
	return m_widget_state;
}


void SyncWidgetStateModifier::SetWidgetStateToEnabled()
{
	OP_DELETE(m_widget_state);
	m_widget_state = OP_NEW(SyncEnabledWidgetState, ());

	GenerateOnStateChanged(m_widget_state);
}


void SyncWidgetStateModifier::SetWidgetStateToDisabled()
{
	OP_DELETE(m_widget_state);
	m_widget_state = OP_NEW(SyncDisabledWidgetState, ());

	GenerateOnStateChanged(m_widget_state);
}

void SyncWidgetStateModifier::SetWidgetStateToBusy()
{
	OP_DELETE(m_widget_state);
	m_widget_state = OP_NEW(SyncBusyWidgetState, ());

	GenerateOnStateChanged(m_widget_state);
}

void SyncWidgetStateModifier::SetWidgetStateToError()
{
	OP_DELETE(m_widget_state);
	m_widget_state = OP_NEW(SyncErrorWidgetState, ());

	GenerateOnStateChanged(m_widget_state);
}
