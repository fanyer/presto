/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef EXTENSION_SUPPORT
#include "modules/dom/src/extensions/domextensioncontext.h"
#include "modules/dom/src/extensions/domextensionuiitem.h"
#include "modules/dom/src/extensions/domextensionuiitemevent.h"
#include "modules/dom/src/extensions/domextensionuipopup.h"
#include "modules/dom/src/extensions/domextensionuibadge.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/doc/frm_doc.h"
#include "modules/gadgets/OpGadgetManager.h"


/* static */ OP_STATUS
DOM_ExtensionContext::Make(DOM_ExtensionContext *&new_obj, DOM_ExtensionSupport *support, DOM_ExtensionContexts::ContextType type, DOM_Runtime *origining_runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj = OP_NEW(DOM_ExtensionContext, (support, static_cast<unsigned int>(type))), origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::EXTENSION_CONTEXT_PROTOTYPE), "Context"));

	if (type == DOM_ExtensionContexts::CONTEXT_TOOLBAR)
		new_obj->m_max_items = 1;

	return OpStatus::OK;
}

void
DOM_ExtensionContext::ExtensionUIListenerCallbacks::ItemAdded(OpExtensionUIListener::ExtensionId i, OpExtensionUIListener::ItemAddedStatus status)
{
	OP_ASSERT((m_owner->m_parent && m_owner->m_parent->m_active_extension_id == i) || (!m_owner->m_parent && m_owner->m_active_extension_id == i));

	if (m_owner->m_parent)
		m_owner->m_parent->m_add_status = status;
	else
		m_owner->m_add_status = status;
	/* Unblocking the thread will restart the call => status will be seen & acted on. */
	if (m_owner->m_parent && m_owner->m_parent->m_blocked_thread)
	{
		OpStatus::Ignore(m_owner->m_parent->m_blocked_thread->Unblock());
		m_owner->m_parent->GetThreadListener()->ES_ThreadListener::Remove();
		m_owner->m_parent->m_blocked_thread = NULL;
	}
	else if (m_owner->m_blocked_thread)
	{
		OpStatus::Ignore(m_owner->m_blocked_thread->Unblock());
		m_owner->GetThreadListener()->ES_ThreadListener::Remove();
		m_owner->m_blocked_thread = NULL;
	}

	/* Only keep the parent alive across an async call. */
	if (m_owner->m_parent)
		m_owner->m_parent = NULL;
}

void
DOM_ExtensionContext::ExtensionUIListenerCallbacks::ItemRemoved(OpExtensionUIListener::ExtensionId i, OpExtensionUIListener::ItemRemovedStatus status)
{
	OP_ASSERT((m_owner->m_parent && m_owner->m_parent->m_active_extension_id == i) || (!m_owner->m_parent && m_owner->m_active_extension_id == i));

	if (m_owner->m_parent)
		m_owner->m_parent->m_remove_status = status;
	if (m_owner->m_parent && m_owner->m_parent->m_blocked_thread)
	{
		OpStatus::Ignore(m_owner->m_parent->m_blocked_thread->Unblock());
		m_owner->m_parent->GetThreadListener()->Out();
		m_owner->m_parent->m_blocked_thread = NULL;
	}

	if (status == OpExtensionUIListener::ItemRemovedSuccess)
		OnRemove(i);
}

void
DOM_ExtensionContext::ExtensionUIListenerCallbacks::OnClick(OpExtensionUIListener::ExtensionId i)
{
	if (m_owner->IsA(DOM_TYPE_EXTENSION_UIITEM))
	{
		DOM_ExtensionUIItem *it = static_cast<DOM_ExtensionUIItem*>(m_owner);

		DOM_Runtime *runtime = it->GetRuntime();
		DOM_ExtensionUIItemEvent *click_event;
		RETURN_VOID_IF_ERROR(DOM_ExtensionUIItemEvent::Make(click_event, it, runtime));

		ES_Value argv[3]; /* ARRAY OK 2010-09-13 sof */
		ES_Value value;

		DOMSetString(&argv[0], UNI_L("click"));
		DOMSetBoolean(&argv[1], FALSE);
		DOMSetBoolean(&argv[2], FALSE);

		DOM_Event::initEvent(click_event, argv, ARRAY_SIZE(argv), &value, runtime);

		click_event->InitEvent(ONCLICK, it);
		click_event->SetCurrentTarget(it);
		click_event->SetSynthetic();
		click_event->SetEventPhase(ES_PHASE_ANY);

		it->GetEnvironment()->SendEvent(click_event);
	}
}

void
DOM_ExtensionContext::ExtensionUIListenerCallbacks::OnRemove(OpExtensionUIListener::ExtensionId i)
{
	if (m_owner->IsA(DOM_TYPE_EXTENSION_UIITEM))
	{
		DOM_ExtensionUIItem *it = static_cast<DOM_ExtensionUIItem*>(m_owner);

		DOM_Runtime *runtime = it->GetRuntime();
		DOM_ExtensionUIItemEvent *remove_event;
		RETURN_VOID_IF_ERROR(DOM_ExtensionUIItemEvent::Make(remove_event, it, runtime));

		ES_Value argv[3]; /* ARRAY OK 2010-09-13 sof */
		ES_Value unused_value;

		DOM_Object::DOMSetString(&argv[0], UNI_L("remove"));
		DOM_Object::DOMSetBoolean(&argv[1], FALSE);
		DOM_Object::DOMSetBoolean(&argv[2], FALSE);

		DOM_Event::initEvent(remove_event, argv, ARRAY_SIZE(argv), &unused_value, runtime);

		remove_event->InitEvent(ONREMOVE, it);
		remove_event->SetCurrentTarget(it);
		remove_event->SetSynthetic();
		remove_event->SetEventPhase(ES_PHASE_ANY);

		it->GetEnvironment()->SendEvent(remove_event);
	}
}

/* static */ int
DOM_ExtensionContext::createItem(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(context, DOM_TYPE_EXTENSION_CONTEXT, DOM_ExtensionContext);

	if (argc < 0)
	{
		DOM_ExtensionUIItem *item = DOM_VALUE2OBJECT(*return_value, DOM_ExtensionUIItem);
		item->m_is_loading_image = FALSE;
		return DOM_ExtensionUIItem::CopyIconImage(this_object, item, return_value);
	}

	DOM_CHECK_ARGUMENTS("o");

	if (context->IsA(DOM_TYPE_EXTENSION_UIITEM))
		return DOM_CALL_INTERNALEXCEPTION(WRONG_THIS_ERR);

	if (static_cast<DOM_ExtensionContexts::ContextType>(context->m_id) != DOM_ExtensionContexts::CONTEXT_TOOLBAR)
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);

	return CreateItem(this_object, context->m_extension_support, argv[0].value.object, return_value, origining_runtime);
}


/* static */ int
DOM_ExtensionContext::CreateItem(DOM_Object *this_object, DOM_ExtensionSupport *extension_support, ES_Object *properties, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_Runtime *runtime = this_object->GetRuntime();
	ES_Value value;
	OP_BOOLEAN result;

	DOM_ExtensionUIItem *item;

	CALL_FAILED_IF_ERROR(DOM_ExtensionUIItem::Make(item, extension_support, runtime));

	CALL_FAILED_IF_ERROR(result = runtime->GetName(properties, UNI_L("disabled"), &value));
	item->m_disabled = result == OpBoolean::IS_TRUE && value.type == VALUE_BOOLEAN && value.value.boolean;

	CALL_FAILED_IF_ERROR(result = runtime->GetName(properties, UNI_L("title"), &value));
	if (result == OpBoolean::IS_TRUE && value.type == VALUE_STRING)
		if ((item->m_title = UniSetNewStr(value.value.string)) == NULL)
			return ES_NO_MEMORY;

	CALL_FAILED_IF_ERROR(result = runtime->GetName(properties, UNI_L("icon"), &value));
	if (result == OpBoolean::IS_TRUE && value.type == VALUE_STRING)
		if ((item->m_favicon = UniSetNewStr(value.value.string)) == NULL)
			return ES_NO_MEMORY;

	CALL_FAILED_IF_ERROR(DOM_ExtensionSupport::AddEventHandler(item, item->m_onclick_handler, properties, NULL, origining_runtime, UNI_L("onclick"), ONCLICK));
	CALL_FAILED_IF_ERROR(DOM_ExtensionSupport::AddEventHandler(item, item->m_onremove_handler, properties, NULL, origining_runtime, UNI_L("onremove"), ONREMOVE));

	CALL_FAILED_IF_ERROR(result = runtime->GetName(properties, UNI_L("popup"), &value));
	if (result == OpBoolean::IS_TRUE && value.type == VALUE_OBJECT)
		CALL_FAILED_IF_ERROR(DOM_ExtensionUIPopup::Make(item->m_popup, value.value.object, item, return_value, origining_runtime));

	CALL_FAILED_IF_ERROR(result = runtime->GetName(properties, UNI_L("badge"), &value));
	if (result == OpBoolean::IS_TRUE && value.type == VALUE_OBJECT)
		CALL_FAILED_IF_ERROR(DOM_ExtensionUIBadge::Make(this_object, item->m_badge, value.value.object, item, return_value, origining_runtime));

	item->m_id = g_gadget_manager->NextExtensionUnique();

	if (item->m_favicon)
	{
		int result = DOM_ExtensionUIItem::LoadImage(extension_support->GetGadget(), item, FALSE, return_value, origining_runtime);
		if (result != ES_EXCEPTION)
		{
			if ((result & ES_SUSPEND) == ES_SUSPEND)
				item->m_is_loading_image = TRUE;
			return result;
		}
		else
		{
			OP_ASSERT(return_value->type == VALUE_OBJECT);
			CALL_FAILED_IF_ERROR(DOM_ExtensionUIItem::ReportImageLoadFailure(item->m_favicon, item->GetEnvironment()->GetFramesDocument()->GetWindow()->Id(), return_value, origining_runtime));
		}
	}

	DOM_Object::DOMSetObject(return_value, item);
	return ES_VALUE;
}

/* static */ int
DOM_ExtensionContext::addItem(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(context, DOM_TYPE_EXTENSION_CONTEXT, DOM_ExtensionContext);

	if (argc < 0)
	{
		context->m_blocked_thread = NULL;
		OpExtensionUIListener::ItemAddedStatus call_status = context->m_add_status;
		context->m_add_status = OpExtensionUIListener::ItemAddedUnknown;

		return DOM_ExtensionUIItem::HandleItemAddedStatus(this_object, call_status, return_value);
	}

	DOM_CHECK_ARGUMENTS("o");
	DOM_ARGUMENT_OBJECT(new_item, 0, DOM_TYPE_EXTENSION_UIITEM, DOM_ExtensionUIItem);
	if (new_item->m_is_attached)
		return ES_FAILED;

	if (context->m_max_items >= 0 && static_cast<int>(context->m_items.Cardinal()) >= context->m_max_items)
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);

	new_item->Into(&context->m_items);

	new_item->m_parent_id = context->GetId();
	new_item->m_is_attached = TRUE;

	CALL_INVALID_IF_ERROR(new_item->NotifyItemUpdate(origining_runtime, context));
	return (ES_SUSPEND | ES_RESTART);

}

/* static */ int
DOM_ExtensionContext::removeItem(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(context, DOM_TYPE_EXTENSION_CONTEXT, DOM_ExtensionContext);

	if (argc < 0)
	{
		context->m_blocked_thread = NULL;
		context->GetThreadListener()->Remove();
		OpExtensionUIListener::ItemRemovedStatus call_status = context->m_remove_status;
		context->m_remove_status = OpExtensionUIListener::ItemRemovedUnknown;

		return DOM_ExtensionUIItem::HandleItemRemovedStatus(this_object, call_status, return_value);
	}
	else if (argc == 0)
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);

	DOM_ExtensionUIItem *item_to_remove = NULL;
	if (argv[0].type == VALUE_NUMBER)
	{
		DOM_CHECK_ARGUMENTS("n");
		double d = argv[0].value.number;
		if (d < 0 && d >= UINT_MAX)
			return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);

		unsigned int id = static_cast<unsigned int>(d);

		for (DOM_ExtensionUIItem *i = context->m_items.First(); i; i = i->Suc())
			if (i->GetId() == id)
			{
				i->Out();
				item_to_remove = i;
				break;
			}
	}
	else if (argv[0].type == VALUE_OBJECT)
	{
		DOM_ARGUMENT_OBJECT(uiitem, 0, DOM_TYPE_EXTENSION_UIITEM, DOM_ExtensionUIItem);

		if (!uiitem->m_is_attached)
			return ES_FAILED;

		for (DOM_ExtensionUIItem *i = context->m_items.First(); i; i = i->Suc())
			if (i == uiitem)
			{
				i->Out();
				item_to_remove = i;
				break;
			}
	}
	else
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);

	if (item_to_remove)
	{
		/* Async call; block current thread.. */
		ES_Thread *blocking_thread = origining_runtime->GetESScheduler()->GetCurrentThread();

		if (blocking_thread)
		{
			/* Record the parent connection for the duration of an async call. */
			item_to_remove->m_parent = context;
			blocking_thread->AddListener(context->GetThreadListener());
			blocking_thread->Block();
		}
		context->m_blocked_thread = blocking_thread;

		context->m_active_extension_id = item_to_remove->m_id;

		OP_STATUS status = item_to_remove->NotifyItemRemove();
		if (OpStatus::IsError(status))
		{
			item_to_remove->m_parent = NULL;

			/* ..but unblock if notification wasn't communicated OK. */
			if (blocking_thread)
			{
				OpStatus::Ignore(blocking_thread->Unblock());
				context->GetThreadListener()->Remove();
			}
			CALL_INVALID_IF_ERROR(status);
		}
		else
		{
			item_to_remove->m_is_attached = FALSE;
			return (ES_SUSPEND | ES_RESTART);
		}
	}

	return ES_FAILED;
}

/* virtual */ ES_GetState
DOM_ExtensionContext::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_length:
		DOMSetNumber(value, m_items.Cardinal());
		return GET_SUCCESS;
	default:
		return DOM_Object::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_GetState
DOM_ExtensionContext::GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	BOOL allowed = OriginCheck(origining_runtime);

	DOM_ExtensionUIItem *it = m_items.First();

	while (it && property_index-- > 0)
		it = it->Suc();

	if (it)
		if (!allowed)
			return GET_SECURITY_VIOLATION;
		else
		{
			DOMSetObject(value, it);
			return GET_SUCCESS;
		}
	else
		return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_ExtensionContext::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_length:
		return PUT_READ_ONLY;
	default:
		return DOM_Object::PutName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_ExtensionContext::PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	return PUT_FAILED;
}

int
DOM_ExtensionContext::item(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(context, DOM_TYPE_EXTENSION_CONTEXT, DOM_ExtensionContext);
	DOM_CHECK_ARGUMENTS("n");

	double index = argv[0].value.number;
	if (index < 0.0 || index >= static_cast<double>(context->m_items.Cardinal()))
		DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	unsigned idx = static_cast<unsigned>(argv[0].value.number);
	DOM_ExtensionUIItem *it = context->m_items.First();

	while (it && idx-- > 0)
		it = it->Suc();

	DOMSetObject(return_value, it);
	return ES_VALUE;
}

/* virtual */ ES_GetState
DOM_ExtensionContext::GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime)
{
	count = m_items.Cardinal();
	return GET_SUCCESS;
}

/* virtual */ void
DOM_ExtensionContext::GCTrace()
{
	for (DOM_ExtensionUIItem *i = m_items.First(); i; i = i->Suc())
		GCMark(i);

	GCMark(m_parent);
}

/* virtual */
DOM_ExtensionContext::~DOM_ExtensionContext()
{
	OP_NEW_DBG("DOM_ExtensionContext::~DOM_ExtensionContext()", "extensions.dom");
	OP_DBG(("this: %p", this));

	OP_ASSERT(m_items.Empty());

	m_notifications.Out();
	m_thread_listener.Out();
}

/* virtual */ void
DOM_ExtensionContext::BeforeDestroy()
{
	OP_NEW_DBG("DOM_ExtensionContext::BeforeDestroy()", "extensions.dom");
	OP_DBG(("this: %p", this));

	while (DOM_ExtensionUIItem *it = m_items.First())
	{
		it->Out();
		OpStatus::Ignore(it->NotifyItemRemove(FALSE));
	}
}

/* virtual */OP_STATUS
DOM_ExtensionContext::ThreadListener::Signal(ES_Thread *thread, ES_ThreadSignal signal)
{
	m_owner->m_blocked_thread = NULL;
	ES_ThreadListener::Remove();
	return OpStatus::OK;
}


#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_ExtensionContext)
	DOM_FUNCTIONS_FUNCTION(DOM_ExtensionContext, DOM_ExtensionContext::createItem, "createItem", "o-")
	DOM_FUNCTIONS_FUNCTION(DOM_ExtensionContext, DOM_ExtensionContext::addItem, "addItem", "o-")
	DOM_FUNCTIONS_FUNCTION(DOM_ExtensionContext, DOM_ExtensionContext::removeItem, "removeItem", "")
	DOM_FUNCTIONS_FUNCTION(DOM_ExtensionContext, DOM_ExtensionContext::item, "item", "n-")
DOM_FUNCTIONS_END(DOM_ExtensionContext)

#endif // EXTENSION_SUPPORT
