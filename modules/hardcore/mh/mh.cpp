/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/hardcore/mh/mh.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/dochand/win.h"
#include "modules/doc/frm_doc.h"
#include "modules/util/str.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/locale/locale-enum.h"
#include "modules/libcrypto/include/OpRandomGenerator.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"
#ifdef OPERA_PERFORMANCE
#include "modules/url/url_man.h"
#endif // OPERA_PERFORMANCE
#include "modules/hardcore/cpuusagetracker/cpuusagetracker.h"
#include "modules/hardcore/cpuusagetracker/cpuusagetrackeractivator.h"

MessageHandler::MessageHandler(Window* win, DocumentManager* docman) :
	window(win),
#ifdef CPUUSAGETRACKING
	cpu_usage_tracker(win ? win->GetCPUUsageTracker() : NULL),
#endif // CPUUSAGETRACKING
	docman(docman),
	component(g_opera),
	handle_msg_count(0),
	msb_box_id(0)
{
}

OP_STATUS
MessageHandler::SetCallBack(MessageObject* obj, OpMessage msg, MH_PARAM_1 id, int priority)
{
	// OP_ASSERT(FALSE); // This function is deprecated. Use SetCallBack(MessageObject*, OpMessage, MH_PARAM_1) instead.
	return SetCallBack(obj, msg, id);
}

OP_STATUS
MessageHandler::SetCallBack(MessageObject* obj, OpMessage msg, MH_PARAM_1 id)
{
	return SetCallBackList(obj, id, &msg, 1);
}

OP_STATUS
MessageHandler::SetCallBackList(MessageObject* obj, MH_PARAM_1 id, int priority, const OpMessage* messagearray, size_t messages)
{
	// OP_ASSERT(FALSE); // This function is deprecated. Use SetCallBackList(MessageObject*, MH_PARAM_1, const OpMessage*, int) instead.
	return SetCallBackList(obj, id, messagearray, messages);
}

OP_STATUS
MessageHandler::SetCallBackList(MessageObject* obj, MH_PARAM_1 id, const OpMessage* messagearray, size_t messages)
{
	return listeners.AddListeners(obj, messagearray, messages, id);
}

BOOL
MessageHandler::HasCallBack(MessageObject* obj, OpMessage msg, MH_PARAM_1 id)
{
	return listeners.HasListener(obj, msg, id);
}

void
MessageHandler::UnsetCallBack(MessageObject* obj, OpMessage msg, MH_PARAM_1 id)
{
	listeners.RemoveListener(obj, msg, id);
}

void
MessageHandler::UnsetCallBack(MessageObject* obj, OpMessage msg)
{
	listeners.RemoveListeners(obj, msg);
}

void
MessageHandler::RemoveCallBack(MessageObject* obj, MH_PARAM_1 id)
{
	RemoveCallBacks(obj, id);
}

void
MessageHandler::RemoveCallBack(MessageObject* obj, OpMessage msg)
{
	UnsetCallBack(obj, msg);
}

void
MessageHandler::RemoveCallBacks(MessageObject* obj, MH_PARAM_1 id)
{
	listeners.RemoveListeners(obj, id);
}

void
MessageHandler::UnsetCallBacks(MessageObject* obj)
{
	listeners.RemoveListeners(obj);
}

void
MsgHndlList::Clean()
{
	for (Iterator itr = mh_list.Begin(); itr != mh_list.End(); ++itr)
	{
		mh_list.Remove(itr);
		OP_DELETE(*itr);
	}
}

void
MsgHndlList::CleanList()
{
	for (Iterator itr = mh_list.Begin(); itr != mh_list.End(); ++itr)
	{
		if ((*itr)->mh.get() == NULL)
		{
			mh_list.Remove(itr);
			OP_DELETE(*itr);
		}
	}
}

OP_STATUS
MsgHndlList::Add(MessageHandler* m, BOOL inline_loading, BOOL check_modified_silent, BOOL load_silent, BOOL always_new, BOOL first)
{
	CleanList();

	if (!always_new)
	{
		for (Iterator itr = mh_list.Begin(); itr != mh_list.End(); ++itr)
		{
			if ((*itr)->mh.get() == m)
			{
				(*itr)->load_as_inline = inline_loading;
				(*itr)->check_modified_silent = check_modified_silent;
				(*itr)->load_silent = load_silent;

				return OpStatus::OK;
			}
		}
	}

	if (m)
	{
		MsgHndlListElm* mle = OP_NEW(MsgHndlListElm,(m, inline_loading, check_modified_silent, load_silent));
		RETURN_OOM_IF_NULL(mle);
		OP_STATUS status;
        if (first)
			status = mh_list.Prepend(mle);
		else
			status = mh_list.Append(mle);
		if (OpStatus::IsError(status))
			OP_DELETE(mle);
		return status;
	}

	return OpStatus::OK;
}

void
MsgHndlList::Remove(MessageHandler* m)
{
	for (Iterator itr = mh_list.Begin(); itr != mh_list.End(); ++itr)
	{
		if ((*itr)->mh.get() == m)
		{
			mh_list.Remove(itr);
			OP_DELETE(*itr);
		}
	}
}

BOOL
MsgHndlList::LocalBroadcastMessage(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, int flags, unsigned long delay)
{
	CleanList();
	for (Iterator itr = mh_list.Begin(); itr != mh_list.End(); ++itr)
	{
		if ((flags == MH_LIST_ALL ||
			 ((((*itr)->load_as_inline && (flags & MH_LIST_INLINE_ONLY)) ||
			   (!(*itr)->load_as_inline && (flags & MH_LIST_NOT_INLINE_ONLY)) ||
              (!(flags & MH_LIST_INLINE_ONLY) && !(flags & MH_LIST_NOT_INLINE_ONLY))) &&
			  (!(*itr)->load_silent || !(flags & MH_LIST_NOT_LOAD_SILENT_ONLY)) &&
			  (!(*itr)->check_modified_silent || !(flags & MH_LIST_NOT_MODIFIED_SILENT_ONLY)))) )
		{
			(*itr)->mh->PostMessage(msg, par1, par2, delay);
		}
	}

	return TRUE;
}

MessageHandler*
MsgHndlList::FirstMsgHandler()
{
	CleanList();
	if (!mh_list.IsEmpty())
		return mh_list.First()->mh.get();
	else
		return NULL;
}

BOOL
MsgHndlList::HasMsgHandler(MessageHandler* m, BOOL inline_loading, BOOL check_modified_silent, BOOL load_silent)
{
	CleanList();
	for (Iterator itr = mh_list.Begin(); itr != mh_list.End(); ++itr)
	{
		if ((*itr)->mh.get() == m &&
			(*itr)->load_as_inline == inline_loading &&
			(*itr)->check_modified_silent == check_modified_silent &&
			(*itr)->load_silent == load_silent)
			return TRUE;
	}

	return FALSE;
}

void
MsgHndlList::SetProgressInformation(ProgressState progress_level,
									unsigned long progress_info1,
									const uni_char *progress_info2,
									URL* url)
{
	CleanList();
	for (Iterator itr = mh_list.Begin(); itr != mh_list.End(); ++itr)
	{
		MessageHandler* cmh = (*itr)->GetMessageHandler();
		Window *win = cmh ? cmh->GetWindow() : NULL;
		if(!win && cmh && cmh->GetDocumentManager())
			win = cmh->GetDocumentManager()->GetWindow();
		if (win)
		{
			switch (progress_level)
			{
			case REQUEST_QUEUED:
			case START_NAME_LOOKUP:
			case START_CONNECT_PROXY:
			case WAITING_FOR_CONNECTION:
			case START_CONNECT:
			case START_REQUEST:
			case UPLOADING_FINISHED:
			case REQUEST_FINISHED:
			case WAITING_FOR_COOKIES:
			case WAITING_FOR_COOKIES_DNS:
			case STARTED_40SECOND_TIMEOUT:
			case EXECUTING_ECMASCRIPT:
			case START_NAME_COMPLETION  : // 27/11/98 YNP
#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_)
			case START_SECURE_CONNECTION: // 28/11/98 YNP
#endif
				win->SetProgressState(progress_level, progress_info2, cmh, 0, url);
				break;
			case SET_SECURITYLEVEL:
					win->SetSecurityState((unsigned char)progress_info1,(*itr)->GetLoadAsInline(), progress_info2, url);
					break;
			case UPLOADING_PROGRESS:
				{
					win->HandleDataProgress(progress_info1, TRUE, progress_info2, cmh, url);
					break;
				}
			case UPLOAD_COUNT:
				{
					win->SetUploadFileCount(HIWORD(progress_info1), LOWORD(progress_info1));
					win->SetProgressState(UPLOADING_FILES, progress_info2, NULL, 0, url);
					break;
				}
			case LOAD_PROGRESS:
				{
					win->HandleDataProgress(progress_info1, FALSE, progress_info2, NULL, url);
					break;
				}
			case GET_ORIGINATING_WINDOW:
			{
				*((const OpWindow **)progress_info2) = win->GetOpWindow();
				break;
			}
			case GET_ORIGINATING_CORE_WINDOW:
			{
				*((const Window **)progress_info2) = win;
                break;
			}
			default:
				break;
			}
		}
	}
}

void
MessageHandler::AddObserver(MessageHandlerObserver *observer)
{
	if (observer->InList())
		if (observers.HasLink(observer))
			return;
		else
			observer->Out();

	observer->Into(&observers);
}

void
MessageHandler::RemoveObserver(MessageHandlerObserver *observer)
{
	OP_ASSERT(observers.HasLink(observer));
	observer->Out();
}

// The return can be TRUE (message was posted) or FALSE (not posted due to OOM).
//
// It is not normally necessary to check for OOM because it is handled
// inside the logic of PostDelayedMessage(), and because the code has
// been written to minimize the chance of OOM ever happening.  You only
// need to check the return value if you are intensely curious.

BOOL
MessageHandler::PostDelayedMessage(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, unsigned long delay)
{
	return PostMessage(msg, par1, par2, delay);
}

class LegacyMessageSelection
	: public OpTypedMessageSelection
{
	CoreComponent* m_component;
public:
	LegacyMessageSelection(CoreComponent* component) : m_component(component) {}

	virtual BOOL IsMember(const OpLegacyMessage* m) const = 0;

	virtual BOOL IsMember(const OpTypedMessage* m) const {
		return m->GetType() == OpLegacyMessage::Type
			&& m->GetSrc() == m_component->GetAddress()
			&& IsMember(OpLegacyMessage::Cast(m));
	}
};

class DelayedMessageSelection1
	: public LegacyMessageSelection
{
public:
	DelayedMessageSelection1(CoreComponent* component, OpMessage msg, MH_PARAM_1 par1, MessageHandler* mh)
		: LegacyMessageSelection(component)
		, m_msg(msg), m_par1(par1), m_mh(mh) {}

	virtual BOOL IsMember(const OpLegacyMessage* m) const {
		return m->GetMessage() == m_msg && m->GetParam1() == m_par1 && m->GetMessageHandler() == m_mh;
	}

private:
	OpMessage m_msg;
	MH_PARAM_1 m_par1;
	MessageHandler* m_mh;
};

class DelayedMessageSelection2
	: public DelayedMessageSelection1
{
public:
	DelayedMessageSelection2(CoreComponent* component, OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, MessageHandler* mh)
		: DelayedMessageSelection1(component, msg, par1, mh), m_par2(par2) {}

	virtual BOOL IsMember(const OpLegacyMessage* m) const {
		return DelayedMessageSelection1::IsMember(m) && m->GetParam2() == m_par2;
	}

private:
	MH_PARAM_2 m_par2;
};

void
MessageHandler::RemoveDelayedMessage(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(component == g_opera);
	DelayedMessageSelection2 s(component, msg, par1, par2, this);
	g_message_dispatcher->RemoveMessages(&s);
}

void
MessageHandler::RemoveDelayedMessages(OpMessage msg, MH_PARAM_1 par1)
{
	OP_ASSERT(component == g_opera);
	DelayedMessageSelection1 s(component, msg, par1, this);
	g_message_dispatcher->RemoveMessages(&s);
}

void
MessageHandler::RemoveFirstDelayedMessage(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(component == g_opera);
	DelayedMessageSelection2 s(component, msg, par1, par2, this);
	g_message_dispatcher->RemoveFirstMessage(&s);
}

void
MessageHandler::RemoveFirstDelayedMessage(OpMessage msg, MH_PARAM_1 par1)
{
	OP_ASSERT(component == g_opera);
	DelayedMessageSelection1 s(component, msg, par1, this);
	g_message_dispatcher->RemoveFirstMessage(&s);
}

class MessageHandlerSelection
	: public LegacyMessageSelection
{
public:
	MessageHandlerSelection(CoreComponent* component, MessageHandler* mh)
		: LegacyMessageSelection(component)
		, m_mh(mh) {}
	virtual BOOL IsMember(const OpLegacyMessage* m) const { return m->GetMessageHandler() == m_mh; }

private:
	MessageHandler* m_mh;
};

MessageHandler::~MessageHandler()
{
#ifdef DEBUG_HC_MESSAGEHANDLER
	OP_NEW_DBG("MessageHandler::~MessageHandler()", "mh");
	OP_DBG(("this=%p", this));
#endif // DEBUG_HC_MESSAGEHANDLER

	// Remove queued messages destined for this handler.
	OP_ASSERT(component == g_opera);
	MessageHandlerSelection s(component, this);
	g_message_dispatcher->RemoveMessages(&s);

	while (MessageHandlerObserver *observer = static_cast<MessageHandlerObserver *>(observers.First()))
	{
		observer->Out();
		observer->MessageHandlerDestroyed(this);
	}
}

OP_STATUS
MessageHandler::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(component == g_opera);
	TRACK_CPU_PER_TAB(cpu_usage_tracker);

#ifdef OPERA_PERFORMANCE
	urlManager->MessageStarted(msg);
#endif // OPERA_PERFORMANCE

	//probe all messages with msg param so that each message is probed differently
	OP_PROBE0_PARAM(OP_PROBE_HC_MSG, (int)msg);

	listeners.CallListeners(msg, par1, par2);

#ifdef OPERA_PERFORMANCE
	urlManager->MessageStopped(msg);
#endif // OPERA_PERFORMANCE
	return OpStatus::OK;
}

BOOL
MessageHandler::PostMessage(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, unsigned long delay /* = 0 */)
{
	OP_ASSERT(component == g_opera);
	OP_ASSERT(this); // If you trigger this assert, fix the calling code.
	if (!this || !component->IsAlive())
		return FALSE;

	OpLegacyMessage* message = OpLegacyMessage::Create(
		msg, par1, par2, this, component->GetAddress(), component->GetAddress(), static_cast<double>(delay));
	if (!message)
		return FALSE;

	OP_STATUS s = component->SendMessage(message);
	if (OpStatus::IsError(s))
		return FALSE;

#ifdef HC_DETECT_SINGLETON_COMPONENT
	if (g_opera->GetType() == COMPONENT_SINGLETON && !g_opera->running && g_component_manager->GetPlatform())
		g_component_manager->GetPlatform()->RequestRunSlice(delay);
#endif // HC_DETECT_SINGLETON_COMPONENT

	return TRUE;
}


#ifdef DEBUG_HC_MESSAGEHANDLER

#include "modules/util/datefun.h"

extern const char* GetStringFromMessage(OpMessage);

void AppendDebugMessage(const char* format, ...)
{
	OpFile f;
	f.Construct(UNI_L("debug_mh.txt"), OPFILE_HOME_FOLDER);
	f.Open(OPFILE_APPEND);

	OpString8 s;

	va_list args;
	va_start(args, format);
	s.AppendVFormat(format, args);
	va_end(args);

	uni_char timebuf[64]; // ARRAY OK 2008-03-07 jl
	time_t t = op_time(NULL);
	struct tm *now_tm = op_localtime(&t);

	const char *timetemplate =
			"�D �n �Y �h:�m:�s ";
	FMakeDate(*now_tm, timetemplate, timebuf, ARRAY_SIZE(timebuf));
	char* b = uni_down_strdup(timebuf);
	if (b)
	{
		f.Write(b, op_strlen(b));
		op_free(b);

		f.Write(s, s.Length());
	}

	f.Close();
}

double g_last_message_time = 0;
double g_expected_wait = DBL_MAX;

#endif // DEBUG_HC_MESSAGEHANDLER
