/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2009-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** @author sof
*/
#include "core/pch.h"

#ifdef EXTENSION_SUPPORT
#ifdef EXTENSION_FEATURE_IMPORTSCRIPT

#include "modules/dom/src/opera/domextscriptloader.h"
#include "modules/dom/src/extensions/domextension_userjs.h"
#include "modules/dom/src/domwebworkers/domcrossmessage.h"
#include "modules/dom/src/domwebworkers/domcrossutils.h"
#include "modules/encodings/utility/charsetnames.h"
void
DOM_ExtensionScriptLoader::Shutdown()
{
	Abort(NULL);
	if (owner)
		owner->GetRuntime()->SetExceptionCallback(owner);

	owner = NULL;
}

/*static*/ OP_STATUS
DOM_ExtensionScriptLoader::RetrieveData(URL_DataDescriptor *descriptor, BOOL &more)
{
	TRAPD(status, descriptor->RetrieveData(more));
	return status;
}

/*static*/ OP_STATUS
DOM_ExtensionScriptLoader::Make(DOM_ExtensionScriptLoader *&loader, const URL &load_url, FramesDocument *document, DOM_Extension *extension, DOM_Runtime *origining_runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(loader = OP_NEW(DOM_ExtensionScriptLoader, ()), origining_runtime, origining_runtime->GetObjectPrototype(), "Object"));

	/* The loader uses the FramesDocument of the extension to perform the actual loading. */
	loader->script_url = load_url;
	loader->document = document;
	loader->loading_started = FALSE;
	loader->owner = extension;

	return OpStatus::OK;
}

OP_BOOLEAN
DOM_ExtensionScriptLoader::LoadScript(DOM_Object *this_object, ES_Value *return_value, ES_Thread *interrupt_thread)
{
	OP_LOAD_INLINE_STATUS load_status = document->LoadInline(script_url, this);
	if (load_status == LoadInlineStatus::USE_LOADED)
	{
		if (return_value)
			*return_value = excn_value;
		DOMSetUndefined(&excn_value);
		return OpBoolean::IS_TRUE;
	}
	else if (load_status == LoadInlineStatus::LOADING_STARTED)
	{
		loading_started = TRUE;
		/* Not available; block the dependent thread. When unblocked at completion of load, it'll restart. */
		if (interrupt_thread)
		{
			waiting = interrupt_thread;
			interrupt_thread->AddListener(this);
			interrupt_thread->Block();
		}
		return OpBoolean::IS_FALSE;
	}
	else if (load_status == LoadInlineStatus::LOADING_CANCELLED && return_value && this_object)
	{
		DOM_CALL_DOMEXCEPTION(NOT_FOUND_ERR);
		return OpStatus::ERR;
	}
	else
		return (OpStatus::IsError(load_status) ? load_status : OpStatus::OK);
}

OP_STATUS
DOM_ExtensionScriptLoader::GetData(TempBuffer *buffer, BOOL &success)
{
	/* Unfortunate, but GetData() may be called by inline loader, so need to handle the case if this will happen even after we've aborted. */
	if (aborted)
		return OpStatus::ERR;

	if (FramesDocument *document = owner ? owner->GetEnvironment()->GetFramesDocument() : NULL)
		document->StopLoadingInline(script_url, this);

	if (script_url.Status(TRUE) != URL_LOADED)
		success = FALSE;
	else
	{
		if (script_url.Type() == URL_HTTP || script_url.Type() == URL_HTTPS || script_url.Type() == URL_WIDGET)
		{
			unsigned response_code = script_url.GetAttribute(URL::KHTTP_Response_Code, TRUE);
			switch (response_code)
			{
			case HTTP_NO_RESPONSE:
			case HTTP_OK:
			case HTTP_NOT_MODIFIED:
				success = TRUE;
				break;
			default:
				success = FALSE;
				if (response_code >= 300)
				{
					/* Failure; issue an error event or exception. */
					DOM_ErrorEvent *error_event = NULL;
					OpString description;
					uni_char code[16]; /* ARRAY OK 2010-09-20 sof */

					OpString url_str;
					OpStatus::Ignore(script_url.GetAttribute(URL::KUniName_With_Fragment, url_str));

					description.Append(UNI_L("Network error, status: "));
					uni_snprintf(code, ARRAY_SIZE(code), UNI_L("%d"), response_code);
					description.Append(code);

					if (OpStatus::IsSuccess(DOM_ErrorException_Utils::BuildErrorEvent(owner, error_event, url_str.CStr(), description.CStr(), response_code, TRUE)))
					{
						ES_Value exception;
						DOMSetObject(&exception, error_event);
						/* Unblock caller and have it handle the exception. */
						if (owner)
							owner->SetLoaderReturnValue(exception);
						Abort(NULL);
					}
				}
				break;
			}
		}
		else
			success = TRUE;
	}
	if (success)
	{
		/* Insist on scripts being UTF-8 encoded, like for Web Workers' import mechanism. */
		unsigned short charset_id = 0;
		OpStatus::Ignore(g_charsetManager->GetCharsetID("utf-8", &charset_id));

		URL_DataDescriptor *descriptor = script_url.GetDescriptor(NULL, FALSE, FALSE, TRUE, NULL, URL_X_JAVASCRIPT, charset_id);
		if (descriptor)
		{
			OpAutoPtr<URL_DataDescriptor> descriptor_anchor(descriptor);
			BOOL more = TRUE;

			while (more)
			{
				RETURN_IF_ERROR(DOM_ExtensionScriptLoader::RetrieveData(descriptor, more));
				RETURN_IF_ERROR(buffer->Append(reinterpret_cast<uni_char *>(descriptor->GetBuffer()), UNICODE_DOWNSIZE(descriptor->GetBufSize())));
				descriptor->ConsumeData(descriptor->GetBufSize());
			}
		}
		else
			success = FALSE;
	}
	lock.UnsetURL();
	return OpStatus::OK;
}

OP_STATUS
DOM_ExtensionScriptLoader::GetScriptText(TempBuffer &buffer, ES_Value *return_value)
{
	BOOL success;
	/* Quirky - success reference + OP_STATUS */
	RETURN_IF_ERROR(GetData(&buffer, success));
	if (!success || !owner)
	{
		Abort(NULL);
		if (owner)
			owner->CallDOMException(DOM_Object::NOT_FOUND_ERR, return_value);
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_ExtensionScriptLoader::HandleCallback(ES_AsyncStatus status, const ES_Value &result)
{
	/* The loader is notified by DOM_ExtensionScriptLoader::ES_AsyncCallback::HandleCallback() (before doing any other processing.)
	   On the loader side we check to see if the status update means we're done loading, or if not, start loading the
	   next step. */
	if (aborted)
		return OpStatus::OK;

	if (status == ES_ASYNC_EXCEPTION && waiting)
	{
		/* Record the exception value, which the owner will fetch upon being unblocked. */
		if (owner)
			RETURN_IF_ERROR(owner->SetLoaderReturnValue(result));
		Shutdown();
	}
	else
		Abort(NULL);

	return OpStatus::OK;
}

/*virtual*/ OP_STATUS
DOM_ExtensionScriptLoader::Signal(ES_Thread *signalled, ES_ThreadSignal signal)
{
	switch (signal)
	{
	case ES_SIGNAL_FINISHED:
		ES_ThreadListener::Remove();
		break;
	case ES_SIGNAL_FAILED:
	case ES_SIGNAL_CANCELLED:
		Abort(NULL);
		break;
	}
	return OpStatus::OK;
}

/*virtual*/ void
DOM_ExtensionScriptLoader::LoadingStopped(FramesDocument *document, const URL &url)
{
	if (!owner)
	{
		Abort(document);
		OP_ASSERT(!ES_ThreadListener::InList());
		return;
	}
	script_url = url;

	lock.SetURL(script_url);
	TempBuffer buffer;
	if (OpStatus::IsSuccess(GetScriptText(buffer, &excn_value)))
	{
		if (loading_started && owner)
		{
			DOMSetString(&excn_value, buffer.GetStorage());
			owner->SetLoaderReturnValue(excn_value);
		}
	}
	else
		owner->SetLoaderReturnValue(excn_value);

	DOMSetUndefined(&excn_value);

	loading_started = FALSE;
	Abort(NULL);
}

void
DOM_ExtensionScriptLoader::Abort(FramesDocument *doc)
{
	if (aborted)
		return;

	aborted = TRUE;
	ES_ThreadListener::Remove();

	if (waiting)
	{
		OpStatus::Ignore(waiting->Unblock());
		waiting = NULL;
	}

	if (!doc)
		doc = document;

	if (doc)
		doc->StopLoadingInline(script_url, this);
}

/*virtual*/
DOM_ExtensionScriptLoader::~DOM_ExtensionScriptLoader()
{
	DOMFreeValue(excn_value);
	ES_ThreadListener::Remove();
}

#endif // EXTENSION_FEATURE_IMPORTSCRIPT
#endif // EXTENSION_SUPPORT
