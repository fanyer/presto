/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.	All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef DOM_EXTENSIONS_UIBADGE_H
#define DOM_EXTENSIONS_UIBADGE_H

#ifdef EXTENSION_SUPPORT

#include "modules/dom/src/domobj.h"

class DOM_ExtensionUIItem;

/** This class represents badge(label attached to the button)
 *  associated to a UIItem.
 *  \see DOM_ExtensionUIItem
 */
class DOM_ExtensionUIBadge
	: public DOM_Object
{
public:
	static OP_STATUS Make(DOM_Object *this_object, DOM_ExtensionUIBadge *&new_obj, ES_Object *properties, DOM_ExtensionUIItem *owner, ES_Value *return_value, DOM_Runtime *origining_runtime);

	virtual ~DOM_ExtensionUIBadge();

	/* from DOM_Object */
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object *restart_object);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_EXTENSION_UIBADGE || DOM_Object::IsA(type); }

	DOM_DECLARE_FUNCTION(update);
	enum
	{
		FUNCTIONS_update = 1,
		FUNCTIONS_ARRAY_SIZE
	};

private:
	friend class DOM_ExtensionUIItem;

	DOM_ExtensionUIBadge();

	OP_STATUS Initialize(DOM_Object *this_object, ES_Object *properties, DOM_ExtensionUIItem *owner, ES_Value *return_value, DOM_Runtime *origining_runtime);

	DOM_ExtensionUIItem *m_owner;
	/**< The UIItem that the badge belongs to. */

	const uni_char *m_text_content;
	BOOL m_displayed;
	UINT32 m_background_color;
	UINT32 m_color;
};

#endif // EXTENSION_SUPPORT
#endif // DOM_EXTENSIONS_UIBADGE_H
