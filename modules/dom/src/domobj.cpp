/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/* This comment is the main documentation for the DOM object structure
   and storage management.


   OBJECT STRUCTURE.

   Before you read this you need to read and be familiar with the
   description of the ECMAScript object structure and storage management
   as documented in modules/ecmascript/ecmascript.cpp.

   DOM_Object is a direct subclass of EcmaScript_Object and serves as the
   base class for almost all DOM objects.  (It really could be *all* DOM
   objects but there are some that are derived from EcmaScript_Object.)

   DOM_Node is a direct subclass of DOM_Object and serves as the base
   class for Element and Attribute nodes.

   HTML_Element is a class that represents HTML elements.  Each HTML_Element
   has a field, "exo_object", that if non-NULL points to a chain of DOM_Node
   objects associated with that element (threaded through the "next_exo_object"
   field of the DOM_Node).

	 INVARIANT 1: If the exo_object chain is not empty, then the first object
	 is always the one that represents the Element (except when the chain is
	 being deallocated; see below).

   Each DOM_Node contains a pointer to the HTML_Element to which it belongs.
   DOM_Node::GetThisElement() returns the appropriate element.

	 INVARIANT 2: the "associated_element" field of a DOM_Node is NULL
	 only when the node represents an Attribute that has been decoupled
	 from its element.

   Recall the following invariant (INVARIANT 1 from ecmascript.cpp):

	 INVARIANT 3: For any x of type ES_Object or EcmaScript_Object,
	 x->foreign_object != 0 implies x->foreign_object->foreign_object == x.

   If two objects are coupled and one is deleted then the other must be
   notified of this deletion so that it can clear its foreign_object pointer;
   this happens automatically for you.

   HTML_Element::GetESElement() and HTML_Element::GetESAttr() are used
   to retrieve the appropriate DOM_Nodes on an element; these DOM_Node
   objects are allocated on demand, so the methods may signal OOM.

   HTML_Element objects are normally part of a linked structure that
   represents the document's element tree -- an element has a parent, and
   children, and siblings.  In some cases the HTML_Element may become
   disconnected from a structure, as when it is newly allocated, or when
   it is removed from the structure, or when the structure is deleted but
   there is a DOM node that references the element (so the element must
   continue to live).


   STORAGE MANAGEMENT.

   The basic EcmaScript_Object/ES_Object deletion protocol, based on counts of
   references across the engine boundary, ensures that ES_Objects, as well as
   EcmaScript_Objects that have ES_Objects, are deleted properly.  For this to
   work, we establish another invariant:

	     INVARIANT 4: For ES_Objects that are coupled to DOM_Nodes the
	     reference count is zero or one, never higher.

   The cluster of HTML_Element, its DOM_Nodes, and their ES_Objects are
   interdependent in the sense that they all become garbage at the same
   time -- as long as any one of them is referenced from outside the cluster,
   then they are all live.

   The decision about when to delete the object cluster is distributed across
   three places in the object code:

	 - HTML_Element::Clean and DOM_Node::Clean together decide whether
	   the cluster can be deleted when the document structure is deleted.
	   If there are no exo_objects in an HTML_Element or if none of the
	   exo_objects have an associated ES_Object, then the cluster can be
	   deleted right away.

	 - The ES engine garbage collector decides when the ES_Objects can
	   be deleted.  They can all be deleted when none of them are reachable.
	   This in turn leads to the destruction of the associated DOM_Nodes.

	 - DOM_Node::~DOM_Node removes the _this_ object from the list of exo_objects,
	   and if the list is then empty, also deletes the HTML element.

	   (Since the DOM_Nodes can be deleted in arbitrary order, Invariant 1
	   is violated while the cluster is being deleted.  This violation
	   is not observable to ES code because garbage collection is atomic as
	   far as the program is concerned, but Opera code must be careful not
	   to look up the Element DOM_Node during DOM_Node deletion.)

   The trick is now to ensure that none of the ES_Objects in a cluster are
   collected before all can be collected, and that none are collected while
   the HTML_Element is connected to a document structure.

   There are three parts to the solution:

	 - The external reference count on the ES_Object that shadows the
	   DOM_Node that represents the Element is nonzero while the HTML_Element
	   is connected to a document structure, and zero while the HTML_Element
	   is disconnected.

	 - The external reference counts on the ES_Objects that shadow the
	   DOM_Nodes that represent the Attributes are always zero.

	 - Every ES_Object connected to a particular HTML_Element is reachable
	   from every other ES_Object connected to that same HTML_Element

   These rules ensure that only when the HTML_Element is disconnected and
   none of the ES_Objects are reachable from other nodes than themselves,
   can the entire cluster be collected.  The collection is initiated by
   the ES garbage collector, which collects the ES_Objects, and then deletion
   of the other nodes follow the rules listed earlier.

   In practical terms, each ES_Object in a cluster has a property called
   "opera#domchain" which is a circular list of the objects in the cluster.
   This makes the space overhead moderate, and the only hairy operation
   is to remove an attribute (which happens only on removeAttributeNode,
   hardly a common operation).

*/

#include "core/pch.h"

#include "modules/dom/src/domobj.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domglobaldata.h"
#include "modules/dom/src/opatom.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/dom/src/domevents/domeventlistener.h"
#include "modules/dom/src/domload/lsparser.h"
#include "modules/dom/src/userjs/userjsmanager.h"
#include "modules/dom/src/js/window.h"

#ifdef WEBSERVER_SUPPORT
# include "modules/dom/src/domwebserver/domwebserver.h"
#endif // WEBSERVER_SUPPORT

#ifdef DOM_GADGET_FILE_API_SUPPORT
# include "modules/dom/src/opera/domgadgetfile.h"
#endif // DOM_GADGET_FILE_API_SUPPORT

#include "modules/dom/src/domfile/domfilereader.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/ecmascript/ecmascript.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/util/tempbuf.h"
#include "modules/url/url_sn.h"
#include "modules/url/url2.h"

#include "modules/security_manager/include/security_manager.h"

#ifdef JS_PLUGIN_SUPPORT
# include "modules/jsplugins/src/js_plugin_context.h"
#endif // JS_PLUGIN_SUPPORT

#ifdef GADGET_SUPPORT
# include "modules/dom/src/opera/domwidget.h"
#endif // GADGET_SUPPORT

#include "modules/dom/src/domsuspendcallback.h"

#ifdef WEBSOCKETS_SUPPORT
#include "modules/dom/src/websockets/domwebsocket.h"
#endif //WEBSOCKETS_SUPPORT

#ifdef DOM_CROSSDOCUMENT_MESSAGING_SUPPORT
# include "modules/dom/src/domwebworkers/domwebworkers.h"
#endif // DOM_CROSSDOCUMENT_MESSAGING_SUPPORT

#if defined(DOM_WEBWORKERS_SUPPORT)
# include "modules/dom/src/domwebworkers/domwebworkers.h"
#endif // DOM_WEBWORKERS_SUPPORT

#ifdef EVENT_SOURCE_SUPPORT
# include "modules/dom/src/domevents/domeventsource.h"
#endif // EVENT_SOURCE_SUPPORT

#ifdef DOM_STREAM_API_SUPPORT
# include "modules/dom/src/media/domstream.h"
#endif // DOM_STREAM_API_SUPPORT

#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT
#include "modules/dom/src/extensions/domextensionmenucontext.h"
#include "modules/dom/src/extensions/domextensionmenucontext_proxy.h"
#endif // DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT

int
DOM_CheckType(DOM_Runtime *runtime, DOM_Object *object, DOM_ObjectType type, ES_Value *return_value, DOM_Object::InternalException exception)
{
	if (object && object->IsA(type))
		return ES_VALUE;

	if (exception != DOM_Object::INTERNALEXCEPTION_NO_ERR)
		return runtime->GetEnvironment()->GetWindow()->CallInternalException(exception, return_value);
	else
		return ES_FAILED;
}

#ifdef DOM_JIL_API_SUPPORT

int
JIL_CheckType(DOM_Runtime *runtime, DOM_Object *object, DOM_ObjectType type, ES_Value *return_value)
{
	if (object && object->IsA(type))
		return ES_VALUE;

	CALL_FAILED_IF_ERROR(DOM_Object::CreateJILException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, runtime, UNI_L("Invalid object type")));
	return ES_EXCEPTION;
}

#endif // DOM_JIL_API_SUPPORT

enum CheckFormatResult
{
	DOM_CHECK_FORMAT_SUCCESS,
	DOM_CHECK_FORMAT_WRONG_ARG_TYPES,
	DOM_CHECK_FORMAT_ARGS_NOT_IN_RANGE
};

static CheckFormatResult
DOM_CheckESArgsFormat(const char *format, int argc, const ES_Value *argv)
{
	BOOL optional = FALSE;

	while (*format)
	{
		if (*format == '|')
		{
			optional = TRUE;
			++format;
			OP_ASSERT(*format); // Must be something after the vertical bar
		}

		if (argc == 0)
			if (optional)
				break;
			else
				return DOM_CHECK_FORMAT_WRONG_ARG_TYPES;

		ES_Value_Type type = argv->type;

		if (*format == 'b' && type != VALUE_BOOLEAN ||
		    (*format == 'n' || *format == 'N') && type != VALUE_NUMBER ||
		    *format == 's' && type != VALUE_STRING ||
		    *format == 'S' && type != VALUE_STRING && type != VALUE_NULL && type != VALUE_UNDEFINED ||
		    *format == 'z' && type != VALUE_STRING_WITH_LENGTH ||
		    *format == 'Z' && type != VALUE_STRING_WITH_LENGTH && type != VALUE_NULL && type != VALUE_UNDEFINED ||
		    *format == 'o' && type != VALUE_OBJECT ||
		    *format == 'O' && type != VALUE_OBJECT && type != VALUE_NULL && type != VALUE_UNDEFINED ||
			*format == 'f' && type != VALUE_OBJECT ||
			*format == 'F' && type != VALUE_OBJECT && type != VALUE_NULL && type != VALUE_UNDEFINED)
				return DOM_CHECK_FORMAT_WRONG_ARG_TYPES;

		if (*format == 'n')
		{
			double number = argv->value.number;
			if (op_isnan(number) || op_isinf(number))
				return DOM_CHECK_FORMAT_ARGS_NOT_IN_RANGE;
		}
		else if (*format == 'f' || *format == 'F')
		{
			if (type == VALUE_OBJECT && op_strcmp(ES_Runtime::GetClass(argv->value.object), "Function") != 0)
				return DOM_CHECK_FORMAT_ARGS_NOT_IN_RANGE;
		}

		++format;
		++argv;
		--argc;
	}

	return DOM_CHECK_FORMAT_SUCCESS;
}

int
DOM_CheckFormat(DOM_Runtime *runtime, const char *format, int argc, const ES_Value *argv, ES_Value *return_value)
{
	switch (DOM_CheckESArgsFormat(format, argc, argv))
	{
	case DOM_CHECK_FORMAT_SUCCESS:
		return ES_VALUE;
	case DOM_CHECK_FORMAT_ARGS_NOT_IN_RANGE:
		return runtime->GetEnvironment()->GetWindow()->CallDOMException(DOM_Object::NOT_SUPPORTED_ERR, return_value);
	case DOM_CHECK_FORMAT_WRONG_ARG_TYPES:
		return runtime->GetEnvironment()->GetWindow()->CallInternalException(DOM_Object::WRONG_ARGUMENTS_ERR, return_value, NULL);
	default:
		OP_ASSERT(FALSE);
		return ES_FAILED;
	}
}

int
DOM_CheckFormatNoException(const char *format, int argc, const ES_Value *argv)
{
	if (DOM_CheckESArgsFormat(format, argc, argv) != DOM_CHECK_FORMAT_SUCCESS)
		return ES_FAILED;

	return ES_VALUE;
}

#ifdef DOM_JIL_API_SUPPORT

int
JIL_CheckFormat(DOM_Runtime *runtime, const char *format, int argc, const ES_Value *argv, ES_Value *return_value)
{
	switch (DOM_CheckESArgsFormat(format, argc, argv))
	{
	case DOM_CHECK_FORMAT_SUCCESS:
		return ES_VALUE;
	case DOM_CHECK_FORMAT_ARGS_NOT_IN_RANGE:
	case DOM_CHECK_FORMAT_WRONG_ARG_TYPES:
		CALL_FAILED_IF_ERROR(DOM_Object::CreateJILException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, runtime));
		return ES_EXCEPTION;
	default:
		OP_ASSERT(FALSE);
		return ES_FAILED;
	}
}

#endif // DOM_JIL_API_SUPPORT

DOM_Object *
DOM_GetHostObject(ES_Object *foreignobject)
{
	EcmaScript_Object *unknownhostobject = ES_Runtime::GetHostObject(foreignobject);

	if (!unknownhostobject || !unknownhostobject->IsA(DOM_TYPE_OBJECT))
		return NULL;

	DOM_Object *hostobject = (DOM_Object *) unknownhostobject;

	/* FIXME: temporary code; the ECMAScript engine will calculate the
	   identity before it calls us (the code below doesn't handle OOM
	   that well.) */
	while (hostobject)
	{
		ES_Object *identity1;
		if (OpStatus::IsMemoryError(hostobject->Identity(&identity1)))
			return NULL;
		EcmaScript_Object *identity2 = ES_Runtime::GetHostObject(identity1);
		if (identity2 == hostobject)
			break;
		if (identity2->IsA(DOM_TYPE_OBJECT))
			hostobject = (DOM_Object *) identity2;
		else
			hostobject = NULL;
	}

	return hostobject;
}

/* static */ BOOL
DOM_Object::OriginCheck(URLType type1, int port1, const uni_char *domain1, URLType type2, int port2, const uni_char *domain2)
{
	// Currently this function is only used from window.cpp.  It will go away when
	// the security code there is factored out into the security manager as a distinct
	// action.

	return OpSecurityManager::OriginCheck(type1, port1, domain1, type2, port2, domain2);
}

/* static */ BOOL
DOM_Object::OriginCheck(const URL& url1, const URL& url2)
{
	BOOL allowed = FALSE;
	OpStatus::Ignore(OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_STANDARD,
	                                                  OpSecurityContext(url1),
	                                                  OpSecurityContext(url2),
	                                                  allowed));
	return allowed;
}

BOOL
DOM_Object::OriginCheck(const URL& target_url, ES_Runtime* origining_runtime)
{
	BOOL allowed = FALSE;
	OpStatus::Ignore(OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_STANDARD,
	                                                  OpSecurityContext(static_cast<DOM_Runtime *>(origining_runtime)),
	                                                  OpSecurityContext(target_url),
	                                                  allowed));
	return allowed;
}

BOOL
DOM_Object::OriginCheck(ES_Runtime* target_runtime, ES_Runtime* origining_runtime)
{
	DOM_Runtime *target_dom_runtime = static_cast<DOM_Runtime *>(target_runtime);
	DOM_Runtime *source_dom_runtime = static_cast<DOM_Runtime *>(origining_runtime);
	if (DOM_Runtime::QuickSecurityCheck(target_dom_runtime, source_dom_runtime))
		return TRUE;
#ifdef EXTENSION_SUPPORT
	/* Extension runtime S shares DOM environment with T and is considered equal (but not with other extension runtimes.) */
	else if (target_dom_runtime->GetEnvironment() == source_dom_runtime->GetEnvironment() && target_dom_runtime->HasSharedEnvironment() != source_dom_runtime->HasSharedEnvironment())
	{
		DOM_Runtime::CacheSecurityCheck(target_dom_runtime, source_dom_runtime);
		return TRUE;
	}
#endif // EXTENSION_SUPPORT
	else
	{
		BOOL allowed = FALSE;
		OpStatus::Ignore(OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_STANDARD,
		                                                  OpSecurityContext(source_dom_runtime),
		                                                  OpSecurityContext(target_dom_runtime),
		                                                  allowed));
		if (allowed)
		{
			DOM_Runtime::CacheSecurityCheck(target_dom_runtime, source_dom_runtime);
			return TRUE;
		}
		else
			return FALSE;
	}
}

BOOL
DOM_Object::OriginCheck(ES_Runtime* origining_runtime)
{
	return OriginCheck(GetRuntime(), origining_runtime);
}

BOOL3
DOM_Object::OriginLoadCheck(ES_Runtime* target_runtime, ES_Runtime* origining_runtime)
{
	if (!OriginCheck(target_runtime, origining_runtime))
	{
		FramesDocument* target_doc = target_runtime->GetFramesDocument();
		if (target_doc == NULL)
			return NO;
		else
		{
			int security = target_doc->GetURL().GetAttribute(URL::KSecurityStatus);
			if (security != SECURITY_STATE_NONE &&
				security != SECURITY_STATE_UNKNOWN)
				return NO;
			return MAYBE;
		}
	}
	return YES;
}

/* virtual */ BOOL
DOM_Object::SecurityCheck(ES_Runtime* origining_runtime)
{
	if (OriginCheck(origining_runtime))
		return TRUE;
	else
		return FALSE;
}

/* virtual */ BOOL
DOM_Object::SecurityCheck(OpAtom property_name, ES_Value *value, ES_Runtime* origining_runtime)
{
	return OriginCheck(origining_runtime);
}

#ifdef DOM_ACCESS_SECURITY_RULES
class SecurityCheckCallback : public DOM_SuspendCallback<OpSecurityCheckCallback>
{
public:
	SecurityCheckCallback() :
	  m_allowed(FALSE)
	  {}
	virtual void OnSecurityCheckSuccess(BOOL allowed, ChoicePersistenceType /*type*/)
	{
		m_allowed = allowed;
		OnSuccess();
	}

	virtual void OnSecurityCheckError(OP_STATUS error)
	{
		OnFailed(error);
	}

	BOOL IsAllowed()
	{
		return m_allowed;
	}
private:
	BOOL m_allowed;
};

void
DOM_Object::CheckAccessSecurity(OpSecurityCheckCallback* callback, OpSecurityManager::Operation op, const OpSecurityContext& source, const OpSecurityContext& target)
{
	OP_ASSERT(callback);
	OP_STATUS status = g_secman_instance->CheckSecurity(op, source, target, callback);
	if (OpStatus::IsError(status))
		callback->OnSecurityCheckError(status);
}

/* virtual */ int
DOM_Object::PropertyAccessSecurityCheck(ES_Value* value, ES_Value* argv, int argc, DOM_Runtime* origining_runtime, ES_Value* restart_value, AccessType access_type, const char* operation_name, const uni_char* arg1, const uni_char* arg2)
{
	OpSecurityContext source_context(origining_runtime);
	OpSecurityContext target_context(operation_name, arg1, arg2);
	DOM_CHECK_OR_RESTORE_PERFORMED;

	DOM_SuspendingCall call(this, argv, argc, value, restart_value, origining_runtime, DOM_CallState::PHASE_SECURITY_CHECK);
	NEW_SUSPENDING_CALLBACK(SecurityCheckCallback, callback, restart_value, call, ());
	OpMemberFunctionObject4<DOM_Object, OpSecurityCheckCallback*, OpSecurityManager::Operation, const OpSecurityContext&, const OpSecurityContext&>
			function(this, &DOM_Object::CheckAccessSecurity, callback,
				// params
				(access_type == ACCESS_TYPE_FUNCTION_INVOCATION) ? OpSecurityManager::DOM_INVOKE_FUNCTION : OpSecurityManager::DOM_ACCESS_PROPERTY,
				source_context,
				target_context);

	DOM_SUSPENDING_CALL(call, function, SecurityCheckCallback, callback);
	if (OpStatus::IsError(call.GetCallStatus()))
	{
		OP_ASSERT(!"Unexpected error status");
		CreateJILException(DOM_Object::JIL_UNKNOWN_ERR, value, origining_runtime, UNI_L("Internal security error"));
		return ES_EXCEPTION;
	}

	if (DOM_CallState::GetPhaseFromESValue(restart_value) == DOM_CallState::PHASE_SECURITY_CHECK)
	{
		OP_ASSERT(callback);
		OP_ASSERT(callback->WasCalled());
		if (callback->HasFailed() || !callback->IsAllowed())
		{
			CreateJILException(DOM_Object::JIL_SECURITY_ERR, value, origining_runtime); // TODO: return something sensible on error
			return ES_EXCEPTION;
		}
	}

	return ES_VALUE;
}
#endif // DOM_ACCESS_SECURITY_RULES

/* virtual */ ES_GetState
DOM_Object::GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	OpAtom property_atom = static_cast<OpAtom>(property_code);
	OP_ASSERT(property_atom >= OP_ATOM_UNASSIGNED && property_atom < OP_ATOM_ABSOLUTELY_LAST_ENUM);

	ES_GetState result = GET_FAILED;

	if (property_atom != OP_ATOM_UNASSIGNED)
	{
		result = GetName(property_atom, value, origining_runtime);
		if (result == GET_SUCCESS || result == GET_EXCEPTION)
			if (origining_runtime != GetRuntime() && !SecurityCheck(property_atom, NULL, origining_runtime))
				return GET_SECURITY_VIOLATION;
	}

	if (result != GET_FAILED)
		return result;

#ifdef DOM_JSPLUGIN_GENERAL_OBJECT_HOOK
	// See the documentation for the tweak TWEAK_DOM_JSPLUGIN_GENERAL_OBJECT_HOOK
	// to see what all this is about.
#ifdef JS_PLUGIN_SUPPORT
	JS_Plugin_Context* jsplugin_context = GetEnvironment()->GetJSPluginContext();
	if (jsplugin_context)
	{
		BOOL cacheable;

		OpString absolute_name;
		const char* class_name = GetRuntime()->GetClass(GetNativeObject());
		GET_FAILED_IF_ERROR(absolute_name.Set(class_name));
		GET_FAILED_IF_ERROR(absolute_name.AppendFormat(UNI_L(".%s"), property_name));

		if (jsplugin_context->GetName(absolute_name, NULL, &cacheable) == GET_SUCCESS)
		{
			if (!OriginCheck(origining_runtime))
				return GET_SECURITY_VIOLATION;

			if (value == NULL)
				return GET_SUCCESS;
		}

		switch (result = jsplugin_context->GetName(absolute_name, value, &cacheable))
		{
		case GET_FAILED:
			break;

		case GET_SUCCESS:
			if (cacheable && value)
				GET_FAILED_IF_ERROR(Put(absolute_name, *value));

		default:
			return result;
		}
	}
#endif // JS_PLUGIN_SUPPORT
#endif // DOM_JSPLUGIN_GENERAL_OBJECT_HOOK

	return result;
}

/* virtual */ ES_GetState
DOM_Object::GetName(OpAtom, ES_Value *, ES_Runtime *)
{
	return GET_FAILED;
}

/* virtual */ ES_GetState
DOM_Object::GetNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	OpAtom property_atom = static_cast<OpAtom>(property_code);
	OP_ASSERT(property_atom >= OP_ATOM_UNASSIGNED && property_atom < OP_ATOM_ABSOLUTELY_LAST_ENUM);

	if (property_atom == OP_ATOM_UNASSIGNED)
		return GET_FAILED;
	else
	{
		ES_GetState result = GetNameRestart(property_atom, value, origining_runtime, restart_object);
		if (result == GET_SUCCESS || result == GET_EXCEPTION)
			if (origining_runtime != GetRuntime() && !SecurityCheck(property_atom, NULL, origining_runtime))
				return GET_SECURITY_VIOLATION;

		return result;
	}
}

/* virtual */ ES_GetState
DOM_Object::GetNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_Object::PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	OpAtom property_atom = static_cast<OpAtom>(property_code);
	OP_ASSERT(property_atom >= OP_ATOM_UNASSIGNED && property_atom < OP_ATOM_ABSOLUTELY_LAST_ENUM);

	if (property_atom == OP_ATOM_UNASSIGNED)
		return PUT_FAILED;
	else
	{
		if (origining_runtime != GetRuntime())
			if (GetName(property_name, property_atom, NULL, origining_runtime) != GET_FAILED)
				if (!SecurityCheck(property_atom, value, origining_runtime))
					return PUT_SECURITY_VIOLATION;
		return PutName(property_atom, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_Object::PutName(OpAtom, ES_Value *, ES_Runtime *)
{
	return PUT_FAILED;
}

/* virtual */ ES_PutState
DOM_Object::PutNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	OpAtom property_atom = static_cast<OpAtom>(property_code);
	OP_ASSERT(property_atom >= OP_ATOM_UNASSIGNED && property_atom < OP_ATOM_ABSOLUTELY_LAST_ENUM);

	if (property_atom == OP_ATOM_UNASSIGNED)
		return PUT_FAILED;
	else
		return PutNameRestart(property_atom, value, origining_runtime, restart_object);
}

/* virtual */ ES_PutState
DOM_Object::PutNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	return PUT_FAILED;
}

class DOM_Object_FetchProperties_RestartObject
	: public ES_RestartObject
{
public:
	static DOM_Object_FetchProperties_RestartObject *Make();

	virtual BOOL ThreadCancelled(ES_Thread *thread)
	{
		return TRUE;
	}

	void Block(unsigned atom, ES_Runtime *origining_runtime);

	unsigned GetAtom() { return restart_atom; }

	static DOM_Object_FetchProperties_RestartObject *PopRestartObject(ES_Runtime *origining_runtime);

private:
	DOM_Object_FetchProperties_RestartObject()
		: ES_RestartObject(OPERA_MODULE_DOM, DOM_RESTART_OBJECT_FETCHPROPERTIES),
		  restart_atom(0)
	{
	}

	unsigned restart_atom;
};

/* static */ DOM_Object_FetchProperties_RestartObject *
DOM_Object_FetchProperties_RestartObject::Make()
{
	DOM_Object_FetchProperties_RestartObject *restart = OP_NEW(DOM_Object_FetchProperties_RestartObject, ());
	return restart;
}

void
DOM_Object_FetchProperties_RestartObject::Block(unsigned atom, ES_Runtime *origining_runtime)
{
	ES_Thread *thread = DOM_Object::GetCurrentThread(origining_runtime);
	Push(thread, ES_BLOCK_UNSPECIFIED);
	this->restart_atom = atom;
}

/* static */ DOM_Object_FetchProperties_RestartObject *
DOM_Object_FetchProperties_RestartObject::PopRestartObject(ES_Runtime *origining_runtime)
{
	ES_Thread *thread = DOM_Object::GetCurrentThread(origining_runtime);
	return static_cast<DOM_Object_FetchProperties_RestartObject *>(ES_RestartObject::Pop(thread, OPERA_MODULE_DOM, DOM_RESTART_OBJECT_FETCHPROPERTIES));
}

/* virtual */ ES_GetState
DOM_Object::FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime)
{
	unsigned atom = UINT_MAX;
	DOM_Object_FetchProperties_RestartObject *restart = DOM_Object_FetchProperties_RestartObject::PopRestartObject(origining_runtime);
	if (restart)
		atom = restart->GetAtom();

	OpStackAutoPtr<DOM_Object_FetchProperties_RestartObject> anchor_restart(restart);

	if (atom == UINT_MAX)
	{
		/* Assume that all host objects with indexed properties are compact (and all enumerable.) */
		unsigned count;
		ES_GetState result = GetIndexedPropertiesLength(count, origining_runtime);
		if (result == GET_SUSPEND)
		{
			if (!restart)
			{
				restart = DOM_Object_FetchProperties_RestartObject::Make();
				if (!restart)
					LEAVE(OpStatus::ERR_NO_MEMORY);
			}
			else
				anchor_restart.release();

			restart->Block(UINT_MAX, origining_runtime);
			return GET_SUSPEND;
		}
		else if (result != GET_SUCCESS)
			return result;

		if (count != 0)
			enumerator->AddPropertiesL(0, count);

		atom = 0;
	}

	for (; atom < OP_ATOM_ABSOLUTELY_LAST_ENUM; ++atom)
		switch (GetName(static_cast<OpAtom>(atom), NULL, origining_runtime))
		{
		case GET_SUCCESS:
			enumerator->AddPropertyL(DOM_AtomToString(static_cast<OpAtom>(atom)), static_cast<OpAtom>(atom));
			break;

		case GET_NO_MEMORY:
			return GET_NO_MEMORY;

		case GET_SUSPEND:
			if (!restart)
			{
				restart = DOM_Object_FetchProperties_RestartObject::Make();
				if (!restart)
					LEAVE(OpStatus::ERR_NO_MEMORY);
			}
			else
				anchor_restart.release();

			restart->Block(atom, origining_runtime);
			return GET_SUSPEND;

		default:
			OP_ASSERT(!"Unexpected return value");
		case GET_FAILED:
			break;
		}

	return GET_SUCCESS;
}

void
DOM_Object::AddFunctionL(DOM_Object *function, const char *name, const char *arguments, DOM_PropertyStorage **insecure_storage)
{
	TempBuffer buffer; ANCHOR(TempBuffer, buffer);
	buffer.AppendL(name);

	AddFunctionL(GetNativeObject(), GetRuntime(), function, buffer.GetStorage(), arguments, insecure_storage);
}

void
DOM_Object::AddFunctionL(DOM_Object *function, const uni_char *name, const char *arguments, DOM_PropertyStorage **insecure_storage)
{
	AddFunctionL(GetNativeObject(), GetRuntime(), function, name, arguments, insecure_storage);
}

void
DOM_Object::AddFunctionL(DOM_FunctionImpl *functionimpl, const char *name, const char *arguments, DOM_PropertyStorage **insecure_storage)
{
	AddFunctionL(GetNativeObject(), GetRuntime(), functionimpl, name, arguments, insecure_storage);
}

void
DOM_Object::AddFunctionL(DOM_FunctionWithDataImpl *functionimpl, int data, const char *name, const char *arguments, DOM_PropertyStorage **insecure_storage)
{
	AddFunctionL(GetNativeObject(), GetRuntime(), functionimpl, data, name, arguments, insecure_storage);
}

/* static */ void
DOM_Object::AddFunctionL(ES_Object *target, DOM_Runtime *runtime, DOM_Object *function, const uni_char *name, const char *arguments, DOM_PropertyStorage **insecure_storage)
{
	BOOL dontenum = FALSE, insecure = FALSE;
	while (*name)
		if (*name == '-')
		{
			++name;
			dontenum = TRUE;
		}
		else if (*name == '!')
		{
			++name;
			insecure = TRUE;
		}
		else
			break;

	if (!function->GetNativeObject())
		DOMSetFunctionRuntimeL(function, runtime, name, arguments);

	ES_Value value;
	DOMSetObject(&value, function);

	OP_STATUS status = OpStatus::OK;

	if (insecure && (insecure_storage || ES_Runtime::GetHostObject(target)))
		if (insecure_storage)
			status = DOM_PropertyStorage::Put(*insecure_storage, name, !dontenum, value);
		else
			status = static_cast<DOM_Object *>(ES_Runtime::GetHostObject(target))->SetInternalValue(name, value);
	else
		status = runtime->PutName(target, name, value, dontenum ? PROP_DONT_ENUM : 0);

	LEAVE_IF_ERROR(status);
}

static BOOL
IsInsecureFunction(const uni_char *name)
{
	while (*name)
		if (*name == '-')
			++name;
		else if (*name == '!')
			return TRUE;
		else
			break;
		return FALSE;
}

#ifdef DOM_ACCESS_SECURITY_RULES

void
DOM_Object::AddFunctionL(DOM_FunctionImpl *functionimpl, const char *name, const char *arguments, DOM_PropertyStorage **insecure_storage, const DOM_FunctionSecurityRuleDesc *security_rule)
{
	if (security_rule)
		AddFunctionL(OP_NEW_L(DOM_FunctionWithSecurityRule, (functionimpl, security_rule)), name, arguments, insecure_storage);
	else
		AddFunctionL(functionimpl, name, arguments, insecure_storage);
}

#endif // DOM_ACCESS_SECURITY_RULES

/* static */ void
DOM_Object::AddFunctionL(ES_Object *target, DOM_Runtime *runtime, DOM_FunctionImpl *functionimpl, const char *name8, const char *arguments, DOM_PropertyStorage **insecure_storage)
{
	DOM_Object *function;

	TempBuffer buffer; ANCHOR(TempBuffer, buffer);
	buffer.AppendL(name8);

	const uni_char *fullname = buffer.GetStorage(), *name = fullname;

	while (*name == '-' || *name == '!')
		++name;

	if (IsInsecureFunction(fullname))
		DOM_ALLOCATE_FUNCTION_L(function, DOM_InsecureFunction, (functionimpl), runtime->GetFunctionPrototype(), name, "Function", arguments, runtime);
	else
		DOM_ALLOCATE_FUNCTION_L(function, DOM_Function, (functionimpl), runtime->GetFunctionPrototype(), name, "Function", arguments, runtime);

	AddFunctionL(target, runtime, function, fullname, arguments, insecure_storage);
}

#ifdef DOM_ACCESS_SECURITY_RULES

/* static */ void
DOM_Object::AddFunctionL(ES_Object *target, DOM_Runtime *runtime, DOM_FunctionImpl *functionimpl, const char *name8, const char *arguments, DOM_PropertyStorage **insecure_storage, const DOM_FunctionSecurityRuleDesc *security_rule)
{
	if (security_rule)
	{
		DOM_Object *function;

		TempBuffer buffer; ANCHOR(TempBuffer, buffer);
		buffer.AppendL(name8);

		const uni_char *fullname = buffer.GetStorage(), *name = fullname;

		DOM_ALLOCATE_FUNCTION_L(function, DOM_FunctionWithSecurityRule, (functionimpl, security_rule), runtime->GetFunctionPrototype(), name, "Function", arguments, runtime);
		AddFunctionL(target, runtime, function, fullname, arguments, insecure_storage);
	}
	else
		AddFunctionL(target, runtime, functionimpl, name8, arguments, insecure_storage);
}

#endif // DOM_ACCESS_SECURITY_RULES

/* static */ void
DOM_Object::AddFunctionL(ES_Object *target, DOM_Runtime *runtime, DOM_FunctionWithDataImpl *functionimpl, int data, const char *name8, const char *arguments, DOM_PropertyStorage **insecure_storage)
{
	DOM_Object *function;

	TempBuffer buffer; ANCHOR(TempBuffer, buffer);
	buffer.AppendL(name8);

	const uni_char *fullname = buffer.GetStorage(), *name = fullname;

	while (*name == '-' || *name == '!')
		++name;

	if (IsInsecureFunction(fullname))
		DOM_ALLOCATE_FUNCTION_L(function, DOM_InsecureFunctionWithData, (functionimpl, data), runtime->GetFunctionPrototype(), name, "Function", arguments, runtime);
	else
		DOM_ALLOCATE_FUNCTION_L(function, DOM_FunctionWithData, (functionimpl, data), runtime->GetFunctionPrototype(), name, "Function", arguments, runtime);

	AddFunctionL(target, runtime, function, fullname, arguments, insecure_storage);
}

BOOL
DOM_Object::GetInternalValue(const uni_char *name, ES_Value *value)
{
	ES_Value dummy;

	if (!value)
		value = &dummy;

	ES_Value internalValues;
	if (GetPrivate(DOM_PRIVATE_internalValues, &internalValues) != OpBoolean::IS_TRUE)
		return FALSE;
	DOM_Object *target = DOM_VALUE2OBJECT(internalValues, DOM_Object);

	return target->Get(name, value) == OpBoolean::IS_TRUE;
}

OP_STATUS
DOM_Object::SetInternalValue(const uni_char *name, const ES_Value &value)
{
	ES_Value internalValues;
	OP_BOOLEAN result;

	RETURN_IF_ERROR(result = GetPrivate(DOM_PRIVATE_internalValues, &internalValues));

	DOM_Object *target;

	if (result == OpBoolean::IS_FALSE)
	{
		target = OP_NEW(DOM_InternalObject, ());
		if (!target || OpStatus::IsMemoryError(target->SetObjectRuntime(GetRuntime(), NULL, "InternalValues")))
		{
			OP_DELETE(target);
			return OpStatus::ERR_NO_MEMORY;
		}
		DOMSetObject(&internalValues, target);
		RETURN_IF_ERROR(PutPrivate(DOM_PRIVATE_internalValues, internalValues));
	}
	else
		target = DOM_VALUE2OBJECT(internalValues, DOM_Object);

	return target->Put(name, value);
}

/* virtual */ void
DOM_Object::DOMChangeRuntime(DOM_Runtime *new_runtime)
{
	ChangeRuntime(new_runtime);
}

void
DOM_Object::DOMChangeRuntimeOnPrivate(DOM_PrivateName private_name)
{
	ES_Value value;
	if (GetPrivate(private_name, &value) == OpBoolean::IS_TRUE && value.type == VALUE_OBJECT)
		if (DOM_Object *object = DOM_GetHostObject(value.value.object))
			object->DOMChangeRuntime(GetRuntime());
}

/* static */ OP_STATUS
DOM_Object::DOMSetObjectRuntime(DOM_Object *object, DOM_Runtime *runtime)
{
	if (object)
		if (OpStatus::IsMemoryError(object->SetObjectRuntime(runtime, runtime->GetObjectPrototype(), "Object")))
			OP_DELETE(object);
		else
			return OpStatus::OK;
	return OpStatus::ERR_NO_MEMORY;
}

/* static */ OP_STATUS
DOM_Object::DOMSetObjectRuntime(DOM_Object *object, DOM_Runtime *runtime, ES_Object *prototype, const char *class_name)
{
	if (object && prototype)
		if (!OpStatus::IsMemoryError(object->SetObjectRuntime(runtime, prototype, class_name)))
			return OpStatus::OK;

	OP_DELETE(object);
	return OpStatus::ERR_NO_MEMORY;
}

/* static */ void
DOM_Object::DOMSetObjectRuntimeL(DOM_Object *object, DOM_Runtime *runtime, ES_Object *prototype, const char *class_name)
{
	LEAVE_IF_ERROR(DOMSetObjectRuntime(object, runtime, prototype, class_name));
}

/* static */ OP_STATUS
DOM_Object::DOMSetFunctionRuntime(DOM_Object *object, DOM_Runtime *runtime, const char *class_name)
{
	if (!object || OpStatus::IsMemoryError(object->SetFunctionRuntime(runtime, runtime->GetFunctionPrototype(), class_name, NULL)))
	{
		OP_DELETE(object);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

/* static */ void
DOM_Object::DOMSetFunctionRuntimeL(DOM_Object *object, DOM_Runtime *runtime, const char *class_name)
{
	LEAVE_IF_ERROR(DOMSetFunctionRuntime(object, runtime, class_name));
}

/* static */ void
DOM_Object::DOMSetFunctionRuntimeL(DOM_Object *function, DOM_Runtime *runtime, const uni_char *name, const char *arguments)
{
	if (!function || OpStatus::IsMemoryError(function->SetFunctionRuntime(runtime, name, NULL, arguments)))
	{
		OP_DELETE(function);
		LEAVE(OpStatus::ERR_NO_MEMORY);
	}
}

/* static */ void
DOM_Object::DOMSetBoolean(ES_Value *v, BOOL b)
{
	if (v)
	{
		v->type = VALUE_BOOLEAN;
		v->value.boolean = !!b;
	}
}

/* static */ void
DOM_Object::DOMSetNumber(ES_Value *v, double n)
{
	if (v)
	{
		v->type = VALUE_NUMBER;
		v->value.number = n;
	}
}

/* static */ void
DOM_Object::DOMSetObject(ES_Value *v, ES_Object* o)
{
	if (v && o)
	{
		v->type = VALUE_OBJECT;
		v->value.object = o;
	}
	else
		DOMSetNull(v);
}

/* static */ void
DOM_Object::DOMSetObject(ES_Value *v, EcmaScript_Object* o)
{
	if (o)
		DOMSetObject(v, *o);
	else
		DOMSetNull(v);
}

/* static */ void
DOM_Object::DOMSetObject(ES_Value *v, DOM_EventListener *l)
{
	if (l)
		DOMSetObject(v, l->GetNativeHandler());
	else
		DOMSetNull(v);
}

/* static */ void
DOM_Object::DOMSetString(ES_Value* v, const uni_char *s)
{
	if (v)
	{
		v->type = VALUE_STRING;
		v->value.string = s ? s : UNI_L("");
	}
}

/* static */ void
DOM_Object::DOMSetString(ES_Value* v, const uni_char *s, unsigned length)
{
	if (v)
	{
		v->type = VALUE_STRING;
		v->value.string = s ? s : UNI_L("");
		OP_ASSERT(length == 0xffffffu || length < (0x1 << 24));
		v->string_length = length;
	}
}

/* static */ void
DOM_Object::DOMSetString(ES_Value* v, const uni_char *s, const uni_char *ds)
{
	if (v)
	{
		v->type = VALUE_STRING;
		v->value.string = s ? s : ds;
	}
}

/* static */ void
DOM_Object::DOMSetString(ES_Value* v, TempBuffer *s)
{
	DOMSetString(v, s->GetStorage());
}

/* static */ void
DOM_Object::DOMSetStringWithLength(ES_Value* v, ES_ValueString* string_holder, const uni_char *s, int len /* = -1 */)
{
	if (v)
	{
		OP_ASSERT(string_holder);
		v->type = VALUE_STRING_WITH_LENGTH;
		v->value.string_with_length = string_holder;
		if (s)
		{
			string_holder->string = s;
			if (len < 0)
				len = uni_strlen(s);
			string_holder->length = len;
		}
		else
		{
			string_holder->string = UNI_L("");
			string_holder->length = 0;
		}
	}
}

/* static */ OP_STATUS
DOM_Object::DOMSetDate(ES_Value* value, ES_Runtime* runtime, double miliseconds_timestamp)
{
	if (value)
	{
		ES_Value timestamp_val;
		DOM_Object::DOMSetNumber(&timestamp_val, miliseconds_timestamp);
		ES_Object* date_obj;
		RETURN_IF_ERROR(runtime->CreateNativeObject(timestamp_val, ENGINE_DATE_PROTOTYPE, &date_obj));
		DOM_Object::DOMSetObject(value, date_obj);
	}
	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_Object::DOMSetDate(ES_Value* value, ES_Runtime* runtime, time_t timestamp)
{
	return DOMSetDate(value, runtime, static_cast<double>(timestamp) * 1000);
}

ES_GetState
DOM_Object::DOMSetPrivate(ES_Value *value, int private_name)
{
	if (value)
	{
		OP_BOOLEAN result;

		GET_FAILED_IF_ERROR(result = GetPrivate(private_name, value));

		if (result == OpBoolean::IS_FALSE)
			return GET_FAILED;
	}
	return GET_SUCCESS;
}

/* static */ OP_STATUS
DOM_Object::DOMCopyValue(ES_Value &destination, const ES_Value &source)
{
	destination = source;

	if (destination.type == VALUE_STRING)
	{
		unsigned str_length = destination.GetStringLength();
		// String may contain NULs, length copy it.
		uni_char *str_buffer = OP_NEWA(uni_char, str_length + 1);
		if (!str_buffer)
		{
			destination.type = VALUE_UNDEFINED;
			return OpStatus::ERR_NO_MEMORY;
		}
		op_memcpy(str_buffer, source.value.string, sizeof(uni_char) * str_length);
		str_buffer[str_length] = 0;
		destination.value.string = str_buffer;
	}
	else if (destination.type == VALUE_STRING_WITH_LENGTH)
	{
		// The string may contain nulls so copy the string manually.
		unsigned int str_length = source.value.string_with_length->length;
		uni_char *str_buffer = OP_NEWA(uni_char, str_length);
		if (!str_buffer)
		{
			destination.type = VALUE_UNDEFINED;
			return OpStatus::ERR_NO_MEMORY;
		}
		ES_ValueString *value_string = OP_NEW(ES_ValueString, ());
		if (!value_string)
		{
			OP_DELETEA(str_buffer);
			destination.type = VALUE_UNDEFINED;
			return OpStatus::ERR_NO_MEMORY;
		}
		op_memcpy((void *)str_buffer, source.value.string_with_length->string, sizeof(uni_char) * str_length);
		DOMSetStringWithLength(&destination, value_string, str_buffer, str_length);
	}

	return OpStatus::OK;
}

/* static */ void
DOM_Object::DOMFreeValue(ES_Value &value)
{
	if (value.type == VALUE_STRING)
		OP_DELETEA((uni_char *) value.value.string);
	else if (value.type == VALUE_STRING_WITH_LENGTH)
	{
		OP_DELETEA(value.value.string_with_length->string);
		OP_DELETE(value.value.string_with_length);
	}

	value.type = VALUE_UNDEFINED;
}

BOOL
DOM_Object::DOMGetArrayLength(ES_Object *array, unsigned &length)
{
	OP_ASSERT(array);
	ES_Value value;
	OP_BOOLEAN result = GetRuntime()->GetName(array, UNI_L("length"), &value);

	if (result == OpBoolean::IS_TRUE)
	{
		OP_ASSERT(value.type == VALUE_NUMBER);
		OP_ASSERT(value.value.number == TruncateDoubleToUInt(value.value.number));
		length = TruncateDoubleToUInt(value.value.number);
		return TRUE;
	}
	else
		return FALSE;
}

BOOL
DOM_Object::DOMGetDictionaryBoolean(ES_Object *dictionary, const uni_char *name, BOOL default_value)
{
	ES_Value value;
	OP_BOOLEAN result = GetRuntime()->GetName(dictionary, name, &value);

	if (result == OpBoolean::IS_TRUE)
	{
		OP_ASSERT(value.type == VALUE_BOOLEAN);
		return value.value.boolean;
	}
	else
		return default_value;
}

double
DOM_Object::DOMGetDictionaryNumber(ES_Object *dictionary, const uni_char *name, double default_value)
{
	ES_Value value;
	OP_BOOLEAN result = GetRuntime()->GetName(dictionary, name, &value);

	if (result == OpBoolean::IS_TRUE)
	{
		OP_ASSERT(value.type == VALUE_NUMBER);
		return value.value.number;
	}
	else
		return default_value;
}

const uni_char *
DOM_Object::DOMGetDictionaryString(ES_Object *dictionary, const uni_char *name, const uni_char *default_value)
{
	ES_Value value;
	OP_BOOLEAN result = GetRuntime()->GetName(dictionary, name, &value);

	if (result == OpBoolean::IS_TRUE)
	{
		OP_ASSERT(value.type == VALUE_STRING);
		return value.value.string;
	}
	else
		return default_value;
}

static OP_STATUS
CreateException(const char *classname, const uni_char *message, unsigned code, ES_Value *value, DOM_Runtime *runtime, ES_Object *prototype)
{
	ES_Object *exception;
	ES_Value property_value;

	CALL_FAILED_IF_ERROR(runtime->CreateErrorObject(&exception, message, prototype));

	DOM_Object::DOMSetNumber(&property_value, code);
	CALL_FAILED_IF_ERROR(runtime->PutName(exception, UNI_L("code"), property_value));

	DOM_Object::DOMSetObject(value, exception);
	return OpStatus::OK;
}

static OP_STATUS
CreateException(const char *classname, const char *message, unsigned code, ES_Value *value, DOM_Runtime *runtime, ES_Object *prototype)
{
	if (message)
	{
		TempBuffer buffer;
		CALL_FAILED_IF_ERROR(buffer.Append(message));
		return CreateException(classname, buffer.GetStorage(), code, value, runtime, prototype);
	}
	else
		return CreateException(classname, static_cast<const uni_char *>(NULL), code, value, runtime, prototype);
}

static OP_STATUS
CreateDOMException(DOM_Object::DOMException code, ES_Value *value, DOM_Runtime *runtime)
{
#ifdef DOM_NO_COMPLEX_GLOBALS
#define MESSAGES_START() const char *message[DOM_Object::DOMEXCEPTION_COUNT]; message[0] = NULL;
#define MESSAGES_ITEM(id) message[DOM_Object::id] = #id;
#define MESSAGES_ITEM_LAST(id) message[DOM_Object::id] = #id;
#define MESSAGES_END()
#else // DOM_NO_COMPLEX_GLOBALS
#define MESSAGES_START() const char *const message[DOM_Object::DOMEXCEPTION_COUNT] = { 0,
#define MESSAGES_ITEM(id) #id,
#define MESSAGES_ITEM_LAST(id) #id
#define MESSAGES_END() };
#endif // DOM_NO_COMPLEX_GLOBALS

	MESSAGES_START()
		MESSAGES_ITEM(INDEX_SIZE_ERR)
		MESSAGES_ITEM(DOMSTRING_SIZE_ERR)
		MESSAGES_ITEM(HIERARCHY_REQUEST_ERR)
		MESSAGES_ITEM(WRONG_DOCUMENT_ERR)
		MESSAGES_ITEM(INVALID_CHARACTER_ERR)
		MESSAGES_ITEM(NO_DATA_ALLOWED_ERR)
		MESSAGES_ITEM(NO_MODIFICATION_ALLOWED_ERR)
		MESSAGES_ITEM(NOT_FOUND_ERR)
		MESSAGES_ITEM(NOT_SUPPORTED_ERR)
		MESSAGES_ITEM(INUSE_ATTRIBUTE_ERR)
		MESSAGES_ITEM(INVALID_STATE_ERR)
		MESSAGES_ITEM(SYNTAX_ERR)
		MESSAGES_ITEM(INVALID_MODIFICATION_ERR)
		MESSAGES_ITEM(NAMESPACE_ERR)
		MESSAGES_ITEM(INVALID_ACCESS_ERR)
		MESSAGES_ITEM(VALIDATION_ERR)
		MESSAGES_ITEM(TYPE_MISMATCH_ERR)
		MESSAGES_ITEM(SECURITY_ERR)
		MESSAGES_ITEM(NETWORK_ERR)
		MESSAGES_ITEM(ABORT_ERR)
		MESSAGES_ITEM(URL_MISMATCH_ERR)
		MESSAGES_ITEM(QUOTA_EXCEEDED_ERR)
		MESSAGES_ITEM(TIMEOUT_ERR)
		MESSAGES_ITEM(INVALID_NODE_TYPE_ERR)
		MESSAGES_ITEM_LAST(DATA_CLONE_ERR)
	MESSAGES_END()

#undef MESSAGES_START
#undef MESSAGES_ITEM
#undef MESSAGES_ITEM_LAST
#undef MESSAGES_END

	return CreateException("DOMException", message[code], code, value, runtime, runtime->GetPrototype(DOM_Runtime::DOMEXCEPTION_PROTOTYPE));
}

static OP_STATUS
CreateEventException(DOM_Object::EventException code, ES_Value *value, DOM_Runtime *runtime)
{
#ifdef DOM_NO_COMPLEX_GLOBALS
#define MESSAGES_START() const char *message[DOM_Object::EVENTEXCEPTION_COUNT]; message[0] = NULL;
#define MESSAGES_ITEM(id) message[DOM_Object::id] = #id;
#define MESSAGES_ITEM_LAST(id) message[DOM_Object::id] = #id;
#define MESSAGES_END()
#else // DOM_NO_COMPLEX_GLOBALS
#define MESSAGES_START() const char *const message[DOM_Object::EVENTEXCEPTION_COUNT] = { 0,
#define MESSAGES_ITEM(id) #id,
#define MESSAGES_ITEM_LAST(id) #id
#define MESSAGES_END() };
#endif // DOM_NO_COMPLEX_GLOBALS

	MESSAGES_START()
		MESSAGES_ITEM(UNSPECIFIED_EVENT_TYPE_ERR)
		MESSAGES_ITEM(DISPATCH_REQUEST_ERR)
	MESSAGES_END()

#undef MESSAGES_START
#undef MESSAGES_ITEM
#undef MESSAGES_ITEM_LAST
#undef MESSAGES_END
	return CreateException("EventException", message[code], code, value, runtime, runtime->GetErrorPrototype());
}

#ifdef DOM3_XPATH

static OP_STATUS
CreateXPathException(DOM_Object::XPathException code, ES_Value *value, DOM_Runtime *runtime, const uni_char *detail = NULL)
{
	const char *message;

	if (code == DOM_Object::INVALID_EXPRESSION_ERR)
		message = "INVALID_EXPRESSION_ERR";
	else
		message = "TYPE_ERR";

	if (detail)
	{
		OpString message_string;
		message_string.Set(message);
		message_string.AppendFormat(UNI_L(": %s"), detail);
		return CreateException("XPathException", message_string.CStr(), code, value, runtime, runtime->GetPrototype(DOM_Runtime::XPATHEXCEPTION_PROTOTYPE));
	}
	else
		return CreateException("XPathException", message, code, value, runtime, runtime->GetPrototype(DOM_Runtime::XPATHEXCEPTION_PROTOTYPE));
}

#endif // DOM3_XPATH

#ifdef DOM_JIL_API_SUPPORT

/* static */ OP_STATUS
DOM_Object::CreateJILException(DOM_Object::JILException code, ES_Value *value, DOM_Runtime* runtime, const uni_char* custom_message /* = NULL */)
{
	const uni_char* message = 0;
	const uni_char* type = 0;
	OP_ASSERT(runtime);
	OP_ASSERT(value); // this will not fail if value is NULL, but still it's probably programmer error

	switch (code)
	{
	case DOM_Object::JIL_INVALID_PARAMETER_ERR:
		message = UNI_L("Invalid parameter");
		type = UNI_L("invalid_parameter");
		break;
	case DOM_Object::JIL_SECURITY_ERR:
		message = UNI_L("Security denied");
		type = UNI_L("security");
		break;
	case DOM_Object::JIL_UNKNOWN_ERR:
		message = UNI_L("Unknown error");
		type = UNI_L("unknown");
		break;
	case DOM_Object::JIL_UNSUPPORTED_ERR:
		message = UNI_L("Unsupported");
		type = UNI_L("unsupported");
		break;
	default:
		OP_ASSERT(!"Not reachable");
	}
	if (custom_message)
		message = custom_message;

	OP_ASSERT(message);

	RETURN_IF_ERROR(CreateException("JILException", message, code, value, runtime, runtime->GetPrototype(DOM_Runtime::JIL_EXCEPTION_PROTOTYPE)));
	OP_ASSERT(value->type == VALUE_OBJECT);

	ES_Value prototype_value;
	DOMSetObject(&prototype_value, runtime->GetPrototype(DOM_Runtime::JIL_EXCEPTION_PROTOTYPE));
	RETURN_IF_ERROR(runtime->PutName(value->value.object, UNI_L("prototype"), prototype_value, PROP_DONT_DELETE));

	ES_Value type_value;
	DOMSetString(&type_value, type);
	return runtime->PutName(value->value.object, UNI_L("type"), type_value, PROP_DONT_DELETE);
}

#endif // DOM_JIL_API_SUPPORT

ES_GetState
DOM_Object::GetNameDOMException(DOMException code, ES_Value *value)
{
	GET_FAILED_IF_ERROR(CreateDOMException(code, value, GetRuntime()));
	return GET_EXCEPTION;
}

#ifdef DOM3_XPATH

ES_GetState
DOM_Object::GetNameXPathException(XPathException code, ES_Value *value)
{
	GET_FAILED_IF_ERROR(CreateXPathException(code, value, GetRuntime()));
	return GET_EXCEPTION;
}

#endif // DOM3_XPATH

ES_PutState
DOM_Object::PutNameDOMException(DOMException code, ES_Value *value)
{
	PUT_FAILED_IF_ERROR(CreateDOMException(code, value, GetRuntime()));
	return PUT_EXCEPTION;
}

int
DOM_Object::CallDOMException(DOMException code, ES_Value *value)
{
	CALL_FAILED_IF_ERROR(CreateDOMException(code, value, GetRuntime()));
	return ES_EXCEPTION;
}

int
DOM_Object::CallEventException(EventException code, ES_Value *value)
{
	CALL_FAILED_IF_ERROR(CreateEventException(code, value, GetRuntime()));
	return ES_EXCEPTION;
}

#ifdef DOM3_XPATH

int
DOM_Object::CallXPathException(XPathException code, ES_Value *value, const uni_char *detail)
{
	CALL_FAILED_IF_ERROR(CreateXPathException(code, value, GetRuntime(), detail));
	return ES_EXCEPTION;
}

#endif // DOM3_XPATH

#ifdef SVG_DOM

int
DOM_Object::CallSVGException(SVGException code, ES_Value *value)
{
	const char *message;

	if (code == DOM_Object::SVG_WRONG_TYPE_ERR)
		message = "SVG_WRONG_TYPE_ERR";
	else if (code == DOM_Object::SVG_INVALID_VALUE_ERR)
		message = "SVG_INVALID_VALUE_ERR";
	else
		message = "SVG_MATRIX_NOT_INVERTABLE";

	CALL_FAILED_IF_ERROR(CreateException("SVGException", message, code, value, GetRuntime(), GetRuntime()->GetErrorPrototype()));
	return ES_EXCEPTION;
}

#endif // SVG_DOM

#ifdef DOM_GADGET_FILE_API_SUPPORT

int
DOM_Object::CallFileAPIException(FileAPIException code, ES_Value *value)
{
	const char *message;

	if (code == DOM_Object::FILEAPI_WRONG_ARGUMENTS_ERR)
		message = "WRONG_ARGUMENTS_ERR";
	else if (code == DOM_Object::FILEAPI_WRONG_TYPE_OF_OBJECT_ERR)
		message = "WRONG_TYPE_OF_OBJECT_ERR";
	else if (code == DOM_Object::FILEAPI_GENERIC_ERR)
		message = "GENERIC_ERR";
	else if (code == DOM_Object::FILEAPI_NO_ACCESS_ERR)
		message = "NO_ACCESS_ERR";
	else if (code == DOM_Object::FILEAPI_FILE_NOT_FOUND_ERR)
		message = "FILE_NOT_FOUND_ERR";
	else if (code == DOM_Object::FILEAPI_FILE_ALREADY_EXISTS_ERR)
		message = "FILE_ALREADY_EXISTS_ERR";
	else if (code == DOM_Object::FILEAPI_NOT_SUPPORTED_ERR)
		message = "NOT_SUPPORTED_ERR";
	else if (code == DOM_Object::FILEAPI_RESOURCE_UNAVAILABLE_ERR)
		message = "RESOURCE_UNAVAILABLE_ERR";
	else
		message = "TYPE_NOT_SUPPORTED_ERR";

	CALL_FAILED_IF_ERROR(CreateException("FileAPIException", message, code, value, GetRuntime(), GetRuntime()->GetErrorPrototype()));
	return ES_EXCEPTION;
}

#endif // DOM_GADGET_FILE_API_SUPPORT

#ifdef WEBSERVER_SUPPORT

int
DOM_Object::CallWebServerException(WebServerException code, ES_Value *value)
{
	const char *message;

	if (code == DOM_Object::WEBSERVER_TYPE_NOT_SUPPORTED_ERR)
		message = "TYPE_NOT_SUPPORTED_ERR";
	else if (code == DOM_Object::WEBSERVER_USER_EXISTS_ERR)
		message = "USER_EXISTS_ERR";
	else if (code == DOM_Object::WEBSERVER_USER_NOT_FOUND_ERR)
		message = "USER_NOT_FOUND_ERR";
	else if (code == DOM_Object::WEBSERVER_AUTH_PATH_EXISTS_ERR)
		message = "AUTH_PATH_EXISTS_ERR";
	else if (code == DOM_Object::WEBSERVER_SANDBOX_EXISTS_ERR)
		message = "SANDBOX_EXISTS_ERR";
	else
		message = "ALREADY_SHARED_ERR";

	CALL_FAILED_IF_ERROR(CreateException("WebServerException", message, code, value, GetRuntime(), GetRuntime()->GetErrorPrototype()));
	return ES_EXCEPTION;
}

#endif // WEBSERVER_SUPPORT

int
DOM_Object::CallInternalException(InternalException code, ES_Value *value, const uni_char *detail)
{
#ifdef DOM_NO_COMPLEX_GLOBALS
#define MESSAGES_START() const char *message[DOM_Object::LAST_INTERNAL_EXCEPTION]; message[0] = NULL;
#define MESSAGES_ITEM(id) message[DOM_Object::id] = #id;
#define MESSAGES_ITEM_LAST(id) message[DOM_Object::id] = #id;
#define MESSAGES_END()
#else // DOM_NO_COMPLEX_GLOBALS
#define MESSAGES_START() const char *const message[DOM_Object::LAST_INTERNAL_EXCEPTION] = { NULL,
#define MESSAGES_ITEM(id) #id,
#define MESSAGES_ITEM_LAST(id) #id
#define MESSAGES_END() };
#endif // DOM_NO_COMPLEX_GLOBALS

	MESSAGES_START()
		MESSAGES_ITEM(UNEXPECTED_MUTATION_ERR)
		MESSAGES_ITEM(RESOURCE_BUSY_ERR)
		MESSAGES_ITEM(RESOURCE_UNAVAILABLE_ERR)
#ifdef DOM_XSLT_SUPPORT
		MESSAGES_ITEM(XSLT_PARSING_FAILED_ERR)
		MESSAGES_ITEM(XSLT_PROCESSING_FAILED_ERR)
#endif // DOM_XSLT_SUPPORT
		MESSAGES_ITEM(WRONG_ARGUMENTS_ERR)
		MESSAGES_ITEM(WRONG_THIS_ERR)
		MESSAGES_ITEM_LAST(UNSUPPORTED_DOCUMENT_OPEN_ERR)
	MESSAGES_END()

#undef MESSAGES_START
#undef MESSAGES_ITEM
#undef MESSAGES_ITEM_LAST
#undef MESSAGES_END

	DOM_Runtime *runtime = GetRuntime();

	if (detail)
	{
		OpString message_string;
		message_string.Set(message[code]);
		message_string.AppendFormat(UNI_L(": %s"), detail);
		CALL_FAILED_IF_ERROR(CreateException("InternalException", message_string.CStr(), code, value, runtime, runtime->GetErrorPrototype()));
	}
	else
		CALL_FAILED_IF_ERROR(CreateException("InternalException", message[code], code, value, runtime, runtime->GetErrorPrototype()));

	return ES_EXCEPTION;
}

int
DOM_Object::CallCustomException(const char *classname, const char *message, unsigned code, ES_Value *value, ES_Object *prototype /* = NULL */)
{
	CALL_FAILED_IF_ERROR(CreateException(classname, message, code, value, GetRuntime(), prototype ? prototype : GetRuntime()->GetErrorPrototype()));
	return ES_EXCEPTION;
}

int
DOM_Object::CallNativeException(ES_NativeError type, const uni_char *message, ES_Value *value)
{
	ES_Object *exception;
	CALL_FAILED_IF_ERROR(GetRuntime()->CreateNativeErrorObject(&exception, type, message));

	DOMSetObject(value, exception);
	return ES_EXCEPTION;
}

ES_GetState
DOM_Object::ConvertCallToGetName(int result, ES_Value *value)
{
	if (result == ES_VALUE)
		return GET_SUCCESS;
	else if (result == ES_FAILED)
	{
		value->type = VALUE_UNDEFINED;
		return GET_SUCCESS;
	}
	else if (result == ES_NO_MEMORY)
		return GET_NO_MEMORY;
	else if (result == (ES_SUSPEND | ES_RESTART))
		return GET_SUSPEND;
	else if (result == ES_EXCEPTION)
		return GET_EXCEPTION;

	OP_ASSERT(FALSE);
	return GET_FAILED;
}

ES_PutState
DOM_Object::ConvertCallToPutName(int result, ES_Value *value)
{
	if (result == ES_VALUE || result == ES_FAILED)
		return PUT_SUCCESS;
	else if (result == ES_NO_MEMORY)
		return PUT_NO_MEMORY;
	else if (result == (ES_SUSPEND | ES_RESTART))
		return PUT_SUSPEND;
	else if (result == ES_EXCEPTION)
		return PUT_EXCEPTION;

	OP_ASSERT(FALSE);
	return PUT_FAILED;
}

int
DOM_Object::ConvertGetNameToCall(ES_GetState result, ES_Value *value)
{
	if (result == GET_SUCCESS)
		return ES_VALUE;
	else if (result == GET_FAILED)
		return ES_FAILED;
	else if (result == GET_NO_MEMORY)
		return ES_NO_MEMORY;
	else if (result == GET_EXCEPTION)
		return ES_EXCEPTION;

	OP_ASSERT(FALSE);
	return ES_FAILED;
}

int
DOM_Object::ConvertPutNameToCall(ES_PutState result, ES_Value *value)
{
	if (result == PUT_SUCCESS || result == PUT_FAILED)
		return ES_FAILED;
	else if (result == PUT_NO_MEMORY)
		return ES_NO_MEMORY;
	else if (result == PUT_EXCEPTION)
		return ES_EXCEPTION;

	OP_ASSERT(FALSE);
	return ES_FAILED;
}

int
DOM_Object::CallAsGetNameOrGetIndex(const uni_char *arg, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	int index = 0;
	BOOL is_number = FALSE;

	const uni_char* ptr = arg;

	while (*ptr >= '0' && *ptr <= '9')
		++ptr;

	unsigned length = ptr - arg;

	if (!*ptr && length > 0 && (*arg != '0' || length == 1) && length <= 10)
	{
		uni_char *endptr;

		/* The possible loss of precision here is okay, a name that is
		   bigger than INT_MAX isn't a index anyway, it is just a
		   string that looks like a large integer. */
		index = static_cast<int>(uni_strtol(arg, &endptr, 10));

		if (index >= 0)
		{
			char buffer[16]; /* ARRAY OK 2008-02-28 jl */
			op_snprintf(buffer, sizeof buffer, "%d", (int) index);

			is_number = uni_str_eq(arg, buffer);
		}
	}

	ES_GetState state;

	if (is_number)
		state = GetIndex(index, return_value, origining_runtime);
	else
		state = GetName(arg, DOM_StringToAtom(arg), return_value, origining_runtime);

	if (state == GET_FAILED)
	{
		DOMSetNull(return_value);
		return ES_VALUE;
	}
	else
		return ConvertGetNameToCall(state, return_value);
}

static OP_STATUS
DOM_Put(DOM_Runtime *runtime, ES_Object *object, const char *name, const ES_Value &value, int flags)
{
	TempBuffer buf;
	buf.AppendL(name);
	return runtime->PutName(object, buf.GetStorage(), value, flags & DOM_Object::PROP_MASK);
}

void
DOM_Object::PutL(const char *name, const ES_Value &value, int flags)
{
	LEAVE_IF_ERROR(DOM_Put(GetRuntime(), GetNativeObject(), name, value, flags));
}

/* static */ void
DOM_Object::PutL(ES_Object *target, const char *name, const ES_Value &value, DOM_Runtime *runtime, int flags)
{
	LEAVE_IF_ERROR(DOM_Put(runtime, target, name, value, flags));
}

void
DOM_Object::PutPrivateL(int name, DOM_Object *object)
{
	LEAVE_IF_ERROR(PutPrivate(name, *object));
}

void
DOM_Object::PutObjectL(const char *name, DOM_Object *object, const char *classname, int flags)
{
	if (flags & PROP_IS_FUNCTION)
		DOMSetFunctionRuntimeL(object, GetRuntime(), classname);
	else
		DOMSetObjectRuntimeL(object, GetRuntime(), GetRuntime()->GetObjectPrototype(), classname);

	ES_Value value;
	DOMSetObject(&value, object);

	PutL(name, value, flags);
}

ES_Object *
DOM_Object::PutSingletonObjectL(const char *name, int flags)
{
	ES_Object *object;

	GetRuntime()->CreateNativeObject(&object, GetRuntime()->GetObjectPrototype(), name, ES_Runtime::FLAG_SINGLETON);

	ES_Value value;
	DOMSetObject(&value, object);

	PutL(name, value, flags);

	return object;
}

void
DOM_Object::PutFunctionL(const char *name, DOM_Object *function, const char *classname, const char *arguments, BOOL is_object)
{
	if (!function || OpStatus::IsMemoryError(function->SetFunctionRuntime(GetRuntime(), GetRuntime()->GetFunctionPrototype(), classname, arguments)))
	{
		OP_DELETE(function);
		LEAVE(OpStatus::ERR_NO_MEMORY);
	}

	ES_Value value;
	DOMSetObject(&value, function);

	PutL(name, value);

	if (is_object)
	{
		int DOM_toString(DOM_Object *, ES_Value *, int, ES_Value *, DOM_Runtime *);
		function->AddFunctionL(DOM_toString, "toString");
	}
}

void
DOM_Object::PutConstructorL(DOM_BuiltInConstructor *constructor, const char *arguments /*= NULL*/)
{
	if (!constructor)
		LEAVE(OpStatus::ERR_NO_MEMORY);
	DOM_Runtime* runtime = GetRuntime();
	const char* name = runtime->GetConstructorName(constructor->GetPrototype());
	if (OpStatus::IsMemoryError(constructor->SetFunctionRuntime(runtime, runtime->GetFunctionPrototype(), name, arguments)))
	{
		OP_DELETE(constructor);
		LEAVE(OpStatus::ERR_NO_MEMORY);
	}

	ES_Value value;
	DOMSetObject(&value, constructor);

	PutL(name, value, PROP_DONT_ENUM);

	runtime->RecordConstructor(constructor->GetPrototype(), constructor);
}

DOM_Object *
DOM_Object::PutConstructorL(const char *name8, DOM_Runtime::Prototype type, BOOL singleton)
{
	// Possible optimization: if name8 if always identical to GetRuntime()->GetConstructorName(type)
	// then that argument isn't needed.
	OP_ASSERT(op_strlen(name8) < 64);

	uni_char name[64], *p = name;
	const char *p8 = name8;

	while ((*p = *p8) != 0)
		++p, ++p8;

	DOM_Object *object;

	DOM_ALLOCATE_FUNCTION_FLAGS_L(object, DOM_BuiltInConstructor, (type), GetRuntime()->GetFunctionPrototype(), name, name8, NULL, GetRuntime(), singleton ? ES_Runtime::FLAG_SINGLETON : 0);

	if (GetRuntime()->GetName(GetNativeObject(), name, NULL) == OpBoolean::IS_FALSE)
	{
		/* Do not put if the property is already set (e.g., overridden by user).
		   See CORE-30130. */
		ES_Value value;
		DOMSetObject(&value, object);
		LEAVE_IF_ERROR(GetRuntime()->PutName(GetNativeObject(), name, value, PROP_DONT_ENUM));
	}

	GetRuntime()->RecordConstructor(type, object);
	return object;
}

DOM_Object *
DOM_Object::PutConstructorL(const char *name, DOM_Runtime::HTMLElementPrototype type)
{
	DOM_Object *object;
	PutObjectL(name, object = OP_NEW(DOM_BuiltInConstructor, (type)), name, PROP_DONT_ENUM | PROP_IS_FUNCTION);
	return object;
}

#ifdef SVG_DOM
DOM_Object *
DOM_Object::PutConstructorL(const char *name, DOM_Runtime::SVGObjectPrototype type)
{
	DOM_Object *object;
	PutObjectL(name, object = OP_NEW(DOM_BuiltInConstructor, (type)), name, PROP_DONT_ENUM | PROP_IS_FUNCTION);
	return object;
}

DOM_Object *
DOM_Object::PutConstructorL(const char *name, DOM_Runtime::SVGElementPrototype type, DOM_SVGElementInterface ifs)
{
	DOM_Object *object;
	PutObjectL(name, object = OP_NEW(DOM_BuiltInConstructor, (type, ifs)), name, PROP_DONT_ENUM | PROP_IS_FUNCTION);
	return object;
}
#endif // SVG_DOM

/* static */ void
DOM_Object::PutNumericConstantL(ES_Object *target, const char *name, int constant, DOM_Runtime *runtime)
{
	ES_Value value;
	value.type = VALUE_NUMBER;
	value.value.number = constant;
	PutL(target, name, value, runtime, PROP_READ_ONLY | PROP_DONT_DELETE);
}

/* static */ void
DOM_Object::PutNumericConstantL(ES_Object *target, const char *name, unsigned constant, DOM_Runtime *runtime)
{
	ES_Value value;
	value.type = VALUE_NUMBER;
	value.value.number = constant;
	PutL(target, name, value, runtime, PROP_READ_ONLY | PROP_DONT_DELETE);
}

/* static */ OP_STATUS
DOM_Object::PutNumericConstant(ES_Object *object, const uni_char *name, int constant, DOM_Runtime *runtime)
{
	ES_Value value;
	value.type = VALUE_NUMBER;
	value.value.number = constant;
	return runtime->PutName(object, name, value, PROP_READ_ONLY | PROP_DONT_DELETE);
}

OP_STATUS
DOM_Object::PutStringConstant(const uni_char* name, const uni_char* constant)
{
	return PutStringConstant(GetNativeObject(), name, constant, GetRuntime());
}

/* static */ OP_STATUS
DOM_Object::PutStringConstant(ES_Object* object, const uni_char* name, const uni_char* constant, DOM_Runtime *runtime)
{
	ES_Value value;
	DOMSetString(&value, constant);
	return runtime->PutName(object, name, value, PROP_READ_ONLY | PROP_DONT_DELETE);
}

DOM_EnvironmentImpl *
DOM_Object::GetEnvironment()
{
	return GetRuntime()->GetEnvironment();
}

DOM_Runtime *
DOM_Object::GetRuntime()
{
	return (DOM_Runtime *) EcmaScript_Object::GetRuntime();
}

FramesDocument *
DOM_Object::GetFramesDocument()
{
	return GetRuntime()->GetFramesDocument();
}

HTML_Document *
DOM_Object::GetHTML_Document()
{
	if (FramesDocument *frames_doc = GetFramesDocument())
		return frames_doc->GetHtmlDocument();
	return NULL;
}

HLDocProfile *
DOM_Object::GetHLDocProfile()
{
	if (FramesDocument *frames_doc = GetFramesDocument())
		return frames_doc->GetHLDocProfile();
	return NULL;
}

LogicalDocument *
DOM_Object::GetLogicalDocument()
{
	if (FramesDocument *frames_doc = GetFramesDocument())
		return frames_doc->GetLogicalDocument();
	return NULL;
}

ServerName *
DOM_Object::GetHostName()
{
	return DOM_EnvironmentImpl::GetHostName(GetFramesDocument());
}

DOM_EventTargetOwner *
DOM_Object::GetEventTargetOwner()
{
	if (IsA(DOM_TYPE_NODE))
		return static_cast<DOM_Node *>(this);
	else if (IsA(DOM_TYPE_WINDOW))
		return static_cast<JS_Window *>(this);
#ifdef SVG_DOM
	else if (IsA(DOM_TYPE_SVG_ELEMENT_INSTANCE))
		return static_cast<DOM_Node *>(this);
#endif // SVG_DOM
#ifdef USER_JAVASCRIPT
	else if (IsA(DOM_TYPE_OPERA))
		if (DOM_UserJSManager *user_js_manager = GetEnvironment()->GetUserJSManager())
			return user_js_manager;
		else
			return NULL;
#endif // USER_JAVASCRIPT
#ifdef DOM3_LOAD
	else if (IsA(DOM_TYPE_LSPARSER))
		return static_cast<DOM_LSParser *>(this);
#endif // DOM3_LOAD
#ifdef MEDIA_HTML_SUPPORT
	else if (IsA(DOM_TYPE_TEXTTRACK))
		return static_cast<DOM_TextTrack *>(this);
	else if (IsA(DOM_TYPE_TEXTTRACKCUE))
		return static_cast<DOM_TextTrackCue *>(this);
	else if (IsA(DOM_TYPE_TEXTTRACKLIST))
		return static_cast<DOM_TextTrackList *>(this);
#endif // MEDIA_HTML_SUPPORT
#ifdef WEBSERVER_SUPPORT
	else if (IsA(DOM_TYPE_WEBSERVER))
		return static_cast<DOM_WebServer *>(this);
#endif // WEBSERVER_SUPPORT
	else if (IsA(DOM_TYPE_FILEREADER))
		return static_cast<DOM_FileReader *>(this);
#ifdef APPLICATION_CACHE_SUPPORT
	else if (IsA(DOM_TYPE_APPLICATION_CACHE))
		return static_cast<DOM_ApplicationCache *>(this);
#endif // APPLICATION_CACHE_SUPPORT
#ifdef DOM_GADGET_FILE_API_SUPPORT
	else if (IsA(DOM_TYPE_GADGETFILE))
		return static_cast<DOM_GadgetFile *>(this);
	else if (IsA(DOM_TYPE_FILESTREAM))
		return static_cast<DOM_FileStream *>(this);
#endif // DOM_GADGET_FILE_API_SUPPORT
#if defined GADGET_SUPPORT
	else if (IsA(DOM_TYPE_WIDGET))
		return static_cast<DOM_Widget *>(this);
#endif // GADGET_SUPPORT
#ifdef WEBSOCKETS_SUPPORT
	else if (IsA(DOM_TYPE_WEBSOCKET))
		return static_cast<DOM_WebSocket *>(this);
#endif //WEBSOCKETS_SUPPORT
#if defined(DOM_CROSSDOCUMENT_MESSAGING_SUPPORT)
	else if (IsA(DOM_TYPE_MESSAGEPORT))
		return static_cast<DOM_MessagePort *>(this);
	/* Other DOM_TYPE_MESSAGE* types aren't event targets, so fall thru */
#endif // DOM_CROSSDOCUMENT_MESSAGING_SUPPORT
#if defined(DOM_WEBWORKERS_SUPPORT)
	else if (IsA(DOM_TYPE_WEBWORKERS_WORKER_OBJECT))
		return static_cast<DOM_WebWorkerObject *>(this);
	else if (IsA(DOM_TYPE_WEBWORKERS_WORKER))
		return static_cast<DOM_WebWorker *>(this);
#endif	// DOM_WEBWORKERS_SUPPORT

#ifdef EVENT_SOURCE_SUPPORT
	else if (IsA(DOM_TYPE_EVENTSOURCE))
		return static_cast<DOM_EventSource *>(this);
#endif // EVENT_SOURCE_SUPPORT

#ifdef DOM_HTTP_SUPPORT
	else if (IsA(DOM_TYPE_XMLHTTPREQUEST))
		return static_cast<DOM_XMLHttpRequest *>(this);
# ifdef PROGRESS_EVENTS_SUPPORT
	else if (IsA(DOM_TYPE_XMLHTTPREQUEST_UPLOAD))
		return static_cast<DOM_XMLHttpRequestUpload *>(this);
# endif // PROGRESS_EVENTS_SUPPORT
#endif // DOM_HTTP_SUPPORT

#if defined(EXTENSION_SUPPORT)
	else if (IsA(DOM_TYPE_EXTENSION_UIITEM))
		return static_cast<DOM_ExtensionUIItem *>(this);
	else if (IsA(DOM_TYPE_EXTENSION_PAGE_CONTEXT))
		return static_cast<DOM_ExtensionPageContext *>(this);
	else if (IsA(DOM_TYPE_EXTENSION_BACKGROUND))
		return static_cast<DOM_ExtensionBackground *>(this);
	else if (IsA(DOM_TYPE_EXTENSION))
		return static_cast<DOM_Extension *>(this);
# if defined(URL_FILTER)
	else if (IsA(DOM_TYPE_EXTENSION_URLFILTER))
		return static_cast<DOM_ExtensionURLFilter *>(this);
# endif // URL_FILTER
#endif // EXTENSION_SUPPORT

#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT
	else if (IsA(DOM_TYPE_EXTENSION_MENUCONTEXT))
		return static_cast<DOM_ExtensionMenuContext *>(this);
	else if (IsA(DOM_TYPE_EXTENSION_MENUCONTEXT_PROXY))
		return static_cast<DOM_ExtensionMenuContextProxy *>(this);
#endif // DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT

#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT
	else if (IsA(DOM_TYPE_BROWSER_TAB_MANAGER))
		return static_cast<DOM_BrowserTabManager *>(this);
	else if (IsA(DOM_TYPE_BROWSER_WINDOW_MANAGER))
		return static_cast<DOM_BrowserWindowManager *>(this);
	else if (IsA(DOM_TYPE_BROWSER_TAB_GROUP_MANAGER))
		return static_cast<DOM_BrowserTabGroupManager *>(this);
#endif // DOM_EXTENSIONS_TAB_API_SUPPORT

#ifdef DOM_STREAM_API_SUPPORT
	else if (IsA(DOM_TYPE_LOCALMEDIASTREAM))
		return static_cast<DOM_LocalMediaStream *>(this);
#endif // DOM_STREAM_API_SUPPORT
	else
		return NULL;
}

OP_STATUS
DOM_Object::CreateEventTarget()
{
	DOM_EventTargetOwner *owner = GetEventTargetOwner();

	if (!owner)
		return OpStatus::ERR;
	else
		return owner->CreateEventTarget();
}

DOM_EventTarget *
DOM_Object::GetEventTarget()
{
	DOM_EventTargetOwner *owner = GetEventTargetOwner();

	if (!owner)
		return NULL;
	else
		return owner->FetchEventTarget();
}

/* static */ ES_Thread *
DOM_Object::GetCurrentThread(ES_Runtime *runtime)
{
	ES_ThreadScheduler *scheduler = runtime->GetESScheduler();

	if (scheduler->IsActive())
		return scheduler->GetCurrentThread();
	else
		return NULL;
}

/* static */ const ServerName *
DOM_Object::GetServerName(const URL url)
{
	return static_cast<const ServerName *>(url.GetAttribute(URL::KServerName, (void *) NULL));
}

/* static */ void
DOM_Object::GCMark(ES_Runtime *runtime, ES_Object *object)
{
	if (object)
		runtime->GCMark(object);
}

/* static */ void
DOM_Object::GCMark(ES_Runtime *runtime, DOM_Object *object)
{
	if (object)
		runtime->GCMark(*object);
}

void
DOM_Object::GCMark(ES_Object *object)
{
	if (object)
		GetRuntime()->GCMark(object);
}

/* static */ void
DOM_Object::GCMark(DOM_Object *object)
{
	if (object)
		object->GetRuntime()->GCMark(object);
}

/* static */ void
DOM_Object::GCMark(ES_Runtime *runtime, ES_Value &value)
{
	if (value.type == VALUE_OBJECT)
		runtime->GCMark(value.value.object);
}

void
DOM_Object::GCMark(ES_Value &value)
{
	if (value.type == VALUE_OBJECT)
		GetRuntime()->GCMark(value.value.object);
}

/* static */ void
DOM_Object::GCMark(DOM_EventTarget *event_target)
{
	if (event_target)
		event_target->GCTrace();
}

ES_GetState
DOM_Object::GetEventProperty(const uni_char *property_name, ES_Value *value, DOM_Runtime *origining_runtime)
{
	if (property_name[0] == 'o' && property_name[1] == 'n')
	{
#ifdef DOM3_EVENTS
		DOM_EventType type = DOM_Event::GetEventType(NULL, property_name + 2, TRUE);
#else // DOM3_EVENTS
		DOM_EventType type = DOM_Event::GetEventType(property_name + 2, TRUE);
#endif // DOM3_EVENTS

		if (type != DOM_EVENT_NONE)
		{
			if (!OriginCheck(origining_runtime))
				return GET_SECURITY_VIOLATION;

			if (DOM_EventTarget *event_target = GetEventTarget())
			{
				if (event_target)
				{
					ES_Object *handler = NULL;

					OP_BOOLEAN result = event_target->FindOldStyleHandler(type, value ? &handler : NULL);
					GET_FAILED_IF_ERROR(result);

					if (result == OpBoolean::IS_TRUE)
					{
						if (value)
							DOMSetObject(value, handler);

						return GET_SUCCESS;
					}
				}
			}

			if (DOM_Event::IsAlwaysPresentAsProperty(this, type))
			{
				DOMSetNull(value);
				return GET_SUCCESS;
			}
		}
	}

	return GET_FAILED;
}

ES_PutState
DOM_Object::PutEventProperty(const uni_char *property_name, ES_Value *value, DOM_Runtime *origining_runtime)
{
#ifdef DOM3_EVENTS
	DOM_EventType type = DOM_Event::GetEventType(NULL, property_name + 2, TRUE);
#else // DOM3_EVENTS
	DOM_EventType type = DOM_Event::GetEventType(property_name + 2, TRUE);
#endif // DOM3_EVENTS

	if (type != DOM_EVENT_NONE)
	{
		if (!OriginCheck(origining_runtime))
			return PUT_SECURITY_VIOLATION;

		PUT_FAILED_IF_ERROR(SetInternalValue(property_name, *value));

		if (value->type == VALUE_OBJECT && ES_Runtime::IsFunctionObject(value->value.object))
		{
			PUT_FAILED_IF_ERROR(CreateEventTarget());

			DOM_EventListener *listener = OP_NEW(DOM_EventListener, ());

#ifdef ECMASCRIPT_DEBUGGER
			// If debugging is enabled we store the caller which registered the event listener.
			ES_Context *context = DOM_Object::GetCurrentThread(origining_runtime)->GetContext();
			if (origining_runtime->IsContextDebugged(context))
			{
				ES_Runtime::CallerInformation call_info;
				OP_STATUS status = ES_Runtime::GetCallerInformation(context, call_info);
				if (OpStatus::IsSuccess(status))
					listener->SetCallerInformation(call_info.script_guid, call_info.line_no);
			}
#endif // ECMASCRIPT_DEBUGGER

			if (!listener || OpStatus::IsMemoryError(listener->SetNative(GetEnvironment(), type, NULL, FALSE, TRUE, *this, value->value.object)))
			{
				if (listener)
					DOM_EventListener::DecRefCount(listener);
				return PUT_NO_MEMORY;
			}

			GetEventTarget()->AddListener(listener);
		}
		else if (DOM_EventTarget *event_target = GetEventTarget())
		{
			DOM_EventListener listener;

			OpStatus::Ignore(listener.SetNative(GetEnvironment(), type, NULL, FALSE, TRUE, NULL, NULL));

			event_target->RemoveListener(&listener);
		}

		SignalPropertySetChanged();

		return PUT_SUCCESS;
	}
	else
		return PUT_FAILED;
}

/* virtual */
DOM_ProxyObject::~DOM_ProxyObject()
{
	if (provider)
		provider->ProxyObjectDestroyed(this);
}

/* static */ OP_STATUS
DOM_ProxyObject::Make(DOM_ProxyObject *&object, DOM_Runtime *runtime, Provider *provider)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(object = OP_NEW(DOM_ProxyObject, ()), runtime));
	object->Init(provider);
	return OpStatus::OK;
}

void
DOM_ProxyObject::Init(Provider *provider)
{
	SetHasMultipleIdentities();
	this->provider = provider;
}

/* virtual */ OP_STATUS
DOM_ProxyObject::Identity(ES_Object **loc)
{
	if (provider)
	{
		// We _are_ going to merge in a second. Make sure we merge immediately instead
		// to avoid unnecessary fragmentation and chunk allocations.
		GetRuntime()->SuitableForReuse(TRUE);
		OP_STATUS status = provider->GetObject(object);
		GetRuntime()->SuitableForReuse(FALSE);
		if (OpStatus::IsMemoryError(status))
			return status;
		else if (OpStatus::IsError(status))
			object = NULL;
		else if (object)
			if (OpStatus::IsError(status = GetRuntime()->MergeHeapWith(object->GetRuntime())))
			{
				object = NULL;
				return status;
			}
	}

	if (object)
		return object->Identity(loc);
	else
	{
		*loc = *this;
		return OpStatus::OK;
	}
}

/* virtual */ void
DOM_ProxyObject::GCTrace()
{
	GCMark(object);
}

/* virtual */ BOOL
DOM_ProxyObject::SecurityCheck(ES_Runtime *origining_runtime)
{
	// This is only called if the proxy object has no current object.
	// In that case it doesn't really matter what we do, so allow all
	// access to it to avoid bogus error messages.
	return TRUE;
}

OP_STATUS
DOM_ProxyObject::GetObject(DOM_Object *&object_out)
{
	if (provider)
		RETURN_IF_ERROR(provider->GetObject(object));

	object_out = object;
	return OpStatus::OK;
}

DOM_Object *
DOM_ProxyObject::GetObject()
{
	return object;
}

void
DOM_ProxyObject::SetObject(DOM_Object *new_object)
{
	OP_ASSERT(!provider);
	if (object)
		SignalIdentityChange(object->GetNativeObject());
	object = new_object;
}

void
DOM_ProxyObject::ResetObject()
{
	if (object)
		SignalIdentityChange(object->GetNativeObject());
	object = NULL;
}

void
DOM_ProxyObject::SetProvider(Provider *new_provider)
{
	if (new_provider)
		ResetObject();

	provider = new_provider;
}

void
DOM_ProxyObject::DisableCaches()
{
	if (object)
		SignalDisableCaches(object->GetNativeObject());
}

/* static */ OP_STATUS
DOM_WindowProxyObject::Make(DOM_ProxyObject *&object, DOM_Runtime *runtime, Provider *provider)
{
	DOM_WindowProxyObject* window_proxy_object;
	RETURN_IF_ERROR(DOMSetObjectRuntime(window_proxy_object = OP_NEW(DOM_WindowProxyObject, ()), runtime));
	window_proxy_object->Init(provider);
	object = window_proxy_object;
	return OpStatus::OK;
}

ES_GetState
DOM_WindowProxyObject::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_closed)
	{
		DOMSetBoolean(value, TRUE);
		return GET_SUCCESS;
	}

	return GET_FAILED;
}

int
DOM_Function::Call(ES_Object* this_object0, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
	DOM_Object *this_object = DOM_HOSTOBJECT(this_object0, DOM_Object);

	/* If there is no this object, the function call will either (and most
	likely) fail immediately, or not use it at all.  In either case, it
	is not used (since it isn't there) and the lack of security check
	won't matter. */

	if (this_object && !this_object->SecurityCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	return impl(this_object, argv, argc, return_value, (DOM_Runtime *) origining_runtime);
}

int
DOM_InsecureFunction::Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
	return impl(DOM_HOSTOBJECT(this_object, DOM_Object), argv, argc, return_value, (DOM_Runtime *) origining_runtime);
}

#ifdef DOM_ACCESS_SECURITY_RULES
/* static */ const uni_char *
DOM_FunctionWithSecurityRule::GetStringArgument(ES_Value* argv, int argc, int arg_index)
{
	if (arg_index >= 0 && arg_index < argc && argv[arg_index].type == VALUE_STRING)
		return argv[arg_index].value.string;
	else
		return NULL;
}

int
DOM_FunctionWithSecurityRule::Call(ES_Object* this_object0, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime0)
{
	DOM_Object* this_object = DOM_HOSTOBJECT(this_object0, DOM_Object);
	DOM_Runtime* origining_runtime = static_cast<DOM_Runtime *>(origining_runtime0);

	if (argc < 0)
		CALL_FAILED_IF_ERROR(DOM_CallState::RestoreArgumentsFromRestartValue(return_value, argv, argc));

	if (this_object && !this_object->SecurityCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	int status = ES_VALUE;
	if (this_object && security_rule)
	{
		const uni_char* arg1 = GetStringArgument(argv, argc, security_rule->arg_1_index);
		const uni_char* arg2 = GetStringArgument(argv, argc, security_rule->arg_2_index);

		status = PropertyAccessSecurityCheck(return_value, argv, argc, origining_runtime, return_value, DOM_Object::ACCESS_TYPE_FUNCTION_INVOCATION, security_rule->rule_name, arg1, arg2);
	}

	if (status == ES_VALUE)
		return impl(this_object, argv, argc, return_value, (DOM_Runtime *) origining_runtime);
	else
		return status;
};
#endif // DOM_ACCESS_SECURITY_RULES

int
DOM_FunctionWithData::Call(ES_Object* this_object0, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
	DOM_Object *this_object = DOM_HOSTOBJECT(this_object0, DOM_Object);

	/* If there is no this object, the function call will either (and most
	   likely) fail immediately, or not use it at all.  In either case, it
	   is not used (since it isn't there) and the lack of security check
	   won't matter. */
	if (this_object && !this_object->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	return ((DOM_FunctionWithDataImpl *) impl)(this_object, argv, argc, return_value, (DOM_Runtime *) origining_runtime, data);
}

int
DOM_InsecureFunctionWithData::Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
	return ((DOM_FunctionWithDataImpl *) impl)(DOM_HOSTOBJECT(this_object, DOM_Object), argv, argc, return_value, (DOM_Runtime *) origining_runtime, data);
}

int
DOM_dummyMethod(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	return ES_FAILED;
}

int
DOM_toString(ES_Object* object, TempBuffer* buffer, ES_Value* return_value)
{
	CALL_FAILED_IF_ERROR(buffer->Append("[object "));
	CALL_FAILED_IF_ERROR(buffer->Append(ES_Runtime::GetClass(object)));
	CALL_FAILED_IF_ERROR(buffer->Append("]"));

	DOM_Object::DOMSetString(return_value, buffer);
	return ES_VALUE;
}

int
DOM_toString(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	TempBuffer *buffer = DOM_Object::GetEmptyTempBuf();
	return DOM_toString(*this_object, buffer, return_value);
}

int
DOM_Constructor::Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
	return Construct(argv, argc, return_value, origining_runtime);
}

DOM_BuiltInConstructor::DOM_BuiltInConstructor(DOM_Runtime::Prototype prototype)
		: prototype(prototype),
		  prototype_type(OBJECT_PROTOTYPE)
{
}

DOM_BuiltInConstructor::DOM_BuiltInConstructor(DOM_Runtime::HTMLElementPrototype html_element_prototype)
	: html_element_prototype(html_element_prototype),
	  prototype_type(HTML_ELEMENT_PROTOTYPE)
{
}

#ifdef SVG_DOM
DOM_BuiltInConstructor::DOM_BuiltInConstructor(DOM_Runtime::SVGObjectPrototype prototype)
	: prototype_type(SVG_OBJECT_PROTOTYPE)
{
	svg_object_prototype = prototype;
}

DOM_BuiltInConstructor::DOM_BuiltInConstructor(DOM_Runtime::SVGElementPrototype prototype, DOM_SVGElementInterface ifs)
	: prototype_type(SVG_ELEMENT_PROTOTYPE)
{
	svg_element_prototype = prototype;
	svg_element_ifs = ifs;
}
#endif // SVG_DOM

DOM_BuiltInConstructor::DOM_BuiltInConstructor(DOM_Prototype *prototype)
	: prototype_type(HOST_OBJECT_PROTOTYPE)
{
	host_object_prototype = prototype;
}

ES_Object *
DOM_BuiltInConstructor::GetPrototypeObject()
{
	DOM_Runtime *runtime = GetRuntime();

	switch(prototype_type)
	{
	case OBJECT_PROTOTYPE:
		return runtime->GetPrototype(prototype);

	case HTML_ELEMENT_PROTOTYPE:
		return runtime->GetHTMLElementPrototype(html_element_prototype);

#ifdef SVG_DOM
	case SVG_ELEMENT_PROTOTYPE:
		return runtime->GetSVGElementPrototype(svg_element_prototype, svg_element_ifs);

	case SVG_OBJECT_PROTOTYPE:
		return runtime->GetSVGObjectPrototype(svg_object_prototype);
#endif // SVG_DOM

	case HOST_OBJECT_PROTOTYPE:
		return *host_object_prototype;

	default:
		OP_ASSERT(!"Unknown prototype type");
		return NULL; // Better way to handle this? Ignore?
	}
}

/* virtual */ ES_GetState
DOM_BuiltInConstructor::GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	// We implement this in the string version of GetName rather than in the OpAtom version
	// to keep "prototype" out of enumerations. This will bypass some security code
	// but since the prototype and constructor come from the same place, that security
	// check isn't necessary.
	if (property_code == OP_ATOM_prototype)
	{
		if (value)
		{
			ES_Object *object = GetPrototypeObject();

			if (!object)
				return GET_NO_MEMORY;

			DOMSetObject(value, object);
		}

		return GET_SUCCESS;
	}

	return DOM_Object::GetName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_BuiltInConstructor::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	return DOM_Object::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_BuiltInConstructor::PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	return DOM_Object::PutName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_BuiltInConstructor::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	return property_name == OP_ATOM_prototype ? PUT_READ_ONLY : PUT_FAILED;
}

/* virtual */ int
DOM_BuiltInConstructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime)
{
	return CallDOMException(DOM_Object::NOT_SUPPORTED_ERR, return_value);
}

/* static */ ES_Object *
DOM_Prototype::MakeL(ES_Object *prototype, const char* class_name, const char *constructor_name, const DOM_FunctionDesc *functions, const DOM_FunctionWithDataDesc *functions_with_data, DOM_Runtime *runtime)
{
	ES_Object *object;

	unsigned size = 0;

	const DOM_FunctionDesc *functions1 = functions;
	if (functions1)
		while (functions1->impl)
			++size, ++functions1;

	const DOM_FunctionWithDataDesc *functions_with_data1 = functions_with_data;
	if (functions_with_data1)
		while (functions_with_data1->impl)
			++size, ++functions_with_data1;

	if (prototype == runtime->GetErrorPrototype())
		LEAVE_IF_ERROR(runtime->CreateErrorPrototype(&object, prototype, constructor_name));
	else
		LEAVE_IF_ERROR(runtime->CreatePrototypeObject(&object, prototype, class_name, size));

	if (functions)
		while (functions->impl)
		{
#ifdef DOM_ACCESS_SECURITY_RULES
			const DOM_FunctionSecurityRuleDesc* security_rule = functions->security_rule.rule_name ? &functions->security_rule : NULL;
			AddFunctionL(object, runtime, functions->impl, functions->name, functions->arguments, NULL, security_rule);
#else
			AddFunctionL(object, runtime, functions->impl, functions->name, functions->arguments);
#endif // DOM_ACCESS_SECURITY_RULES
			++functions;
		}

	if (functions_with_data)
		while (functions_with_data->impl)
		{
			AddFunctionL(object, runtime, functions_with_data->impl, functions_with_data->data, functions_with_data->name, functions_with_data->arguments);
			++functions_with_data;
		}

	return object;
}

/* static */ OP_STATUS
DOM_Prototype::Make(ES_Object *&object, ES_Object *prototype, const char *class_name, const char *constructor_name, const DOM_FunctionDesc *functions, const DOM_FunctionWithDataDesc *functions_with_data, DOM_Runtime *runtime)
{
	TRAP_AND_RETURN(status, object = MakeL(prototype, class_name, constructor_name, functions, functions_with_data, runtime));
	return OpStatus::OK;
}

/* virtual */
DOM_Prototype::~DOM_Prototype()
{
	OP_DELETE(insecure_storage);
}

void
DOM_Prototype::InitializeL()
{
	if (functions)
		while (functions->impl)
		{
#ifdef DOM_ACCESS_SECURITY_RULES
			const DOM_FunctionSecurityRuleDesc* security_rule = functions->security_rule.rule_name ? &functions->security_rule : NULL;
			AddFunctionL(functions->impl, functions->name, functions->arguments, &insecure_storage, security_rule);
#else // DOM_ACCESS_SECURITY_RULES
			AddFunctionL(functions->impl, functions->name, functions->arguments, &insecure_storage);
#endif // DOM_ACCESS_SECURITY_RULES
			++functions;
		}

	if (functions_with_data)
		while (functions_with_data->impl)
		{
			AddFunctionL(functions_with_data->impl, functions_with_data->data, functions_with_data->name, functions_with_data->arguments, &insecure_storage);
			++functions_with_data;
		}

#ifdef DOM_LIBRARY_FUNCTIONS
	if (library_functions)
		while (library_functions->impl)
		{
			AddLibraryFunctionL(library_functions->impl, library_functions->name, &insecure_storage);
			++library_functions;
		}
#endif // DOM_LIBRARY_FUNCTIONS
}

/* virtual */ ES_GetState
DOM_Prototype::GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (DOM_PropertyStorage::Get(insecure_storage, property_name, value))
		return GET_SUCCESS;
	else
		return DOM_Object::GetName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_Prototype::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	return GET_FAILED;
}

/* virtual */ ES_GetState
DOM_Prototype::FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime)
{
	DOM_PropertyStorage::FetchPropertiesL(enumerator, insecure_storage);
	return GET_SUCCESS;
}

/* virtual */ void
DOM_Prototype::GCTrace()
{
	DOM_PropertyStorage::GCTrace(GetRuntime(), insecure_storage);
}

#ifdef DOM_LIBRARY_FUNCTIONS

void
DOM_Prototype::AddLibraryFunctionL(const DOM_LibraryFunction *impl, const char *name)
{
	DOM_Object *scopeobject;
	ES_Object *scope[1];
	unsigned scope_length;

	if (impl->bindings)
	{
		LEAVE_IF_ERROR(DOMSetObjectRuntime(scopeobject = OP_NEW(DOM_Object, ()), GetRuntime()));

		const DOM_LibraryFunction::Binding *bindings = impl->bindings;
		int index = 0;

		while (bindings->impl)
		{
			char name[16]; // ARRAY OK jl 2008-02-28
			op_snprintf(name, 16, "$%d", index);

			if (bindings->data == INT_MAX)
				scopeobject->AddFunctionL((DOM_FunctionImpl *) bindings->impl, name);
			else
				scopeobject->AddFunctionL((DOM_FunctionWithDataImpl *) bindings->impl, bindings->data, name);

			++bindings;
			++index;
		}

		scope[0] = *scopeobject;
		scope_length = 1;
	}
	else
		scope_length = 0;

	TempBuffer namebuffer; ANCHOR(TempBuffer, namebuffer);
	TempBuffer textbuffer; ANCHOR(TempBuffer, textbuffer);

	namebuffer.AppendL(name);

	const char *const *text = impl->text;

	while (*text)
	{
		textbuffer.AppendL(*text);
		++text;
	}

	ES_Object *function;
	ES_ErrorInfo err(UNI_L(""));

	OP_STATUS status = GetRuntime()->CreateFunction(scope, scope_length, textbuffer.GetStorage(), textbuffer.Length(), FALSE, &function, 0, NULL, &err);

	/* There should be no compile errors in these functions. */
	OP_ASSERT(status == OpStatus::OK || status == OpStatus::ERR_NO_MEMORY);

	LEAVE_IF_ERROR(status);

	ES_Value value;
	DOMSetObject(&value, function);

	LEAVE_IF_ERROR(GetRuntime()->PutName(GetNativeObject(), namebuffer.GetStorage(), value, 0));
}

#endif // DOM_LIBRARY_FUNCTIONS

/* static */ TempBuffer *
DOM_Object::GetTempBuf()
{
	return g_DOM_globalData ? &g_DOM_tempbuf : NULL;
}

/* static */ TempBuffer *
DOM_Object::GetEmptyTempBuf()
{
	TempBuffer *b = GetTempBuf();
	if (b)
	{
		b->Clear();
		b->SetCachedLengthPolicy(TempBuffer::TRUSTED);
	}
	return b;
}

/* virtual */ BOOL
DOM_InternalObject::SecurityCheck(ES_Runtime *origining_runtime)
{
	return TRUE;
}

/* static */ int
DOM_Object::TruncateDoubleToInt(double d)
{
	// This is the non-clamping conversion from WebIDL.
	return op_double2int32(d);
}

/* static */ unsigned
DOM_Object::TruncateDoubleToUInt(double d)
{
	// This is the non-clamping conversion from WebIDL.
	return op_double2uint32(d);
}

/* static */ unsigned short
DOM_Object::TruncateDoubleToUShort(double d)
{
	// This is the non-clamping conversion from WebIDL.
	return op_double2uint32(d) % (1 << 16);
}

union float_and_bits
{
	UINT32 bits;
	float f;
};
/* static */ float
DOM_Object::TruncateDoubleToFloat(double d)
{
	if (d > static_cast<double>(FLT_MAX))
	{
		float_and_bits inf;
		// Bit pattern for +Inf.0
		inf.bits = 0x7f800000;
		return inf.f;
	}
	else if (d < static_cast<double>(-FLT_MAX))
	{
		float_and_bits inf;
		// Bit pattern for -Inf.0
		inf.bits = 0xff800000;
		return inf.f;
	}
	return static_cast<float>(d);
}

/* static */ OP_STATUS
DOM_Object::DOMSetObjectAsHidden(ES_Value *value, DOM_Object *object)
{
	DOM_ProxyObject *proxy;
	RETURN_IF_ERROR(DOM_ProxyObject::Make(proxy, object->GetRuntime(), NULL));
	proxy->SetObject(object);
	ES_Runtime::MarkObjectAsHidden(*proxy);
	DOMSetObject(value, proxy);
	return OpStatus::OK;
}

/* static */ BOOL
DOM_Object::ConvertDoubleToUnsignedLongLong(double d, LongLongConversion kind, double &result)
{
	switch (kind)
	{
	default:
		OP_ASSERT(!"Unexpected kind.");
		/* fallthrough */
	case ModuloRange:
	{
		if (op_isnan(d) || !op_isfinite(d) || d == 0.0)
		{
			result = 0.;
			return TRUE;
		}

		if (d < 0.0)
			d = -op_floor(op_fabs(d));
		else
			d = op_floor(d);

		/* The spec (currently) says that (modulo 2^64) should be
		   performed, followed by a 2^53 range check. Do not impose
		   the range check for this conversion. */

		double two_pow_64 = op_pow(2, 64);
		if (0.0 <= d && d < two_pow_64)
			;
		else
		{
			double q = op_floor(d / two_pow_64);
			d = d - (q * two_pow_64);
		}
		if (d < 0.0)
			d = two_pow_64 - d;

		result = d;
		return TRUE;
	}
	case EnforceRange:
	{
		if (op_isnan(d) || !op_isfinite(d))
			return FALSE;

		d = op_truncate(d);
		double two_pow_53 = op_pow(2, 53);

		if (d < 0 || d > two_pow_53 - 1)
			return FALSE;

		result = d;
		return TRUE;
	}
	case Clamp:
	{
		if (op_isnan(d) || !op_isfinite(d) || d == 0.0)
		{
			result = 0.;
			return TRUE;
		}

		d = op_truncate(d);
		double two_pow_53 = op_pow(2, 53);

		result = MIN(MAX(d, 0), two_pow_53 - 1);
		return TRUE;
	}
	}
}

/* static */ BOOL
DOM_Object::ConvertDoubleToLongLong(double d, LongLongConversion kind, double &result)
{
	switch (kind)
	{
	default:
		OP_ASSERT(!"Unexpected kind.");
		/* fallthrough */
	case ModuloRange:
	{
		if (op_isnan(d) || !op_isfinite(d) || d == 0.0)
		{
			result = 0.;
			return TRUE;
		}

		if (d < 0.0)
			d = -op_floor(op_fabs(d));
		else
			d = op_floor(d);

		double two_pow_64 = op_pow(2, 64);
		if (d >= -two_pow_64/2 && d < two_pow_64/2)
			;
		else
		{
			double q = op_floor(d / two_pow_64);
			d = d - (q * two_pow_64);
		}

		if (d == 0.0)
		{
			result = 0.;
			return TRUE;
		}
		if (d < 0.0)
			if (d >= -two_pow_64/2)
			{
				result = d;
				return TRUE;
			}
			else
				d = d + two_pow_64;

		if (d >= two_pow_64/2)
			d = d - two_pow_64;

		result = d;
		return TRUE;
	}
	case EnforceRange:
	{
		if (op_isnan(d) || !op_isfinite(d))
			return FALSE;

		d = op_truncate(d);
		double two_pow_53 = op_pow(2, 53);

		if (d < (-two_pow_53 + 1) || d > two_pow_53 - 1)
			return FALSE;

		result = d;
		return TRUE;
	}
	case Clamp:
	{
		if (op_isnan(d) || !op_isfinite(d) || d == 0.0)
		{
			result = 0.;
			return TRUE;
		}

		d = op_truncate(d);
		double two_pow_53 = op_pow(2, 53);

		result = MIN(MAX(d, -two_pow_53 + 1), two_pow_53 - 1);
		return TRUE;
	}
	}
}
