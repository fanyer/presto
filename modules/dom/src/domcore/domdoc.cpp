/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/attr.h"
#include "modules/dom/src/domcore/chardata.h"
#include "modules/dom/src/domcore/comment.h"
#include "modules/dom/src/domcore/cdatasection.h"
#include "modules/dom/src/domcore/docfrag.h"
#include "modules/dom/src/domcore/element.h"
#include "modules/dom/src/domcore/procinst.h"
#include "modules/dom/src/domcore/text.h"
#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domevents/domprogressevent.h"
#include "modules/dom/src/domevents/domtransitionevent.h"
#include "modules/dom/src/domevents/domanimationevent.h"
#include "modules/dom/src/domevents/domtouchevent.h"
#include "modules/dom/src/domhtml/htmlcoll.h"
#include "modules/dom/src/domrange/range.h"
#include "modules/dom/src/domstylesheets/stylesheetlist.h"
#include "modules/dom/src/domtraversal/nodeiterator.h"
#include "modules/dom/src/domtraversal/treewalker.h"
#include "modules/dom/src/domxpath/xpathevaluator.h"
#include "modules/dom/src/js/window.h"
#include "modules/dom/src/domselectors/domselector.h"

#include "modules/ecmascript_utils/esasyncif.h"

#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"

#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/htm_ldoc.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

#include "modules/util/tempbuf.h"
#include "modules/xmlutils/xmldocumentinfo.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/xmlutils/xmlutils.h"

#ifdef DOCUMENT_EDIT_SUPPORT
# include "modules/documentedit/OpDocumentEdit.h"
#endif // DOCUMENT_EDIT_SUPPORT

#ifdef SVG_SUPPORT
# include "modules/svg/svg_image.h"
# include "modules/svg/SVGManager.h"
# include "modules/dom/src/domsvg/domsvgelementinstance.h"
#endif // SVG_SUPPORT

#ifdef DOM3_LOAD
# include "modules/dom/src/domload/lsparser.h"
# include "modules/dom/src/domload/lsinput.h"
#endif // DOM3_LOAD

#ifdef CLIENTSIDE_STORAGE_SUPPORT
# include "modules/dom/src/storage/storageevent.h"
#endif // CLIENTSIDE_STORAGE_SUPPORT

#ifdef DOM_CROSSDOCUMENT_MESSAGING_SUPPORT
# include "modules/dom/src/domwebworkers/domcrossmessage.h"
#endif // DOM_CROSSDOCUMENT_MESSAGING_SUPPORT

#ifdef DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
#include "modules/dom/src/orientation/deviceorientationevent.h"
#include "modules/dom/src/orientation/devicemotionevent.h"
#endif // DOM_DEVICE_ORIENTATION_EVENT_SUPPORT

#ifdef PI_UIINFO_TOUCH_EVENTS
#include "modules/pi/ui/OpUiInfo.h"
#endif // PI_UIINFO_TOUCH_EVENTS

DOM_Document::DOM_Document(DOM_DOMImplementation *implementation, BOOL is_xml)
	: DOM_Node(DOCUMENT_NODE),
	  placeholder(NULL),
	  root(NULL),
	  is_xml(is_xml),
	  xml_document_info(NULL),
	  implementation(implementation)
{
	SetOwnerDocument(this);
}

DOM_Document::~DOM_Document()
{
	if (placeholder)
		FreeElementTree(placeholder);

	OP_DELETE(xml_document_info);
}

/* status */ OP_STATUS
DOM_Document::Make(DOM_Document *&document, DOM_DOMImplementation *implementation, BOOL create_placeholder, BOOL is_xml)
{
	DOM_Runtime *runtime = implementation->GetRuntime();
	RETURN_IF_ERROR(DOMSetObjectRuntime(document = OP_NEW(DOM_Document, (implementation, is_xml)), runtime, runtime->GetPrototype(DOM_Runtime::DOCUMENT_PROTOTYPE), "Document"));

	if (create_placeholder)
		RETURN_IF_ERROR(document->ResetPlaceholderElement());

	return OpStatus::OK;
}

/* static */ void
DOM_Document::ConstructDocumentObjectL(ES_Object *object, DOM_Runtime *runtime)
{
#ifdef TOUCH_EVENTS_SUPPORT
# ifdef PI_UIINFO_TOUCH_EVENTS
	if (g_op_ui_info->IsTouchEventSupportWanted())
# endif // PI_UIINFO_TOUCH_EVENTS
	{
		AddFunctionL(object, runtime, DOM_Touch::createTouch, "createTouch", "OOnnnnn-");
		AddFunctionL(object, runtime, DOM_TouchList::createTouchList, "createTouchList", "(#Touch|[(#Touch|o)])-");
	}
#endif // TOUCH_EVENTS_SUPPORT

}

/* virtual */ ES_GetState
DOM_Document::GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	ES_GetState result = GetEventProperty(property_name, value, static_cast<DOM_Runtime *>(origining_runtime));
	if (result != GET_FAILED)
		return result;

	// Can't call GetName(uni_char*..) on our parent (DOM_Node) since C++ then thinks that we want DOM_Node::GetName(OpAtom, ...)
	return DOM_Object::GetName(property_name, property_code, value, origining_runtime);
}


static OP_STATUS
DOM_GetCookie(URL &url, const char *&cookie_out)
{
	OP_STATUS status = OpStatus::OK;
	OpStatus::Ignore(status);

	const char *OP_MEMORY_VAR cookie = NULL;
	int version, max_version;

	BOOL have_password = FALSE;
	BOOL have_authentication = FALSE;

#ifdef OOM_SAFE_API
	TRAP(status, cookie = urlManager->GetCookiesL(url.GetRep(), version, max_version,
	                                              !!url.GetAttribute(URL::KHavePassword), !!url.GetAttribute(URL::KHaveAuthentication),
	                                              have_password, have_authentication));
#else // OOM_SAFE_API
	TRAP(status, cookie = g_url_api->GetCookiesL(url, version, max_version,
	                                             !!url.GetAttribute(URL::KHavePassword), !!url.GetAttribute(URL::KHaveAuthentication),
	                                             have_password, have_authentication));
#endif // OOM_SAFE_API

	cookie_out = cookie;
	return status;
}

#if defined _ASK_COOKIE || defined COOKIE_USE_DNS_LOOKUP

class JS_CookieProcessor
	: public MessageObject,
	  public ES_ThreadListener // For cleanup.
{
private:
	MessageHandler *mh;
	ES_Thread *thread;
	URL url;
	BOOL done;

public:
	JS_CookieProcessor(MessageHandler *mh, ES_Thread *thread, URL url)
		: mh(mh),
		  thread(thread),
		  url(url),
		  done(FALSE)
	{
		thread->Block(ES_BLOCK_USER_INTERACTION);

		/* Adding this as a listener to a) get a notification if the
		   thread is cancelled and b) for cleanup, the thread owns the
		   listener and will delete it. */
		thread->AddListener(this);
	}

	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
	{
		if (g_url_api->HandlingCookieForURL(url))
			mh->PostDelayedMessage(MSG_ES_ASYNC_MESSAGE, 0, 0, 10);
		else
		{
			mh->UnsetCallBack(this, MSG_ES_ASYNC_MESSAGE);
			OpStatus::Ignore(thread->Unblock(ES_BLOCK_USER_INTERACTION));
			done = TRUE;
		}
	}

	virtual OP_STATUS Signal(ES_Thread *, ES_ThreadSignal signal)
	{
		if (!done)
		{
			OP_ASSERT(signal != ES_SIGNAL_FINISHED && signal != ES_SIGNAL_FAILED && "it is not possible to fail or finish while blocked");
			if (signal == ES_SIGNAL_CANCELLED)
				mh->UnsetCallBack(this, MSG_ES_ASYNC_MESSAGE);
		}
		return OpStatus::OK;
	}
};

#endif // _ASK_COOKIE

/* virtual */ ES_GetState
DOM_Document::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_documentElement:
		DOMSetObject(value, root);
		return GET_SUCCESS;

	case OP_ATOM_nodeName:
		DOMSetString(value, UNI_L("#document"));
		return GET_SUCCESS;

	case OP_ATOM_firstChild:
		if (placeholder)
			return DOMSetElement(value, placeholder->FirstChildActual());
		else
		{
			DOMSetNull(value);
			return GET_SUCCESS;
		}

	case OP_ATOM_lastChild:
		if (placeholder)
			return DOMSetElement(value, placeholder->LastChildActual());
		else
		{
			DOMSetNull(value);
			return GET_SUCCESS;
		}

	case OP_ATOM_parentNode:
		DOMSetNull(value);
		return GET_SUCCESS;

	case OP_ATOM_childNodes:
		if (value)
		{
			ES_GetState result = DOMSetPrivate(value, DOM_PRIVATE_childNodes);

			if (result != GET_FAILED)
				return result;

			DOM_SimpleCollectionFilter filter(CHILDNODES);
			DOM_Collection *collection;

			GET_FAILED_IF_ERROR(DOM_Collection::MakeNodeList(collection, GetEnvironment(), this, FALSE, FALSE, filter));

			collection->SetCreateSubcollections();

			GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_childNodes, *collection));

			DOMSetObject(value, collection);
		}
		return GET_SUCCESS;

	case OP_ATOM_doctype:
		if (value)
		{
			DOM_Node *doctype;
			GET_FAILED_IF_ERROR(GetDocumentType(doctype));
			DOMSetObject(value, doctype);
		}
		return GET_SUCCESS;

	case OP_ATOM_characterSet:
		if (value)
		{
			DOMSetString(value);
			if (HLDocProfile *hld_profile = GetHLDocProfile())
			{
				const char *charset = hld_profile->GetCharacterSet();
				if (charset)
				{
					TempBuffer *buffer = GetEmptyTempBuf();
					GET_FAILED_IF_ERROR(buffer->Append(charset));
					DOMSetString(value, buffer);
				}
			}
		}
		return GET_SUCCESS;

	case OP_ATOM_implementation:
		DOMSetObject(value, implementation);
		return GET_SUCCESS;

	case OP_ATOM_ownerDocument:
	case OP_ATOM_textContent:
		DOMSetNull(value);
		return GET_SUCCESS;

	case OP_ATOM_documentURI:
		return GetDocumentURI(value);

	case OP_ATOM_contentType:
		if (value)
		{
			TempBuffer *buffer = GetEmptyTempBuf();
			URL url = GetURL();

			if (!url.IsEmpty())
				// For HTTP requests, the header is usually present.
				GET_FAILED_IF_MEMORY_ERROR(buffer->Append(url.GetAttribute(URL::KHTTP_ContentType).CStr()));

			if (buffer->Length() == 0)
			{
				const char *mime_type;
				if (url.IsEmpty())
					// For a document created by the DOM API, without a url, we use the xml-ness
					// bit to decide whether to tell it's html or xml.
					mime_type = IsA(DOM_TYPE_XML_DOCUMENT) || IsXML() ? "application/xml" : "text/html";
				else
				{
					// For a non-HTTP request or a request that does not return a mime-type
					// then we use the sniffed type and convert it into a mime-type string.
					Viewer *viewer;
					GET_FAILED_IF_MEMORY_ERROR(g_viewers->FindViewerByURL(url, viewer, FALSE));
					mime_type = viewer != NULL ? viewer->GetContentTypeString8() : NULL;
				}

				if (mime_type)
					GET_FAILED_IF_ERROR(buffer->Append(mime_type));
			}
			DOMSetString(value, buffer);
		}
		return GET_SUCCESS;

	case OP_ATOM_parentWindow:
	case OP_ATOM_defaultView:
		if (value)
		{
			DOMSetNull(value);

			if (IsMainDocument())
				if (FramesDocument* doc = GetFramesDocument())
				{
					GET_FAILED_IF_ERROR(doc->GetDocManager()->ConstructDOMProxyEnvironment());
					DOM_Object *window;
					GET_FAILED_IF_ERROR(static_cast<DOM_ProxyEnvironmentImpl *>(doc->GetDocManager()->GetDOMEnvironment())->GetProxyWindow(window, origining_runtime));
					DOMSetObject(value, window);
				}
		}
		return GET_SUCCESS;

	case OP_ATOM_styleSheets:
		if (value)
		{
			ES_GetState result = DOMSetPrivate(value, DOM_PRIVATE_styleSheets);
			if (result != GET_FAILED)
				return result;

			DOM_StyleSheetList *stylesheetlist;

			GET_FAILED_IF_ERROR(DOM_StyleSheetList::Make(stylesheetlist, this));
			GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_styleSheets, *stylesheetlist));

			DOMSetObject(value, stylesheetlist);
		}
		return GET_SUCCESS;

#ifdef DOCUMENT_EDIT_SUPPORT
	case OP_ATOM_designMode:
		if (value)
		{
			BOOL designMode = FALSE;

			if (IsMainDocument())
				if (FramesDocument *frames_doc = GetFramesDocument())
					designMode = frames_doc->GetDesignMode();

			DOMSetString(value, designMode ? UNI_L("on") : UNI_L("off"));
		}
		return GET_SUCCESS;
#endif // DOCUMENT_EDIT_SUPPORT

	case OP_ATOM_readyState:
		GetDocumentReadiness(value, GetFramesDocument());
		return GET_SUCCESS;

	case OP_ATOM_referrer:
		DOMSetString(value);
		if (value && IsMainDocument())
			if (FramesDocument *frames_doc = GetFramesDocument())
				if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::ReferrerEnabled, GetHostName()))
				{
					URL ref_url = frames_doc->GetRefURL().url;

					switch (ref_url.Type())
					{
					case URL_HTTPS:
						if (frames_doc->GetURL().Type() != URL_HTTPS)
							break;

					case URL_HTTP:
						DOMSetString(value, ref_url.GetAttribute(URL::KUniName).CStr());
					}
				}
		return GET_SUCCESS;

	case OP_ATOM_lastModified:
		if (value)
		{
			time_t last_mod_time = 0;
			URL url = GetURL();
			url.GetAttribute(URL::KVLastModified, &last_mod_time, TRUE);
			if (!last_mod_time)
				last_mod_time = op_time(NULL);

			DOMSetString(value);
			if (last_mod_time != -1)
			{
				struct tm *last_modified = op_localtime(&last_mod_time);
				if (last_modified)
				{
					TempBuffer *buffer = GetEmptyTempBuf();
					OpString formatted;
					GET_FAILED_IF_ERROR(formatted.AppendFormat(UNI_L("%02d/%02d/%04d %02d:%02d:%02d"), last_modified->tm_mon + 1, last_modified->tm_mday, 1900 + last_modified->tm_year, last_modified->tm_hour, last_modified->tm_min, last_modified->tm_sec));
					GET_FAILED_IF_ERROR(buffer->Append(formatted.CStr()));
					DOMSetString(value, buffer);
				}
			}
		}
		return GET_SUCCESS;

	case OP_ATOM_location:
		if (value)
			if (IsMainDocument())
			{
				DOM_Object *window = GetEnvironment()->GetWindow();
				return window->GetName(OP_ATOM_location, value, origining_runtime);
			}
			else
				DOMSetNull(value);
		return GET_SUCCESS;

	case OP_ATOM_cookie:
		if (GetRuntime()->GetMutableOrigin()->IsUniqueOrigin())
			return GET_SECURITY_VIOLATION;

		DOMSetString(value);

		if (value && IsMainDocument())
		{
			if (FramesDocument *frames_doc = GetEnvironment()->GetFramesDocument())
			{
#if defined _ASK_COOKIE || defined COOKIE_USE_DNS_LOOKUP
				URL doc_url = frames_doc->GetURL();
				if (g_url_api->HandlingCookieForURL(doc_url))
				{
					// Set up a poll to check for processed cookies, and return a code
					// that blocks the thread.
					MessageHandler *mh = frames_doc->GetMessageHandler();
					ES_Thread *thread = GetCurrentThread(origining_runtime);

					JS_CookieProcessor *cookie_context = OP_NEW(JS_CookieProcessor, (mh, thread, doc_url));
					if (cookie_context == NULL || OpStatus::IsMemoryError(mh->SetCallBack(cookie_context, MSG_ES_ASYNC_MESSAGE, 0)))
					{
						OP_DELETE(cookie_context);
						return GET_NO_MEMORY;
					}

					mh->PostMessage(MSG_ES_ASYNC_MESSAGE, 0, 0);
					DOMSetNull(value);
					return GET_SUSPEND;
				}
				else
#endif //_ASK_COOKIE || defined COOKIE_USE_DNS_LOOKUP
				{
					return DOM_Document::GetNameRestart(property_name, value, origining_runtime, NULL);
				}
			}
		}

		return GET_SUCCESS;

	case OP_ATOM_hidden:
	case OP_ATOM_visibilityState:
		if (value)
		{
			BOOL visible = FALSE;
			if (FramesDocument* doc = GetFramesDocument())
				if (IsMainDocument() && doc->GetWindow()->IsVisibleOnScreen())
					visible = TRUE;

			if (property_name == OP_ATOM_hidden)
				DOMSetBoolean(value, !visible);
			else
				DOMSetString(value, visible ? UNI_L("visible") : UNI_L("hidden"));
		}
		return GET_SUCCESS;

#ifdef DOM_FULLSCREEN_MODE
	case OP_ATOM_fullscreenEnabled:
		if (value)
		{
			DOMSetBoolean(value, FALSE);
			if (GetFramesDocument())
				DOMSetBoolean(value, GetFramesDocument()->DOMGetFullscreenEnabled());
		}
		return GET_SUCCESS;

	case OP_ATOM_fullscreenElement:
		if (value)
		{
			DOMSetNull(value);
			if (GetFramesDocument())
				GET_FAILED_IF_ERROR(DOMSetElement(value, GetFramesDocument()->GetFullscreenElement()));
		}
		return GET_SUCCESS;
#endif // DOM_FULLSCREEN_MODE

	case OP_ATOM_dir:
		if (root)
			return root->GetName(OP_ATOM_dir, value, origining_runtime);

		DOMSetString(value);
		return GET_SUCCESS;

	default:
		return DOM_Node::GetName(property_name, value, origining_runtime);
	}
}


/* virtual */ ES_PutState
DOM_Document::PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name[0] == 'o' && property_name[1] == 'n')
	{
		ES_PutState state = PutEventProperty(property_name, value, static_cast<DOM_Runtime *>(origining_runtime));
		if (state != PUT_FAILED)
			return state;
	}

	return DOM_Node::PutName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_Document::GetNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	OP_ASSERT(property_name == OP_ATOM_cookie);

	DOMSetString(value);

	if (FramesDocument *frames_doc = GetEnvironment()->GetFramesDocument())
	{
		const char *cookie;
		GET_FAILED_IF_ERROR(DOM_GetCookie(frames_doc->GetURL(), cookie));

		if (cookie)
		{
			// We assume UTF8 encoded (at least that is what we insert)
			OpString decoder;
			GET_FAILED_IF_ERROR(decoder.SetFromUTF8(cookie));
			TempBuffer *buffer = GetEmptyTempBuf();
			GET_FAILED_IF_ERROR(buffer->Append(decoder.CStr()));
			DOMSetString(value, buffer);
		}
	}

	return GET_SUCCESS;
}

/* virtual */ ES_PutState
DOM_Document::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_doctype:
	case OP_ATOM_implementation:
	case OP_ATOM_documentElement:
	case OP_ATOM_parentWindow:
	case OP_ATOM_defaultView:
	case OP_ATOM_documentURI: // Shouldn't really be read-only.
	case OP_ATOM_referrer:
	case OP_ATOM_lastModified:
	case OP_ATOM_hidden:
	case OP_ATOM_visibilityState:
		return PUT_READ_ONLY;

#ifdef DOCUMENT_EDIT_SUPPORT
	case OP_ATOM_designMode:
		if (IsMainDocument())
			if (FramesDocument *frames_doc = GetEnvironment()->GetFramesDocument())
			{
				if (value->type != VALUE_STRING)
					return PUT_NEEDS_STRING;

				if (uni_stri_eq(value->value.string, "ON"))
				{
					frames_doc->GetHLDocProfile()->SetHandleScript(FALSE);
					frames_doc->SetEditable(FramesDocument::DESIGNMODE);
				}
				else if (uni_stri_eq(value->value.string, "OFF"))
				{
					frames_doc->GetHLDocProfile()->SetHandleScript(TRUE);
					frames_doc->SetEditable(FramesDocument::DESIGNMODE_OFF);
				}

				return PUT_SUCCESS;
			}
		return DOM_PUTNAME_DOMEXCEPTION(NOT_SUPPORTED_ERR);
#endif // DOCUMENT_EDIT_SUPPORT

	case OP_ATOM_textContent:
		return PUT_SUCCESS;

	case OP_ATOM_location:
		if (IsMainDocument())
			return GetEnvironment()->GetWindow()->PutName(OP_ATOM_location, value, origining_runtime);
		else
			return PUT_SUCCESS;

	case OP_ATOM_cookie:
		if (GetRuntime()->GetMutableOrigin()->IsUniqueOrigin())
			return PUT_SECURITY_VIOLATION;
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		else if (IsMainDocument())
			if (FramesDocument* frames_doc = GetEnvironment()->GetFramesDocument())
			{
				URL url = frames_doc->GetURL();

				OpString8 cookie;
				PUT_FAILED_IF_ERROR(cookie.SetUTF8FromUTF16(value->value.string));

				TRAPD(op_err, g_url_api->HandleSingleCookieL(url, cookie.CStr(), cookie.CStr(), 0, frames_doc->GetWindow()));
				if (OpStatus::IsMemoryError(op_err))
					return PUT_NO_MEMORY;

#if defined _ASK_COOKIE || defined COOKIE_USE_DNS_LOOKUP
				if (g_url_api->HandlingCookieForURL(url))
				{
					// Set up a poll to check for processed cookies, and return a code
					// that blocks the thread.
					MessageHandler *mh = frames_doc->GetMessageHandler();
					ES_Thread *thread = GetCurrentThread(origining_runtime);

					JS_CookieProcessor *cookie_context = OP_NEW(JS_CookieProcessor, (mh, thread, url));
					if (cookie_context == NULL || OpStatus::IsMemoryError(mh->SetCallBack(cookie_context, MSG_ES_ASYNC_MESSAGE, 0)))
					{
						OP_DELETE(cookie_context);
						return PUT_NO_MEMORY;
					}

					mh->PostMessage(MSG_ES_ASYNC_MESSAGE, 0, 0);
				}
#endif // _ASK_COOKIE
			}
		return PUT_SUCCESS;

#ifdef DOM_FULLSCREEN_MODE
	case OP_ATOM_fullscreenEnabled:
	case OP_ATOM_fullscreenElement:
		return PUT_READ_ONLY;
#endif // DOM_FULLSCREEN_MODE

	case OP_ATOM_dir:
		if (root)
			return root->PutName(OP_ATOM_dir, value, origining_runtime);
		else
			return PUT_SUCCESS;

	default:
		return DOM_Node::PutName(property_name, value, origining_runtime);
	}
}

/* virtual */ BOOL
DOM_Document::AllowOperationOnProperty(const uni_char *property_name, PropertyOperation op)
{
	if (uni_str_eq(property_name, "location"))
		return FALSE;
	else
		return TRUE;
}

/* virtual */ void
DOM_Document::GCTraceSpecial(BOOL via_tree)
{
	DOM_Node::GCTraceSpecial(via_tree);

	if (!via_tree && placeholder)
		TraceElementTree(placeholder);

	GCMark(implementation);
}

URL
DOM_Document::GetURL()
{
	if (url.IsEmpty() && IsMainDocument() && GetFramesDocument())
		return GetFramesDocument()->GetURL();
	else
		return url;
}

ES_GetState
DOM_Document::GetDocumentURI(ES_Value *value)
{
	if (value)
	{
		URL url = GetURL();
		if (url.IsEmpty())
			DOMSetNull(value);
		else
		{
			TempBuffer *buffer = GetEmptyTempBuf();
			GET_FAILED_IF_ERROR(buffer->Append(url.GetAttribute(URL::KUniName).CStr()));
			DOMSetString(value, buffer);
		}
	}
	return GET_SUCCESS;
}

OP_STATUS
DOM_Document::ResetPlaceholderElement(HTML_Element *new_placeholder)
{
	if (placeholder)
		placeholder->SetESElement(NULL);

	placeholder = new_placeholder;

	if (!placeholder)
		RETURN_IF_ERROR(HTML_Element::DOMCreateNullElement(placeholder, implementation->GetEnvironment()));

	placeholder->SetESElement(this);
	return OpStatus::OK;
}

void
DOM_Document::ClearPlaceholderElement()
{
	if (placeholder)
		placeholder->SetESElement(NULL);

	placeholder = NULL;
}

void
DOM_Document::SetDocRootElement(HTML_Element* doc_root)
{
	OP_ASSERT(doc_root->Type() == HE_DOC_ROOT);
	OP_ASSERT(!doc_root->GetESElement());
	OP_ASSERT(!placeholder);

	placeholder = doc_root;
	doc_root->SetESElement(this);
}

/* virtual */ OP_STATUS
DOM_Document::SetRootElement(DOM_Element *new_root)
{
	if (new_root)
	{
		if (new_root->GetThisElement()->ParentActual() != placeholder)
		{
			if (placeholder)
				placeholder->SetESElement(NULL);

			placeholder = new_root->GetThisElement()->ParentActual();
			placeholder->SetESElement(this);
		}

		root = new_root;
		root->SetIsSignificant();
	}
	else
		root = NULL;

	return OpStatus::OK;
}

OP_STATUS
DOM_Document::GetDocumentType(DOM_Node *&doctype)
{
	doctype = NULL;

	if (placeholder)
	{
		HTML_Element *child = placeholder->FirstChildActual();
		while (child)
		{
			if (child->Type() == HE_DOCTYPE)
				return GetEnvironment()->ConstructNode(doctype, child, this);

			child = child->SucActual();
		}
	}

	return OpStatus::OK;
}

LogicalDocument *
DOM_Document::GetLogicalDocument()
{
	if (IsMainDocument())
		return DOM_Object::GetLogicalDocument();
	else
		return NULL;
}

int
DOM_Document::GetDefaultNS()
{
	if (IsXML())
		return root ? g_ns_manager->GetNsIdxWithoutPrefix(root->GetThisElement()->GetNsIdx()) : NS_IDX_DEFAULT;
	else
		return NS_IDX_HTML;
}

BOOL
DOM_Document::IsMainDocument()
{
	return GetEnvironment()->GetDocument() == this;
}

/* static */ OP_STATUS
DOM_Document::GetProxy(DOM_Object *&document, ES_Runtime *origining_runtime)
{
	DOM_EnvironmentImpl *environment = document->GetEnvironment();

	if (document->IsA(DOM_TYPE_DOCUMENT) && ((DOM_Document *) document)->IsMainDocument())
		return environment->GetProxyDocument(document, origining_runtime);
	else
		return OpStatus::OK;
}

OP_STATUS
DOM_Document::SetXMLDocumentInfo(const XMLDocumentInformation *document_info)
{
	if (!xml_document_info)
	{
		xml_document_info = OP_NEW(XMLDocumentInformation, ());
		if (!xml_document_info)
			return OpStatus::ERR_NO_MEMORY;
	}

	if (document_info->GetXMLDeclarationPresent() && OpStatus::IsMemoryError(xml_document_info->SetXMLDeclaration(document_info->GetVersion(), document_info->GetStandalone(), document_info->GetEncoding())) ||
		document_info->GetDoctypeDeclarationPresent() && OpStatus::IsMemoryError(xml_document_info->SetDoctypeDeclaration(document_info->GetDoctypeName(), document_info->GetPublicId(), document_info->GetSystemId(), document_info->GetInternalSubset())))
		return OpStatus::ERR_NO_MEMORY;

	if (document_info->GetDoctype())
		xml_document_info->SetDoctype(document_info->GetDoctype());

	return OpStatus::OK;
}

OP_STATUS
DOM_Document::UpdateXMLDocumentInfo()
{
	if (placeholder)
	{
		HTML_Element *child = placeholder->FirstChildActual();
		while (child)
		{
			if (child->Type() == HE_DOCTYPE)
			{
				const XMLDocumentInformation *docinfo = child->GetXMLDocumentInfo();
				if (docinfo)
				{
					if (!xml_document_info)
					{
						xml_document_info = OP_NEW(XMLDocumentInformation, ());
						if (!xml_document_info)
							return OpStatus::ERR_NO_MEMORY;
					}

					return xml_document_info->SetDoctypeDeclaration(docinfo->GetDoctypeName(), docinfo->GetPublicId(), docinfo->GetSystemId(), docinfo->GetInternalSubset());
				}
				break;
			}
			child = child->SucActual();
		}
	}

	if (!xml_document_info && IsMainDocument())
		if (LogicalDocument *logdoc = GetLogicalDocument())
		{
			const XMLDocumentInformation *document_info = logdoc->GetXMLDocumentInfo();
			if (document_info)
				return SetXMLDocumentInfo(document_info);
		}

	return OpStatus::OK;
}

DOM_Object::DOMException
DOM_Document::CheckName(const uni_char *name, BOOL is_qname, unsigned length)
{
	XMLVersion xmlversion;

	if (xml_document_info)
		xmlversion = xml_document_info->GetVersion();
	else
		xmlversion = XMLVERSION_1_0;

	if (!XMLUtils::IsValidName(xmlversion, name, length))
		return INVALID_CHARACTER_ERR;
	else if (is_qname && !XMLUtils::IsValidQName(xmlversion, name, length))
		return NAMESPACE_ERR;
	else
		return DOMEXCEPTION_NO_ERR;
}

/* static */ int
DOM_Document::createDocumentFragment(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(document, DOM_TYPE_DOCUMENT, DOM_Document);

	if (!document->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	DOM_DocumentFragment* document_fragment;

	CALL_FAILED_IF_ERROR(DOM_DocumentFragment::Make(document_fragment, document));

	DOMSetObject(return_value, document_fragment);
	return ES_VALUE;
}

/* static */ int
DOM_Document::createEvent(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(document, DOM_TYPE_DOCUMENT, DOM_Document);
	DOM_CHECK_ARGUMENTS("s");

	if (!document->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	const uni_char *event_type = argv[0].value.string;

	DOM_Event *event;
	DOM_Runtime::Prototype prototype;
	const char *class_name;

	if (uni_stri_eq(event_type, "UIEVENTS") || uni_stri_eq(event_type, "UIEvent"))
	{
		event = OP_NEW(DOM_UIEvent, ());
		prototype = DOM_Runtime::UIEVENT_PROTOTYPE;
		class_name = "UIEvent";
	}
	else if (uni_stri_eq(event_type, "MOUSEEVENTS") || uni_stri_eq(event_type, "MouseEvent"))
	{
		event = OP_NEW(DOM_MouseEvent, ());
		prototype = DOM_Runtime::MOUSEEVENT_PROTOTYPE;
		class_name = "MouseEvent";
	}
#ifdef DOM2_MUTATION_EVENTS
	else if (uni_stri_eq(event_type, "MUTATIONEVENTS") || uni_stri_eq(event_type, "MutationEvent"))
	{
		event = OP_NEW(DOM_MutationEvent, ());
		prototype = DOM_Runtime::MUTATIONEVENT_PROTOTYPE;
		class_name = "MutationEvent";
	}
#endif // DOM2_MUTATION_EVENTS
	else if (uni_stri_eq(event_type, "HTMLEVENTS") || uni_stri_eq(event_type, "EVENTS") || uni_stri_eq(event_type, "Event"))
	{
		event = OP_NEW(DOM_Event, ());
		prototype = DOM_Runtime::EVENT_PROTOTYPE;
		class_name = "Event";
	}
	else if (uni_stri_eq(event_type, "ErrorEvent"))
	{
		event = OP_NEW(DOM_ErrorEvent, ());
		prototype = DOM_Runtime::ERROREVENT_PROTOTYPE;
		class_name = "ErrorEvent";
	}
	else if (uni_stri_eq(event_type, "KeyboardEvent"))
	{
		event = OP_NEW(DOM_KeyboardEvent, ());
		prototype = DOM_Runtime::KEYBOARDEVENT_PROTOTYPE;
		class_name = "KeyboardEvent";
	}
#ifdef TOUCH_EVENTS_SUPPORT
	else if ((uni_stri_eq(event_type, "TOUCHEVENTS") || uni_str_eq(event_type, "TouchEvent"))
# ifdef PI_UIINFO_TOUCH_EVENTS
	         && g_op_ui_info->IsTouchEventSupportWanted()
# endif // PI_UIINFO_TOUCH_EVENTS
	        )
	{
		event = OP_NEW(DOM_TouchEvent, ());
		prototype = DOM_Runtime::TOUCHEVENT_PROTOTYPE;
		class_name = "TouchEvent";
	}
#endif
#ifdef PROGRESS_EVENTS_SUPPORT
	else if (uni_stri_eq(event_type, "ProgressEvent"))
	{
		event = OP_NEW(DOM_ProgressEvent, ());
		prototype = DOM_Runtime::PROGRESSEVENT_PROTOTYPE;
		class_name = "ProgressEvent";
	}
#endif // PROGRESS_EVENTS_SUPPORT
#ifdef CSS_TRANSITIONS
	else if (uni_stri_eq(event_type, "TransitionEvent"))
	{
		event = OP_NEW(DOM_TransitionEvent, (0, -1));
		prototype = DOM_Runtime::TRANSITIONEVENT_PROTOTYPE;
		class_name = "TransitionEvent";
	}
	else if (uni_stri_eq(event_type, "OTransitionEvent"))
	{
		event = OP_NEW(DOM_TransitionEvent, (0, -1));
		prototype = DOM_Runtime::OTRANSITIONEVENT_PROTOTYPE;
		class_name = "OTransitionEvent";
	}
#endif // CSS_TRANSITIONS
#ifdef CSS_ANIMATIONS
	else if (uni_str_eq(event_type, "AnimationEvent"))
	{
		event = OP_NEW(DOM_AnimationEvent, ());
		prototype = DOM_Runtime::ANIMATIONEVENT_PROTOTYPE;
		class_name = "AnimationEvent";
	}
#endif // CSS_ANIMATIONS
#ifdef CLIENTSIDE_STORAGE_SUPPORT
	else if (uni_stri_eq(event_type, "StorageEvent"))
	{
		event = OP_NEW(DOM_StorageEvent, ());
		prototype = DOM_Runtime::STORAGEEVENT_PROTOTYPE;
		class_name = "StorageEvent";
	}
#endif // CLIENTSIDE_STORAGE_SUPPORT
	else if (uni_stri_eq(event_type, "HashChangeEvent"))
	{
		event = OP_NEW(DOM_HashChangeEvent, ());
		prototype = DOM_Runtime::HASHCHANGEEVENT_PROTOTYPE;
		class_name = "HashChangeEvent";
	}
#ifdef WEBSOCKETS_SUPPORT
	else if (uni_stri_eq(event_type, "CloseEvent"))
	{
		event = OP_NEW(DOM_CloseEvent, ());
		prototype = DOM_Runtime::CLOSEEVENT_PROTOTYPE;
		class_name = "CloseEvent";
	}
#endif // WEBSOCKETS_SUPPORT
#if defined(DOM_CROSSDOCUMENT_MESSAGING_SUPPORT) || defined(WEBSOCKETS_SUPPORT)
	else if (uni_stri_eq(event_type, "MessageEvent"))
	{
		event = OP_NEW(DOM_MessageEvent, ());
		prototype = DOM_Runtime::CROSSDOCUMENT_MESSAGEEVENT_PROTOTYPE;
		class_name = "MessageEvent";
	}
#endif
	else if (uni_stri_eq(event_type, "PopStateEvent"))
	{
		event = OP_NEW(DOM_PopStateEvent, ());
		prototype = DOM_Runtime::POPSTATEEVENT_PROTOTYPE;
		class_name = "PopStateEvent";
	}
#ifdef DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
	else if (uni_stri_eq(event_type, "DeviceOrientationEvent"))
	{
		event = OP_NEW(DOM_DeviceOrientationEvent, ());
		prototype = DOM_Runtime::DEVICEORIENTATION_EVENT_PROTOTYPE;
		class_name = "DeviceOrientationEvent";
	}
	else if (uni_stri_eq(event_type, "DeviceMotionEvent"))
	{
		event = OP_NEW(DOM_DeviceMotionEvent, ());
		prototype = DOM_Runtime::DEVICEMOTION_EVENT_PROTOTYPE;
		class_name = "DeviceMotionEvent";
	}
#endif
#ifdef DRAG_SUPPORT
	else if (uni_stri_eq(event_type, "DragEvent"))
	{
		event = OP_NEW(DOM_DragEvent, ());
		prototype = DOM_Runtime::DRAGEVENT_PROTOTYPE;
		class_name = "DragEvent";
	}
#endif // DRAG_SUPPORT
#ifdef USE_OP_CLIPBOARD
	else if (uni_stri_eq(event_type, "ClipboardEvent"))
	{
		event = OP_NEW(DOM_ClipboardEvent, ());
		prototype = DOM_Runtime::CLIPBOARDEVENT_PROTOTYPE;
		class_name = "ClipboardEvent";
	}
#endif // USE_OP_CLIPBOARD

	else if (uni_stri_eq(event_type, "CustomEvent"))
	{
		event = OP_NEW(DOM_CustomEvent, ());
		prototype = DOM_Runtime::CUSTOMEVENT_PROTOTYPE;
		class_name = "CustomEvent";
	}
	else
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);

	DOM_Runtime *runtime = document->GetRuntime();

	CALL_FAILED_IF_ERROR(DOMSetObjectRuntime(event, runtime, runtime->GetPrototype(prototype), class_name));

	event->SetSynthetic();

	DOMSetObject(return_value, event);
	return ES_VALUE;
}

void DOM_Document::MoveAllToThisDocument(HTML_Element *start, DOM_EnvironmentImpl *from)
{
	DOM_EnvironmentImpl *to = GetEnvironment();
	if (to != from)
	{
		OP_ASSERT(start);
		HTML_Element *iter = start;
		HTML_Element *stop = static_cast<HTML_Element *>(start->NextSibling());
		while (iter && iter != stop)
		{
			iter->DOMMoveToOtherDocument(from, to);
			iter = iter->Next();
		}
	}
}

/* static */ int
DOM_Document::importNode(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(document, DOM_TYPE_DOCUMENT, DOM_Document);
	DOM_CHECK_ARGUMENTS("ob");
	DOM_ARGUMENT_OBJECT(ext_node, 0, DOM_TYPE_NODE, DOM_Node);

	if (!document->OriginCheck(origining_runtime) || !ext_node->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	BOOL deep = argv[1].value.boolean;

	DOM_EnvironmentImpl *environment = document->GetEnvironment();

	if (ext_node->IsA(DOM_TYPE_ELEMENT) || ext_node->IsA(DOM_TYPE_CHARACTERDATA) || ext_node->IsA(DOM_TYPE_PROCESSINGINSTRUCTION) || ext_node->IsA(DOM_TYPE_DOCUMENTTYPE))
	{
		HTML_Element *ext_elm = ext_node->GetThisElement();
		HTML_Element *imp_elm;
		DOM_Node *imp_node;

		CALL_FAILED_IF_ERROR(HTML_Element::DOMCloneElement(imp_elm, document->GetEnvironment(), ext_elm, deep));
		document->MoveAllToThisDocument(imp_elm, ext_node->GetEnvironment());
		CALL_FAILED_IF_ERROR(environment->ConstructNode(imp_node, imp_elm, document));

		if (ext_node->IsA(DOM_TYPE_DOCUMENTTYPE))
		{
			OP_ASSERT(imp_node->IsA(DOM_TYPE_DOCUMENTTYPE));
			static_cast<DOM_DocumentType *>(imp_node)->CopyFrom(static_cast<DOM_DocumentType *>(ext_node));
			imp_node->SetIsSignificant();
		}

		DOMSetObject(return_value, imp_node);
		return ES_VALUE;
	}
	else if (ext_node->IsA(DOM_TYPE_DOCUMENTFRAGMENT))
	{
		HTML_Element *ext_elm = ((DOM_DocumentFragment *) ext_node)->GetPlaceholderElement();
		DOM_DocumentFragment *imp_frag;

		HTML_Element *imp_elm;
		if (deep)
		{
			CALL_FAILED_IF_ERROR(HTML_Element::DOMCloneElement(imp_elm, document->GetEnvironment(), ext_elm, TRUE));
			document->MoveAllToThisDocument(imp_elm, ext_node->GetEnvironment());
		}
		else
			imp_elm = NULL;

		CALL_FAILED_IF_ERROR(DOM_DocumentFragment::Make(imp_frag, document, imp_elm));

		DOMSetObject(return_value, imp_frag);
		return ES_VALUE;
	}
	else if (ext_node->IsA(DOM_TYPE_ATTR))
	{
		DOM_Attr *imp_attr;

		CALL_FAILED_IF_ERROR(((DOM_Attr *) ext_node)->Import(imp_attr, document));

		DOMSetObject(return_value, imp_attr);
		return ES_VALUE;
	}
	else
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
}

class DOM_adoptNode_State
	: public DOM_Object
{
public:
	enum State
	{
		STATE_INITIAL,
		STATE_REMOVECHILD,
		STATE_REMOVEATTRIBUTENODE
	};

	State state;
	DOM_Document *document;
	DOM_Node *node;
	ES_Object *restart_object;

	virtual void GCTrace()
	{
		GCMark(document);
		GCMark(node);
		GCMark(restart_object);
	}
};

/* static */ int
DOM_Document::adoptNode(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_Document *document;
	DOM_Node *node;
	DOM_adoptNode_State *state_object = NULL;
	DOM_adoptNode_State::State state = DOM_adoptNode_State::STATE_INITIAL;

	if (argc >= 0)
	{
		DOM_THIS_OBJECT_EXISTING(document, DOM_TYPE_DOCUMENT, DOM_Document);
		DOM_CHECK_ARGUMENTS("o");
		DOM_ARGUMENT_OBJECT_EXISTING(node, 0, DOM_TYPE_NODE, DOM_Node);

		if (!document->OriginCheck(origining_runtime) || !node->OriginCheck(origining_runtime))
			return ES_EXCEPT_SECURITY;

		switch (node->GetNodeType())
		{
		case ENTITY_NODE:
		case DOCUMENT_NODE:
		case NOTATION_NODE:
			return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
		}
	}
	else
	{
		state_object = DOM_VALUE2OBJECT(*return_value, DOM_adoptNode_State);

		document = state_object->document;
		node = state_object->node;

		int result = ES_VALUE;

		DOMSetObject(return_value, state_object->restart_object);
		state_object->restart_object = NULL;

		switch (state_object->state)
		{
		case DOM_adoptNode_State::STATE_REMOVECHILD:
			result = DOM_Node::removeChild(NULL, NULL, -1, return_value, origining_runtime);
			break;

		case DOM_adoptNode_State::STATE_REMOVEATTRIBUTENODE:
			result = DOM_Element::removeAttributeNode(NULL, NULL, -1, return_value, origining_runtime);
		}

		if (result == (ES_SUSPEND | ES_RESTART))
		{
			OP_ASSERT(return_value->type == VALUE_OBJECT);

			state_object->restart_object = return_value->value.object;
			DOMSetObject(return_value, state_object);

			return ES_SUSPEND | ES_RESTART;
		}
		else if (GetCurrentThread(origining_runtime)->IsBlocked())
		{
			state_object->state = DOM_adoptNode_State::STATE_INITIAL;
			DOMSetObject(return_value, state_object);

			return ES_SUSPEND | ES_RESTART;
		}
	}

	DOM_Runtime *runtime = document->GetRuntime();

	if (HTML_Element *this_element = node->GetThisElement())
		if (HTML_Element *parent_element = this_element->ParentActual())
		{
			DOM_Node *parent_node;

			CALL_FAILED_IF_ERROR(node->GetEnvironment()->ConstructNode(parent_node, parent_element, node->GetOwnerDocument()));

			ES_Value arguments[1];
			DOMSetObject(&arguments[0], node);

			int result = DOM_Node::removeChild(parent_node, arguments, 1, return_value, origining_runtime);

			if (result == (ES_SUSPEND | ES_RESTART))
			{
				state = DOM_adoptNode_State::STATE_REMOVECHILD;
				goto suspend;
			}
			else if (GetCurrentThread(origining_runtime)->IsBlocked())
				goto suspend;
			else if (result != ES_VALUE)
				return result;
		}

	if (node->GetNodeType() == ATTRIBUTE_NODE)
		if (DOM_Element *owner_element = ((DOM_Attr *) node)->GetOwnerElement())
		{
			ES_Value arguments[1];
			DOMSetObject(&arguments[0], node);

			int result = DOM_Element::removeAttributeNode(owner_element, arguments, 1, return_value, origining_runtime);

			if (result == (ES_SUSPEND | ES_RESTART))
			{
				state = DOM_adoptNode_State::STATE_REMOVEATTRIBUTENODE;
				goto suspend;
			}
			else if (GetCurrentThread(origining_runtime)->IsBlocked())
				goto suspend;
		}

	bool change_runtime;
	bool change_ownerDocument;

	if (change_runtime = node->GetRuntime() != runtime, change_ownerDocument = node->GetOwnerDocument() != document, change_runtime || change_ownerDocument)
	{
		DOM_EnvironmentImpl *from_environment = node->GetEnvironment();
		DOM_EnvironmentImpl *to_environment = document->GetEnvironment();

		if (node->GetNodeType() == ATTRIBUTE_NODE)
		{
			if (change_runtime)
				node->DOMChangeRuntime(runtime);
			if (change_ownerDocument)
				node->SetOwnerDocument(document);
		}
		else
		{
			HTML_Element *iter = node->GetPlaceholderElement();
			if (!iter)
				iter = node->GetThisElement();
			while (iter)
			{
				if (change_runtime)
					iter->DOMMoveToOtherDocument(from_environment, to_environment);

				if (DOM_Node *node = (DOM_Node *) iter->GetESElement())
				{
					if (change_runtime)
						node->DOMChangeRuntime(runtime);
					if (change_ownerDocument)
						node->DOMChangeOwnerDocument(document);
				}

#ifdef SVG_SUPPORT
				// Markup::SVGE_SVG nodes know about their document and won't
				// be happy if they're not informed of document
				// switches. They might even crash to get even.
				if (change_ownerDocument && iter->IsMatchingType(Markup::SVGE_SVG, NS_SVG))
					if (FramesDocument *frames_doc = document->GetFramesDocument())
						if (LogicalDocument* logdoc = frames_doc->GetLogicalDocument())
							g_svg_manager->SwitchDocument(logdoc, iter);
#endif // SVG_SUPPORT

				iter = iter->Next();
			}
		}

		if (change_runtime)
			from_environment->MigrateCollections();
	}

	DOMSetObject(return_value, node);
	return ES_VALUE;

suspend:
	if (!state_object)
	{
		CALL_FAILED_IF_ERROR(DOMSetObjectRuntime(state_object = OP_NEW(DOM_adoptNode_State, ()), runtime));
		state_object->document = document;
		state_object->node = node;
	}

	OP_ASSERT(state == DOM_adoptNode_State::STATE_INITIAL || return_value->type == VALUE_OBJECT);

	state_object->state = state;
	state_object->restart_object = return_value->value.object;
	DOMSetObject(return_value, state_object);

	return ES_SUSPEND | ES_RESTART;
}

/* static */ int
DOM_Document::elementFromPoint(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(document, DOM_TYPE_DOCUMENT, DOM_Document);
	DOM_CHECK_ARGUMENTS("nn");

	/* MSIE compatibility: this function should also work on frameset
	   documents, returning the appropriate BODY element.  It doesn't. */

	FramesDocument*	frames_doc = document->GetFramesDocument();
	if (!frames_doc)
		return ES_FAILED;

	HTML_Document* html_doc = frames_doc->GetHtmlDocument();
	if (!html_doc)
		return ES_FAILED;

	OpPoint scroll_offset = frames_doc->DOMGetScrollOffset();

	int x = scroll_offset.x + TruncateDoubleToInt(argv[0].value.number);
	int y = scroll_offset.y + TruncateDoubleToInt(argv[1].value.number);
	unsigned int inner_width;
	unsigned int inner_height;

	frames_doc->DOMGetInnerWidthAndHeight(inner_width, inner_height);

	HTML_Element* element = NULL;

	if (OpRect(scroll_offset.x, scroll_offset.y, inner_width, inner_height).Contains(OpPoint(x, y)))
		element = html_doc->GetHTML_Element(x, y, FALSE);

	return ConvertGetNameToCall(document->DOMSetElement(return_value, element), return_value);
}

/* static */ int
DOM_Document::getElementById(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_Document *document;
	HTML_Element *root_element;

#ifdef SVG_DOM
	if (data == 0)
#endif // SVG_DOM
	{
		DOM_THIS_OBJECT_EXISTING(document, DOM_TYPE_DOCUMENT, DOM_Document);
		root_element = NULL;
	}
#ifdef SVG_DOM
	else
	{
		DOM_THIS_OBJECT(element, DOM_TYPE_SVG_ELEMENT, DOM_Element);
		document = element->GetOwnerDocument();
		root_element = element->GetThisElement();
	}
#endif // SVG_DOM

	DOM_CHECK_ARGUMENTS("z");

	if (!this_object->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	const uni_char *name = argv[0].value.string_with_length->string;

	unsigned length = uni_strlen(name);
	if (length == 0 || length != argv[0].value.string_with_length->length)
	{
		/* Empty string or string with embedded null character: not a valid ID,
		   and not one we can ever find, so return null. */
		DOMSetNull(return_value);
		return ES_VALUE;
	}

	HTML_Element *element = NULL;

	if (LogicalDocument *logdoc = document->GetLogicalDocument())
	{
		NamedElementsIterator iterator;

		int found = logdoc->SearchNamedElements(iterator, root_element, name, TRUE, FALSE);

		if (found == -1)
			return ES_NO_MEMORY;
		else if (found > 0)
			element = iterator.GetNextElement();
	}
	else
	{
		element = root_element ? root_element : document->GetPlaceholderElement();

		while (element)
		{
			const uni_char *id = element->GetId();

			if (id && uni_strcmp(id, name) == 0)
				break;

			element = element->NextActual();
		}
	}

	RETURN_GETNAME_AS_CALL(document->DOMSetElement(return_value, element));
}

#ifdef DOM_FULLSCREEN_MODE
/* static */ int
DOM_Document::exitFullscreen(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(document, DOM_TYPE_DOCUMENT, DOM_Document);

	if (document->GetFramesDocument())
		document->GetFramesDocument()->DOMExitFullscreen(origining_runtime);

	return ES_FAILED;
}

OP_STATUS
DOM_Document::RequestFullscreen(DOM_Element *element, DOM_Runtime *origining_runtime)
{
	if (GetFramesDocument())
		return GetFramesDocument()->DOMRequestFullscreen(element->GetThisElement(), origining_runtime);

	return OpStatus::OK;
}
#endif // DOM_FULLSCREEN_MODE

#ifdef DOM2_RANGE

/* static */ int
DOM_Document::createRange(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(document, DOM_TYPE_DOCUMENT, DOM_Document);

	if (!document->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	DOM_Range *range;
	CALL_FAILED_IF_ERROR(DOM_Range::Make(range, document));

	DOMSetObject(return_value, range);
	return ES_VALUE;
}

#endif // DOM2_RANGE

/* static */ int
DOM_Document::createProcessingInstruction(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(document, DOM_TYPE_DOCUMENT, DOM_Document);
	DOM_CHECK_ARGUMENTS("ss");

	DOM_ProcessingInstruction *procinst;
	CALL_FAILED_IF_ERROR(DOM_ProcessingInstruction::Make(procinst, document, argv[0].value.string, argv[1].value.string));

	DOMSetObject(return_value, procinst);
	return ES_VALUE;
}

#ifndef HAS_NOTEXTSELECTION
/* static */ int
DOM_Document::getSelection(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(document, DOM_TYPE_DOCUMENT, DOM_Document);

	return static_cast<JS_Window*>(document->GetEnvironment()->GetWindow())->GetSelection(return_value);
}
#endif // !HAS_NOTEXTSELECTION

class DOM_ClassNameCollectionFilter
	: public DOM_CollectionFilter
{
public:
	DOM_ClassNameCollectionFilter(){}

	virtual ~DOM_ClassNameCollectionFilter()
	{
		UINT32 i = m_classnames.GetCount();
		while (i--)
			m_classnames.Get(i)->UnRef();
	}

	virtual void Visit(HTML_Element *element, BOOL &include, BOOL &visit_children, LogicalDocument *logdoc);
	virtual DOM_CollectionFilter *Clone() const;

	virtual Type GetType() { return TYPE_CLASSNAME; }
	virtual BOOL IsNameSearch(const uni_char *&name) { return FALSE; }
	virtual BOOL CanSkipChildren() { return FALSE; }
	virtual BOOL CanIncludeGrandChildren() { return TRUE; }
	virtual BOOL IsMatched(unsigned collections);
	virtual BOOL IsEqual(DOM_CollectionFilter *other);

	OP_STATUS AddClassName(ReferencedHTMLClass *class_ref);
	BOOL HasClassNames() { return m_classnames.GetCount() != 0; }

protected:
	OpVector<ReferencedHTMLClass> m_classnames;
};

//defined on style\src\css.cpp

/*
 TODO: this method can be improved when we get element.classList property.
 That can bypass all the tokenization or substring comparison, but
 performance will have to measured

 Selftests at -test=DOM.Core.Document.functions.getElementsByClassName
 */
/* virtual */ void
DOM_ClassNameCollectionFilter::Visit(HTML_Element *element, BOOL &include, BOOL &visit_children, LogicalDocument *logdoc)
{
	visit_children = TRUE;
	include = FALSE;

	// empty arguments -> just quit
	if (m_classnames.GetCount() == 0)
		return;

	// no class -> rejected immediately
	if (!element->HasClass())
		return;
	const ClassAttribute *class_attr = element->GetClassAttribute();
	if (class_attr == NULL)
		return;

	// start by assuming that node is not acceptable and flag it later if it actually is
	include = FALSE;

	// loop over the class names specified as function arguments
	unsigned class_name_index;
	for (class_name_index = 0; class_name_index < m_classnames.GetCount(); ++class_name_index)
		if (!class_attr->MatchClass(m_classnames.Get(class_name_index)))
			return;

	// the happy end -> the element was not rejected
	include = TRUE;
}

/* virtual */ DOM_CollectionFilter *
DOM_ClassNameCollectionFilter::Clone() const
{
	DOM_ClassNameCollectionFilter *clone = OP_NEW(DOM_ClassNameCollectionFilter, ());

	if (clone)
		for (unsigned index = 0; index < m_classnames.GetCount(); ++index)
		{
			ReferencedHTMLClass* class_ref = m_classnames.Get(index);
			if (OpStatus::IsMemoryError(clone->AddClassName(class_ref)))
			{
				OP_DELETE(clone);
				return NULL;
			}
			else
				class_ref->Ref();
		}

	return clone;
}

/* virtual */ BOOL
DOM_ClassNameCollectionFilter::IsMatched(unsigned collections)
{
	return (collections & DOM_Environment::COLLECTION_CLASSNAME) != 0;
}

/* virtual */ BOOL
DOM_ClassNameCollectionFilter::IsEqual(DOM_CollectionFilter* other)
{
	if (other->GetType() != TYPE_CLASSNAME)
		return FALSE;

	DOM_ClassNameCollectionFilter *class_other = static_cast<DOM_ClassNameCollectionFilter *>(other);

	/* A strict notion of equality. We could admit a larger set by handling permutations; not worth it. */
	if (m_classnames.GetCount() == class_other->m_classnames.GetCount())
	{
		for(unsigned index = 0; index < m_classnames.GetCount(); index++)
		{
			ReferencedHTMLClass* class_ref = m_classnames.Get(index);
			ReferencedHTMLClass* other_ref = class_other->m_classnames.Get(index);

			/* Strings are hashed, pointer equality suffice. */
			if (class_ref->GetString() != other_ref->GetString())
				return FALSE;
		}
		return TRUE;
	}
	else
		return FALSE;
}

OP_STATUS
DOM_ClassNameCollectionFilter::AddClassName(ReferencedHTMLClass* class_ref)
{
	OP_ASSERT(class_ref);

	if (OpStatus::IsError(m_classnames.Add(class_ref)))
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

class DOM_SingleClassNameCollectionFilter
	: public DOM_CollectionFilter
{
public:
	DOM_SingleClassNameCollectionFilter(const uni_char *cls)
		: class_name(cls)
	{
	}

	virtual ~DOM_SingleClassNameCollectionFilter()
	{
		if (allocated)
			OP_DELETEA(class_name);
	}

	virtual void Visit(HTML_Element *element, BOOL &include, BOOL &visit_children, LogicalDocument *logdoc);
	virtual DOM_CollectionFilter *Clone() const;

	virtual Type GetType() { return TYPE_CLASSNAME_SINGLE; }
	virtual BOOL IsNameSearch(const uni_char *&name) { return FALSE; }
	virtual BOOL CanSkipChildren() { return FALSE; }
	virtual BOOL CanIncludeGrandChildren() { return TRUE; }
	virtual BOOL IsMatched(unsigned collections);
	virtual BOOL IsEqual(DOM_CollectionFilter *other);

protected:
	const uni_char *class_name;
};


/* virtual */ void
DOM_SingleClassNameCollectionFilter::Visit(HTML_Element *element, BOOL &include, BOOL &visit_children, LogicalDocument *logdoc)
{
	visit_children = TRUE;
	include = FALSE;

	// no class -> rejected immediately
	if (!element->HasClass())
		return;

	const ClassAttribute *class_attr = element->GetClassAttribute();
	if (class_attr == NULL)
		return;

	include = class_attr->MatchClass(class_name, TRUE);
}

/* virtual */ DOM_CollectionFilter *
DOM_SingleClassNameCollectionFilter::Clone() const
{
	uni_char *cls = UniSetNewStr(class_name);

	if (!cls)
		return NULL;

	DOM_SingleClassNameCollectionFilter *clone = OP_NEW(DOM_SingleClassNameCollectionFilter, (cls));

	if (!clone)
	{
		OP_DELETEA(cls);
		return NULL;
	}
	clone->allocated = TRUE;
	return clone;
}

/* virtual */ BOOL
DOM_SingleClassNameCollectionFilter::IsMatched(unsigned collections)
{
	return (collections & DOM_Environment::COLLECTION_CLASSNAME) != 0;
}

/* virtual */ BOOL
DOM_SingleClassNameCollectionFilter::IsEqual(DOM_CollectionFilter* other)
{
	if (other->GetType() != TYPE_CLASSNAME_SINGLE)
		return FALSE;

	DOM_SingleClassNameCollectionFilter *class_other = static_cast<DOM_SingleClassNameCollectionFilter *>(other);

	return (class_name && class_other->class_name && uni_str_eq(class_name, class_other->class_name));
}


/* static */ int
DOM_Document::getElementsByClassName(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_CHECK_ARGUMENTS("s");
	DOM_Node *root;

	if (data == 0)
	{
		DOM_THIS_OBJECT(document, DOM_TYPE_DOCUMENT, DOM_Document);
		root = document;
	}
	else
	{
		DOM_THIS_OBJECT(element, DOM_TYPE_ELEMENT, DOM_Element);
		root = element;
	}

	const uni_char *arg_class_string = argv[0].value.string;

	/* Simple and common case: a single name */
	if (arg_class_string && !uni_strpbrk(arg_class_string, UNI_L(" \t\r\n\f")))
	{
		DOM_SingleClassNameCollectionFilter filter(arg_class_string);
		DOM_Collection *collection;

		CALL_FAILED_IF_ERROR(DOM_Collection::MakeNodeList(collection, root->GetEnvironment(), root, FALSE, TRUE, filter));

		DOMSetObject(return_value, collection);
		return ES_VALUE;
	}

	/* general case, > 1 class names. */

	DOM_ClassNameCollectionFilter filter;

	/* Iterate over the classes, make each into a legal C string and add them to the filter */
	OpString classes_string;
	CALL_FAILED_IF_ERROR(classes_string.Set(arg_class_string));
	uni_char* token_string = classes_string.CStr();
	uni_char* orig_class_name = uni_strtok(token_string, UNI_L(" \t\r\n\f"));
	uni_char* class_name = orig_class_name;

	LogicalDocument* logdoc = NULL;

	if (FramesDocument *frames_doc = root->GetFramesDocument())
		logdoc = frames_doc->GetLogicalDocument();

	if (logdoc)
	{
		while (class_name)
		{
			uni_char* classname = UniSetNewStr(class_name);
			if (!classname)
				return ES_NO_MEMORY;

			ReferencedHTMLClass* class_ref = logdoc->GetClassReference(classname);
			if (!class_ref)
				return ES_NO_MEMORY;

			if (filter.AddClassName(class_ref) != OpStatus::OK)
			{
				class_ref->UnRef();
				return ES_NO_MEMORY;
			}

			class_name = uni_strtok(NULL, UNI_L(" \t\r\n\f"));
		}
	}

	DOM_Collection *collection;
	CALL_FAILED_IF_ERROR(DOM_Collection::MakeNodeList(collection, root->GetEnvironment(), orig_class_name ? root : NULL, FALSE, TRUE, filter));

	DOMSetObject(return_value, collection);
	return ES_VALUE;
}

/* static */ int
DOM_Document::createNode(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(document, DOM_TYPE_DOCUMENT, DOM_Document);

	if (!document->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	XMLCompleteName completename;

	CALL_FAILED_IF_ERROR(document->UpdateXMLDocumentInfo());

	const uni_char *ns_uri, *ns_prefix, *name;
	int ns_idx;

	if ((data & 1) == 0)
	{
		DOM_CHECK_ARGUMENTS("z");

		ns_uri = ns_prefix = NULL;
		ns_idx = NS_IDX_NOT_FOUND;

		name = argv[0].value.string_with_length->string;
		unsigned length = argv[0].value.string_with_length->length;

		if (uni_strlen(name) != length)
			/* Embedded null characters: not a valid character in element names: */
			return document->CallDOMException(INVALID_CHARACTER_ERR, return_value);

		DOMException code = document->CheckName(name, FALSE, length);
		if (code != DOMEXCEPTION_NO_ERR)
			return document->CallDOMException(code, return_value);

		if (document->IsXML())
		{
			if (DOM_Element *root = document->GetRootElement())
				ns_idx = g_ns_manager->GetNsIdxWithoutPrefix(root->GetThisElement()->GetNsIdx());

			if (ns_idx < 0)
				ns_idx = document->GetDefaultNS();
		}
		else if (data < 2)
			ns_idx = NS_IDX_HTML;
		else
			ns_idx = NS_IDX_DEFAULT;
	}
	else
	{
		DOM_CHECK_ARGUMENTS("Sz");

		if (argv[0].type == VALUE_STRING)
			ns_uri = argv[0].value.string;
		else
			ns_uri = NULL;

		const uni_char *qname = argv[1].value.string_with_length->string;
		unsigned length = argv[1].value.string_with_length->length;

		if (uni_strlen(qname) != length)
			/* Embedded null characters: not a valid character in element names: */
			return document->CallDOMException(INVALID_CHARACTER_ERR, return_value);

		DOMException code = document->CheckName(qname, TRUE, length);
		if (code != DOMEXCEPTION_NO_ERR)
			return document->CallDOMException(code, return_value);

		XMLCompleteNameN completenamen(qname, length);

		CALL_FAILED_IF_ERROR(completename.Set(completenamen));

		ns_prefix = completename.GetPrefix();
		name = completename.GetLocalPart();

		if (ns_prefix && !ns_uri ||
		    ns_prefix && uni_str_eq(ns_prefix, "xml") && !(ns_uri && uni_str_eq(ns_uri, "http://www.w3.org/XML/1998/namespace")) ||
		    ns_prefix && uni_str_eq(ns_prefix, "xmlns") && !(ns_uri && uni_str_eq(ns_uri, "http://www.w3.org/2000/xmlns/")) ||
		    ns_uri && uni_str_eq(ns_uri, "http://www.w3.org/2000/xmlns/") && !(!ns_prefix && uni_str_eq(name, "xmlns") || ns_prefix && uni_str_eq(ns_prefix, "xmlns")))
			return DOM_CALL_DOMEXCEPTION(NAMESPACE_ERR);

		if (ns_uri)
		{
			if (HTML_Element *element = document->GetPlaceholderElement())
				ns_idx = element->DOMGetNamespaceIndex(document->GetEnvironment(), ns_uri, ns_prefix);
			else
				ns_idx = -1;

			if (ns_idx < 0)
				ns_idx = document->GetDefaultNS();
		}
		else
			ns_idx = NS_IDX_DEFAULT;
	}

	if (data < 2)
	{
		HTML_Element *element;
		DOM_Node *node;

		BOOL case_sensitive = data == 1 || document->IsXML() || ns_idx != NS_IDX_HTML;
		CALL_FAILED_IF_ERROR(HTML_Element::DOMCreateElement(element, document->GetEnvironment(), name, ns_idx, case_sensitive));
		OpAutoPtr<HTML_Element> element_anchor(element);
		CALL_FAILED_IF_ERROR(document->GetEnvironment()->ConstructNode(node, element, document));
		element_anchor.release();

		DOMSetObject(return_value, node);
	}
	else
	{
		DOM_Attr *attr;

		CALL_FAILED_IF_ERROR(DOM_Attr::Make(attr, document, name, ns_uri, ns_prefix, (data & 1) == 0));

		DOMSetObject(return_value, attr);
	}

	return ES_VALUE;
}

/* static */ int
DOM_Document::createCharacterData(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(document, DOM_TYPE_DOCUMENT, DOM_Document);
	DOM_CHECK_ARGUMENTS("s");

	if (!document->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	const uni_char *contents = argv[0].value.string;

	OP_STATUS status;
	DOM_Node *node;

	if (data == 0)
	{
		DOM_Text *text;
		status = DOM_Text::Make(text, document, contents);
		node = (DOM_Text *) text;
	}
	else if (data == 1)
	{
		DOM_Comment *comment;
		status = DOM_Comment::Make(comment, document, contents);
		node = (DOM_Comment *) comment;
	}
	else
	{
		DOM_CDATASection *cdata_section;
		status = DOM_CDATASection::Make(cdata_section, document, contents);
		node = (DOM_CDATASection *) cdata_section;
	}

	CALL_FAILED_IF_ERROR(status);

	DOMSetObject(return_value, node);
	return ES_VALUE;
}

#ifdef DOM2_TRAVERSAL

/* static */ int
DOM_Document::createTraversalObject(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(document, DOM_TYPE_DOCUMENT, DOM_Document);
	DOM_CHECK_ARGUMENTS("OnOb");
	DOM_ARGUMENT_OBJECT(root, 0, DOM_TYPE_NODE, DOM_Node);

	if (!root)
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);

	if (!document->OriginCheck(origining_runtime) || !root->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	unsigned what_to_show;
	ES_Object *filter;
	BOOL entity_reference_expansion;

	what_to_show = (unsigned) argv[1].value.number;

	if (argv[2].type == VALUE_OBJECT)
		filter = argv[2].value.object;
	else
		filter = NULL;

	entity_reference_expansion = argv[3].value.boolean;

	if (data == 0)
	{
		DOM_NodeIterator *node_iterator;

		CALL_FAILED_IF_ERROR(DOM_NodeIterator::Make(node_iterator, root->GetEnvironment(), root,
		                                            what_to_show, filter, entity_reference_expansion));

		DOMSetObject(return_value, node_iterator);
	}
	else
	{
		DOM_TreeWalker *tree_walker;

		CALL_FAILED_IF_ERROR(DOM_TreeWalker::Make(tree_walker, root->GetEnvironment(), root,
		                                          what_to_show, filter, entity_reference_expansion));

		DOMSetObject(return_value, tree_walker);
	}

	return ES_VALUE;
}

#endif // DOM2_TRAVERSAL

#ifdef DOCUMENT_EDIT_SUPPORT

class DOM_DocumentEditState
	: public DOM_Object
{
public:
	OpString command;
	OpString value;
	BOOL ui;

	DOM_DocumentEditState(BOOL ui)
		: ui(ui)
	{
	}
};

/* static */ int
DOM_Document::documentEdit(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(document, DOM_TYPE_DOCUMENT, DOM_Document);

	DOM_EnvironmentImpl *environment = document->GetEnvironment();

	if (document->IsMainDocument())
		if (FramesDocument *frames_doc = environment->GetFramesDocument())
			if (data == 0)
			{
				DOM_DocumentEditState *state;
				const uni_char *command = NULL;
				BOOL ui = FALSE;
				const uni_char* value = NULL;

				if (argc < 0)
				{
					state = DOM_VALUE2OBJECT(*return_value, DOM_DocumentEditState);
					command = state->command.CStr();
					ui = state->ui;
					value = state->value.CStr();
				}
				else
				{
					DOM_CHECK_ARGUMENTS("s");
					command = argv[0].value.string;

					if (argc != 1)
					{
						DOM_CHECK_ARGUMENTS("sbs");
						ui = argv[1].value.boolean;
						value = argv[2].value.string;
					}

					if (ShouldBlockWaitingForStyle(frames_doc, origining_runtime))
					{
						DOM_DelayedLayoutListener *listener = OP_NEW(DOM_DelayedLayoutListener, (frames_doc, GetCurrentThread(origining_runtime)));
						if (!listener)
							return ES_NO_MEMORY;

						CALL_FAILED_IF_ERROR(DOMSetObjectRuntime(state = OP_NEW(DOM_DocumentEditState, (ui)), document->GetRuntime()));
						CALL_FAILED_IF_ERROR(state->command.Set(command));
						CALL_FAILED_IF_ERROR(state->value.Set(value));

						DOMSetObject(return_value, state);
						return ES_SUSPEND|ES_RESTART;
					}
				}

				URL origin_url = origining_runtime->GetOriginURL();
				OpDocumentEdit *document_edit = frames_doc->GetDocumentEdit();
				OP_STATUS status = OpDocumentEdit::execCommand(command, document_edit, ui, value, origin_url, frames_doc);

				if (status == OpStatus::ERR_NO_ACCESS)
					return DOM_CALL_DOMEXCEPTION(SECURITY_ERR);

				DOMSetBoolean(return_value, OpStatus::IsSuccess(status));
				return ES_VALUE;
			}
			else
			{
				DOM_CHECK_ARGUMENTS("s");
				const uni_char *command = argv[0].value.string;
				OpDocumentEdit *document_edit = frames_doc->GetDocumentEdit();

				if (data == 1)
				{
					URL origin_url = origining_runtime->GetOriginURL();
					DOMSetBoolean(return_value, OpDocumentEdit::queryCommandEnabled(command, document_edit, origin_url, frames_doc));
				}
				else if (data == 2 && document_edit)
					DOMSetBoolean(return_value, document_edit->queryCommandState(command));
				else if (data == 3)
				{
					URL origin_url = origining_runtime->GetOriginURL();
					DOMSetBoolean(return_value, OpDocumentEdit::queryCommandSupported(command, document_edit, origin_url, frames_doc));
				}
				else if (data == 4 && document_edit)
				{
					TempBuffer *value = GetEmptyTempBuf();
					CALL_FAILED_IF_ERROR(document_edit->queryCommandValue(command, *value));
					DOMSetString(return_value, value);
				}
				else
					/* Deliberately not sending an exception here. */
					DOMSetBoolean(return_value, FALSE);

				return ES_VALUE;
			}

	return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
}

#endif // DOCUMENT_EDIT_SUPPORT

/* static */ void
DOM_Document::GetDocumentReadiness(ES_Value* value, FramesDocument* frames_doc)
{
	if (value)
	{
		DOMSetString(value, UNI_L("complete"));
		if (frames_doc)
		{
			LogicalDocument *logdoc = frames_doc->GetLogicalDocument();
			if (!logdoc || !logdoc->IsParsed())
				DOMSetString(value, UNI_L("loading"));
			else if (!frames_doc->GetDocumentFinishedAtLeastOnce())
			{
				// readyState shouldn't revert back to interactive once the document finished
				// loading for the first time. This matches IE's behaviour (CORE-27693).
				DOMSetString(value, UNI_L("interactive"));
			}
		}
		else
		{
			DOMSetString(value, UNI_L("uninitialized"));
		}
	}
}

#include "modules/dom/src/domglobaldata.h"

#ifdef DOM_XPATH_REFERENCE_IMPLEMENTATION
extern DOM_FunctionImpl DOM_XPathRef_Evaluate;
#endif // DOM_XPATH_REFERENCE_IMPLEMENTATION

DOM_FUNCTIONS_START(DOM_Document)
	DOM_FUNCTIONS_FUNCTION(DOM_Document, DOM_Document::createDocumentFragment, "createDocumentFragment", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_Document, DOM_Document::createEvent, "createEvent", "s-")
#ifdef DOM2_RANGE
	DOM_FUNCTIONS_FUNCTION(DOM_Document, DOM_Document::createRange, "createRange", 0)
#endif // DOM2_RANGE
	DOM_FUNCTIONS_FUNCTION(DOM_Document, DOM_Document::createProcessingInstruction, "createProcessingInstruction", "ss-")
#ifdef DOM2_MISSING_FUNCTIONS
	DOM_FUNCTIONS_FUNCTION(DOM_Document, DOM_Document::createEntityReference, "createEntityReference", "s-")
#endif // DOM2_MISSING_FUNCTIONS
	DOM_FUNCTIONS_FUNCTION(DOM_Document, DOM_Document::importNode, "importNode", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_Document, DOM_Document::adoptNode, "adoptNode", 0)
	DOM_FUNCTIONS_FUNCTION(DOM_Document, DOM_Document::elementFromPoint, "elementFromPoint", "nn-")
#ifndef HAS_NOTEXTSELECTION
	DOM_FUNCTIONS_FUNCTION(DOM_Document, DOM_Document::getSelection, "getSelection", 0)
#endif // HAS_NOTEXTSELECTION
#ifdef DOM_FULLSCREEN_MODE
	DOM_FUNCTIONS_FUNCTION(DOM_Document, DOM_Document::exitFullscreen, "exitFullscreen", 0)
#endif // DOM_FULLSCREEN_MODE
#ifdef DOM3_XPATH
	DOM_FUNCTIONS_FUNCTION(DOM_Document, DOM_XPathEvaluator::createNSResolver, "createNSResolver", "o-")
#endif // DOM3_XPATH
#ifdef DOM_XPATH_REFERENCE_IMPLEMENTATION
	DOM_FUNCTIONS_FUNCTION(DOM_Document, DOM_XPathRef_Evaluate, "evaluate0", "so")
#endif // DOM_XPATH_REFERENCE_IMPLEMENTATION
DOM_FUNCTIONS_END(DOM_Document)

DOM_FUNCTIONS_WITH_DATA_START(DOM_Document)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Document, DOM_Document::createNode, 0, "createElement", "z-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Document, DOM_Document::createNode, 1, "createElementNS", "Sz-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Document, DOM_Document::createNode, 2, "createAttribute", "z-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Document, DOM_Document::createNode, 3, "createAttributeNS", "Sz-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Document, DOM_Document::createCharacterData, 0, "createTextNode", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Document, DOM_Document::createCharacterData, 1, "createComment", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Document, DOM_Document::createCharacterData, 2, "createCDATASection", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Document, DOM_Document::getElementById, 0, "getElementById", "z-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Document, DOM_Document::getElementsByClassName, 0, "getElementsByClassName", "s-")
#ifdef DOM2_TRAVERSAL
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Document, DOM_Document::createTraversalObject, 0, "createNodeIterator", "-n-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Document, DOM_Document::createTraversalObject, 1, "createTreeWalker", "-n-b-")
#endif // DOM2_TRAVERSAL
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Document, DOM_Element::getElementsByTagName, 2, "getElementsByTagName", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Document, DOM_Element::getElementsByTagName, 3, "getElementsByTagNameNS", "Ss-")
#ifdef DOCUMENT_EDIT_SUPPORT
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Document, DOM_Document::documentEdit, 0, "execCommand", "sbs-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Document, DOM_Document::documentEdit, 1, "queryCommandEnabled", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Document, DOM_Document::documentEdit, 2, "queryCommandState", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Document, DOM_Document::documentEdit, 3, "queryCommandSupported", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Document, DOM_Document::documentEdit, 4, "queryCommandValue", "s-")
#endif // DOCUMENT_EDIT_SUPPORT
#ifdef DOM3_XPATH
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Document, DOM_XPathEvaluator::createExpressionOrEvaluate, 0, "createExpression", "sO-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Document, DOM_XPathEvaluator::createExpressionOrEvaluate, 1, "evaluate", "soOnO-")
#endif // DOM3_XPATH
#ifdef DOM_SELECTORS_API
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Document, DOM_Selector::querySelector, DOM_Selector::QUERY_DOCUMENT, "querySelector", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Document, DOM_Selector::querySelector, DOM_Selector::QUERY_DOCUMENT | DOM_Selector::QUERY_ALL, "querySelectorAll", "s-")
#endif // DOM_SELECTORS_API
DOM_FUNCTIONS_WITH_DATA_END(DOM_Document)

