// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#ifndef IM_OP_INPUTCONTEXT_H
#define IM_OP_INPUTCONTEXT_H

#include "modules/inputmanager/inputmanager.h"

#include "modules/hardcore/unicode/unicode.h"
#ifdef QUICK
# include "adjunct/desktop_util/treemodel/optreemodelitem.h"
#endif
#include "modules/pi/OpInputMethod.h"

class OpInputAction;

/**
 * @short Representents a context that is affected by input actions.
 *
 * An input context can be anything that can receive or in other ways
 * deal with input actions in the OpInputManager system. The contexts
 * are stored in a logical tree where typically the main application
 * action handler is the root and the leaves are low-level widgets.
 */
class OpInputContext
#ifdef QUICK
 : public OpTreeModelItem
#else // QUICK
 : public OpTypedObject
#endif // QUICK
{
	public:

								OpInputContext() : m_parent_context(NULL), m_recent_keyboard_child_context(NULL) {}
		virtual					~OpInputContext();


		/** OnIputAction is called to let the input context handle an action, and if so return TRUE. */
		virtual BOOL			OnInputAction(OpInputAction* /*action*/) {return FALSE;}

		/**
		 * Returns TRUE and sets the rect to the bounding rect for the inputcontext, relative to the view. Or FALSE if
		 * no special boundingrect is used. The bounding rect is used for selectionscroll.
		 */
		virtual BOOL			GetBoundingRect(OpRect &/*rect*/) { return FALSE; }

#ifdef WIDGETS_IME_SUPPORT
		/**
		 * Input Methods hooks. See OpInputMethod.h for details.
		 * Returnvalue should be the rectangle where the OpInputMethodString is rendered
		 * relative to the screen.
		 */
		virtual IM_WIDGETINFO OnStartComposing(OpInputMethodString* imstring, IM_COMPOSE compose);
		virtual IM_WIDGETINFO OnCompose();
		virtual void OnCommitResult() {}
		virtual void OnStopComposing(BOOL /*cancel*/) {}
		virtual void OnCandidateShow(BOOL /*visible*/) {}
		virtual IM_WIDGETINFO GetWidgetInfo();

#ifdef IME_RECONVERT_SUPPORT
		virtual void OnPrepareReconvert(const uni_char*& str, int& sel_start, int& sel_stop) {};
		virtual void OnReconvertRange(int sel_start, int sel_stop) {};
#endif
#endif

		// OnKeyboardInputGained and OnKeyboardInputLost are called when the current keyboard input context
		// changes.
		//
		// NOTE! OnKeyboardInputGained is called on every input context starting with the one receiving and
		// all the way up to the root input context.
		//
		// NOTE! OnKeyboardInputLost is called on every input context starting with the one losing and
		// all the way up to the root input context.
		//
		// The same goes for OnMouseInputGained and OnMouseInputLost

		virtual void			OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);
		virtual void			OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);

		virtual void			OnMouseInputGained(OpInputContext* /*new_input_context*/, OpInputContext* /*old_input_context*/) {}
		virtual void			OnMouseInputLost(OpInputContext* /*new_input_context*/, OpInputContext* /*old_input_context*/) {}

		// IsInputContextAvailable should return TRUE if this input context is considered to be available.
		//
		// NOTE! While an input context only answers for itself, the input manager will require that all
		// IsInputContextAvailable() == TRUE up to until IsParentInputContextAvailabilityRequired() == FALSE, otherwise the context cannot get focus

		virtual BOOL			IsInputContextAvailable(FOCUS_REASON /*reason*/) {return TRUE;}
		virtual BOOL			IsParentInputContextAvailabilityRequired() {return TRUE;}


		/**
		 * IsInputDisabled should return TRUE if the context shouldn't be able to act on actions translated from
		 * keyboard or mouse input. This is used for parents of modal dialogs. The dialog itself
		 * will be able to act on actions translated from a parent context, like the Application context, but
		 * not the parent of the dialog and upwards.
		 */
		virtual BOOL			IsInputDisabled() {return FALSE;}

		/** Sets the parent input context (used to create the whole input tree). Releases this as a side action.
		 * @see OpInputManager::ReleaseInputContext()
		 * @param parent_context the new parent_context
		 * @param keep_keyboard_focus when TRUE, then will keep the keyboard focus in the subtree of this.
		 */
		virtual void			SetParentInputContext(OpInputContext* parent_context, BOOL keep_keyboard_focus = FALSE);

		virtual OpInputContext*	GetParentInputContext() {return m_parent_context;}

		// Helper functions for find out if an input context is a child/parent of another

		virtual BOOL			IsParentInputContextOf(OpInputContext* child_context);
		virtual BOOL			IsChildInputContextOf(OpInputContext* parent_context);

		/**
		 * GetInputContextName should return the name of the input context. Used by input manager to
		 * look up input->action mappings etc.
		 */
		virtual const char*		GetInputContextName() {return NULL;}

		// SetRecentKeyboardChildInputContext and GetRecentKeyboardChildInputContext are used by
		// input manager to remember "previous" focused input context in this "sub tree" of input contexts

		virtual void			SetRecentKeyboardChildInputContext(OpInputContext* recent_keyboard_child_context) {m_recent_keyboard_child_context = recent_keyboard_child_context;}
		virtual OpInputContext*	GetRecentKeyboardChildInputContext() {return m_recent_keyboard_child_context;}

		/***********************************************************************************
		**
		**	We now define "Focus" as being the current keyboard input context
		**	We then offer these focus related functions to simplify focus code
		**
		***********************************************************************************/

		/**
		 * SetFocus sets focus to this input context
		 */
		virtual void			SetFocus(FOCUS_REASON reason);

		/**
		 * RestoreFocus sets back focus to the previously focused context in this sub tree of input
		 * contexts. If there is no previous one, focus this one.
		 */
		virtual void			RestoreFocus(FOCUS_REASON reason);

		/**
		 * ReleaseFocus is used when the input context cannot have focus anymore, for example when being made invisble or disabled
		 */
		virtual void			ReleaseFocus(FOCUS_REASON reason = FOCUS_REASON_RELEASE);

		/**
		 * IsFocused() checks if this input context is the currently focused one
		 * HasFocusedChildren = TRUE would also test if the input context has focused children
		 */
		BOOL					IsFocused(BOOL HasFocusedChildren = FALSE);

		/**
		 * OnFocus() is a hook that is called by the default implementation of OnKeyboardInputGained and OnKeyboardInputLost,
		 * and it is only called for the context that is actually losing or gaining focus.
		 */
		virtual	void			OnFocus(BOOL /*focus*/, FOCUS_REASON /*reason*/) {}

		/***********************************************************************************
		**
		**	Implementing OpTreeModelItem (and its superclass OpTypedObject)
		**
		***********************************************************************************/

		virtual Type			GetType() {return UNKNOWN_TYPE;}
		virtual INT32			GetID() {return 0;}
#ifdef QUICK
		virtual OP_STATUS		GetItemData(ItemData* item_data) {return OpStatus::OK;}
#endif // QUICK

	private:

		OpInputContext*			m_parent_context;
		OpInputContext*			m_recent_keyboard_child_context;
};

inline void OpInputContext::SetFocus(FOCUS_REASON reason)
{
	if (g_input_manager)
		g_input_manager->SetKeyboardInputContext(this, reason);
}

inline void OpInputContext::RestoreFocus(FOCUS_REASON reason)
{
	if (g_input_manager)
		g_input_manager->RestoreKeyboardInputContext(this, NULL, reason);
}

inline void OpInputContext::ReleaseFocus(FOCUS_REASON reason)
{
	if (g_input_manager)
		g_input_manager->ReleaseInputContext(this, reason);
}

#endif // !IM_OP_INPUTCONTEXT_H
