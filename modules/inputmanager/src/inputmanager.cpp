/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2002-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/inputmanager/inputmanager.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/inputmanager/inputcontext.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"
#include "modules/prefsfile/prefsentry.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/util/filefun.h"
#include "modules/util/OpLineParser.h"
#include "modules/util/simset.h"
#include "modules/widgets/OpWidget.h"
#include "modules/hardcore/mh/messages.h"

#ifdef QUICK
# include "adjunct/quick/Application.h"
# include "adjunct/quick/managers/CommandLineManager.h"
# include "adjunct/quick/managers/opsetupmanager.h"
#endif // QUICK

#if defined(SDK)
# include "product/sdk/src/operaembedded_internal.h"
#endif // SDK

#ifdef GTK_SDK
extern PrefsSection* _gtk_opera_get_input_contextL(const uni_char* context_name, BOOL is_keyboard);
#endif // GTK_SDK

#ifdef EPOC
# ifndef SYMGOGI
#  include "EpocUtils.h"
#  include "EpocModel.h"
# endif // !SYMGOGI
#endif // EPOC

#if defined (MSWIN)
extern BOOL IsSystemWin2000orXP();
#endif //MSWIN

#include "modules/libcrypto/include/OpRandomGenerator.h"

void Shortcut::GetShortcutStringL(OpString& string)
{
	if (m_modifiers & SHIFTKEY_CTRL)
	{
		OpString tmp;
		g_languageManager->GetStringL(Str::M_KEY_CTRL, tmp);
		tmp.AppendL("+");
		string.AppendL(tmp);
	}

	if (m_modifiers & SHIFTKEY_ALT)
	{
		OpString tmp;
		g_languageManager->GetStringL(Str::M_KEY_ALT, tmp);
		tmp.AppendL("+");
		string.AppendL(tmp);
	}

	if (m_modifiers & SHIFTKEY_META)
	{
		OpString tmp;
		g_languageManager->GetStringL(Str::M_KEY_META, tmp);
		tmp.AppendL("+");
		string.AppendL(tmp);
	}

	if (m_modifiers & SHIFTKEY_SHIFT)
	{
		OpString tmp;
		g_languageManager->GetStringL(Str::M_KEY_SHIFT, tmp);
		tmp.AppendL("+");
		string.AppendL(tmp);
	}

	if (m_key_value == 0)
	{
		OpString key_string;
		LEAVE_IF_ERROR(OpInputManager::OpKeyToLanguageString(m_key, key_string));
		string.AppendL(key_string);
	}
	else
		string.AppendL(&m_key_value, 1);
}

/***********************************************************************************
**
**	ShortcutAction
**
***********************************************************************************/

OP_STATUS
ShortcutAction::Construct(OpInputAction::ActionMethod method, const uni_char* shortcut_string, OpInputAction* input_action)
{
 	m_input_action = input_action;
	OpLineParser line(shortcut_string);

	while (line.HasContent())
	{
		OpString token;
		RETURN_IF_ERROR(line.GetNextToken(token));
		RETURN_IF_ERROR(AddShortcutsFromString(token.CStr()));
	}
	return OpStatus::OK;
}

OP_STATUS
ShortcutAction::AddShortcutsFromString(uni_char *s)
{
	OpKey::Code key = OP_KEY_INVALID;
	uni_char key_value = '\0';
	ShiftKeyState shift_keys = SHIFTKEY_NONE;

	while (s && *s)
	{
		while (uni_isspace(*s) || *s == '+')
		{
			++s;
		}

		if (uni_stristr(s, UNI_L("Ctrl")) == s)
		{
			shift_keys |= SHIFTKEY_CTRL;
			s += 4;
		}
		else if (uni_stristr(s, UNI_L("Alt")) == s)
		{
			shift_keys |= SHIFTKEY_ALT;
			s += 3;
		}
		else if (uni_stristr(s, UNI_L("Shift")) == s)
		{
			shift_keys |= SHIFTKEY_SHIFT;
			s += 5;
		}
		else if (uni_stristr(s, UNI_L("Meta")) == s)
		{
			shift_keys |= SHIFTKEY_META;
			s += 4;
		}
		else
		{
			uni_char* key_string = s;

			while (*s && !uni_isspace(*s))
			{
				++s;
			}

			if (*s)
			{
				*s = 0;
				s++;
			}

			key = OpKey::FromString(key_string);

			switch (key)
			{
				case 0:
				{
					if (uni_isalpha(key_string[0]))
					{
						OP_ASSERT(uni_islower(key_string[0]) && !key_string[1]);
						key = static_cast<OpKey::Code>(OP_KEY_A + (uni_toupper(key_string[0]) - 'A'));
					}
					else
						key_value = key_string[0];

					break;
				}

				case OP_KEY_PLUS:
					key_value = '+';
					break;

				case OP_KEY_COMMA:
					key_value = ',';
					break;

				default:
					if (key >= OP_KEY_A && key <= OP_KEY_Z && uni_isupper(key_string[0]))
						shift_keys |= SHIFTKEY_SHIFT;

				/* Digit key shortcuts are overloaded to apply to both main and numeric pad digit keys. */
					else if (key >= OP_KEY_0 && key <= OP_KEY_9)
						key_value = '0' + (key - OP_KEY_0);
			}
		}
	}
	if (key != OP_KEY_INVALID || key_value != 0)
		RETURN_IF_ERROR(AddShortcut(key, shift_keys, key_value));

	return OpStatus::OK;
}

OP_STATUS
ShortcutAction::AddShortcut(OpKey::Code key, ShiftKeyState shift_keys, uni_char key_value)
{
	OpAutoPtr<Shortcut> shortcut(OP_NEW(Shortcut, ()));
	if (!shortcut.get())
		return OpStatus::ERR_NO_MEMORY;

	shortcut->Set(key, shift_keys, key_value);
	RETURN_IF_ERROR(m_shortcut_list.Add(shortcut.get()));

	shortcut.release();
	return OpStatus::OK;
}

BOOL
ShortcutAction::MatchesKeySequence(Shortcut* shortcuts, unsigned shortcut_count) const
{
	if (shortcut_count >= GetShortcutCount())
		return FALSE;

	for (unsigned i = 0; i <= shortcut_count; i++)
	{
		if (!GetShortcutByIndex(i)->Equals(shortcuts[i]))
		{
			return FALSE;
		}
	}

	return TRUE;
}

/***********************************************************************************
**
**	ShortcutContext
**
***********************************************************************************/

ShortcutContext::ShortcutContext(OpInputAction::ActionMethod method) :
	m_shortcut_actions_hashtable(this),
	m_action_method(method)
{
	m_shortcut_actions_hashtable.SetHashFunctions(this);
#ifndef MOUSE_SUPPORT
	OP_ASSERT(m_action_method != OpInputAction::METHOD_KEYBOARD);
#endif // !MOUSE_SUPPORT
}

OP_STATUS ShortcutContext::Construct(const char* context_name, PrefsSection* section)
{
	RETURN_IF_ERROR(m_context_name.Set(context_name));
	RETURN_IF_ERROR(AddShortcuts(context_name, section));

	return OpStatus::OK;
}

OP_STATUS ShortcutContext::AddShortcuts(const char* context_name, PrefsSection * section)
{
	RETURN_IF_LEAVE( AddShortcutsL(context_name, section) );
	return OpStatus::OK;
}

void ShortcutContext::AddShortcutsL(const char* context_name, PrefsSection * section)
{
	if (section == NULL)
	{
#ifdef QUICK
		section = g_setup_manager->GetSectionL(context_name, m_action_method == OpInputAction::METHOD_KEYBOARD ? OPKEYBOARD_SETUP : OPMOUSE_SETUP);
#elif defined SDK
		section = OperaEmbeddedInternal::GetInternal()->GetInputContextL(context_name, m_action_method == OpInputAction::METHOD_KEYBOARD);
#elif defined GTK_SDK
  		section = _gtk_opera_get_input_contextL(context_name, m_action_method == OpInputAction::METHOD_KEYBOARD);
#elif defined GOGI
		extern PrefsSection * gogi_opera_get_input_contextL(const uni_char* context_name, BOOL is_keyboard);
		OpString uni_context_name;
		ANCHOR(OpString, uni_context_name);
		uni_context_name.SetL(context_name);
	  	section = gogi_opera_get_input_contextL(uni_context_name.CStr(), m_action_method == OpInputAction::METHOD_KEYBOARD);
#elif defined EPOC
	section = g_iModel->GetInputContextL(_M(context_name), m_action_method == OpInputAction::METHOD_KEYBOARD);
#else
# error "You are not using OpSetupManager and do not have a platform specific interface"
#endif
	}

	OpStackAutoPtr<PrefsSection> section_auto_ptr(section);
#ifdef IM_EXTENDED_KEYBOARD_SHORTCUTS
	BOOL use_extended_keyboard = g_pcui->GetIntegerPref(PrefsCollectionUI::ExtendedKeyboardShortcuts);
#endif

	if (section)
	{
		for (const PrefsEntry *entry = section->Entries(); entry; entry = (const PrefsEntry *) entry->Suc())
		{
#ifdef IM_EXTENDED_KEYBOARD_SHORTCUTS
			BOOL key_found = FALSE;
			BOOL ex_key_found = FALSE;
#endif // IM_EXTENDED_KEYBOARD_SHORTCUTS

			const uni_char* key = entry->Key();

			if (key && uni_stristr(key, UNI_L("Platform")) != NULL)
			{
#if defined(_X11_)
				if( uni_stristr(key, UNI_L("Unix")) == NULL )
				{
					continue;
				}
#elif defined(MSWIN)
				BOOL isMCE = (uni_stristr(key, UNI_L("MCE")) != NULL);
				BOOL isWin2000 = (uni_stristr(key, UNI_L("Win2000")) != NULL);
				BOOL isWindows = (uni_stristr(key, UNI_L("Windows")) != NULL) || (uni_stristr(key, UNI_L("Win2000")) != NULL);

				if( CommandLineManager::GetInstance()->GetArgument(CommandLineManager::MediaCenter) )
				{
					if(!isMCE)
					{
						continue;
					}
				}
				else if(isWindows)
				{
 					if(isWin2000 && !IsSystemWin2000orXP())
					{
						continue;
					}
				}
				else
				{
					continue;
				}

#elif defined(_MACINTOSH_)
				if( uni_stristr(key, UNI_L("Mac")) == NULL )
				{
					continue;
				}
#endif
#ifdef IM_EXTENDED_KEYBOARD_SHORTCUTS
				if (key && uni_stristr(key, UNI_L("Feature ExtendedShortcuts")) != NULL)
				{
					// If extended shortcuts are disabled, ignore this shortcut
					if (!use_extended_keyboard)
						continue;

					ex_key_found = TRUE;
				}
#endif // IM_EXTENDED_KEYBOARD_SHORTCUTS
				// skip ahead to real content
				key = uni_stristr(key, UNI_L(","));
				if (key == NULL)
				{
					continue;
				}
				else
				{
					key++;
#ifdef IM_EXTENDED_KEYBOARD_SHORTCUTS
					key_found = TRUE;
#endif // IM_EXTENDED_KEYBOARD_SHORTCUTS
				}
			}
#ifdef IM_EXTENDED_KEYBOARD_SHORTCUTS
			if (key && uni_stristr(key, UNI_L("Feature ExtendedShortcuts")) != NULL)
			{
				// If extended shortcuts are disabled, ignore this shortcut
				if (!use_extended_keyboard)
					continue;

				ex_key_found = TRUE;
			}
			if(!key_found && use_extended_keyboard && ex_key_found)
			{
				// skip ahead to real content
				key = uni_stristr(key, UNI_L(","));
				if (key == NULL)
				{
					continue;
				}
				else
				{
					key++;
				}
			}
#endif // IM_EXTENDED_KEYBOARD_SHORTCUTS

			// translate voice commands, for example
			OpString translated_key; ANCHOR(OpString, translated_key);
			Str::LocaleString key_id(Str::NOT_A_STRING);
			OP_STATUS status;

			if (key && uni_stristr(key, UNI_L(",")) == NULL &&
				m_action_method != OpInputAction::METHOD_KEYBOARD &&
				m_action_method != OpInputAction::METHOD_MOUSE)
			{
				OpLineParser key_line(key);
				status = key_line.GetNextLanguageString(translated_key, &key_id);
				if (OpStatus::IsMemoryError(status))
					LEAVE(status);

				if (translated_key.HasContent())
				{
					key = translated_key.CStr();
				}
			}

			OpInputAction* input_action = NULL;

			status = OpInputAction::CreateInputActionsFromString(entry->Value(), input_action, 0, TRUE);
			if (OpStatus::IsMemoryError(status))
				LEAVE(status);

			if (OpStatus::IsSuccess(status) && input_action)
			{
				ShortcutAction* shortcut_action = OP_NEW(ShortcutAction, ());
				if (shortcut_action == NULL)
				{
					OP_DELETE(input_action);
					LEAVE(OpStatus::ERR_NO_MEMORY);
				}
				status = shortcut_action->Construct(m_action_method, key, input_action);
				if (OpStatus::IsError(status))
				{
					OP_DELETE(shortcut_action);
					LEAVE(status);
				}
				status = m_shortcut_actions_list.Add(shortcut_action);
				if (OpStatus::IsError(status))
				{
					OP_DELETE(shortcut_action);
					LEAVE(status);
				}

				// only add to hash if action doesn't exist there already

				void* same_shortcut_action = NULL;
				m_shortcut_actions_hashtable.GetData(shortcut_action->GetAction(), &same_shortcut_action);

				if (!same_shortcut_action)
				{
					status = m_shortcut_actions_hashtable.Add(shortcut_action->GetAction(), shortcut_action);
					if (OpStatus::IsError(status))
					{
						m_shortcut_actions_list.RemoveByItem(shortcut_action);
						OP_DELETE(shortcut_action);
						LEAVE(status);
					}
				}
			}
		}
	}
}

ShortcutAction* ShortcutContext::GetShortcutActionFromAction(OpInputAction* action)
{
	void* shortcut_action = NULL;

	m_shortcut_actions_hashtable.GetData(action, &shortcut_action);

	return (ShortcutAction*)shortcut_action;
}

UINT32 ShortcutContext::Hash(const void* key)
{
	OpInputAction* input_action = (OpInputAction*) key;

	UINT32 hash = input_action->GetAction() + input_action->GetActionData();

	const uni_char* data_string = input_action->GetActionDataString();

	if (data_string)
	{
		hash += *data_string;
	}

	return hash;
}

BOOL ShortcutContext::KeysAreEqual(const void* key1, const void* key2)
{
	OpInputAction* input_action1 = (OpInputAction*) key1;
	OpInputAction* input_action2 = (OpInputAction*) key2;

	return input_action1->Equals(input_action2);
}

/***********************************************************************************
**
**	ShortcutContextList
**
***********************************************************************************/

ShortcutContext* ShortcutContextList::GetShortcutContextFromName(const char* context_name, PrefsSection* special_section)
{
	if (!context_name)
		return NULL;

	ShortcutContext* shortcut_context = NULL;

	m_shortcut_context_hashtable.GetData(context_name, &shortcut_context);

	if (!shortcut_context)
	{
		shortcut_context = OP_NEW(ShortcutContext, (m_action_method));
		if (shortcut_context && OpStatus::IsSuccess(shortcut_context->Construct(context_name, special_section)))
		{
			m_shortcut_context_hashtable.Add(shortcut_context->GetName(), shortcut_context);
			m_shortcut_context_list.Add(shortcut_context);
		}
		else
		{
			OP_DELETE(shortcut_context);
			return NULL;
		}
	}

	return shortcut_context;
}

void ShortcutContextList::Flush()
{
	m_shortcut_context_hashtable.RemoveAll();
	m_shortcut_context_list.DeleteAll();
}

#ifdef MOUSE_SUPPORT
/***********************************************************************************
**
**    MouseGestureManager
**
***********************************************************************************/

#define MOUSE_GESTURE_UI_DELAY (500) // in ms


MouseGestureManager::MouseGestureManager()
    : m_callback_set(FALSE)
	, m_ui_shown(FALSE)
	, m_instantiation_id(0)
    , m_listener(NULL)
{
}

MouseGestureManager::~MouseGestureManager()
{
	if (m_callback_set)
	{
		g_main_message_handler->UnsetCallBack(this, MSG_MOUSE_GESTURE_UI_SHOW, (INTPTR) this);
		g_main_message_handler->RemoveDelayedMessage(MSG_MOUSE_GESTURE_UI_SHOW, (MH_PARAM_1)this, 0);
	}
}

void MouseGestureManager::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(msg == MSG_MOUSE_GESTURE_UI_SHOW);

	// If the gesture has moved to a new gesture action, this resets the delay,
	// so we need to wait for the appropriate message, indicating a pause on one
	// specific gesture node.
	if (par2 == (MH_PARAM_2)m_instantiation_id && !m_ui_shown && g_input_manager->IsGestureRecognizing())
	{
		if (m_listener)
			m_listener->OnGestureShow();

		m_ui_shown = TRUE;
	}
}

void MouseGestureManager::ShowUIAfterDelay()
{
	if (!m_callback_set)
	{
		g_main_message_handler->SetCallBack(this, MSG_MOUSE_GESTURE_UI_SHOW, (MH_PARAM_1)this);
		m_callback_set = TRUE;
	}

	g_main_message_handler->PostDelayedMessage(
		MSG_MOUSE_GESTURE_UI_SHOW,
		(MH_PARAM_1)this,
		(MH_PARAM_2)m_instantiation_id,
		MOUSE_GESTURE_UI_DELAY);
}

void MouseGestureManager::BeginPotentialGesture(OpPoint origin, OpInputContext* input_context)
{
	m_gesture_origin    = origin;
	m_active_idx        = 0;
	m_ui_shown          = FALSE;

	InitGestureTree(input_context);

	if (m_listener)
		m_listener->OnKillGestureUI();

	m_instantiation_id++;
	ShowUIAfterDelay();
}

void MouseGestureManager::InitGestureTree(OpInputContext* input_context)
{
	m_gesture_tree.DeleteAll();

	GestureNode* root_node = OP_NEW(GestureNode, ());
	if (!root_node)
		return;

	if(OpStatus::IsError(m_gesture_tree.Add(root_node)))
	{
		OP_DELETE(root_node);
		return;
	}

	// So now we gotta find all the valid mouse gestures and add them to the tree.
	OpVector<OpInputContext> input_contexts;
	OpInputContext* ictx = input_context;

	for (;ictx; ictx = ictx->GetParentInputContext())
	{
		input_contexts.Add(ictx);
	}

	for (int i = input_contexts.GetCount()-1; i >= 0; i--)
	{
		ictx = input_contexts.Get(i);

		const char* input_context_name = ictx->GetInputContextName();
		ShortcutContext* shortcut_context = g_input_manager->GetShortcutContextFromActionMethodAndName(OpInputAction::METHOD_MOUSE, input_context_name);
		if (!shortcut_context)
			continue;

		int count = shortcut_context->GetShortcutActionCount();

		for (int shortcut_action_pos = 0; shortcut_action_pos < count; shortcut_action_pos++)
		{
			ShortcutAction* shortcut_action = shortcut_context->GetShortcutActionFromIndex(shortcut_action_pos);
			if (!shortcut_action)
				continue;

			UINT32 shortcut_count = shortcut_action->GetShortcutCount();
			BOOL is_a_gesture = TRUE;

			if (shortcut_count > MAX_SHORTCUT_SEQUENCE_LENGTH)
			{
				// holy shit, this gesture is long! skip it (at least for now)
				is_a_gesture = FALSE;
			}
			else
			{
				for (UINT32 i = 0; is_a_gesture && i < shortcut_count; i++)
				{
					OpKey::Code key = shortcut_action->GetShortcutByIndex(i)->GetKey();

					if (shortcut_action->GetShortcutByIndex(i)->GetShiftKeys())
					{
						// skip shift-gestures (at least for now)
						is_a_gesture = FALSE;
					}
					else
					{
						switch(key)
						{
							case OP_KEY_GESTURE_LEFT:
							case OP_KEY_GESTURE_RIGHT:
							case OP_KEY_GESTURE_UP:
							case OP_KEY_GESTURE_DOWN:
								break; // good, we maybe found a gesture
							default:
								is_a_gesture = FALSE; // this must not be a gesture
						}
					}
				}
			}

			if (!is_a_gesture)
				continue;

			// a good gesture, so add it to the m_gesture_tree
			AddToTree(root_node, shortcut_action, shortcut_action->GetAction());
		}
	}
}

void MouseGestureManager::AddToTree(GestureNode* node, ShortcutAction* sa, OpInputAction* ia)
{
	// First, we need to see how deep we have gone in the tree.
	// 'cut' could just as well have been called 'depth', I suppose,
	// but I called it 'cut' because it's also the index of
	// the relevant Shortcut.
	int cut = 0;
	while (cut < MAX_SHORTCUT_SEQUENCE_LENGTH && node->addr[cut] != -1)
	{
		cut++;
	}

	// Now, depending on 'cut', we do one of the following:
	//  - set the action on this node
	//  - fail (because we went too deep and ran out of space)
	//  - move deeper down the tree, creating nodes as necessary,
	//    until we get to the node we are looking for

	if (cut >= (int) sa->GetShortcutCount())
	{
		// We are at the node we want, so set the corresponding action.
		node->action = ia;
	}
	else if (cut >= MAX_SHORTCUT_SEQUENCE_LENGTH)
	{
		// No more room, so just give up.
		// Maybe we should increase MAX_SHORTCUT_SEQUENCE_LENGTH if this happens.
		// (But seriously, who needs 10-move mouse gestures?!)
		return;
	}
	else
	{
		// Keep moving down the tree, creating nodes if necessary,
		// until we get to the node we are looking for.
		OpKey::Code key = sa->GetShortcutByIndex(cut)->GetKey();

		int dir = 0;
		switch(key)
		{
			case OP_KEY_GESTURE_LEFT:   dir = 0;  break;
			case OP_KEY_GESTURE_RIGHT:  dir = 1;  break;
			case OP_KEY_GESTURE_UP:     dir = 2;  break;
			case OP_KEY_GESTURE_DOWN:   dir = 3;  break;
		}

		int next_node_idx = node->dirs[dir];
		GestureNode* next_node = m_gesture_tree.Get(next_node_idx);

		if (!next_node)
		{
			// The node we want doesn't exist, so we have to create it.
			next_node = OP_NEW(GestureNode, ());
			if (!next_node)
				return;

			if(OpStatus::IsError(m_gesture_tree.Add(next_node)))
			{
				OP_DELETE(next_node);
				return;
			}

			next_node_idx = m_gesture_tree.GetCount() - 1;
			node->dirs[dir] = next_node_idx; // make sure 'node' knows about its children

			// Now we need to store the address of the next node,
			// which is 'node->addr' plus 'dir' at the end.
			for (int i = 0; i < cut; i++)
			{
				next_node->addr[i] = node->addr[i];
			}
			next_node->addr[cut] = dir;
		}

		AddToTree(next_node, sa, ia);
	}
}

void MouseGestureManager::UpdateMouseGesture(int dir)
{
	GestureNode* curr_selected_node = m_gesture_tree.Get(m_active_idx);
	GestureNode* next_selected_node = NULL;

	if (curr_selected_node)
	{
		m_active_idx = curr_selected_node->dirs[dir];
		next_selected_node = m_gesture_tree.Get(m_active_idx);
	}
	m_instantiation_id++;

	if (next_selected_node)
	{
		if (m_listener)
			m_listener->OnGestureMove(dir);

		ShowUIAfterDelay();
	}
	else
	{
		CancelGesture();
		m_active_idx = 0;
	}
}

void MouseGestureManager::EndGesture(BOOL gesture_was_cancelled)
{
	m_ui_shown = TRUE;

	if (!m_listener)
		return;

	if (!gesture_was_cancelled &&
		g_input_manager->WillKeyUpTriggerGestureAction(g_input_manager->GetGestureButton()))
	{
		m_listener->OnFadeOutGestureUI(m_active_idx);
	}
	else
	{
		m_listener->OnFadeOutGestureUI(-1);
	}
}


/* static */ OpKey::Code
MouseGesture::CalculateMouseGesture(int delta_x, int delta_y, int threshold)
{
	if (delta_x*delta_x + delta_y*delta_y < threshold*threshold)
		return OP_KEY_INVALID;

	OpKey::Code gesture = OP_KEY_INVALID;
#if defined(OP_KEY_GESTURE_UP_ENABLED) && defined(OP_KEY_GESTURE_LEFT_ENABLED) && defined(OP_KEY_GESTURE_DOWN_ENABLED) && defined(OP_KEY_GESTURE_RIGHT_ENABLED)
	int delta_a, delta_b;
	if (delta_x >= 0)
	{
		if (delta_y < 0)
		{
			gesture = OP_KEY_GESTURE_UP;
			delta_a = -delta_y;
			delta_b = delta_x;
		}
		else
		{
			gesture = OP_KEY_GESTURE_RIGHT;
			delta_a = delta_x;
			delta_b = delta_y;
		}
	}
	else
	{
		if (delta_y >= 0)
		{
			gesture = OP_KEY_GESTURE_DOWN;
			delta_a = delta_y;
			delta_b = -delta_x;
		}
		else
		{
			gesture = OP_KEY_GESTURE_LEFT;
			delta_a = -delta_x;
			delta_b = -delta_y;
		}
	}
	if (delta_b > delta_a)
	{
		gesture = NextGesture(gesture);
	}

#endif // (OP_KEY_GESTURE_UP_ENABLED) && (OP_KEY_GESTURE_LEFT_ENABLED) && (OP_KEY_GESTURE_DOWN_ENABLED) && (OP_KEY_GESTURE_RIGHT_ENABLED)
	return gesture;
}

#if 0

static OpKey::Code
CalculateMouseGestureRef(int deltax, int deltay, int threshold)
{
	double length = op_sqrt((double)((delta_x * delta_x) + (delta_y * delta_y)));

	if (length > threshold)
	{
		double angle = op_atan2((double)delta_x, (double)delta_y);

		OpKey::Code gesture = OP_KEY_GESTURE_UP;
		OpKey::Code next_gesture = OP_KEY_GESTURE_UP + 1;

		double pi = 3.1415926535;
		double sector_size = pi / 2;
		double sector_angle = (pi * 3 / 4);
		const INT32 last_gesture = OP_KEY_GESTURE_LEFT;

		for (; next_gesture <= last_gesture; next_gesture++, sector_angle -= sector_size)
		{
			if (angle <= sector_angle && angle >= (sector_angle - sector_size))
			{
				gesture = next_gesture;
				break;
			}
		}

		if (gesture != m_last_gesture)
		{
			InvokeKeyPressed(gesture, SHIFTKEY_NONE);
			m_last_gesture = gesture;
		}

		m_last_gesture_mouse_position = new_mouse_position;
	}
}

#endif // 0

/* static */ OpKey::Code
MouseGesture::NextGesture(OpKey::Code gesture, BOOL diagonal)
{
#if defined(OP_KEY_GESTURE_UP_ENABLED) && defined(OP_KEY_GESTURE_LEFT_ENABLED) && defined(OP_KEY_GESTURE_DOWN_ENABLED) && defined(OP_KEY_GESTURE_RIGHT_ENABLED)
	if (!diagonal)
	{
		switch (gesture)
		{
		case OP_KEY_GESTURE_UP:
			return OP_KEY_GESTURE_RIGHT;
		case OP_KEY_GESTURE_LEFT:
			return OP_KEY_GESTURE_UP;
		case OP_KEY_GESTURE_DOWN:
			return OP_KEY_GESTURE_LEFT;
		case OP_KEY_GESTURE_RIGHT:
			return OP_KEY_GESTURE_DOWN;
		}
	}

#endif // OP_KEY_GESTURE_UP_ENABLED && OP_KEY_GESTURE_LEFT_ENABLED && OP_KEY_GESTURE_DOWN_ENABLED && OP_KEY_GESTURE_RIGHT_ENABLED
	return OP_KEY_INVALID;
}
#endif // MOUSE_SUPPORT

/***********************************************************************************
**
**	OpKeyToLanguageString
**
***********************************************************************************/

/* static */ OP_STATUS
OpInputManager::OpKeyToLanguageString(OpKey::Code key, OpString& string)
{
	switch (key)
	{
		case OP_KEY_HOME:
			return g_languageManager->GetStringL(Str::M_KEY_HOME, string);
		case OP_KEY_END:
			return g_languageManager->GetStringL(Str::M_KEY_END, string);
		case OP_KEY_PAGEUP:
			return g_languageManager->GetStringL(Str::M_KEY_PAGEUP, string);
		case OP_KEY_PAGEDOWN:
			return g_languageManager->GetStringL(Str::M_KEY_PAGEDOWN, string);
		case OP_KEY_UP:
			return g_languageManager->GetStringL(Str::M_KEY_UP, string);
		case OP_KEY_DOWN:
			return g_languageManager->GetStringL(Str::M_KEY_DOWN, string);
		case OP_KEY_LEFT:
			return g_languageManager->GetStringL(Str::M_KEY_LEFT, string);
		case OP_KEY_RIGHT:
			return g_languageManager->GetStringL(Str::M_KEY_RIGHT, string);
		case OP_KEY_ESCAPE:
			return g_languageManager->GetStringL(Str::M_KEY_ESCAPE, string);
		case OP_KEY_INSERT:
			return g_languageManager->GetStringL(Str::M_KEY_INSERT, string);
		case OP_KEY_DELETE:
			return g_languageManager->GetStringL(Str::M_KEY_DELETE, string);
		case OP_KEY_BACKSPACE:
			return g_languageManager->GetStringL(Str::M_KEY_BACKSPACE, string);
		case OP_KEY_TAB:
			return g_languageManager->GetStringL(Str::M_KEY_TAB, string);
		case OP_KEY_SPACE:
			return g_languageManager->GetStringL(Str::M_KEY_SPACE, string);
		case OP_KEY_ENTER:
			return g_languageManager->GetStringL(Str::M_KEY_ENTER, string);
#ifdef OP_KEY_SUBTRACT_ENABLED
		case OP_KEY_SUBTRACT:
			return string.Set("-");
#endif
#ifdef OP_KEY_OEM_MINUS_ENABLED
		case OP_KEY_OEM_MINUS:
			return string.Set("-");
#endif
#ifdef OP_KEY_OEM_PLUS_ENABLED
		case OP_KEY_OEM_PLUS:
			return string.Set("+");
#endif
	}

	// fallback
	const uni_char* key_string = OpKey::ToString(key);

	if (key_string)
		RETURN_IF_ERROR(string.Set(key_string));

	return OpStatus::OK;
}

/***********************************************************************************
**
**	OpInputManager
**
***********************************************************************************/

#ifndef HAS_COMPLEX_GLOBALS
extern void init_s_action_strings();
#endif // !HAS_COMPLEX_GLOBALS

OpInputManager::OpInputManager()
	: m_full_update(FALSE)
	, m_update_message_pending(FALSE)
	, m_keyboard_input_context(NULL)
	, m_old_keyboard_input_context(NULL)
	, m_lock_keyboard_input_context(FALSE)
	, m_key_sequence_id(0)
	, m_keyboard_shortcut_context_list(OpInputAction::METHOD_KEYBOARD)
	, m_block_input_recording(FALSE)
	, m_action_first_input_context(NULL)
#if !defined(IM_UI_HANDLES_SHORTCUTS)
	, m_external_input_listener(NULL)
#endif // !IM_UI_HANDLES_SHORTCUTS
#ifdef MOUSE_SUPPORT
	, m_mouse_input_context(NULL)
	, m_lock_mouse_input_context(FALSE)
	, m_is_flipping(FALSE)
	, m_last_gesture(0)
	, m_loaded_gesture_action(NULL)
	, m_gesture_recording_was_attempted(FALSE)
	, m_mouse_shortcut_context_list(OpInputAction::METHOD_MOUSE)
	, m_mouse_gestures_temporarily_disabled(FALSE)
#endif // MOUSE_SUPPORT
	, m_touch_state(0)
{
	CONST_ARRAY_INIT(s_action_strings);
}

OP_STATUS OpInputManager::Construct()
{
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_UPDATE_ALL_INPUT_STATES, (MH_PARAM_1)this));
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_IM_ACTION_DELAY, (MH_PARAM_1)this));

	return OpStatus::OK;
}

OpInputManager::~OpInputManager()
{
	g_main_message_handler->UnsetCallBacks(this);
}

/***********************************************************************************
**
**	Flush
**
***********************************************************************************/

void OpInputManager::Flush()
{
#ifdef MOUSE_SUPPORT
	m_mouse_shortcut_context_list.Flush();
#endif // MOUSE_SUPPORT
	m_keyboard_shortcut_context_list.Flush();
}


/***********************************************************************************
**
**	RegisterActivity()
**
***********************************************************************************/

void OpInputManager::RegisterActivity()
{
#ifdef QUICK
	if( g_application )
	{
		g_application->RegisterActivity();
	}
#endif
}


#ifdef MOUSE_SUPPORT
BOOL OpInputManager::ShouldInvokeGestures()
{
	if (m_block_input_recording || m_mouse_gestures_temporarily_disabled)
	{
		return FALSE;
	}

	return g_pccore->GetIntegerPref(PrefsCollectionCore::EnableGesture);
}
#endif // MOUSE_SUPPORT


/***********************************************************************************
**
**	BroadcastInputContextChanged()
**
***********************************************************************************/

void OpInputManager::BroadcastInputContextChanged(BOOL keyboard, OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason) const
{
	OpInputContext* input_context = old_input_context;

	while (input_context)
	{
		OpInputContext* parent_context = input_context->GetParentInputContext();

		if (keyboard)
		{
			input_context->OnKeyboardInputLost(new_input_context, old_input_context, reason);
		}
		else
		{
			input_context->OnMouseInputLost(new_input_context, old_input_context);
		}
		input_context = parent_context;
	}

	input_context = new_input_context;

	while (input_context)
	{
		if (keyboard)
		{
			input_context->OnKeyboardInputGained(new_input_context, old_input_context, reason);
		}
		else
		{
			input_context->OnMouseInputGained(new_input_context, old_input_context);
		}
		input_context = input_context->GetParentInputContext();
	}
}

/***********************************************************************************
**
**	SetKeyboardInputContext
**
***********************************************************************************/

void OpInputManager::SetKeyboardInputContext(OpInputContext* input_context, FOCUS_REASON reason)
{
	if (m_keyboard_input_context != input_context && !m_lock_keyboard_input_context)
	{
		// check if input context is available, and only then switch (always allow NULL input_context)
		// To be available means that all parents must also be available

		BOOL switch_available = TRUE;

		OpInputContext* parent_context = input_context;

		while (parent_context)
		{
			if (!parent_context->IsInputContextAvailable(reason))
			{
				switch_available = FALSE;
				break;
			}
			else if (!parent_context->IsParentInputContextAvailabilityRequired())
			{
				break;
			}

			parent_context = parent_context->GetParentInputContext();
		}

		if (switch_available)
		{
			// Inform new and old keyboard input contexts and all their parents about the change

			m_old_keyboard_input_context = m_keyboard_input_context;
			m_keyboard_input_context = input_context;

			// input context was available, so update recent child pointers up to root

			UpdateRecentKeyboardChildInputContext(input_context);

			m_key_sequence_id = 0;

			BroadcastInputContextChanged(TRUE, m_keyboard_input_context, m_old_keyboard_input_context, reason);

			if (m_keyboard_input_context)
			{
				OpInputAction input_action(OpInputAction::ACTION_LOWLEVEL_NEW_KEYBOARD_CONTEXT);
				InvokeActionInternal(&input_action);
			}
		}
		else if (input_context)
		{
			// input context wasn't available, but still update active child pointers up to the point where it meets the current active one
			UpdateRecentKeyboardChildInputContext(input_context);
		}
	}
}

/***********************************************************************************
**
**	UpdateRecentKeyboardChildInputContext
**
***********************************************************************************/

void OpInputManager::UpdateRecentKeyboardChildInputContext(OpInputContext* input_context)
{
	OpInputContext* parent_context = input_context;

	while (parent_context && (!m_keyboard_input_context || input_context == m_keyboard_input_context || parent_context->GetRecentKeyboardChildInputContext() != m_keyboard_input_context))
	{
		parent_context->SetRecentKeyboardChildInputContext(input_context);

		if (!parent_context->IsParentInputContextAvailabilityRequired())
		{
			break;
		}

		parent_context = parent_context->GetParentInputContext();
	}
}

/***********************************************************************************
**
**	RestoreKeyboardInputContext
**
***********************************************************************************/

void OpInputManager::RestoreKeyboardInputContext(OpInputContext* input_context, OpInputContext* fallback_input_context, FOCUS_REASON reason)
{
	if (input_context->GetRecentKeyboardChildInputContext() &&
		input_context->GetRecentKeyboardChildInputContext()->IsInputContextAvailable(reason))
	{
		input_context = input_context->GetRecentKeyboardChildInputContext();
	}
	else if (fallback_input_context)
	{
		input_context = fallback_input_context;
	}

	SetKeyboardInputContext(input_context, reason);
}

/***********************************************************************************
**
**	SetMouseInputContext
**
***********************************************************************************/

#ifdef MOUSE_SUPPORT
void OpInputManager::SetMouseInputContext(OpInputContext* input_context)
{
	if (m_mouse_input_context != input_context && !m_lock_mouse_input_context)
	{
		// Inform new and old mouse input context about the change

		OpInputContext* old_mouse_input_context = m_mouse_input_context;
		m_mouse_input_context = input_context;

		BroadcastInputContextChanged(FALSE, m_mouse_input_context, old_mouse_input_context);

		if (m_mouse_input_context)
		{
			OpInputAction input_action(OpInputAction::ACTION_LOWLEVEL_NEW_MOUSE_CONTEXT);
			InvokeActionInternal(&input_action, m_mouse_input_context);
		}
	}
}
#endif // MOUSE_SUPPORT

/***********************************************************************************
**
**	ReleaseInputContext
**
***********************************************************************************/

void OpInputManager::ReleaseInputContext(OpInputContext* input_context, FOCUS_REASON reason, BOOL keep_keyboard_focus /*= FALSE*/)
{
	// make sure noone above has recent children that points to input_context's recent child.

	if (input_context == m_old_keyboard_input_context)
		m_old_keyboard_input_context = NULL;

	OpInputContext* parent_context = input_context->GetParentInputContext();

	if (input_context == m_action_first_input_context)
		m_action_first_input_context = parent_context;

	while (parent_context && parent_context->GetRecentKeyboardChildInputContext() == input_context->GetRecentKeyboardChildInputContext())
	{
		parent_context->SetRecentKeyboardChildInputContext(NULL);
		parent_context = parent_context->GetParentInputContext();
	}

	if (!keep_keyboard_focus)
	{
		// if current context lies in the path now being cut off, give focus to something
		// higher in the chain

		OpInputContext* check_input_context = m_keyboard_input_context;

		while (check_input_context)
		{
			if (input_context == check_input_context)
			{
				break;
			}

			check_input_context = check_input_context->GetParentInputContext();
		}

		if (check_input_context)
		{
			m_lock_keyboard_input_context = FALSE;

			// set focus to first one that is really available

			OpInputContext* parent_input_context = input_context->GetParentInputContext();
			OpInputContext* new_input_context = parent_input_context;

			while (parent_input_context)
			{
				if (!parent_input_context->IsInputContextAvailable(reason))
				{
					new_input_context = parent_input_context->GetParentInputContext();
				}
				else if (!parent_input_context->IsParentInputContextAvailabilityRequired())
				{
					break;
				}

				parent_input_context = parent_input_context->GetParentInputContext();
			}

			SetKeyboardInputContext(new_input_context, reason);
		}
	}

#ifdef MOUSE_SUPPORT
	if (input_context == m_mouse_input_context)
	{
		m_lock_mouse_input_context = FALSE;

		SetMouseInputContext(NULL);
	}
#endif // MOUSE_SUPPORT
}

/***********************************************************************************
**
**	SetMousePosition
**
***********************************************************************************/

#ifdef MOUSE_SUPPORT
void OpInputManager::SetMousePosition(const OpPoint& new_mouse_position, INT32 ignore_modifiers)
{
	OpRandomGenerator::AddEntropyFromTimeAllGenerators();
	if (m_mouse_position.Equals(new_mouse_position))
		return;

	INT32 pos_data[2]; /* ARRAY OK 2009-02-04 haavardm */
	pos_data[0] = new_mouse_position.x;
	pos_data[1] = new_mouse_position.y;

	OpRandomGenerator::AddEntropyAllGenerators(pos_data, sizeof(pos_data), 8);

	m_mouse_position = new_mouse_position;

	RegisterActivity();

	if (IsGestureRecognizing() && ShouldInvokeGestures())
	{
		int threshold = g_pccore->GetIntegerPref(PrefsCollectionCore::GestureThreshold);

		int delta_x = new_mouse_position.x - m_last_gesture_mouse_position.x;
		int delta_y = new_mouse_position.y - m_last_gesture_mouse_position.y;

		OpKey::Code gesture = MouseGesture::CalculateMouseGesture(delta_x, delta_y, threshold);

		if (gesture != OP_KEY_INVALID)
		{
			if (gesture != m_last_gesture)
			{
				INT32 shiftkeys = 0;
# ifdef OPSYSTEMINFO_SHIFTSTATE
				shiftkeys = g_op_system_info->GetShiftKeyState() & ~ignore_modifiers;
# endif // OPSYSTEMINFO_SHIFTSTATE
				InvokeKeyPressed(gesture, shiftkeys);
				m_last_gesture = gesture;

				int dir = -1;
				switch(gesture)
				{
					case OP_KEY_GESTURE_LEFT:   dir = 0;  break;
					case OP_KEY_GESTURE_RIGHT:  dir = 1;  break;
					case OP_KEY_GESTURE_UP:     dir = 2;  break;
					case OP_KEY_GESTURE_DOWN:   dir = 3;  break;
				}
				if (dir >= 0)
					m_gesture_manager.UpdateMouseGesture(dir);
			}
			m_last_gesture_mouse_position = new_mouse_position;
		}
	}
}
#endif // MOUSE_SUPPORT

/***********************************************************************************
**
**	GetFlipButtons
**
***********************************************************************************/

#ifdef MOUSE_SUPPORT
void OpInputManager::GetFlipButtons(OpKey::Code &back_button, OpKey::Code &forward_button) const
{
    if (g_pccore->GetIntegerPref(PrefsCollectionCore::ReverseButtonFlipping))
	{
		back_button = OP_KEY_MOUSE_BUTTON_2;
		forward_button = OP_KEY_MOUSE_BUTTON_1;
	}
	else
	{
		back_button = OP_KEY_MOUSE_BUTTON_1;
		forward_button = OP_KEY_MOUSE_BUTTON_2;
	}
}
#endif // MOUSE_SUPPORT

/***********************************************************************************
**
**	GetInputContextFromKey
**
***********************************************************************************/

OpInputContext*	OpInputManager::GetInputContextFromKey(OpKey::Code key)
{
	(void)key; //Yes compiler, we declare and possibly do not use this variable..

#ifdef MOUSE_SUPPORT
	OpInputAction::ActionMethod method = GetActionMethodFromKey(key);
#endif // MOUSE_SUPPORT

#ifdef MOUSE_SUPPORT
	if (method == OpInputAction::METHOD_MOUSE)
		return m_mouse_input_context;
#endif // MOUSE_SUPPORT

	return m_keyboard_input_context;
}

/***********************************************************************************
**
**	GetActionMethodFromKey
**
***********************************************************************************/

OpInputAction::ActionMethod OpInputManager::GetActionMethodFromKey(OpKey::Code key)
{
	(void)key; //Yes compiler, we declare and possibly do not use this variable..

#ifdef MOUSE_SUPPORT
	if (IS_OP_KEY_MOUSE_BUTTON(key) || IS_OP_KEY_GESTURE(key) || IS_OP_KEY_FLIP(key) || IsGestureRecognizing())
		return OpInputAction::METHOD_MOUSE;
#endif // MOUSE_SUPPORT

	return OpInputAction::METHOD_KEYBOARD;
}

/***********************************************************************************
**
**	InvokeAction
**
***********************************************************************************/

BOOL OpInputManager::InvokeAction(OpInputAction::Action action, INTPTR action_data, const uni_char* action_data_string,  OpInputContext* first, OpInputContext* last, BOOL send_prefilter_action, OpInputAction::ActionMethod action_method)
{
	OpInputAction input_action(action);
	input_action.SetActionData(action_data);
	input_action.SetActionDataString(action_data_string);

	return InvokeAction(&input_action, first, last, send_prefilter_action, action_method);
}

BOOL OpInputManager::InvokeAction(OpInputAction::Action action, INTPTR action_data, const uni_char* action_data_string, const uni_char* action_data_string_param,  OpInputContext* first, OpInputContext* last, BOOL send_prefilter_action, OpInputAction::ActionMethod action_method)
{
	OpInputAction input_action(action);
	input_action.SetActionData(action_data);
	input_action.SetActionDataString(action_data_string);
	input_action.SetActionDataStringParameter(action_data_string_param);

	return InvokeAction(&input_action, first, last, send_prefilter_action, action_method);
}

BOOL OpInputManager::InvokeAction(OpInputAction* action, OpInputContext* first, OpInputContext* last, BOOL send_prefilter_action, OpInputAction::ActionMethod action_method)
{
	if (!action)
		return FALSE;

#ifdef ACTION_CYCLE_TO_NEXT_PAGE_ENABLED
	if (action->GetAction() == OpInputAction::ACTION_CYCLE_TO_NEXT_PAGE)
		m_gesture_manager.CancelGesture();
#endif // ACTION_CYCLE_TO_NEXT_PAGE_ENABLED
#ifdef ACTION_CYCLE_TO_PREVIOUS_PAGE_ENABLED
	if (action->GetAction() == OpInputAction::ACTION_CYCLE_TO_PREVIOUS_PAGE)
		m_gesture_manager.CancelGesture();
#endif // ACTION_CYCLE_TO_PREVIOUS_PAGE_ENABLED

	OpInputAction* action_copy = OpInputAction::CopyInputActions(action);
	if (action_copy == NULL)
		return FALSE;

	if (action_method != OpInputAction::METHOD_OTHER)
	{
		OpInputAction* next = action_copy;

		while (next)
		{
			next->SetActionMethod(action_method);
			next = next->GetNextInputAction();
		}
	}

	BOOL handled = InvokeActionInternal(action_copy, first, last, send_prefilter_action);

	OP_DELETE(action_copy);

	return handled;
}

/***********************************************************************************
**
**	InvokeKeyDown
**
***********************************************************************************/

BOOL
OpInputManager::InvokeKeyDown(OpKey::Code key, ShiftKeyState shift_keys)
{
	/* Should only be used for keys not producing any character values. */
	OP_ASSERT(OpKey::IsModifier(key) || OpKey::IsFunctionKey(key) || key == OP_KEY_ENTER || key == OP_KEY_ESCAPE || key >= 0x100);
	return InvokeKeyDown(key, NULL, 0, NULL, shift_keys, FALSE, OpKey::LOCATION_STANDARD, TRUE);
}

BOOL OpInputManager::InvokeKeyDown(OpKey::Code key, const uni_char *value, OpPlatformKeyEventData *event_data, ShiftKeyState shift_keys, BOOL repeat, OpKey::Location location, BOOL send_prefilter_action)
{
	return InvokeKeyDown(key, reinterpret_cast<const char *>(value), UINT_MAX, event_data, shift_keys, repeat, location, send_prefilter_action);
}

BOOL OpInputManager::InvokeKeyDown(OpKey::Code key, const char *value, unsigned value_length, OpPlatformKeyEventData *event_data, ShiftKeyState shift_keys, BOOL repeat, OpKey::Location location, BOOL send_prefilter_action)
{
	OP_ASSERT(IS_OP_KEY(key) || !"Unmapped keycode, please check virtual keycode remapping.");
	if (!IS_OP_KEY(key))
		return TRUE;

	RegisterActivity();

	if( key != OP_KEY_ESCAPE )
	{
#ifdef MOUSE_SUPPORT
		m_gesture_recording_was_attempted = false;
		m_gesture_manager.CancelGesture();
#endif // MOUSE_SUPPORT
	}

#ifdef SKIN_SUPPORT
	// Count "in place drag" as drag, too.
	if (g_widget_globals->captured_widget && g_widget_globals->captured_widget->IsFloating())
	{
		if( key == OP_KEY_ESCAPE || IS_OP_KEY_MOUSE_BUTTON(key))
		{
			ResetInput();
		}
		return TRUE;
	}
#endif // SKIN_SUPPORT

	if (IS_OP_KEY_MOUSE_BUTTON(key))
	{
		if(g_windowManager)
		{
			g_windowManager->ResetCurrentClickedURLAndWindow();
		}
	}

	if (send_prefilter_action)
	{
		if (!m_block_input_recording)
		{
			if (OpStatus::IsMemoryError(m_key_down_hashtable.Add(key)))
				return FALSE;
		}
	}

#ifdef MOUSE_SUPPORT

#if defined (OP_KEY_FLIP_BACK_ENABLED) && defined (OP_KEY_FLIP_FORWARD_ENABLED)
	OpKey::Code back_button, forward_button;
	GetFlipButtons(back_button, forward_button);

	if (key == back_button && IsKeyDown(forward_button))
	{
		SetFlipping(TRUE);
		if (g_pccore->GetIntegerPref(PrefsCollectionCore::EnableMouseFlips))
			InvokeKeyPressed(OP_KEY_FLIP_BACK, shift_keys);

	}
	else if (key == forward_button && IsKeyDown(back_button))
	{
		SetFlipping(TRUE);
		if (g_pccore->GetIntegerPref(PrefsCollectionCore::EnableMouseFlips))
			InvokeKeyPressed(OP_KEY_FLIP_FORWARD, shift_keys);
	}

	if (IsFlipping())
	{
		return TRUE;
	}
#endif // OP_KEY_FLIP_BACK_ENABLED && OP_KEY_FLIP_FORWARD_ENABLED

	if (key == GetGestureButton())
	{
		m_last_gesture_mouse_position = m_mouse_position;
		m_first_gesture_mouse_position = m_mouse_position;
		m_last_gesture = 0;

		if (ShouldInvokeGestures())
			m_gesture_manager.BeginPotentialGesture(m_mouse_position, m_mouse_input_context);
	}

#endif // MOUSE_SUPPORT

	if (m_key_sequence_id == 0)
	{
		OpInputAction input_action(OpInputAction::ACTION_LOWLEVEL_KEY_DOWN, key, shift_keys, repeat, location, GetActionMethodFromKey(key));
		OpInputContext*	input_context = GetInputContextFromKey(key);
		if (value_length == UINT_MAX)
		{
			if (value && OpStatus::IsError(input_action.SetActionKeyValue(reinterpret_cast<const uni_char *>(value))))
				g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		}
		else if (value_length > 0 && OpStatus::IsError(input_action.SetActionKeyValue(value, value_length)))
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);

		if (event_data)
			input_action.SetPlatformKeyEventData(event_data);

		if (InvokeActionInternal(&input_action, input_context, input_context, send_prefilter_action))
		{
// NOTE: until all code is phased over to use this kind of mouse input
// we should only return TRUE (which means mouseclick handled) only for keyboard keys
// This is so that platform code can like before let the mouse click continue to
// OpView mouse listeners etc.
			return !IS_OP_KEY_MOUSE_BUTTON(key);
//			return TRUE;
		}
	}

#ifdef MOUSE_SUPPORT
	if (key != OP_KEY_MOUSE_BUTTON_1 &&
	    key != OP_KEY_MOUSE_BUTTON_2 &&
	    IS_OP_KEY_MOUSE_BUTTON(key))
	{
		// mouse buttons with index 3 or greater are allowed to be translated into
		// actions, just like key clicks.
		// This is done at KeyDown, while something similar for mouse button 1 and 2
		// must be done at KeyUp so that gestures and dragging can be done
		// without conflicting with those.. .anyway, we don't support translating for
		// the main 1 and 2 buttons now.. too much code around in Opera needs to be
		// changed first.

		return InvokeKeyPressed(key, NULL, shift_keys, FALSE, send_prefilter_action);
	}
#endif // MOUSE_SUPPORT

	return FALSE;
}

/***********************************************************************************
**
**	InvokeKeyUp
**
***********************************************************************************/

BOOL
OpInputManager::InvokeKeyUp(OpKey::Code key, ShiftKeyState shift_keys)
{
	/* Should only be used for keys not producing any character values. */
	OP_ASSERT(OpKey::IsModifier(key) || OpKey::IsFunctionKey(key) || key == OP_KEY_ENTER || key == OP_KEY_ESCAPE || key >= 0x100);
	return InvokeKeyUp(key, NULL, 0, NULL, shift_keys, FALSE, OpKey::LOCATION_STANDARD, TRUE);
}

BOOL OpInputManager::InvokeKeyUp(OpKey::Code key, const uni_char *value, OpPlatformKeyEventData *event_data, ShiftKeyState shift_keys, BOOL repeat, OpKey::Location location, BOOL send_prefilter_action)
{
	return InvokeKeyUp(key, reinterpret_cast<const char *>(value), UINT_MAX, event_data, shift_keys, repeat, location, send_prefilter_action);
}

BOOL OpInputManager::InvokeKeyUp(OpKey::Code key, const char *value, unsigned value_length, OpPlatformKeyEventData *event_data, ShiftKeyState shift_keys, BOOL repeat, OpKey::Location location, BOOL send_prefilter_action)
{
	OP_ASSERT(IS_OP_KEY(key) || !"Unmapped keycode, please check virtual keycode remapping.");
	if (!IS_OP_KEY(key))
		return TRUE;

	RegisterActivity();


	if (send_prefilter_action)
	{
		if (!m_block_input_recording)
			OpStatus::Ignore(m_key_down_hashtable.Remove(key));
	}

	// Code used to close the alt-tab cycler on desktop.
	// FIXME: This code should preferrably not be here.
#ifdef ACTION_CLOSE_CYCLER_ENABLED

	if ((key == OP_KEY_CTRL || key == OP_KEY_ALT
#ifdef OP_KEY_MAC_CTRL_ENABLED
         || key == OP_KEY_MAC_CTRL
#endif // OP_KEY_MAC_CTRL_ENABLED

#ifdef MOUSE_SUPPORT
		 || key == GetGestureButton()
#endif // MOUSE_SUPPORT
		) && InvokeAction(OpInputAction::ACTION_CLOSE_CYCLER, 1))
	{
#ifdef MOUSE_SUPPORT
		SetFlipping(FALSE);
		m_loaded_gesture_action = NULL;
		m_gesture_manager.CancelGesture();
#endif // MOUSE_SUPPORT
		m_key_sequence_id = 0;
		return TRUE;
	}

#endif // ACTION_CLOSE_CYCLER_ENABLED


#ifdef MOUSE_SUPPORT
	if (IsFlipping())
	{
		OpKey::Code back_button, forward_button;
		GetFlipButtons(back_button, forward_button);

		if (!IsKeyDown(back_button) && !IsKeyDown(forward_button))
		{
			SetFlipping(FALSE);
			m_loaded_gesture_action = NULL;
			m_gesture_manager.CancelGesture();
			m_key_sequence_id = 0;
		}

		return TRUE;
	}
#endif // MOUSE_SUPPORT

#ifdef MOUSE_SUPPORT
	if (key == GetGestureButton())
	{
		m_key_sequence_id = 0;

		if (m_loaded_gesture_action)
		{
			OpInputAction* loaded_gesture_action = m_loaded_gesture_action;
			OpInputContext* mouse_input_context = m_mouse_input_context;
			loaded_gesture_action->SetActionPosition(m_first_gesture_mouse_position);
			loaded_gesture_action->SetGestureInvoked(TRUE);

			if (m_mouse_input_context && ShouldInvokeGestures())
			{
				InvokeAction(loaded_gesture_action, mouse_input_context, NULL, TRUE, OpInputAction::METHOD_MOUSE);
			}

			m_gesture_manager.EndGesture();
			m_loaded_gesture_action = NULL;
			return TRUE;
		}
		else if( m_gesture_recording_was_attempted || m_block_input_recording )
		{
			// Do not open popup menu after a failed or aborted gesture stroke or when we block recording
			return TRUE;
		}
		else
		{
			// No gesture action, even though we were gesturing, means that
			// we default to rt-click bringing up the context menu. So we
			// have to end this gesture, but don't return TRUE.
			m_gesture_manager.EndGesture();
		}
	}
#endif // MOUSE_SUPPORT

	if (m_key_sequence_id == 0)
	{
		OpInputAction input_action(OpInputAction::ACTION_LOWLEVEL_KEY_UP, key, shift_keys, repeat, location, GetActionMethodFromKey(key));
		OpInputContext*	input_context = GetInputContextFromKey(key);
		if (value_length == UINT_MAX)
		{
			if (value && OpStatus::IsError(input_action.SetActionKeyValue(reinterpret_cast<const uni_char *>(value))))
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		}
		else if (value_length > 0 && OpStatus::IsError(input_action.SetActionKeyValue(value, value_length)))
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);

		if (event_data)
			input_action.SetPlatformKeyEventData(event_data);

		if (InvokeActionInternal(&input_action, input_context, input_context, send_prefilter_action))
		{
// NOTE: until all code is phased over to use this kind of mouse input
// we should only return TRUE (which means mouseclick handled) only for keyboard keys
// This is so that platform code can like before let the mouse click continue to
// OpView mouse listeners etc.
			return !IS_OP_KEY_MOUSE_BUTTON(key);
//			return TRUE;
		}
	}

	return FALSE;
}

/***********************************************************************************
**
**	InvokeKeyPressed
**
***********************************************************************************/

BOOL
OpInputManager::InvokeKeyPressed(OpKey::Code key, ShiftKeyState shift_keys)
{
	OP_ASSERT(OpKey::IsModifier(key) || OpKey::IsFunctionKey(key) || key == OP_KEY_ENTER || key == OP_KEY_ESCAPE || key >= 0x100);
	return InvokeKeyPressed(key, NULL, 0, shift_keys, FALSE, TRUE);
}

BOOL OpInputManager::InvokeKeyPressed(OpKey::Code key, const uni_char *value, ShiftKeyState shift_keys, BOOL repeat, BOOL send_prefilter_action)
{
	return InvokeKeyPressed(key, reinterpret_cast<const char *>(value), UINT_MAX, shift_keys, repeat, send_prefilter_action);
}

BOOL OpInputManager::InvokeKeyPressed(OpKey::Code key, const char *value, unsigned value_length, ShiftKeyState shift_keys, BOOL repeat, BOOL send_prefilter_action)
{
	OP_ASSERT(IS_OP_KEY(key) || !"Unmapped keycode, please check virtual keycode remapping.");
	if (!IS_OP_KEY(key))
		return TRUE;

	OpRandomGenerator::AddEntropyAllGenerators(&key, sizeof(key), 5);
	OpRandomGenerator::AddEntropyFromTimeAllGenerators();

	RegisterActivity();

	uni_char key_value = 0;

	OpInputAction::ActionMethod method	= GetActionMethodFromKey(key);
	OpInputContext*	input_context = GetInputContextFromKey(key);

	if (m_key_sequence_id > 0
#ifdef MOUSE_SUPPORT
	    || IsGestureRecognizing()
#endif // MOUSE_SUPPORT
		)
	{
		if (key == OP_KEY_ESCAPE)
		{
			m_key_sequence_id = 0;
#ifdef MOUSE_SUPPORT
			m_loaded_gesture_action = NULL;
			m_gesture_manager.CancelGesture();
#endif // MOUSE_SUPPORT
			return TRUE;
		}
	}
	else if (input_context && method == OpInputAction::METHOD_KEYBOARD)
	{
		// not sure how to handle this yet.. kludge for 7.0 final

#ifdef ACTION_CLOSE_CYCLER_ENABLED
		if (key == OP_KEY_ESCAPE && InvokeAction(OpInputAction::ACTION_CLOSE_CYCLER))
			return TRUE;
#endif // ACTION_CLOSE_CYCLER_ENABLED

		// send lowlevel key pressed for real keys

		if (!IS_OP_KEY_MODIFIER(key))
		{
			OpInputAction input_action(OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED, key, shift_keys, repeat, OpKey::LOCATION_STANDARD, method);
			if (value_length == UINT_MAX)
			{
				if (value && OpStatus::IsError(input_action.SetActionKeyValue(reinterpret_cast<const uni_char *>(value))))
					g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			}
			else if (value_length > 0 && OpStatus::IsError(input_action.SetActionKeyValue(value, value_length)))
				g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);

			if (input_action.GetKeyValue())
				key_value = *input_action.GetKeyValue();

			if (InvokeActionInternal(&input_action, input_context, input_context, send_prefilter_action))
			{
				return TRUE;
			}

			// reread input_context, if previous call caused focus to change

			input_context = GetInputContextFromKey(key);
		}
	}

	// don't translate modifier key presses to actions

	if (IS_OP_KEY_MODIFIER(key))
		return FALSE;

	BOOL incomplete_sequence_found = FALSE;

	m_key_sequence[m_key_sequence_id].Set(key, shift_keys, key_value);

	for (;input_context; input_context = input_context->GetParentInputContext())
	{
		ShortcutContext* shortcut_context = GetShortcutContextFromActionMethodAndName(method, input_context->GetInputContextName());

		if (!shortcut_context)
		{
			continue;
		}

		INT32 count = shortcut_context->GetShortcutActionCount();

		for (INT32 shortcut_action_pos = 0; shortcut_action_pos < count; shortcut_action_pos++)
		{
			ShortcutAction* shortcut_action = shortcut_context->GetShortcutActionFromIndex(shortcut_action_pos);

			// Check if the current key sequence is a possible match for this short cut. Skip otherwise.
			if (!shortcut_action->MatchesKeySequence(m_key_sequence, m_key_sequence_id))
				continue;

				// check if more keypresses are needed before action is invoked

				if (shortcut_action->GetShortcutCount() - 1 > m_key_sequence_id)
				{
					incomplete_sequence_found = TRUE;
					continue;
				}

				// if gesture recognizing, save action for later trigger

#ifdef MOUSE_SUPPORT
				if (IsGestureRecognizing())
				{
					m_gesture_recording_was_attempted = TRUE;
					m_loaded_gesture_action = shortcut_action->GetAction();
					if (m_key_sequence_id < MAX_SHORTCUT_SEQUENCE_LENGTH)
					{
						m_key_sequence_id++;
					}
					return TRUE;
				}
#endif // MOUSE_SUPPORT

			OpInputAction * input_action = shortcut_action->GetAction();

			// this will set the correct action states to all actions in the action list
			if (GetActionState(input_action, GetInputContextFromKey(key)) & OpInputAction::STATE_DISABLED)
			{
				while (	input_action )
				{
					INT32 state = input_action->GetActionState();

					// Only look for the next action if this one is specifically disabled
					if (!(state & OpInputAction::STATE_DISABLED))
						break;
					// all actions where disabled, don't invoke anything
					if (!input_action->GetNextInputAction())
						input_action = NULL;
					else
						input_action = input_action->GetNextInputAction();
				}
			}

			BOOL handled = FALSE;
			if (input_action)
				handled = InvokeAction(input_action, GetInputContextFromKey(key), input_context, send_prefilter_action, method);

			if (handled)
			{
				m_key_sequence_id = 0;
				return TRUE;
			}
		}
	}

	if (incomplete_sequence_found
#ifdef MOUSE_SUPPORT
	    || IsGestureRecognizing()
#endif // MOUSE_SUPPORT
	    )
	{
#ifdef MOUSE_SUPPORT
		m_loaded_gesture_action = NULL;
#endif // MOUSE_SUPPORT
		if (m_key_sequence_id < MAX_SHORTCUT_SEQUENCE_LENGTH)
		{
			m_key_sequence_id++;
		}
		return TRUE;
	}
	else
	{
#if !defined(IM_UI_HANDLES_SHORTCUTS)
		if (m_external_input_listener)
			m_external_input_listener->OnKeyPressed(key, shift_keys);
#endif // !IM_UI_HANDLES_SHORTCUTS
		m_key_sequence_id = 0;

		return FALSE;
	}
}

/***********************************************************************************
**
**	ResetInput
**
***********************************************************************************/

void OpInputManager::ResetInput()
{
	m_key_down_hashtable.RemoveAll();
	m_key_sequence_id = 0;
#ifdef MOUSE_SUPPORT
	m_loaded_gesture_action = NULL;
	m_gesture_manager.CancelGesture();
	m_is_flipping = FALSE;
	OpWidget::ZeroCapturedWidget();
	OpWidget::ZeroHoveredWidget();
#endif // MOUSE_SUPPORT
}

/***********************************************************************************
**
**	InvokeActionInternal
**
***********************************************************************************/

BOOL OpInputManager::InvokeActionInternal(OpInputAction* input_action, OpInputContext* first_input_context, OpInputContext* last_input_context, BOOL send_prefilter_action)
{
	if (!input_action)
		return FALSE;

	if (!first_input_context)
	{
		first_input_context = m_keyboard_input_context;

		if (!first_input_context)
			return FALSE;
	}

	input_action->SetFirstInputContext(first_input_context);

#ifdef MOUSE_SUPPORT
	if (input_action->IsMouseInvoked() && !input_action->IsGestureInvoked())
	{
		input_action->SetActionPosition(m_mouse_position);
	}
#endif // MOUSE_SUPPORT

	// send prefilter actions first

		if (send_prefilter_action)
		{
			OpInputAction pre_filter_action(input_action, OpInputAction::ACTION_LOWLEVEL_PREFILTER_ACTION);

			OpInputContext* next_input_context = first_input_context;

			while (next_input_context)
			{
				if (!next_input_context->IsInputDisabled() && next_input_context->OnInputAction(&pre_filter_action))
				{
					return TRUE;
				}

				next_input_context = next_input_context->GetParentInputContext();
			}
		}

	// usually start with first, but if OPERATOR_NEXT is used, search for first selected
	// item and start with the one after that

	OpInputAction* first_input_action = input_action;
	OpInputAction* next_input_action = first_input_action;

	while (next_input_action->GetActionOperator() == OpInputAction::OPERATOR_NEXT && next_input_action->GetNextInputAction())
	{
		INT32 action_state = GetActionState(next_input_action, first_input_context);

		next_input_action = next_input_action->GetNextInputAction();

		if (action_state & OpInputAction::STATE_SELECTED)
		{
			first_input_action = next_input_action;
			break;
		}
	}

	// restart

	next_input_action = first_input_action;

		// send one or more of the actions to first context and up to last one

	BOOL handled = FALSE;
	BOOL update_states =  FALSE;

	m_action_first_input_context = first_input_context;

	while (next_input_action)
	{
# ifdef ACTION_DELAY_ENABLED
		if(next_input_action->GetAction() == OpInputAction::ACTION_DELAY)
		{
			UINT32 delay = (UINT32)next_input_action->GetActionData();
			if(delay)
			{
				g_main_message_handler->PostDelayedMessage(MSG_IM_ACTION_DELAY, (MH_PARAM_1)this, (MH_PARAM_2)next_input_action->GetNextInputAction(), delay);
				next_input_action->SetNextInputAction(NULL);
				return TRUE;
			}
		}
# endif // ACTION_DELAY_ENABLED
#ifdef QUICK
		if( g_application )
		{
			if(g_application->KioskFilter(next_input_action))
				return TRUE;
		}
#endif

		OpInputContext* next_input_context = m_action_first_input_context;

		while (next_input_context)
		{
			if (next_input_action->GetActionMethod() != OpInputAction::METHOD_OTHER && next_input_context->IsInputDisabled())
			{
				break;
			}

			// kludge until we get enum return value from OnInputAction.. just don't bother to change all those now
			next_input_action->SetWasReallyHandled(TRUE);

			if (next_input_context->OnInputAction(next_input_action))
			{
				handled = TRUE;

				if (next_input_action->IsUpdateAction())
				{
					update_states = TRUE;
				}
				break;
			}

			if (next_input_context == last_input_context)
			{
				break;
			}

			next_input_context = next_input_context->GetParentInputContext();
		}

		switch (next_input_action->GetActionOperator())
		{
			case OpInputAction::OPERATOR_AND:
				next_input_action = next_input_action->GetNextInputAction();
				break;

			case OpInputAction::OPERATOR_OR:
			case OpInputAction::OPERATOR_NEXT:

				if (handled && next_input_action->WasReallyHandled())
				{
					next_input_action = NULL;
				}
				else
				{
					next_input_action = next_input_action->GetNextInputAction();

					handled = FALSE;

					if (!next_input_action)
						next_input_action = input_action;

					if (next_input_action == first_input_action)
						next_input_action = NULL;
				}
				break;

			default:
				next_input_action = NULL;
				break;
		}
	}

	if (update_states)
	{
		UpdateAllInputStates();
	}

	return handled;
}

/***********************************************************************************
**
**	GetShortcutStringFromAction
**
***********************************************************************************/

OP_STATUS OpInputManager::GetShortcutStringFromAction(OpInputAction* action, OpString& string, OpInputContext* input_context)
{
	if (action == NULL)
	{
		return OpStatus::ERR_NULL_POINTER;
	}

	if (input_context == NULL)
	{
		input_context = m_keyboard_input_context;
	}

	for (;input_context; input_context = input_context->GetParentInputContext())
	{
		ShortcutContext* shortcut_context = GetShortcutContextFromActionMethodAndName(OpInputAction::METHOD_KEYBOARD, input_context->GetInputContextName());

		if (!shortcut_context)
		{
			continue;
		}

		ShortcutAction* shortcut_action = shortcut_context->GetShortcutActionFromAction(action);

		if (shortcut_action)
		{
			INT32 shortcut_count = shortcut_action->GetShortcutCount();

			for (INT32 i = 0; i < shortcut_count; i++)
			{
				Shortcut* shortcut = shortcut_action->GetShortcutByIndex(i);

				RETURN_IF_ERROR(shortcut->GetShortcutString(string));

				if (i < shortcut_count - 1)
				{
					RETURN_IF_ERROR(string.Append(UNI_L(", ")));
				}
			}

			return OpStatus::OK;
		}
	}

	return OpStatus::OK;
}

/***********************************************************************************
**
**	GetTypedObject
**
***********************************************************************************/

OpTypedObject* OpInputManager::GetTypedObject(OpTypedObject::Type type, OpInputContext* input_context)
{
	OpInputAction get_typed_object_action(OpInputAction::ACTION_GET_TYPED_OBJECT);
	get_typed_object_action.SetActionData(type);

	InvokeActionInternal(&get_typed_object_action, input_context, NULL, FALSE);

	return get_typed_object_action.GetActionObject();
}

/***********************************************************************************
**
**	GetActionState
**
***********************************************************************************/

INT32 OpInputManager::GetActionState(OpInputAction* action, OpInputContext* first_input_context)
{
	if (first_input_context == NULL)
	{
		first_input_context = m_keyboard_input_context;
	}

	INT32 state = 0;

#ifdef QUICK
	if( g_application && action )
	{
		if(g_application->KioskFilter(action))
		{
			return action->GetActionState();
		}
	}
#endif

	for (;action; action = action->GetNextInputAction())
	{
		action->SetActionState(OpInputAction::STATE_ENABLED);

		for (OpInputContext* input_context = first_input_context; input_context; input_context = input_context->GetParentInputContext())
		{
			if (input_context->IsInputDisabled())
			{
				continue;
			}

			OpInputAction get_state_action(action, OpInputAction::ACTION_GET_ACTION_STATE);
			get_state_action.SetFirstInputContext(first_input_context);

			if (input_context->OnInputAction(&get_state_action))
			{
				state |= action->GetActionState();
				break;
			}
		}

		if (action->GetActionOperator() != OpInputAction::OPERATOR_AND)
			break;
	}

	return state;
}

/***********************************************************************************
**
**	GetShortcutContextListFromActionMethod
**
***********************************************************************************/

ShortcutContextList* OpInputManager::GetShortcutContextListFromActionMethod(OpInputAction::ActionMethod method)
{
	(void)method; //Yes compiler, we declare and possibly do not use this variable..

#ifdef MOUSE_SUPPORT
	if (method == OpInputAction::METHOD_MOUSE)
		return &m_mouse_shortcut_context_list;
#endif // MOUSE_SUPPORT

	return &m_keyboard_shortcut_context_list;
}

/***********************************************************************************
**
**	GetShortcutContextFromActionMethodAndName
**
***********************************************************************************/

ShortcutContext* OpInputManager::GetShortcutContextFromActionMethodAndName(OpInputAction::ActionMethod method, const char* context_name)
{
	if (!context_name)
		return NULL;

	PrefsSection* special_section = NULL;

#ifdef SPECIAL_INPUT_SECTIONS
	if (method == OpInputAction::METHOD_KEYBOARD)
	{
		m_special_inputsections.GetData(context_name, &special_section);

		if(special_section)
		{
			m_special_inputsections.Remove(context_name, &special_section);
		}
	}
#endif // SPECIAL_INPUT_SECTIONS

	return GetShortcutContextListFromActionMethod(method)->GetShortcutContextFromName(context_name, special_section);
}

/***********************************************************************************
**
**	AddInputState
**
***********************************************************************************/

void OpInputManager::AddInputState(OpInputState* input_state)
{
	if (!input_state->InList())
		input_state->Into(input_state->GetWantsOnlyFullUpdate() ? &m_full_update_input_states : &m_input_states);
}

/***********************************************************************************
**
**	RemoveInputState
**
***********************************************************************************/

void OpInputManager::RemoveInputState(OpInputState* input_state)
{
	if (input_state->InList())
		input_state->Out();
}

/***********************************************************************************
**
**	UpdateAllInputStates
**
***********************************************************************************/

void OpInputManager::UpdateAllInputStates(BOOL full_update)
{
	if (full_update)
		m_full_update = TRUE;

	if (m_update_message_pending)
		return;

	m_update_message_pending = TRUE;

	g_main_message_handler->PostMessage(MSG_UPDATE_ALL_INPUT_STATES, (MH_PARAM_1)this, 0);
}

/***********************************************************************************
**
**	SyncInputStates
**
***********************************************************************************/

void OpInputManager::SyncInputStates()
{
	if (!m_update_message_pending)
		return;

	OpInputState* input_state = NULL;
	OpInputState* next_input_state = NULL;

	INT32 count = 0;

	if (m_full_update)
	{
		input_state = (OpInputState*) m_full_update_input_states.First();

		while (input_state)
		{
			count++;
			next_input_state = (OpInputState*) input_state->Suc();
			input_state->Update(m_full_update);
			input_state = next_input_state;
		}
	}

	input_state = (OpInputState*) m_input_states.First();

	while (input_state)
	{
		count++;
		next_input_state = (OpInputState*) input_state->Suc();
		input_state->Update(m_full_update);
		input_state = next_input_state;
	}

	m_full_update			 = FALSE;
	m_update_message_pending = FALSE;
}

/***********************************************************************************
**
**	HandleCallback
**
***********************************************************************************/


void OpInputManager::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if(msg == MSG_UPDATE_ALL_INPUT_STATES)
	{
		SyncInputStates();
	}
	else if(msg == MSG_IM_ACTION_DELAY)
	{
		OpInputAction *action = (OpInputAction *)par2;
		if(action)
		{
			InvokeActionInternal(action);

			OP_DELETE(action);
		}
	}
}

/***********************************************************************************
**
**	GetActionFromString
**
***********************************************************************************/

OP_STATUS OpInputManager::GetActionFromString(const char* string, OpInputAction::Action* result)
{
	while (op_isspace(*string))
	{
		string++;
	}

	if (m_action_hashtable.GetCount() == 0)
	{
		for (INT32 i = OpInputAction::ACTION_UNKNOWN + 1; i < OpInputAction::LAST_ACTION; i++)
		{
			const char* action_string = OpInputAction::GetStringFromAction((OpInputAction::Action) i);
			if (action_string)
				RETURN_IF_MEMORY_ERROR(m_action_hashtable.Add(action_string, (OpInputAction::Action) i));
		}
	}

	OpInputAction::Action action = OpInputAction::ACTION_UNKNOWN;

	m_action_hashtable.GetAction(string, &action);

	*result = action;

	return OpStatus::OK;
}

OP_STATUS OpInputManager::OpStringToActionHashTable::GetAction(const char* key, OpInputAction::Action* action)
{
	HashItem item;

	RETURN_IF_ERROR(OpGenericString8HashTable::GetData(key, &item.value));
	*action = item.action;

	return OpStatus::OK;
}

OP_STATUS OpInputManager::OpStringToActionHashTable::Add(const char* key, OpInputAction::Action action)
{
	HashItem item;
	item.value = 0;
	item.action = action;
	return OpGenericString8HashTable::Add(key, item.value);
}
