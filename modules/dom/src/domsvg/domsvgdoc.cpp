/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef SVG_DOM
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domsvg/domsvgdoc.h"

#include "modules/doc/frm_doc.h"
#include "modules/logdoc/htm_elm.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"

DOM_SVGDocument::DOM_SVGDocument(DOM_DOMImplementation *implementation)
	: DOM_Document(implementation, TRUE)
{
}

/* status */ OP_STATUS
DOM_SVGDocument::Make(DOM_SVGDocument *&document, DOM_DOMImplementation *implementation)
{
	DOM_Runtime *runtime = implementation->GetRuntime();
	return DOMSetObjectRuntime(document = OP_NEW(DOM_SVGDocument, (implementation)), runtime, runtime->GetPrototype(DOM_Runtime::SVGDOCUMENT_PROTOTYPE), "SVGDocument");
}

HTML_Element *
DOM_SVGDocument::GetElement(Markup::Type type)
{
	HTML_Element *element = GetPlaceholderElement();

	while (element)
		if (element->IsMatchingType(type, NS_SVG))
			break;
		else
			element = element->NextActual();

	return element;
}

/* virtual */ ES_GetState
DOM_SVGDocument::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
#ifdef SVG_FULL_11
	DOM_EnvironmentImpl *environment = GetEnvironment();
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	FramesDocument *frames_doc = environment->GetFramesDocument();
#endif // SVG_FULL_11

	switch(property_name)
	{
#ifdef SVG_FULL_11
	case OP_ATOM_title:
		{
			TempBuffer *buffer = GetEmptyTempBuf();

			if (IsMainDocument())
				if (FramesDocument *frames_doc = GetFramesDocument())
				{
					DOMSetString(value, frames_doc->Title(buffer));
					return GET_SUCCESS;
				}

			if (HTML_Element *element = GetElement(Markup::SVGE_TITLE))
			{
				GET_FAILED_IF_ERROR(buffer->Expand(element->GetTextContentLength() + 1));
				element->GetTextContent(buffer->GetStorage(), buffer->GetCapacity());
			}

			DOMSetString(value, buffer);
			return GET_SUCCESS;
		}

	case OP_ATOM_referrer:
		DOMSetString(value);
		if (frames_doc)
			if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::ReferrerEnabled, GetHostName()))
			{
				URL ref_url = frames_doc->GetRefURL().url;

				switch (ref_url.Type())
				{
				case URL_HTTPS:
					if (!OriginCheck(ref_url, frames_doc->GetURL()))
						break;

				case URL_HTTP:
					DOMSetString(value, ref_url.GetAttribute(URL::KUniName).CStr());
				}
			}
		return GET_SUCCESS;

	case OP_ATOM_domain:
		if (value)
		{
			DOMSetString(value);
			if (!runtime->GetMutableOrigin()->IsUniqueOrigin())
			{
				const uni_char *domain;
				runtime->GetDomain(&domain, NULL, NULL);
				DOMSetString(value, domain);
			}
		}
		return GET_SUCCESS;

	case OP_ATOM_URL:
		if (value)
		{
			const uni_char *url;

			if (frames_doc)
				url = frames_doc->GetURL().GetAttribute(URL::KUniName_With_Fragment).CStr();
			else
				url = NULL;

			DOMSetString(value, url);
		}
		return GET_SUCCESS;

	case OP_ATOM_rootElement:
		DOMSetElement(value, GetElement(Markup::SVGE_SVG));
		return GET_SUCCESS;
#endif // SVG_FULL_11
	}

	return DOM_Document::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_SVGDocument::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch(property_name)
	{
#ifdef SVG_FULL_11
	case OP_ATOM_title:
	case OP_ATOM_domain:
	case OP_ATOM_referrer:
	case OP_ATOM_URL:
	case OP_ATOM_rootElement:
		return PUT_READ_ONLY;
#endif // SVG_FULL_11
	}

	return DOM_Document::PutName(property_name, value, origining_runtime);
}

#endif // SVG_DOM
