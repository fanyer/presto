/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/scope/src/scope_utils.h"

OP_STATUS
GetHTMLElement(HTML_Element **html_elm, ES_Object *es_object)
{
	EcmaScript_Object *host_object = ES_Runtime::GetHostObject(es_object);

	if (host_object == 0 || !host_object->IsA(ES_HOSTOBJECT_DOM))
		return OpStatus::ERR;

	*html_elm = DOM_Utils::GetHTML_Element((DOM_Object*)host_object);

	if (!*html_elm)
		return OpStatus::ERR;

	return OpStatus::OK;
}
