/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _DOM_WIDGET_H_
#define _DOM_WIDGET_H_

#ifdef GADGET_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/dom/domenvironment.h"
#include "modules/gadgets/OpGadgetClass.h"

class DOM_Widget
	: public DOM_Object, public DOM_EventTargetOwner
{
public:
	/**
	 * Create an object representing the Widget interface.
	 *
	 * @param[out] new_obj Where to create the object pointer.
	 * @param[in]  origining_runtime Runtime this instance lives in.
	 * @param[in]  gadget The widget this instance refers to.
	 * @param[in]  force_admin_privileges
	 *   This widget instance has access to administrative interfaces.
	 *   Unite application widgets can use this to be allowed changing
	 *   configuration of their shared folder.
	 */
	static OP_STATUS Make(DOM_Widget *&new_obj, DOM_Runtime *origining_runtime, OpGadget *gadget, BOOL force_admin_privileges = FALSE);
	virtual ~DOM_Widget();

	virtual BOOL IsA(int type) { return type == DOM_TYPE_WIDGET || DOM_Object::IsA(type); }
	virtual ES_GetState	GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual void GCTrace();

	virtual DOM_Object *GetOwnerObject() { return this; }
	OP_STATUS HandleEvent(DOM_Environment::GadgetEvent event, DOM_Environment::GadgetEventData *data = NULL);

	OpGadget *GetGadget() { return m_gadget; }

	OP_STATUS InjectWidgetManagerApi();

	// Functions
	DOM_DECLARE_FUNCTION(setPreferenceForKey);
	DOM_DECLARE_FUNCTION(preferenceForKey);
	DOM_DECLARE_FUNCTION(setGlobalPolicy);
	DOM_DECLARE_FUNCTION(prepareForTransition);
	DOM_DECLARE_FUNCTION(performTransition);
	DOM_DECLARE_FUNCTION(openURL);
	DOM_DECLARE_FUNCTION(getAttention);
	DOM_DECLARE_FUNCTION(showNotification);
	DOM_DECLARE_FUNCTION(show);
	DOM_DECLARE_FUNCTION(hide);
	DOM_DECLARE_FUNCTION(hasFeature);
	DOM_DECLARE_FUNCTION(setIcon);
#ifdef GADGET_BADGE_SUPPORT
	DOM_DECLARE_FUNCTION(setBadge);
#endif // GADGET_BADGE_SUPPORT
#ifdef SELFTEST
	DOM_DECLARE_FUNCTION(dumpWidget);					// for selftests
#endif // SELFTEST

	DOM_DECLARE_FUNCTION(setExtensionProperty);

private:
	enum
	{
		VIEW_HIDDEN = 0,
		VIEW_DEFAULT = 1,
		VIEW_WINDOW = 2,
		VIEW_FULLSCREEN = 3
	};

	DOM_Widget(OpGadget *gadget);
	OP_STATUS CreateIconsArray();

	ES_Object *m_dragstart_handler;
	ES_Object *m_dragstop_handler;
	ES_Object *m_show_handler;
	ES_Object *m_hide_handler;
	ES_Object *m_shownotification_handler;
	ES_Object *m_viewstate_handler;
	ES_Object *m_beforeupdate_handler;
	ES_Object *m_afterupdate_handler;
	ES_Object *m_current_icon;
	ES_Object *m_icons;

	void InjectOperaExtendedApiL();

	void InjectWidgetManagerApiL();

	virtual ES_GetState	GetNameW3C(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutNameW3C(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual ES_GetState	GetNameOperaExt(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutNameOperaExt(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
#ifdef WEBSERVER_SUPPORT
	virtual ES_GetState	GetNameWebserver(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutNameWebserver(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
#endif // WEBSERVER_SUPPORT

	GadgetStringAttribute GetGadgetStringPropForAtom(OpAtom property_name);

	ES_Object **GetHandlerVariableAddress(OpAtom property_name);

	OpGadget *m_gadget;
#ifdef WEBSERVER_SUPPORT
	BOOL m_admin_privileges;
#endif
};

class DOM_WidgetIcon
	: public DOM_Object, public DOM_EventTargetOwner
{
public:
	static OP_STATUS Make(DOM_WidgetIcon *&new_obj, DOM_Runtime *origining_runtime, INT32 w, INT32 h, const uni_char *src);
	virtual ~DOM_WidgetIcon();

	virtual BOOL IsA(int type) { return type == DOM_TYPE_WIDGETICON || DOM_Object::IsA(type); }
	virtual ES_GetState	GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual DOM_Object *GetOwnerObject() { return this; }

	enum {
		FUNCTIONS_WITH_DATA_addEventListenerNS = 1,
		FUNCTIONS_WITH_DATA_removeEventListenerNS,
		FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};

private:
	DOM_WidgetIcon(INT32 w, INT32 h);
	OP_STATUS Initialize(const uni_char *src);

	INT32 width;
	INT32 height;
	OpString src;
	BOOL active;
};

class DOM_WidgetModeChangeEvent
	: public DOM_Event
{
public:
	static  OP_STATUS Make(DOM_WidgetModeChangeEvent *&event, DOM_Object *target, const uni_char *widgetMode);
	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

private:
	OpString m_widgetMode;
};

class DOM_ResolutionEvent
	: public DOM_Event
{
public:
	static  OP_STATUS Make(DOM_ResolutionEvent *&event, DOM_Object *target, int width, int height);
	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

private:
	int m_width;
	int m_height;
};

#endif // GADGET_SUPPORT
#endif // _DOM_WIDGET_H_
