// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#include "core/pch.h"

#include "modules/util/gen_math.h"
#include "adjunct/m2/src/engine/engine.h"
//#include "irc-message.h"
#include "BT-module.h"
#include "adjunct/m2/src/engine/account.h"
#include "modules/ecmascript/ecmascript.h"
# include "modules/util/OpLineParser.h"
#include "adjunct/m2/src/util/autodelete.h"
#include "adjunct/m2/src/util/str/strutil.h"

#include <assert.h>

//********************************************************************
//
//	BTModule
//
//********************************************************************

OP_STATUS get_BT_module(Module::Interface** module)
{
	static BTModule g_module;
    *module = &g_module;
    return OpStatus::OK;
}


OP_STATUS BTModule::Create(component_id_t cid, ProtocolBackend** component)
{
    *component = NULL;
	*component = OP_NEW(BTBackend, ());

    return OpStatus::OK;
};

const char* BTModule::GetName() const
{
    return "BT";
}



//********************************************************************
//
//	BTBackend
//
//********************************************************************

BTBackend::BTBackend()
//	m_protocol(NULL),
  : m_file(NULL)
{
}

const char* BTBackend::GetName() const
{
	return "BT";
}

BTBackend::~BTBackend()
{
	Disconnect();
}


OP_STATUS BTBackend::Init(Account* account)
{
    if (!account)
        return OpStatus::ERR_NULL_POINTER; //account_id is not a pointer, but 0 is an illegal account_id and should return an error

    m_account = account;

    return OpStatus::OK;
}

OP_STATUS BTBackend::SettingsChanged()
{
    return OpStatus::OK;
}



OP_STATUS BTBackend::Disconnect()
{
	Cleanup();

    return OpStatus::OK;
}

OP_STATUS BTBackend::GetProgress(Account::ProgressInfo& progress) const
{
    return OpStatus::OK;
}

void BTBackend::GetCurrentCharset(OpString8 &charset)
{
	GetAccountPtr()->GetCharset(charset);
}


time_t BTBackend::GetCurrentTime()
{
	return 0;
}


void BTBackend::OnTimeOut(OpTimer* timer)
{
//	OP_ASSERT(timer == m_presence_timer.get());
}


void BTBackend::Cleanup()
{
	// Notify GUI that we has left all rooms and that channel listing has
	// completed (this may have been notified allready, but shouldn't hurt
	// to send it twice).
	OpString empty;

	// Reset variables.
	SignalProgressChanged();
//    m_protocol = 0;
}

OP_STATUS BTBackend::ParseTorrent(OpString& filepath)
{
	OP_STATUS ret;
	OpFile file;

    TRAP(ret, file.ConstructL(filepath));
    if ( (ret != OpStatus::OK) ||
         ((ret=file.Open(OPFILE_READ)) != OpStatus::OK) )
    {
        return ret;
    }


}
