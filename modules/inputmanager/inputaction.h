/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef IM_OP_INPUTACTION_H
#define IM_OP_INPUTACTION_H

#include "modules/hardcore/keys/opkeys.h"
#include "modules/pi/OpKeys.h"

class OpInputContext;
class OpTypedObject;

/**
 * Represents a tooltip and a status text.
 */
class OpInfoText
{
public:

	OpInfoText() : max_line_width(100), max_lines(50), m_auto_truncate(TRUE) {}

	/**
	 * Empty the status and tooltip texts.
	 */
	void Empty() { m_status_text.Empty(); m_tooltip_text.Empty(); }

	/**
	 * Set the status text. If auto truncate is enabled the text will be
	 * truncated after 'max_line_width' characters.
	 */
	OP_STATUS SetStatusText(const uni_char* text);

	/**
	 * Set the tooltip text.
	 */
	OP_STATUS SetTooltipText(const uni_char* text) { return m_tooltip_text.Set(text); }

	/**
	 * Add text of the form 'label: text' to the current tooltip text. The text
	 * will be truncated automatically.
	 */
	OP_STATUS AddTooltipText(const uni_char* label, const uni_char* text);

	/**
	 * Returns a reference to the status text string.
	 */
	const OpString&	GetStatusText() { return m_status_text; }

	/**
	 * Returns a reference to the tooltip text string.
	 */
	const OpString&	GetTooltipText() { return m_tooltip_text; }

	/**
	 * Copy status and tooltip texts from another OpInfoText.
	 */
	OP_STATUS Copy(OpInfoText& src);

	/**
	 * If auto truncate is set SetStatusText() will automatically truncate strings.
	 */
	void SetAutoTruncate(BOOL auto_truncate) {m_auto_truncate = auto_truncate;}

	BOOL HasStatusText() const { return m_status_text.HasContent(); }
	BOOL HasTooltipText() const { return m_tooltip_text.HasContent(); }
private:

	const int	max_line_width,	max_lines;

	OpString	m_status_text;
	OpString	m_tooltip_text;

	BOOL		m_auto_truncate;
};

class OpInputAction
{
	public:

#include "modules/hardcore/actions/generated_actions_enum.h"

		enum ActionState
		{
			STATE_ENABLED		= 0x01,			///< is action to be considered available?
			STATE_DISABLED		= 0x02,			///< is action to be considered unavailable? (disabled menuitem, button etc)
			STATE_SELECTED		= 0x04,			///< is action to be considered selected?
			STATE_UNSELECTED	= 0x08,			///< is action to be considered unselected?
			STATE_ATTENTION		= 0x10,			///< is action demanding user attention?
			STATE_INATTENTION	= 0x20			///< is action not demanding user attention?
		};

		enum ActionMethod
		{
			METHOD_MOUSE,
			METHOD_MENU,
			METHOD_KEYBOARD,
			METHOD_OTHER
		};

		enum ActionOperator
		{
			OPERATOR_NONE	= 0x01,
			OPERATOR_OR		= 0x02, 			///< execute the next action only if the current action was not handled
			OPERATOR_AND	= 0x04,				///< always execute the next action
			OPERATOR_NEXT	= 0x08,  			///< execute the next action only if the current action was not handled
			/**
			 *  The plus operator assigns two actions to one OpButton.
			 *  The first action is executed on a normal click, the second action on a long click.
			 *  
			 *  Example: the "back"-button can be configured with the action "Back + Action": a normal click executes action "Back", a long click executes the second action "Action".
			 *  You can combine several sequences with a "+" like "Reload | Stop + Action1 & Action2", which will then execute "Reload | Stop" on a short click and "Action1 & Action2" on a long click.
			 */
			OPERATOR_PLUS	= 0x10
		};

		/**
		 * Simple constructor.
		 */
		OpInputAction();

		OpInputAction(Action action);

		// LOWLEVEL KEY actions
		OpInputAction(Action action, OpKey::Code key, ShiftKeyState shift_keys = 0, BOOL repeat = FALSE, OpKey::Location location = OpKey::LOCATION_STANDARD, ActionMethod action_method = METHOD_OTHER);

		// ACTION_LOWLEVEL_PREFILTER_ACTION
		OpInputAction(OpInputAction* child_action, Action action);

		~OpInputAction();

		/**
		 * Copy the contents of another OpInputAction.
		 *
		 * @param src_action Action to copy data from.
		 * @return OpStatus::OK or OpStatus::ERROR_NO_MEMORY
		 */
		OP_STATUS				Clone(OpInputAction* src_action);

		ActionMethod			GetActionMethod()						{return m_action_method;}
		ActionOperator			GetActionOperator()						{return m_action_operator;}
		Action					GetAction()								{return m_action;}
		INTPTR					GetActionData()							{return m_action_data;}
		const uni_char*			GetActionDataString()					{return m_action_data_string.CStr();}
		const uni_char*			GetActionDataStringParameter()			{return m_action_data_string_parameter.CStr();}
		const uni_char*			GetActionText()							{return m_action_text.CStr();}
		Str::LocaleString		GetActionTextID();
		OpInfoText&				GetActionInfo()							{return m_action_info;}
		const char*				GetActionImage(BOOL use_predefined = TRUE)	{return m_action_image.HasContent() || !use_predefined ? m_action_image.CStr() : GetStringFromAction(m_action);}
		INT32					GetActionState()						{return m_action_state;}
		void					GetActionPosition(OpRect& rect)			{rect = m_action_position;}
		OpTypedObject*			GetActionObject()						{return m_action_object;}
		const uni_char*			GetKeyValue()							{return m_key_value.CStr();}
		ShiftKeyState			GetShiftKeys()							{return m_shift_keys;}
		BOOL					GetKeyRepeat()							{return m_key_repeat;}
		OpKey::Location			GetKeyLocation()						{return m_key_location;}
		INT32					GetRepeatCount()						{return m_repeat_count;}
		OpPlatformKeyEventData*	GetPlatformKeyEventData() 				{return m_key_event_data; }
		OpInputAction*			GetChildAction()						{return m_child_action;}
		OpInputAction*			GetNextInputAction(ActionOperator after_this_operator = OPERATOR_NONE);
		OpInputContext*			GetFirstInputContext()					{return m_first_input_context;}
		Action 					GetReferrerAction() 					{return m_referrer_action;}
		OpKey::Code				GetActionKeyCode()						{return static_cast<OpKey::Code>(m_action_data);}
		BOOL					IsLowlevelAction();
		BOOL					IsMoveAction();
		BOOL					IsRangeAction();
		BOOL					IsUpdateAction()						{return m_action < LAST_ACTION;}
		BOOL					IsKeyboardInvoked()						{return m_action_method == METHOD_KEYBOARD;}
		BOOL					IsMouseInvoked()						{return m_action_method == METHOD_MOUSE;}
		BOOL					IsMenuInvoked()							{return m_action_method == METHOD_MENU;}
		BOOL					IsGestureInvoked()						{return m_gesture_invoked;}
		BOOL					IsActionDataStringEqualTo(const uni_char* string)	{return m_action_data_string.CompareI(string) == 0;}
		BOOL					HasActionDataString()					{return m_action_data_string.HasContent();}
		BOOL					HasActionDataStringParameter()			{return m_action_data_string_parameter.HasContent();}
		BOOL					HasActionText()							{return m_action_text.HasContent();}
		BOOL					HasActionTextID()						{return m_action_text_id != 0;}
		BOOL					HasActionImage()						{return m_action_image.HasContent();}
		BOOL					WasReallyHandled()						{return m_was_really_handled;}

		BOOL					Equals(OpInputAction* action);

		void					SetEnabled(BOOL enabled);
		void					SetSelected(BOOL selected);
		void					SetSelectedByToggleAction(Action true_action, BOOL toggle);
		void					SetAttention(BOOL attention);
		void					SetActionData(INTPTR data)				{ m_action_data = data; }
		void					SetKeyCode(OpKey::Code k)				{ m_action_data = static_cast<INTPTR>(k); }
		void					SetShiftKeys(ShiftKeyState s)			{  m_shift_keys = s; }
		void					SetActionDataString(const uni_char* string);
		void					SetActionDataStringParameter(const uni_char* string);
		void					SetActionText(const uni_char* string);
		void					SetActionTextID(Str::LocaleString string_id);
		OP_STATUS				SetActionImage(const char* string);
		void					SetActionState(INT32 state) {m_action_state = state;}
		void					SetActionPosition(const OpRect& rect);
		void					SetActionPosition(const OpPoint& point);
		void					SetActionObject(OpTypedObject* object)	{m_action_object = object; }
		void					SetRepeatCount(INT32 repeat_count)		{m_repeat_count = repeat_count; }
		void					SetPlatformKeyEventData(OpPlatformKeyEventData *data);
		void					SetActionOperator(ActionOperator action_operator) { m_action_operator = action_operator; }
		void					SetActionMethod(ActionMethod action_method) { m_action_method = action_method; }
		void					SetNextInputAction(OpInputAction* action) { m_next_input_action = action; }
		void					SetFirstInputContext(OpInputContext* first_input_context) {m_first_input_context = first_input_context; if (m_next_input_action) m_next_input_action->SetFirstInputContext(first_input_context);}
		void					SetWasReallyHandled(BOOL really_handled) {m_was_really_handled = really_handled;}
		void					SetGestureInvoked(BOOL gesture) { m_gesture_invoked = gesture; }
		void 					SetReferrerAction(Action referrer_action) { m_referrer_action = referrer_action; }

		/** Set the character value produced by this key action.
		 *
		 * @param value The character value string.
		 * @return OpStatus::ERR_NO_MEMORY or OpStatus::OK.
		 */
		OP_STATUS				SetActionKeyValue(const uni_char *value);

		/** Set the character value produced by this key action.
		 *
		 * @param value The character value, UTF-8 encoded.
		 * @param length The length of 'value'.
		 * @return OpStatus::ERR_NO_MEMORY or OpStatus::OK.
		 */
		OP_STATUS				SetActionKeyValue(const char *value, unsigned length);

		/**
		 * Convert the action to a string representation.
		 *
		 * @param string Target string.
		 * @return OpStatus::ERR_NO_MEMORY or OpStatus::OK.
		 */
		OP_STATUS				ConvertToString(OpString& string);

		static OP_STATUS		CreateInputActionsFromString(const uni_char* string, OpInputAction*& input_action, INTPTR default_value = 0, BOOL keyboard_invoked = FALSE);
		static OP_STATUS        CreateInputActionsFromString(const char* string8, OpInputAction*& input_action, INTPTR default_value = 0, BOOL keyboard_invoked = FALSE)
			{
				OpString string; RETURN_IF_ERROR(string.Set(string8)); return CreateInputActionsFromString(string.CStr(), input_action, default_value, keyboard_invoked);
			}
		static OpInputAction*	CreateToggleAction(Action action1, Action action2, INTPTR action_data = 0, const uni_char* action_data_string = NULL);
		static OpInputAction*   CopyInputActions(OpInputAction* source_action, INT32 stop_at_operator = OPERATOR_NONE);
		static void				DeleteInputActions(OpInputAction* input_action);

		/**
		 * Get a string representation from an action enum value.
		 */
		static OP_STATUS		GetStringFromAction(OpInputAction::Action action, OpString8& string);
		static const char*		GetStringFromAction(OpInputAction::Action action);
		static OP_STATUS		GetLocaleStringFromAction(OpInputAction::Action action, OpString& string);

	private:
		void					SetMembers(Action action);

		ActionOperator			m_action_operator;
		Action					m_action;
		INTPTR					m_action_data;
		OpString				m_action_data_string;
		OpString				m_action_data_string_parameter;
		OpString				m_action_text;
		int						m_action_text_id;
		OpInfoText				m_action_info;
		OpString8				m_action_image;
		INT32					m_action_state;
		OpRect					m_action_position;
		OpTypedObject*			m_action_object;
		ShiftKeyState			m_shift_keys;
		OpString				m_key_value;
		BOOL					m_key_repeat;
		OpKey::Location			m_key_location;
		INT32					m_repeat_count;
		OpPlatformKeyEventData*	m_key_event_data;
		OpInputAction*			m_child_action;
		ActionMethod			m_action_method;
		OpInputAction*			m_next_input_action;
		OpInputContext*			m_first_input_context;
		BOOL					m_was_really_handled;  // kludge until I bother to change BOOL return value of doing actions to an HANDLED enum
		BOOL					m_gesture_invoked;
		Action					m_referrer_action;
};

class OpInputStateListener
{
	public:
		virtual ~OpInputStateListener() {} // gcc warn: virtual methods => virtual destructor.

		virtual void		OnUpdateInputState(BOOL full_update) = 0;
};

class OpInputState : public Link
{
	public:
								OpInputState() : m_listener(NULL), m_wants_only_full_update(FALSE) {}
								virtual ~OpInputState() {SetEnabled(FALSE);}

		void					SetInputStateListener(OpInputStateListener* listener) {m_listener = listener;}

		void					SetWantsOnlyFullUpdate(BOOL wants_only_full_update) {m_wants_only_full_update = wants_only_full_update;}
		BOOL					GetWantsOnlyFullUpdate() {return m_wants_only_full_update;}

		void					SetEnabled(BOOL enabled);
		BOOL					IsEnabled() {return InList();}

		void					Update(BOOL full_update) {if (m_listener) m_listener->OnUpdateInputState(full_update);}

	private:

		OpInputStateListener*	m_listener;
		BOOL					m_wants_only_full_update;
};

#endif // !IM_OP_INPUTACTION_H
