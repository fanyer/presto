/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef UI_CONTEXT_H
#define UI_CONTEXT_H

#include "modules/inputmanager/inputcontext.h"
#include "modules/util/simset.h"
#include "modules/hardcore/mh/messobj.h"

/**
 * @brief A Context which handles the business logic for UI
 * @author Arjan van Leeuwen
 *
 * Declarative UI in Quick separates the business logic and model from the UI
 * explicitly by having the UI be defined in a set of YAML documents and having
 * the business logic and model be in C++.  The interface between these layers
 * is the "Context".
 *
 * One can think of this structure as an application of the Model-View-Controller
 * (MVC) pattern, where:
 *		@li The Model are domain classes like Account, SpeedDialData, etc.; but
 *			also, preferences.
 *		@li The View is composed of QuickWidgets, whose layout can be defined
 *			declaratively in YAML.  The QuickWidgets live in a QuickWindow.
 *		@li The Controller is the UiContext responsible for handling actions, and
 *			for configuring the bindings between the UI elements (QuickWidgets) and
 *			the Model.
 *
 * A QuickWindow is launched in a Context. The abstract idea here is that the Context
 * will own/provide the business objects needed for the UI, and it will handle the
 * business actions that are initiated in the UI.
 *
 * Then the business-logic-to-UI interface becomes a tree of Contexts, where actions
 * bubble up to the first context to handle it.
 *
 * At the very top there will be an application level Context, the "Global Context".
 * Beneath this will be perhaps a set of BrowserWindowContexts each handling the
 * business logic for one toplevel window. Each of these might have a set of Contexts
 * for currently open dialogs - DialogContext, and so on.
 *
 * All of the contexts are OpInputContexts and the bubbling of actions follow the
 * regular structure of the OpInputContext tree.
 *
 * In addtion to a pure each-window-has-its-context you will have feature specific
 * Contexts, for example M2 can provide a MailContext to which perhaps the Global
 * Context will delegate actions. This MailContext will then also own/administrate
 * the business objects that it chooses to expose.
 */
class UiContext
  : public OpInputContext
  , public ListElement<UiContext>
  , private MessageObject
{
public:

	virtual ~UiContext();

	/** @param action
	  * @return Whether action can ever be handled by this context
	  */
	virtual BOOL CanHandleAction(OpInputAction* action) { return FALSE; }

	/** @param action
	  * @return Whether action cannot be handled by this context in its current state
	  */
	virtual BOOL DisablesAction(OpInputAction* action) { return FALSE; }

	/** @param action
	  * @return Whether action is currently selected by this context in its current state
	  */
	virtual BOOL SelectsAction(OpInputAction* action) { return FALSE; }

	/** Handle the given action
	  * @param action Action to handle
	  */
	virtual OP_STATUS HandleAction(OpInputAction* action) { return OpStatus::ERR; }

	/** @param action
	 * @return Whether action should be canceled (meaning not invoked at all)
	 */
	virtual BOOL CancelsAction(OpInputAction* action) { return FALSE; }

	/** Add a UiContext as a child of this one.
	  * @param child Child to add - will take ownership
	  */
	void AddChildContext(UiContext* child);

	/** Tell the context that the UI has been created and it's about to be
	  * launched in the context.
	  */
	virtual void OnUiCreated() {}

	/** Tell the context that the UI launched in the context is closing.
	  */
	virtual void OnUiClosing();

	// Override OpInputContext
	virtual BOOL OnInputAction(OpInputAction* action);

private:
	BOOL OnActionGetState(OpInputAction* action);

	// Override MessageObject
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	AutoDeleteList<UiContext> m_children;
};


#endif // UI_CONTEXT_H
