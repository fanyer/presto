#include "core/pch.h"

#include "adjunct/quick/state_modifiers/WebServerWidgetStateModifier.h"

WebServerWidgetState::WebServerWidgetState() : m_text(UNI_L(""))
{
	// OpInputAction::CreateInputActionsFromString("Show popup menu, \"New Bookmark Menu\"", m_action);
	m_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_POPUP_MENU, 0, UNI_L("Unite Popup Menu")));
}


WebServerWidgetStateModifier::WebServerWidgetStateModifier()
{
	m_widget_state = OP_NEW(WebServerEnabledWidgetState, ());
}


WebServerWidgetStateModifier::~WebServerWidgetStateModifier()
{
	OP_DELETE(m_widget_state);
}


WidgetState* WebServerWidgetStateModifier::GetCurrentWidgetState()
{
	return m_widget_state;
}


void WebServerWidgetStateModifier::SetWidgetStateToEnabled()
{
	OP_DELETE(m_widget_state);
	m_widget_state = OP_NEW(WebServerEnabledWidgetState, ());

	WebServerEnabledWidgetState enabled_state;
	GenerateOnStateChanged(m_widget_state);
}


void WebServerWidgetStateModifier::SetWidgetStateToDisabled()
{
	OP_DELETE(m_widget_state);
	m_widget_state = OP_NEW(WebServerDisabledWidgetState, ());

	GenerateOnStateChanged(m_widget_state);
}


void WebServerWidgetStateModifier::SetWidgetStateToError()
{
	OP_DELETE(m_widget_state);
	m_widget_state = OP_NEW(WebServerErrorWidgetState, ());

	GenerateOnStateChanged(m_widget_state);
}
