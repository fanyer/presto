/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#ifndef DOM_EXTENSIONS_MENUEVENT_H
#define DOM_EXTENSIONS_MENUEVENT_H

#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT

#include "modules/dom/src/domevents/domevent.h"

class OpDocumentContext;
class FramesDocument;
class HTML_Element;
class DOM_Node;
class DOM_MessagePort;
class DOM_ExtensionSupport;

class DOM_ExtensionMenuEvent_Base
	: public DOM_Event
{
public:
	~DOM_ExtensionMenuEvent_Base();

	/* From DOM_Object: */
	virtual ES_GetState GetName(OpAtom property_atom, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_atom, ES_Value* value, ES_Runtime* origining_runtime);

protected:
	DOM_ExtensionMenuEvent_Base(OpDocumentContext* document_context);

	OpDocumentContext* m_document_context;
};

class DOM_ExtensionMenuEvent_UserJS
	: public DOM_ExtensionMenuEvent_Base
{
public:
	/**
	 * Creates an event object for an extension menu click event for UserJS.
	 *
	 * @param new_event Set to newly created event object.
	 * @param context Document context in which the menu was triggered. NOT NULL.
	 * @param element Element HTML_Element for which the menu was triggered. NULL if it was not triggered in
	 *        any specific element or if the event should not have access to it.
	 * @param runtime Runtime in which the event will be constructed. NOT NULL.
	 * @return OK or ERR_NO_MEMORY
	 */
	static OP_STATUS Make(DOM_Event*& new_event, OpDocumentContext* context, HTML_Element* element, DOM_Runtime* runtime);

	/* From DOM_Object: */
	virtual ES_GetState GetName(OpAtom property_atom, ES_Value* value, ES_Runtime* origining_runtime);
	virtual void GCTrace();
private:
	DOM_ExtensionMenuEvent_UserJS(OpDocumentContext* document_context, DOM_Node* node);

	DOM_Node* m_node;
};

class DOM_ExtensionMenuEvent_Background
	: public DOM_ExtensionMenuEvent_Base
{
public:
	/**
	 * Creates an event object for an extension menu click event for Background process of extension.
	 *
	 * @param new_event Set to newly created event object.
	 * @param context Document context in which the menu was triggered. NOT NULL.
	 * @param document_url Url of a document for which the menu was triggered.
	 *        This doesn't have to be top level document if the menu was invoked for
	 *        example in an iframe.
	 * @param window_id Id of a window in which the context menu was invoked.
	 * @param extension_support Support object of the extension background which will receive the event.
	 * @param runtime Runtime in which the event will be constructed. NOT NULL.
	 * @return OK or ERR_NO_MEMORY.
	 */
	static OP_STATUS Make(DOM_Event*& new_event, OpDocumentContext* context, const uni_char* document_url, UINT32 window_id, DOM_ExtensionSupport* extension_support, DOM_Runtime* runtime);

	/* From DOM_Object: */
	virtual ES_GetState GetName(OpAtom property_atom, ES_Value* value, ES_Runtime* origining_runtime);

	virtual void GCTrace();
private:
	DOM_ExtensionMenuEvent_Background(OpDocumentContext* document_context, UINT32 window_id);

	BOOL HasAccessToWindow(Window* window);

	UINT32 m_window_id;
	OpString m_document_url;
	OpString m_page_url;
	DOM_MessagePort* m_source;
};

#endif // DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT

#endif // DOM_EXTENSIONS_MENUEVENT_H
