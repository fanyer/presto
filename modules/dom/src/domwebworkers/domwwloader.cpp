/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2009-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** @author sof
*/
#include "core/pch.h"

#ifdef DOM_WEBWORKERS_SUPPORT
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domevents/domeventlistener.h"

#include "modules/dom/src/domwebworkers/domwebworkers.h"
#include "modules/dom/src/domwebworkers/domwwloader.h"
#include "modules/dom/src/domwebworkers/domwwutils.h"
#include "modules/dom/src/domwebworkers/domcrossutils.h"

#include "modules/encodings/utility/charsetnames.h"

void
DOM_WebWorker_Loader::Cancel()
{
    Abort(NULL); // returns void.
    worker = NULL;
}

void
DOM_WebWorker_Loader::ClearWorker()
{
    worker = NULL;
}

void
DOM_WebWorker_Loader::Shutdown()
{
    Abort(NULL);
    if (worker)
        worker->GetRuntime()->SetErrorHandler(worker);

    ClearWorker();
}

/* static */ OP_STATUS
DOM_WebWorker_Loader::RetrieveData(URL_DataDescriptor *descriptor, BOOL &more)
{
    TRAPD(status, descriptor->RetrieveData(more));
    return status;
}

/* static */ OP_STATUS
DOM_WebWorker_Loader::LoadScripts(DOM_Object *this_object, DOM_WebWorker *worker, DOM_WebWorkerObject *origin_worker, OpAutoVector<URL> *load_urls, ES_Value *return_value, ES_Thread *interrupt_thread)
{
    FramesDocument *document = DOM_WebWorker_Utils::GetWorkerFramesDocument(worker, NULL);
    if (!document || !worker || load_urls->GetCount() == 0)
    {
        OP_DELETE(load_urls);
        return OpStatus::ERR;
    }

    DOM_Runtime *runtime = worker->GetRuntime();

    /* The loader uses the FramesDocument of the browser context that created it (or its ancestor if nested.) */
    DOM_WebWorker_Loader *loader = OP_NEW(DOM_WebWorker_Loader, ());
    if (!loader || OpStatus::IsError(loader->SetObjectRuntime(runtime, runtime->GetObjectPrototype(), "Object")))
    {
        OP_DELETE(load_urls);
        OP_DELETE(loader);
        return OpStatus::ERR_NO_MEMORY;
    }

    loader->worker = worker;
    loader->origin_worker = origin_worker;
    loader->import_urls = load_urls;
    loader->index_url = 0;

    /* Record the DOM_WebWorker as now using this loader; the Worker 'owns' it for the purposes of GC tracing. */
    RETURN_IF_ERROR(worker->PushActiveLoader(loader));
    OP_BOOLEAN result = loader->LoadNextScript(this_object, return_value, interrupt_thread);
    return (result == OpBoolean::IS_FALSE ? OpStatus::OK : result);
}

OP_BOOLEAN
DOM_WebWorker_Loader::LoadNextScript(DOM_Object *this_object, ES_Value *return_value, ES_Thread *interrupt_thread)
{
    loading_started = FALSE;
    if (NoMoreScripts())
        return OpBoolean::IS_FALSE;
    else if (FramesDocument *frames_doc = DOM_WebWorker_Utils::GetWorkerFramesDocument(worker, NULL))
    {
        URL *url = GetCurrentScriptURL();
        OP_ASSERT(url);

        /* The Worker constructors perform a security check of the initial .js to
           load. And synchronously, once the execution environment has been set
           up. Do not repeat it here for those. */
        if (interrupt_thread)
        {
            BOOL allowed = FALSE;
            OpSecurityContext source(worker->GetLocationURL());
            RETURN_IF_ERROR(g_secman_instance->CheckSecurity(OpSecurityManager::WEB_WORKER_SCRIPT_IMPORT, source, OpSecurityContext(*url), allowed));
            if (!allowed)
            {
                this_object->CallDOMException(DOM_Object::SECURITY_ERR, return_value);
                return OpStatus::ERR;
            }
        }
        target = *url;

        /* Do this within the context of the given runtime */
        has_started = FALSE;

        /* Untidy, but initiating the inline load might cause LoadingStopped()
           to be invoked, signalling that the load has already been completed
           (before the LoadInline() returns.). Need to block the initiating
           interrupt_thread in that case, so temporarily record the thread on
           the object. */
        if (index_url == 0)
            this->interrupting_thread = interrupt_thread;
        OP_LOAD_INLINE_STATUS load_status = frames_doc->LoadInline(target, this, TRUE);
        if (index_url == 0)
            this->interrupting_thread = NULL;
        if (load_status == LoadInlineStatus::USE_LOADED && !has_started)
            return StartEvalScript(return_value);
        else if (load_status == LoadInlineStatus::LOADING_STARTED)
        {
            loading_started = TRUE;
            /* If we're dealing with the first script resource and it isn't readily available;
               block the dependent thread. When unblocked at completion of load & execute
               from importScripts(), it will restart a call to importScripts() and see the
               return value (or exception, if that was the outcome.) */
            if (index_url == 0 && interrupt_thread)
            {
                this->interrupted_thread = interrupt_thread;
                interrupt_thread->AddListener(this);
                interrupt_thread->Block();
            }
            return OpBoolean::IS_TRUE;
        }
        else if (load_status == LoadInlineStatus::LOADING_CANCELLED && return_value && this_object)
        {
            this_object->CallDOMException(DOM_Object::NOT_FOUND_ERR, return_value);
            return OpStatus::ERR;
        }
        else
            return (OpStatus::IsError(load_status) ? load_status : OpStatus::OK);
    }
    else
        return OpStatus::ERR;
}

OP_STATUS
DOM_WebWorker_Loader::GetData(TempBuffer *buffer, BOOL &success)
{
    /* Unfortunate, but GetData() may be called by inline loader, so need to handle the case if this will happen even after we've aborted. */
    if (aborted)
        return OpStatus::ERR;

    DOM_EnvironmentImpl *env = NULL;
    if (FramesDocument *document = DOM_WebWorker_Utils::GetWorkerFramesDocument(worker, &env))
        document->StopLoadingInline(target, this);

    /* target is now the final, redirected URL. */
    if (target.Status(FALSE) != URL_LOADED)
        success = FALSE;
    else
    {
        if (target.Type() == URL_HTTP || target.Type() == URL_HTTPS)
        {
            unsigned response_code = target.GetAttribute(URL::KHTTP_Response_Code, TRUE);
            switch (response_code)
            {
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
                    uni_char code[16]; // ARRAY OK 2010-10-06 sof

                    OpString url_str;
                    OpStatus::Ignore(target.GetAttribute(URL::KUniName_With_Fragment, url_str));

                    RETURN_IF_ERROR(description.Append(UNI_L("Network error, status: ")));
                    uni_snprintf(code, ARRAY_SIZE(code), UNI_L("%d"), response_code);
                    RETURN_IF_ERROR(description.Append(code));

                    /* Figure out the receiver -- if it is destined for the (Shared)Worker object at the other end; ferret it out.
                       If not, forward it to the 'worker'. Painful, the only function of 'origin_worker' is to accomplish this, but
                       this is the only place where we have to do targetted worker object error reporting rather than general
                       broadcast/forwarding story elsewere.
                       And, as it happens, the only place where we care about .onerror on a SharedWorker. */
                    DOM_Object *target_worker = (interrupted_thread || !origin_worker ? static_cast<DOM_Object *>(worker) : static_cast<DOM_Object *>(origin_worker));
                    if (OpStatus::IsSuccess(DOM_ErrorException_Utils::BuildErrorEvent(target_worker, error_event, url_str.CStr(), description.CStr(), response_code, TRUE)))
                    {
                        if (!interrupted_thread)
                        {
                            /* Report the error event at the (Shared)Worker object. */
                            DOM_WebWorkerBase *target = origin_worker ? static_cast<DOM_WebWorkerBase *>(origin_worker) : static_cast<DOM_WebWorkerBase *>(worker);
                            return target->InvokeErrorListeners(error_event, TRUE);
                        }
                        else
                        {
                            ES_Value exception;
                            DOM_Object::DOMSetObject(&exception, error_event);
                            /* Unblock caller and have it handle the exception. */
                            Abort(NULL);
                            worker->CallDOMException(DOM_Object::NETWORK_ERR, &exception); // ignore the ES_EXCEPTION return value;
                            exception_raised = TRUE;
                        }
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
        /* Worker scripts are required to be UTF-8 encoded. */
        unsigned short charset_id = 0;
        OpStatus::Ignore(g_charsetManager->GetCharsetID("utf-8", &charset_id));

        URL_DataDescriptor *descriptor = target.GetDescriptor(NULL, FALSE, FALSE, TRUE, NULL, URL_X_JAVASCRIPT, charset_id);
        if (descriptor)
        {
            OpAutoPtr<URL_DataDescriptor> descriptor_anchor(descriptor);
            BOOL more = TRUE;

            while (more)
            {
                RETURN_IF_ERROR(DOM_WebWorker_Loader::RetrieveData(descriptor, more));
                RETURN_IF_ERROR(buffer->Append(reinterpret_cast<const uni_char *>(descriptor->GetBuffer()), UNICODE_DOWNSIZE(descriptor->GetBufSize())));
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
DOM_WebWorker_Loader::StartEvalScript(ES_Value *return_value)
{
    has_started = TRUE;

    URL moved_to = target.GetAttribute(URL::KMovedToURL, TRUE);
    if (!moved_to.IsEmpty())
        target = moved_to;

    lock.SetURL(target);

    TempBuffer buffer;
    BOOL success;
    RETURN_IF_ERROR(GetData(&buffer, success));
    if (!success || !worker)
    {
        Abort(NULL);
        if (worker && return_value)
            worker->CallDOMException(DOM_Object::NOT_FOUND_ERR, return_value);
        exception_raised = TRUE;
        return OpStatus::ERR;
    }

    if (interrupting_thread)
    {
        interrupted_thread = interrupting_thread;
        interrupting_thread->Block();
        interrupting_thread->AddListener(this);
    }
    if (worker && !interrupted_thread)
        worker->SetLocationURL(target);

    ES_ProgramText program_array[1];
    int program_array_length = ARRAY_SIZE(program_array);
    program_array[0].program_text = buffer.GetStorage();
    program_array[0].program_text_length = buffer.Length();

    OP_ASSERT(worker && worker->GetEnvironment());
    ES_AsyncInterface *asyncif = worker ? worker->GetEnvironment()->GetRuntime()->GetESAsyncInterface() : NULL;

    /* The worker has gone, just shut down loader. */
    if (!asyncif)
    {
        Abort(NULL);
        return OpStatus::ERR;
    }

    /* If exceptions are thrown during evaluation, then we'd want to have those reported directly. */
    asyncif->SetWantExceptions();
    worker->GetRuntime()->SetErrorHandler(this);

    ES_Object *scope_chain[1];
    scope_chain[0] = worker->GetNativeObject();

    OP_STATUS status = asyncif->Eval(program_array, program_array_length, scope_chain, ARRAY_SIZE(scope_chain),
                                     worker /*callback*/, interrupted_thread, scope_chain[0]);
    if (OpStatus::IsError(status))
    {
        if (interrupted_thread || return_value)
        {
            /* Abnormal state, unable to report back the exception, so just perform an orderly shut down. */
            exception_raised = TRUE;
            Abort(NULL);
            if (worker && return_value)
                worker->CallDOMException(DOM_Object::SYNTAX_ERR, return_value); // ignore the ES_EXCEPTION return value;
            else if (worker)
            {
                worker->CallDOMException(DOM_Object::SYNTAX_ERR, &excn_value); // ignore the ES_EXCEPTION return value;
                OpStatus::Ignore(worker->SetLoaderReturnValue(excn_value)); // ignoring OOM.
            }
        }
        else
        {
            /* An async invocation (via a Worker constructor call), so propagate error via the usual .onerror path */
            DOM_ErrorEvent *error_event = NULL;
            const uni_char *message = UNI_L("Syntax error processing script");

            OpString url_str;
            OpStatus::Ignore(GetCurrentScriptURL()->GetAttribute(URL::KUniName_With_Fragment, url_str));
            if (OpStatus::IsSuccess(DOM_ErrorException_Utils::BuildErrorEvent(worker, error_event, url_str.CStr(), message, 0, TRUE)))
            {
                ES_Runtime::ErrorHandler::RuntimeErrorInformation error_information;
                DOM_Object::DOMSetObject(&error_information.exception, error_event);
                ES_SourcePosition position;
                OpStatus::Ignore(worker->HandleError(worker->GetRuntime(), &error_information, message, url_str.CStr(), position, NULL));
                Abort(NULL);
            }
        }
        /* return status of failing evaluation */
    }
    return status;
}

/* virtual */ OP_STATUS
DOM_WebWorker_Loader::HandleError(ES_Runtime *runtime, const ES_Runtime::ErrorHandler::ErrorInformation *information, const uni_char *msg, const uni_char *url, const ES_SourcePosition &position, const ES_ErrorData *data)
{
    /* Record the exception value, which the worker will fetch upon being unblocked. */
    if (information->type == ES_Runtime::ErrorHandler::RUNTIME_ERROR)
    {
        if (worker)
            RETURN_IF_ERROR(worker->SetLoaderReturnValue(static_cast<const RuntimeErrorInformation *>(information)->exception));
        exception_raised = TRUE;
        Abort(NULL);
        OP_ASSERT(!ES_ThreadListener::InList());
        OP_STATUS status = OpStatus::OK;
        if (worker && !worker->IsClosed())
            status = worker->HandleError(runtime, information, msg, url, position, data);
        else
            runtime->IgnoreError(data);
        ClearWorker();
        return status;
    }
    else
    {
        runtime->IgnoreError(data);
        return OpStatus::OK;
    }
}

OP_STATUS
DOM_WebWorker_Loader::HandleCallback(ES_AsyncStatus status, const ES_Value &result)
{
    /* The loader is notified by DOM_WebWorker::ES_AsyncCallback::HandleCallback() (before doing any other processing.)
       On the loader side we check to see if the status update means we're done loading, or if not, start loading the
       next step. */
    if (aborted)
        return OpStatus::OK;

    BOOL stop_loading = FALSE;
    OP_BOOLEAN load_result = OpBoolean::IS_FALSE;
    OP_STATUS stat = OpStatus::OK;

    if (status == ES_ASYNC_EXCEPTION && interrupted_thread)
    {
        /* Record the exception value, which the worker will fetch upon being unblocked. */
        if (worker)
            RETURN_IF_ERROR(worker->SetLoaderReturnValue(result));
        exception_raised = TRUE;
        stop_loading = TRUE;
    }
    else if (status == ES_ASYNC_SUCCESS && (loading_started || !MoveToNextScript()) )
        /* no more scripts to process. */
        stop_loading = TRUE;
    else if (status == ES_ASYNC_SUCCESS && (OpStatus::IsError(load_result = LoadNextScript()) || load_result == OpBoolean::IS_FALSE))
    {
        /* unable to load next script */
        stop_loading = TRUE;
        stat = load_result == OpBoolean::IS_FALSE ? OpStatus::OK : load_result;
    }

    /* end of scripts to be loaded; shut down. */
    if (!aborted && stop_loading)
        Abort(NULL);

    if (!exception_raised && aborted)
        Shutdown();

    return stat;
}

/* virtual */ OP_STATUS
DOM_WebWorker_Loader::Signal(ES_Thread *signalled, ES_ThreadSignal signal)
{
    switch (signal)
    {
    case ES_SIGNAL_FINISHED:
        {
            OP_BOOLEAN result = OpBoolean::IS_FALSE;
            /* start loading any successive scripts. */
            if ( !MoveToNextScript() || OpStatus::IsError(result = LoadNextScript()) || result == OpBoolean::IS_FALSE )
                return (result == OpBoolean::IS_FALSE ? OpStatus::OK : result);
            else
                Abort(NULL);
            break;
        }
    case ES_SIGNAL_FAILED:
    case ES_SIGNAL_CANCELLED:
        Abort(NULL);
        break;
    }
    return OpStatus::OK;
}

/* virtual */ void
DOM_WebWorker_Loader::LoadingStopped(FramesDocument *document, const URL &url)
{
    if (!worker || worker->IsClosed())
    {
        Abort(document);
        OP_ASSERT(!ES_ThreadListener::InList());
        return;
    }
    target = *GetCurrentScriptURL();
    loading_started = FALSE;
    OpStatus::Ignore(StartEvalScript(NULL));
}

/* virtual */ void
DOM_WebWorker_Loader::LoadingRedirected(FramesDocument *document, const URL &from, const URL &to)
{
    if (!interrupted_thread && !DOM_Object::OriginCheck(from, to))
    {
        /* Loading of worker scripts are bound by a same-origin restriction
           on redirects. If that is violated, fire an 'error' event at the
           worker and abort. */
        Abort(document);
        if (worker && !worker->IsClosed())
        {
            DOM_ErrorEvent *event = NULL;
            const uni_char *msg = UNI_L("Security error");
            unsigned int line_number = 0;

            OpString url_str;
            RETURN_VOID_IF_ERROR(from.GetAttribute(URL::KUniName_With_Fragment, url_str));
            RETURN_VOID_IF_ERROR(DOM_ErrorException_Utils::BuildErrorEvent(worker, event, url_str.CStr(), msg, line_number, TRUE));

            ES_Runtime::ErrorHandler::RuntimeErrorInformation error_information;
            DOM_Object::DOMSetObject(&error_information.exception, event);
            ES_SourcePosition position;
            OpStatus::Ignore(worker->HandleError(worker->GetRuntime(), &error_information, msg, url_str.CStr(), position, NULL));
        }
    }
}

void
DOM_WebWorker_Loader::Abort(FramesDocument *doc)
{
    if (aborted)
        return;

    aborted = TRUE;
    ES_ThreadListener::Remove();

    if (interrupted_thread)
    {
        /* The unblocking may return an error if there's a blocking type mismatch, but we
           elect to ignore it here during abort. */
        OpStatus::Ignore(interrupted_thread->Unblock());
        interrupted_thread = NULL;
    }

    if (!doc && worker)
        doc = DOM_WebWorker_Utils::GetWorkerFramesDocument(worker, NULL);

    if (doc)
        doc->StopLoadingInline(target, this);
}

/* virtual */
DOM_WebWorker_Loader::~DOM_WebWorker_Loader()
{
    if (import_urls)
    {
        OP_DELETE(import_urls);
        import_urls = NULL;
    }

    ES_ThreadListener::Remove();
    ExternalInlineListener::Out();
}

#endif // DOM_WEBWORKERS_SUPPORT
