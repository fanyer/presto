/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.	All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef DOM_EXTENSIONS_UIPOPUP_H
#define DOM_EXTENSIONS_UIPOPUP_H

#ifdef EXTENSION_SUPPORT

#include "modules/dom/src/domobj.h"

class DOM_ExtensionUIItem;

/** This class represents popup associated to a UIItem.
 *  \see DOM_ExtensionUIItem
 */
class DOM_ExtensionUIPopup
	: public DOM_Object
{
public:
	static OP_STATUS Make(DOM_ExtensionUIPopup *&new_obj, ES_Object *properties, DOM_ExtensionUIItem *owner, ES_Value *return_value, DOM_Runtime *origining_runtime);

	virtual ~DOM_ExtensionUIPopup();

	/* from DOM_Object */
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object *restart_object);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_EXTENSION_UIPOPUP || DOM_Object::IsA(type); }

	DOM_DECLARE_FUNCTION(update);

	enum
	{
		FUNCTIONS_update = 1,
		FUNCTIONS_ARRAY_SIZE
	};

private:
	friend class DOM_ExtensionUIItem;
	DOM_ExtensionUIPopup();
	OP_STATUS Initialize(ES_Object *properties, DOM_ExtensionUIItem *owner, ES_Value *return_value, DOM_Runtime *origining_runtime);

	OP_STATUS ResolvePopupURL(const uni_char *popup_href_string);
	/**< Verified and translate the value given for popup's .href field into
	     an appropriate URL for its popup window. If successful, updates the
	     object's '.href' field. */

	DOM_ExtensionUIItem *m_owner;
	/**< The UIItem that the popup belongs to. */

	const uni_char *m_href;
	unsigned int m_window_id;
	unsigned int m_width;
	unsigned int m_height;
};

#endif // EXTENSION_SUPPORT
#endif // DOM_EXTENSIONS_UIPOPUP_H
