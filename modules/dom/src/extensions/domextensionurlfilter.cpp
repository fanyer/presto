/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef EXTENSION_SUPPORT

#ifdef URL_FILTER
#include "modules/dom/src/extensions/domextensionurlfilter.h"
#include "modules/content_filter/content_filter.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/dom/src/domcore/node.h"


DOM_ExtensionURLFilter::DOM_ExtensionURLFilter()
	: m_owner(NULL),
	  m_block(NULL),
	  m_allow(NULL),
	  m_allow_rules(FALSE),
	  m_allow_events(FALSE)
{ }

/* static */ OP_STATUS
DOM_ExtensionURLFilter::Make(DOM_ExtensionURLFilter *&new_obj, OpGadget *extension_owner, DOM_Runtime *runtime, BOOL allow_rules, BOOL allow_events)
{
	// Enabled only on extensions with feature "opera:urlfilter"
	new_obj = OP_NEW(DOM_ExtensionURLFilter, ());

	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj, runtime, runtime->GetPrototype(DOM_Runtime::EXTENSION_URLFILTER_PROTOTYPE), "URLFilter"));

	new_obj->m_owner = extension_owner;
	new_obj->m_allow_rules = allow_rules;
	new_obj->m_allow_events = allow_events;

	if (new_obj->m_allow_rules)
	{
		// Create the block list
		RETURN_IF_ERROR(DOM_ExtensionRuleList::Make(new_obj->m_block, runtime, extension_owner, TRUE));
		// Create the allow list
		RETURN_IF_ERROR(DOM_ExtensionRuleList::Make(new_obj->m_allow, runtime, extension_owner, FALSE));
	}

	return OpStatus::OK;
}

void DOM_ExtensionURLFilter::ConstructExtensionURLFilterL(ES_Object *object, DOM_Runtime *runtime)
{
	PutNumericConstantL(object, "RESOURCE_OTHER", RESOURCE_OTHER, runtime);
	PutNumericConstantL(object, "RESOURCE_SCRIPT", RESOURCE_SCRIPT, runtime);
	PutNumericConstantL(object, "RESOURCE_IMAGE", RESOURCE_IMAGE, runtime);
	PutNumericConstantL(object, "RESOURCE_STYLESHEET", RESOURCE_STYLESHEET, runtime);
	PutNumericConstantL(object, "RESOURCE_OBJECT", RESOURCE_OBJECT, runtime);
	PutNumericConstantL(object, "RESOURCE_SUBDOCUMENT", RESOURCE_SUBDOCUMENT, runtime);
	PutNumericConstantL(object, "RESOURCE_DOCUMENT", RESOURCE_DOCUMENT, runtime);
	PutNumericConstantL(object, "RESOURCE_REFRESH", RESOURCE_REFRESH, runtime);
	PutNumericConstantL(object, "RESOURCE_XMLHTTPREQUEST", RESOURCE_XMLHTTPREQUEST, runtime);
	PutNumericConstantL(object, "RESOURCE_OBJECT_SUBREQUEST", RESOURCE_OBJECT_SUBREQUEST, runtime);
	PutNumericConstantL(object, "RESOURCE_MEDIA", RESOURCE_MEDIA, runtime);
	PutNumericConstantL(object, "RESOURCE_FONT", RESOURCE_FONT, runtime);
}

/* virtual */ ES_GetState
DOM_ExtensionURLFilter::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_block:
	{
		if (!m_allow_rules)
			return DOM_Object::GetName(property_name, value, origining_runtime);

		DOMSetObject(value, m_block);
		return GET_SUCCESS;
	}

	case OP_ATOM_allow:
	{
		if (!m_allow_rules)
			return DOM_Object::GetName(property_name, value, origining_runtime);

		DOMSetObject(value, m_allow);
		return GET_SUCCESS;
	}

	default:
		return DOM_Object::GetName(property_name, value, origining_runtime);
	}
}

void DOM_ExtensionURLFilterEventTarget::AddListener(DOM_EventListener *listener)
{
	DOM_EventTarget::AddListener(listener);

	if ( ( listener->HandlesEvent(ONCONTENTBLOCKED, NULL, ES_PHASE_AT_TARGET)
		|| listener->HandlesEvent(ONCONTENTUNBLOCKED, NULL, ES_PHASE_AT_TARGET)
#ifdef SELFTEST
		|| listener->HandlesEvent(ONCONTENTALLOWED, NULL, ES_PHASE_AT_TARGET)
#endif // SELFTEST
		) && g_urlfilter)
		OpStatus::Ignore(g_urlfilter->AddExtensionListener(GetDOMExtensionURLFilter()->GetExtensionGadget(), GetDOMExtensionURLFilter()));
}

void DOM_ExtensionURLFilterEventTarget::RemoveListener(DOM_EventListener *listener)
{
	BOOL maybe_remove_listener = HasListeners(ONCONTENTBLOCKED, NULL, ES_PHASE_AT_TARGET)
		|| HasListeners(ONCONTENTUNBLOCKED, NULL, ES_PHASE_AT_TARGET)
#ifdef SELFTEST
		|| HasListeners(ONCONTENTALLOWED, NULL, ES_PHASE_AT_TARGET)
#endif // SELFTEST
		;

	DOM_EventTarget::RemoveListener(listener);

	BOOL has_listeners = HasListeners(ONCONTENTBLOCKED, NULL, ES_PHASE_AT_TARGET)
		|| HasListeners(ONCONTENTUNBLOCKED, NULL, ES_PHASE_AT_TARGET)
#ifdef SELFTEST
		|| HasListeners(ONCONTENTALLOWED, NULL, ES_PHASE_AT_TARGET)
#endif // SELFTEST
		;

	if (maybe_remove_listener && !has_listeners && g_urlfilter)
		g_urlfilter->RemoveExtensionListener(GetDOMExtensionURLFilter()->GetExtensionGadget(), GetDOMExtensionURLFilter());
}

/* virtual */ OP_STATUS
DOM_ExtensionURLFilter::CreateEventTarget()
{
	if (!event_target)
		RETURN_OOM_IF_NULL(event_target = OP_NEW(DOM_ExtensionURLFilterEventTarget, (this)));

	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_ExtensionURLFilter::GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	ES_GetState result = GetEventProperty(property_name, value, static_cast<DOM_Runtime *>(origining_runtime));
	if (result != GET_FAILED)
		return result;

	return DOM_Object::GetName(property_name, property_code, value, origining_runtime);
}



/* virtual */ ES_PutState
DOM_ExtensionURLFilter::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_block:
		return PUT_READ_ONLY;

	case OP_ATOM_allow:
		return PUT_READ_ONLY;

	default:
		return DOM_Object::PutName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_ExtensionURLFilter::PutName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime)
{
	return DOM_Object::PutName(property_name, property_code, value, origining_runtime);
}

/* virtual */ void
DOM_ExtensionURLFilter::GCTrace()
{
	GCMark(m_block);
	GCMark(m_allow);
	GCMark(event_target);
}

/* static */ OP_STATUS
DOM_ExtensionRuleList::Make(DOM_ExtensionRuleList *&new_obj, DOM_Runtime *runtime, OpGadget *ext_ptr, BOOL exclude)
{
	new_obj = OP_NEW(DOM_ExtensionRuleList, (ext_ptr, exclude));

	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj, runtime, runtime->GetPrototype(DOM_Runtime::EXTENSION_RULELIST_PROTOTYPE), "RuleList"));

	return OpStatus::OK;
}

OP_STATUS DOM_ExtensionRuleList::DOMGetDictionaryStringArray(ES_Object *dictionary, const uni_char *name, OpVector<uni_char> *vector)
{
	OP_ASSERT(vector);

	ES_Value value;
	OP_BOOLEAN result = GetRuntime()->GetName(dictionary, name, &value);

	if (result == OpBoolean::IS_TRUE)
	{
		if (value.type == VALUE_STRING)		// The value is a single string
			return vector->Add(const_cast<uni_char *> (value.value.string));
		else if (value.type == VALUE_OBJECT) // The should be an array of strings
		{
			ES_Object *obj = value.value.object;
			unsigned int len = 0;

			if (DOMGetArrayLength(obj, len))
			{
				for (unsigned int i = 0; i < len; i++)
				{
					ES_Value array_value;

					if (GetRuntime()->GetIndex(obj, i, &array_value) == OpBoolean::IS_TRUE)
					{
						RETURN_IF_ERROR(vector->Add(const_cast<uni_char *> (array_value.value.string)));
					}
					else
						return OpStatus::ERR_NOT_SUPPORTED;
				}

				return OpStatus::OK;
			}
		}

		return OpStatus::ERR_NOT_SUPPORTED;
	}
	else
		return OpStatus::OK;
}

/* static */ int
DOM_ExtensionRuleList::add(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	OP_STATUS status = OpStatus::OK;

	if (g_urlfilter)
	{
		DOM_THIS_OBJECT(rule_list, DOM_TYPE_EXTENSION_RULELIST, DOM_ExtensionRuleList);
		DOM_CHECK_ARGUMENTS("s|o");

		FilterURLnode *node = OP_NEW(FilterURLnode, ());

		if (!node)
			return ES_NO_MEMORY;

		node->SetIsExclude(rule_list->m_exclude);

		// Add all the rules specified by the user
		if (OpStatus::IsSuccess(status = node->SetURL(argv[0].value.string)))
		{
			BOOL node_acquired;

			ES_Object *options = (argc >= 2 && argv[1].type == VALUE_OBJECT) ? argv[1].value.object : NULL;

			if (options)
			{
				OpVector<uni_char> domains;
				OpVector<uni_char> exclude_domains;
				UINT32 resources = RESOURCE_UNKNOWN;
				BOOL third_party = FALSE;
				BOOL not_third_party = FALSE;
				ES_Value value;
				OP_BOOLEAN third_party_present = rule_list->GetRuntime()->GetName(options, UNI_L("thirdParty"), &value);
				OP_BOOLEAN domains_present = rule_list->GetRuntime()->GetName(options, UNI_L("includeDomains"), NULL);
				OP_BOOLEAN exclude_domains_present = rule_list->GetRuntime()->GetName(options, UNI_L("excludeDomains"), NULL);

				// We support 3 values:
				// third_party == TRUE: the url should be from a third party site
				// third_party == FALSE: the url should not be from a third party site
				// third_party not specified: urls from both third party and not third party servers are acceptable
				if (third_party_present == OpBoolean::IS_TRUE)
				{
					if (value.type != VALUE_NULL && value.type != VALUE_UNDEFINED)
					{
						if (value.value.boolean)
							third_party = TRUE;
						else
							not_third_party = TRUE;
					}
				}

				OP_STATUS ops;

				if (OpStatus::IsError(ops = rule_list->DOMGetDictionaryStringArray(options, UNI_L("includeDomains"), &domains)) ||
					OpStatus::IsError(ops = rule_list->DOMGetDictionaryStringArray(options, UNI_L("excludeDomains"), &exclude_domains))
					)
				{
					if (ops == OpStatus::ERR_NOT_SUPPORTED)
						return DOM_CALL_INTERNALEXCEPTION(WRONG_ARGUMENTS_ERR);

					CALL_FAILED_IF_ERROR(ops);
				}

				double resources_dbl = rule_list->DOMGetDictionaryNumber(options, UNI_L("resources"), 0.0);

				if (resources_dbl != 0.0)
					resources = TruncateDoubleToInt(resources_dbl + 0.5);

				if (third_party)
					status = node->AddRuleThirdParty();
				else if (not_third_party && OpStatus::IsSuccess(status))
					status = node->AddRuleNotThirdParty();
				if (domains_present == OpBoolean::IS_TRUE && OpStatus::IsSuccess(status))
					status = node->AddRuleInDomains(domains);
				if (exclude_domains_present == OpBoolean::IS_TRUE && OpStatus::IsSuccess(status))
					status = node->AddRuleNotInDomains(exclude_domains);
				if (resources != 0 && OpStatus::IsSuccess(status))
					status = node->AddRuleResources(resources);

				CALL_FAILED_IF_ERROR(status);
			}

			if (OpStatus::IsSuccess(status = g_urlfilter->AddURL(node, rule_list->m_ext_ptr, &node_acquired)))
			{
				if (!node_acquired)
					OP_DELETE(node);  // Duplicated node

				return ES_FAILED;
			}
		}

		OP_DELETE(node);
	}

	CALL_FAILED_IF_ERROR(status);

	return ES_FAILED;
}

/* static */ int
DOM_ExtensionRuleList::remove(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	if (g_urlfilter)
	{
		DOM_THIS_OBJECT(rule_list, DOM_TYPE_EXTENSION_RULELIST, DOM_ExtensionRuleList);
		DOM_CHECK_ARGUMENTS("s");

		FilterURLnode node;

		if (OpStatus::IsSuccess(node.SetURL(argv[0].value.string)))
		{
			g_urlfilter->DeleteURL(&node, rule_list->m_exclude, rule_list->m_ext_ptr);

			return ES_FAILED;
		}
	}

	return ES_FAILED;
}

/* static */ int
DOM_ExtensionURLFilter::accessEventListener(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(url_filter, DOM_TYPE_EXTENSION_URLFILTER, DOM_ExtensionURLFilter);

	if (!url_filter->m_allow_events)
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);

	return DOM_Node::accessEventListener(url_filter, argv, argc, return_value, origining_runtime, data);
}

DOM_ExtensionURLFilter::~DOM_ExtensionURLFilter()
{
	if (g_urlfilter)
		g_urlfilter->RemoveExtensionListener(m_owner, this);
}

OP_STATUS DOM_ExtensionURLFilter::SendEvent(ExtensionList::EventType evt_type, const uni_char* url, OpWindowCommander* wic, DOMLoadContext *dom_ctx)
{
	DOM_ExtensionURLFilterEvent *evt = OP_NEW(DOM_ExtensionURLFilterEvent, ());
	DOM_Runtime *runtime = GetRuntime();

	RETURN_IF_ERROR(DOMSetObjectRuntime(evt, runtime, runtime->GetPrototype(DOM_Runtime::EXTENSION_URLFILTER_EVENT_PROTOTYPE), "URLFilterEvent"));

	ES_Value value;
	DOM_EventType dom_evt_type = ONCONTENTBLOCKED;

	if (evt_type == ExtensionList::BLOCKED)
		dom_evt_type = ONCONTENTBLOCKED;
	else if (evt_type == ExtensionList::UNBLOCKED)
		dom_evt_type = ONCONTENTUNBLOCKED;
#ifdef SELFTEST
	else if (evt_type == ExtensionList::ALLOWED)
		dom_evt_type = ONCONTENTALLOWED;
#endif // SELFTEST


	ES_Value value_url;

	DOMSetString(&value_url, url);
	evt->PutL("url", value_url);

	if (dom_ctx)
	{
		DOM_Object *elem;

		if (OpStatus::IsSuccess(dom_ctx->GetDOMObject(elem, m_owner)))
		{
			ES_Value value_element;

			DOMSetObject(&value_element, elem);
			evt->PutL("element", value_element);
		}
	}

	evt->InitEvent(dom_evt_type, this);

	evt->SetCurrentTarget(this);
	evt->SetEventPhase(ES_PHASE_ANY);

	return runtime->GetEnvironment()->SendEvent(evt);
}

OP_STATUS DOMLoadContext::GetDOMObject(DOM_Object *&obj, OpGadget *owner)
{
	if (!element || !env)
		return OpStatus::ERR_NULL_POINTER;

	RETURN_IF_ERROR(env->ConstructNode(obj, element));

	return OpStatus::OK;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_WITH_DATA_START(DOM_ExtensionURLFilter)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_ExtensionURLFilter, DOM_ExtensionURLFilter::accessEventListener, 0, "addEventListener", "s-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_ExtensionURLFilter, DOM_ExtensionURLFilter::accessEventListener, 1, "removeEventListener", "s-b-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_ExtensionURLFilter)

DOM_FUNCTIONS_START(DOM_ExtensionRuleList)
	DOM_FUNCTIONS_FUNCTION(DOM_ExtensionRuleList, DOM_ExtensionRuleList::add, "add", "s?{includeDomains:(s|[s]),excludeDomains:(s|[s]),thirdParty:b,resources:n}-")
	DOM_FUNCTIONS_FUNCTION(DOM_ExtensionRuleList, DOM_ExtensionRuleList::remove, "remove", "s-")
DOM_FUNCTIONS_END(DOM_ExtensionRuleList)
#endif // URL_FILTER
#endif // EXTENSION_SUPPORT
