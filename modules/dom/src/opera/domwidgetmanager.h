/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#if !defined(DOM_WIDGETMANAGER_H) && (defined(DOM_WIDGETMANAGER_SUPPORT) || defined(DOM_UNITEAPPMANAGER_SUPPORT) || defined(DOM_EXTENSIONMANAGER_SUPPORT))
#define DOM_WIDGETMANAGER_H

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/dom/domenvironment.h"
#include "modules/gadgets/OpGadgetManager.h"

class DOM_WidgetCollection;

class DOM_WidgetManager
	: public DOM_Object
	, public OpGadgetInstallListener
{
public:
	static OP_STATUS Make(DOM_WidgetManager *&new_obj, DOM_Runtime *origining_runtime, BOOL unite=FALSE, BOOL extensions=FALSE);
	virtual ~DOM_WidgetManager();

	virtual BOOL IsA(int type) { return type == DOM_TYPE_WIDGETMANAGER || DOM_Object::IsA(type); }
	virtual ES_GetState	GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual void GCTrace();

	// Functions
	DOM_DECLARE_FUNCTION(install);
	DOM_DECLARE_FUNCTION(uninstall);
	DOM_DECLARE_FUNCTION(run);
	DOM_DECLARE_FUNCTION(kill);
	DOM_DECLARE_FUNCTION(checkForUpdate);
	DOM_DECLARE_FUNCTION(checkForUpdateByUrl);
	DOM_DECLARE_FUNCTION(getWidgetIconURL);
#ifdef EXTENSION_SUPPORT
	DOM_DECLARE_FUNCTION(options);
#endif // EXTENSION_SUPPORT
	DOM_DECLARE_FUNCTION(setWidgetMode);

	enum {
		FUNCTIONS_install = 1,
		FUNCTIONS_uninstall,
		FUNCTIONS_run,
		FUNCTIONS_kill,
		FUNCTIONS_checkForUpdate,
		FUNCTIONS_checkForUpdateByUrl,
		FUNCTIONS_getWidgetIconURL,
#ifdef EXTENSION_SUPPORT
		FUNCTIONS_options,
#endif // EXTENSION_SUPPORT
		FUNCTIONS_setWidgetMode,
		FUNCTIONS_ARRAY_SIZE
	};

#ifdef WEBSERVER_SUPPORT
	inline BOOL IsUnite() { return m_isUnite; }
#endif

protected:
	void HandleGadgetInstallEvent(GadgetInstallEvent &evt);
#ifdef WEBSERVER_SUPPORT
	BOOL m_isUnite;
#endif
#ifdef EXTENSION_SUPPORT
	BOOL m_isExtensions;
#endif

private:
	DOM_WidgetManager();
	OP_STATUS Initialize(BOOL unite);
	static OpGadget* GadgetArgument(ES_Value *argv, int argc, int n);

	OpVector<ES_Object> m_callbacks;
};

class DOM_WidgetCollection
	: public DOM_Object
{
public:
	static OP_STATUS Make(DOM_WidgetCollection *&new_obj, DOM_Runtime *origining_runtime, BOOL unite=FALSE, BOOL extensions=FALSE);
	virtual ~DOM_WidgetCollection();

	virtual BOOL IsA(int type) { return type == DOM_TYPE_WIDGETCOLLECTION || DOM_Object::IsA(type); }
	virtual ES_GetState	GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);

private:
	DOM_WidgetCollection(BOOL unite, BOOL extensions);
#if !defined WEBSERVER_SUPPORT && !defined EXTENSION_SUPPORT
	BOOL IncludeWidget(OpGadget *gadget) { return TRUE; }
#else
	BOOL IncludeWidget(OpGadget *gadget);
#endif

#ifdef WEBSERVER_SUPPORT
	BOOL m_isUniteCollection;
#endif
#ifdef EXTENSION_SUPPORT
	BOOL m_isExtensionCollection;
#endif
};


#endif // !DOM_WIDGETMANAGER_H && (DOM_WIDGETMANAGER_SUPPORT || DOM_UNITEAPPMANAGER_SUPPORT || DOM_EXTENSIONMANAGER_SUPPORT)
