/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domcss/cssmediarule.h"
#include "modules/dom/src/domcss/cssstylesheet.h"
#include "modules/dom/src/domcss/cssrulelist.h"
#include "modules/dom/src/domstylesheets/medialist.h"

#include "modules/style/css_dom.h"

DOM_CSSMediaRule::DOM_CSSMediaRule(DOM_CSSStyleSheet* sheet, CSS_DOMRule* css_rule)
	: DOM_CSSConditionalRule(sheet, css_rule)
{
}

/* static */ OP_STATUS
DOM_CSSMediaRule::Make(DOM_CSSMediaRule*& rule, DOM_CSSStyleSheet* sheet, CSS_DOMRule* css_rule)
{
	DOM_Runtime *runtime = sheet->GetRuntime();
	return DOMSetObjectRuntime(rule = OP_NEW(DOM_CSSMediaRule, (sheet, css_rule)), runtime, runtime->GetPrototype(DOM_Runtime::CSSMEDIARULE_PROTOTYPE), "CSSMediaRule");
}

/* virtual */ ES_GetState
DOM_CSSMediaRule::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_media)
	{
		if (value)
		{
			ES_GetState result = DOMSetPrivate(value, DOM_PRIVATE_media);
			if (result != GET_FAILED)
				return result;

			DOM_MediaList *medialist;

			GET_FAILED_IF_ERROR(DOM_MediaList::Make(medialist, this));
			GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_media, *medialist));

			DOMSetObject(value, medialist);
		}
		return GET_SUCCESS;
	}
	else
		return DOM_CSSConditionalRule::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_CSSMediaRule::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_media)
		return PUT_READ_ONLY;
	else
		return DOM_CSSConditionalRule::PutName(property_name, value, origining_runtime);
}
