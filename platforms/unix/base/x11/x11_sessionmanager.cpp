/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#include "core/pch.h"

#include "platforms/unix/base/x11/x11_sessionmanager.h"
#include "platforms/unix/base/x11/x11_atomizer.h"
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_opmessageloop.h"
#include "platforms/quix/commandline/StartupSettings.h"
#include "platforms/utilix/x11_all.h"
#include "modules/inputmanager/inputmanager.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/managers/CommandLineManager.h"

#include <sys/time.h>
#include <sys/types.h>
#include <pwd.h>
#include <time.h>
#include <stdarg.h>

using X11Types::Atom;

BOOL g_debug_session = FALSE;


void SmLogger(const char* format, ...)
{
	if (!g_debug_session)
		return;

	va_list args;
	va_start(args, format);

	va_list args_copy;
	va_copy(args_copy, args);

	int length = vsnprintf(NULL, 0, format, args_copy) + 1;
	va_end(args_copy);

	char* buf = OP_NEWA(char, length);
	if (buf)
	{
		vsnprintf(buf, length, format, args);
		va_end(args);

		printf("opera [session] %s\n", buf);

		if (g_folder_manager)
		{
			OpString8 tmp;
			OpString tmp_storage;
			tmp.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_HOME_FOLDER, tmp_storage));
			tmp.Append("/session.log");
			FILE* file = fopen(tmp.CStr(), "a");	
			if (file)
			{
				fprintf(file, "opera [session] %s\n", buf);
				fclose(file);
			}
		}
		OP_DELETEA(buf);
	}
}




X11SessionManager* X11SessionManager::m_manager = 0;

// static
OP_STATUS X11SessionManager::Create()
{
	OP_STATUS rc = OpStatus::OK;
	if (!m_manager && IsSystemSessionManagerActive())
	{
		m_manager = OP_NEW(X11SessionManager,());
		if (!m_manager)
			return OpStatus::ERR_NO_MEMORY;
		rc = m_manager->Init();
		if (OpStatus::IsError(rc))
		{
			OP_DELETE(m_manager);
			m_manager = 0;
		}
	}
	return rc;
}


// static
void X11SessionManager::Destroy()
{
	OP_DELETE(m_manager);
	m_manager = 0;
}


// static
X11SessionManager* X11SessionManager::Self()
{
	return m_manager;
}


// static 
OP_STATUS X11SessionManager::AddX11SessionManagerListener(X11SessionManagerListener* listener)
{
	OP_STATUS rc = OpStatus::OK;
	if (m_manager)
		rc = m_manager->m_session_manager_listener.Add(listener);
	return rc;
}


// static 
OP_STATUS X11SessionManager::RemoveX11SessionManagerListener(X11SessionManagerListener* listener)
{
	OP_STATUS rc = OpStatus::OK;
	if (m_manager)
		rc = m_manager->m_session_manager_listener.Remove(listener);
	return rc;
}


X11SessionManager::X11SessionManager()
	:m_connection(0)
	,m_save_type(-1)
	,m_interact_style(SmInteractStyleNone)
	,m_interact_state(InteractNone)
	,m_save_yourself_state(SaveYourselfNone)
	,m_shutdown(FALSE)
	,m_cancelled(FALSE)
{
}


X11SessionManager::~X11SessionManager()
{
	SmLogger("destruct");
	Detach(); // Always detach before close!
	if (m_connection)
        SmcCloseConnection(m_connection, 0, 0);
}


OP_STATUS X11SessionManager::Init()
{
	OP_STATUS rc = OpStatus::OK;

	OpString8 error;
	error.Reserve(500);

	SmcCallbacks cb;
    cb.save_yourself.callback = SM_save_yourself_CB;
    cb.save_yourself.client_data = (SmPointer)this;
    cb.die.callback = SM_die_CB;
    cb.die.client_data = (SmPointer)this;
    cb.save_complete.callback = SM_save_complete_CB;
    cb.save_complete.client_data = (SmPointer)this;
    cb.shutdown_cancelled.callback = SM_shutdown_cancelled_CB;
    cb.shutdown_cancelled.client_data = (SmPointer)this;

	m_id.Set(g_startup_settings.session_id.CStr());
	m_key.Set(g_startup_settings.session_key.CStr());

	char* new_session_id = 0;

	m_connection = SmcOpenConnection(
		0, 0, 1, 0, SmcSaveYourselfProcMask | SmcDieProcMask | SmcSaveCompleteProcMask | SmcShutdownCancelledProcMask,
		&cb, m_id.CStr(), &new_session_id, error.Capacity(), error.CStr() );

	if (m_connection)
	{
		int fd = IceConnectionNumber(SmcGetIceConnection(m_connection));
		rc = g_posix_selector->Watch(fd, PosixSelector::READ, this);
		if (OpStatus::IsError(rc))
		{
			printf("opera: Session management problem: %d\n", rc);
		}
	}
	else
	{
		if (error.HasContent())
			printf("opera: Session management problem: %s\n", error.CStr());
		rc = OpStatus::ERR;
	}

	if (OpStatus::IsSuccess(rc))
	{
		m_id.Set(new_session_id);
		free(new_session_id);

		SmLogger("session id: %s", m_id.CStr());
		SmLogger("connection: %d", IceConnectionNumber(SmcGetIceConnection(m_connection)));
	
		if (m_id.HasContent())
		{
			XChangeProperty(g_x11->GetDisplay(), g_x11->GetAppWindow(), ATOMIZE(SM_CLIENT_ID),
							XA_STRING, 8, PropModeReplace,(unsigned char *)m_id.CStr(), m_id.Length());
		}

		m_timer.SetTimerListener(this);
	}

	return rc;
}


void X11SessionManager::Reset()
{
	m_save_type      = -1;
	m_interact_style = SmInteractStyleNone;
	m_interact_state = InteractNone;
	m_shutdown       = FALSE;
	m_cancelled      = FALSE;

	m_timer.Stop();
}


void X11SessionManager::OnReadReady(int fd)
{
	SmLogger("read ready on %d", fd);

	const IceConn ice_connection = SmcGetIceConnection(m_connection);

	// Don't wait, just poll.
	timeval select_timeout = {0, 0};

	// Call IceProcessMessages() repeatedly until all the data currently
	// available in 'fd' is consumed.  Use a time-out of 2 seconds to avoid
	// looping endlessly.
	const double start = g_op_time_info->GetRuntimeMS();
	const int retry_timeout = 2000;
	int retry_count = 0;
	while (g_op_time_info->GetRuntimeMS() - start < retry_timeout)
	{
		fd_set files;
		FD_ZERO(&files);
		FD_SET(fd, &files);
		const int res = select(fd + 1, &files, NULL, NULL, &select_timeout);
		SmLogger("select() returned %d", res);

		if (res > 0)
		{
			++retry_count;
			SmLogger("IceProcessMessages() try %d", retry_count);
			const IceProcessMessagesStatus ice_status = IceProcessMessages(ice_connection, 0, 0);
			SmLogger("IceProcessMessages() try %d returned %d", retry_count, ice_status);
		}
		else
		{
			if (res < 0)
				SmLogger("ICE connection gone, or other select() error");
			return;
		}
	}

	SmLogger("Failed to process all messages after %dms, bailed out.", retry_timeout);
}


void X11SessionManager::OnTimeOut(OpTimer* timer)
{
	SmLogger("timed out");
	if( timer == &m_timer )
		OnSaveYourself();
}


BOOL X11SessionManager::AllowUserInteraction(InteractUrgency urgency)
{
	if (m_interact_state == InteractAccepted)
		return TRUE;
	if (m_interact_state == InteractWaiting)
		return FALSE;
	if (m_interact_style == SmInteractStyleNone)
		return FALSE;

	BOOL allowed = FALSE;

	// Spec: The client may only call SmcInteractRequest() after it receives a Save Yourself message 
	// and before it calls SmcSaveYourselfDone()

	X11Types::Status ok = 0;
	if (urgency == InteractError && (m_interact_style == SmInteractStyleAny || m_interact_style == SmInteractStyleErrors) )
	{
		ok = SmcInteractRequest(m_connection, SmDialogError, SM_interact_request_CB, (SmPointer*)this);
	}
	else if (m_interact_style == SmInteractStyleAny)
	{
		ok = SmcInteractRequest(m_connection, SmDialogNormal, SM_interact_request_CB, (SmPointer*)this);
	}

	if (ok)
	{
		m_interact_state = InteractWaiting;
		SmLogger("request interaction");
		X11OpMessageLoop::Self()->NestLoop(); // Will be stopped in OnInteractRequest()
		SmLogger("request interaction: %d", m_interact_state == InteractAccepted);
		allowed = m_interact_state == InteractAccepted;
	}

	return allowed;
}


void X11SessionManager::Accept()
{
	SmLogger("accept");

	if (m_interact_state == InteractAccepted)
	{
		SmcInteractDone(m_connection, False);
		m_interact_state = InteractNone;
	}

	if (m_save_yourself_state == SaveYourselfWaiting)
	{
		SmcSaveYourselfDone(m_connection, True);
		m_save_yourself_state = SaveYourselfNone;
	}
}


void X11SessionManager::Cancel()
{
	SmLogger("cancel");

	if (m_interact_state == InteractAccepted)
	{
		SmcInteractDone(m_connection, m_shutdown ? True : False);
		m_interact_state = InteractNone;
		m_cancelled = TRUE;
	}

	if (m_save_yourself_state == SaveYourselfWaiting)
	{
		SmcSaveYourselfDone(m_connection, m_cancelled ? False : True);
		m_save_yourself_state = SaveYourselfNone;
	}
}


void X11SessionManager::OnSaveYourself(int save_type, X11Types::Bool shutdown, int interact_style, X11Types::Bool fast)
{
	Reset();

	m_save_type      = save_type;
	m_interact_style = interact_style;
	m_shutdown       = shutdown == True;

	SmLogger("saveyourself type=%d shutdown=%d style=%d fast=%d", save_type, shutdown, interact_style, fast);

	// Make a new unique key
	timeval tv;
    gettimeofday(&tv, 0);	
	m_key.Empty();
	m_key.AppendFormat("%ld_%ld", tv.tv_sec, tv.tv_usec);	

	// Register SmProgram
	SetProperty(SmProgram, g_startup_settings.our_binary);

	// Register SmUserID
	struct passwd* pw =  getpwuid(geteuid());
	if (pw)
		SetProperty(SmUserID, pw->pw_name);

	// Register SmRestartStyleHint
	SetProperty(SmRestartStyleHint, SmRestartIfRunning);

	// Register SmRestartCommand
	if (1)
	{
		OpAutoVector<OpString8> list;
		SetRestartList(list);
		SetProperty(SmRestartCommand, list);
	}
	// Register SmCloneCommand
	if (1)
	{
		OpAutoVector<OpString8> list;
		OpString8* s = OP_NEW(OpString8,());
		s->Set(g_startup_settings.our_binary);
		list.Add(s);
		SetProperty(SmCloneCommand, list);
	}

	if (m_interact_style == SmInteractStyleNone)
	{
		OnSaveYourself();
	}
	else
	{
		// We are on a stack from Poll() in network code. Since m_interact_style
		// is not SmInteractStyleNone we can end up in a call to AllowUserInteraction()
		// that will create local event loop and a new Poll(). A Poll() in a Poll()
		// is no good (fails) so we delay the handling a short while.
		m_timer.Start(0);
	}
}


void X11SessionManager::OnSaveYourself()
{
	SmLogger("saveyourself stage 2");

	m_save_yourself_state = SaveYourselfWaiting;

	BOOL has_listener = FALSE;
	if (m_save_type == SmSaveGlobal || m_save_type == SmSaveLocal || m_save_type == SmSaveBoth)
	{
		for (OpListenersIterator iterator(m_session_manager_listener); m_session_manager_listener.HasNext(iterator);)
		{
			has_listener = TRUE;
			m_session_manager_listener.GetNext(iterator)->OnSaveYourself(*this);
		}
	}

	if (!has_listener)
	{
		Cancel();
	}
}


void X11SessionManager::OnDie()
{
	SmLogger("OnDie");
	Reset();
	g_application->Exit(TRUE);
}


void X11SessionManager::OnShutdownCancelled()
{
	SmLogger("OnShutdownCancelled");

	if (m_interact_state == InteractWaiting)
	{
		m_interact_state = InteractNone;
		X11OpMessageLoop::Self()->StopCurrent();
	}

	for (OpListenersIterator iterator(m_session_manager_listener); m_session_manager_listener.HasNext(iterator);)
		m_session_manager_listener.GetNext(iterator)->OnShutdownCancelled();

	Reset();
}


void X11SessionManager::OnSaveComplete()
{
	SmLogger("OnSaveComplete");
	Reset();
}


void X11SessionManager::OnInteractRequest()
{
	SmLogger("OnInteractRequest");
	if (m_interact_state == InteractWaiting)
	{
		m_interact_state = InteractAccepted;
		X11OpMessageLoop::Self()->StopCurrent();
	}
}


void X11SessionManager::SetProperty(const OpStringC8& key, INT8 value)
{
	SmPropValue prop_value;
    prop_value.length = 1;
	prop_value.value  = (SmPointer)&value;
	
	SmProp prop;
	prop.name     = (char*)key.CStr();
	prop.type     = (char*)SmCARD8;
	prop.num_vals = 1;
	prop.vals     = &prop_value;
	
	SmProp* pp = &prop; 
	SmcSetProperties(m_connection, 1, &pp);
}


void X11SessionManager::SetProperty(const OpStringC8& key, const OpStringC8& value)
{
	SmPropValue prop_value;
    prop_value.length = value.Length();
    prop_value.value  = (SmPointer)value.CStr();

	SmProp prop;
	prop.name     = (char*)key.CStr();
	prop.type     = (char*)SmARRAY8;
	prop.num_vals = 1;
	prop.vals     = &prop_value;

	SmProp* pp = &prop; 
	SmcSetProperties(m_connection, 1, &pp);
}


OP_STATUS X11SessionManager::SetProperty(const OpStringC8& key, OpVector<OpString8>& value)
{
	SmPropValue* prop_value = OP_NEWA(SmPropValue,value.GetCount());
	if (!prop_value)
		return OpStatus::ERR_NO_MEMORY;
	
	for (UINT32 i=0; i<value.GetCount(); i++)
	{
		prop_value[i].length = value.Get(i)->Length();
		prop_value[i].value  = (SmPointer)value.Get(i)->CStr();
	}

	SmProp prop;
	prop.name     = (char*)key.CStr();
	prop.type     = (char*)SmLISTofARRAY8;
	prop.num_vals = value.GetCount();
	prop.vals     = prop_value;

	SmProp* pp = &prop; 
	SmcSetProperties(m_connection, 1, &pp);

	OP_DELETEA(prop_value);

	return OpStatus::OK;
}



OP_STATUS X11SessionManager::SetRestartList(OpVector<OpString8>& list)
{
	OpString8* s;
	
	RETURN_OOM_IF_NULL(s = OP_NEW(OpString8,()));
	RETURN_IF_ERROR(s->Set(g_startup_settings.our_binary));
	RETURN_IF_ERROR(list.Add(s));
	RETURN_OOM_IF_NULL(s = OP_NEW(OpString8,()));
	RETURN_IF_ERROR(s->Set("-session"));
	RETURN_IF_ERROR(list.Add(s));
	RETURN_OOM_IF_NULL(s = OP_NEW(OpString8,()));
	RETURN_IF_ERROR(s->AppendFormat("%s_%s", m_id.CStr(), m_key.CStr()));
	RETURN_IF_ERROR(list.Add(s));

	const OpVector<OpString8>& arg = CommandLineManager::GetInstance()->GetRestartArguments();
	for (UINT32 i=0; i<arg.GetCount(); i++)
	{
		RETURN_OOM_IF_NULL(s = OP_NEW(OpString8,()));
		RETURN_IF_ERROR(s->Set(arg.Get(i)->CStr()));
		RETURN_IF_ERROR(list.Add(s));
	}
	return OpStatus::OK;
}


// static
BOOL X11SessionManager::IsSystemSessionManagerActive()
{
	char* s = op_getenv("SESSION_MANAGER");
	return s && *s;
}


// static
void X11SessionManager::SM_save_yourself_CB(SmcConn connection, SmPointer client_data, int save_type, X11Types::Bool shutdown ,int interact_style, X11Types::Bool fast)
{
	if (connection != ((X11SessionManager*)client_data)->GetConnectionHandle())
		return;
	((X11SessionManager*)client_data)->OnSaveYourself(save_type, shutdown, interact_style, fast);
}


// static 
void X11SessionManager::SM_die_CB(SmcConn connection, SmPointer client_data)
{
	if (connection != ((X11SessionManager*)client_data)->GetConnectionHandle())
		return;
	((X11SessionManager*)client_data)->OnDie();
}


//static 
void X11SessionManager::SM_save_complete_CB(SmcConn connection, SmPointer client_data)
{
	if (connection != ((X11SessionManager*)client_data)->GetConnectionHandle())
		return;
	((X11SessionManager*)client_data)->OnSaveComplete();
}


// static 
void X11SessionManager::SM_shutdown_cancelled_CB(SmcConn connection, SmPointer client_data)
{
	if (connection != ((X11SessionManager*)client_data)->GetConnectionHandle())
		return;
	((X11SessionManager*)client_data)->OnShutdownCancelled();
}


// static 
void X11SessionManager::SM_interact_request_CB(SmcConn connection, SmPointer client_data)
{
	if (connection != ((X11SessionManager*)client_data)->GetConnectionHandle())
		return;
	((X11SessionManager*)client_data)->OnInteractRequest();
}

