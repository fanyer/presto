/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mh/mh.h"

#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/commcleaner.h"

// scomm.cpp 

// Super Comm base communication class

#define SCOMM_WAIT_STATUS_DELETE		0x01
#define SCOMM_WAIT_STATUS_IS_DELETED	0x02


SComm::SComm(MessageHandler* msg_handler, SComm *prnt)
{
    if (g_scomm_id==0)
        g_scomm_id = 1;
	id = g_scomm_id++;
	mh = msg_handler;
	parent = prnt;
	call_count = 0;
	is_connected = FALSE;
    //hWnd = mh->GetHWindow();            
#ifdef _DEBUG
	/*
	{
		FILE *fp = fopen("c:\\klient\\winsck.txt","a");
		fprintf(fp,"[%d] SComm:SComm()\n", id);
		fclose(fp);
	}
	*/
#endif
#ifdef NEED_URL_ABORTIVE_CLOSE
	m_abortive_close = FALSE;
#endif	
#ifdef URL_DISABLE_INTERACTION
	user_interaction_blocked = FALSE;
#endif
#ifdef URL_PER_SITE_PROXY_POLICY
    force_socks_proxy = FALSE;
#endif
}

SComm::~SComm()
{
    SCommWaitElm *lwe = (SCommWaitElm*) g_DeletedCommList->First();
    while (lwe)
    {
        if (lwe->comm == this )
        {
            lwe->comm = NULL;
			lwe->status = SCOMM_WAIT_STATUS_IS_DELETED;
        }
        lwe = (SCommWaitElm*) lwe->Suc();
    }

#ifdef _DEBUG
	/*
	{
		FILE *fp = fopen("c:\\klient\\winsck.txt","a");
		fprintf(fp,"[%d] SComm:~SComm() - call_count=%d\n", id, call_count);
		fclose(fp);
	}
	*/
#endif
	(mh ? mh  :g_main_message_handler)->UnsetCallBacks(this);

	OP_ASSERT(!parent);
}

CommState SComm::SignalConnectionEstablished()
{
    if(is_connected)
	    return parent ? parent->SignalConnectionEstablished() : COMM_LOADING;
	is_connected = TRUE;
	return ConnectionEstablished();
}

CommState SComm::ConnectionEstablished()
{	
	return parent ? parent->SignalConnectionEstablished() : COMM_LOADING;
}

void SComm::SignalProcessReceivedData()
{
	if(is_connected || !parent)
		ProcessReceivedData();
	else 
	{
		if (!parent->FindInDeletedCommList())
			parent->ProcessReceivedData();
	}
}

void SComm::ProcessReceivedData()
{
	if(parent)
	{
		SComm *sink = parent->GetSink();
		NormalCallCount blocker(this);
		if(sink == this)
		{
			parent->SignalProcessReceivedData();
		}
		else
		{
			parent->SecondaryProcessReceivedData();
		}	
	}
	else
		mh->PostMessage(MSG_COMM_DATA_READY, Id(), 0);
	
}

unsigned int SComm::SignalReadData(char* buf, unsigned blen)
{
	return ReadData(buf, blen);
}

void SComm::SecondaryProcessReceivedData()
{
	if (FindInDeletedCommList())
		return;
	ProcessReceivedData();
}


void SComm::RequestMoreData()
{
	NormalCallCount blocker(this);

	if (parent)
		parent->RequestMoreData();
}

BOOL SComm::SafeToDelete() const
{
	return (call_count == 0);
}

void SComm::Stop()
{
	is_connected = FALSE;
}

void SComm::EndLoading()
{
	is_connected = FALSE;
	NormalCallCount blocker(this);
	if(parent)
		parent->EndLoading();
}

void SComm::Clear()
{
	NormalCallCount blocker(this);
	if(parent)
		parent->Clear();
}

BOOL SComm::IsActiveConnection() const
{
	return parent ? parent->IsActiveConnection() : FALSE;
}

BOOL SComm::IsNeededConnection() const
{
	return parent ? parent->IsNeededConnection() : FALSE;
}

void SComm::SetProgressInformation(ProgressState progress_level, 
								   unsigned long progress_info1, 
								   const void *progress_info2)
{
	NormalCallCount blocker(this);
	if(parent)
		parent->SetProgressInformation(progress_level,progress_info1,progress_info2);
}


void SComm::RemoveDeletedComm()
{
	SCommRemoveDeletedComm();
}

void SComm::SCommRemoveDeletedComm()
{
    SCommWaitElm *lwe = (SCommWaitElm*) g_DeletedCommList->First();
    SCommWaitElm *next_lwe;
    while (lwe)
    {
#ifdef DEBUG_MH
        FILE *fp = fopen("c:\\klient\\msghan.txt", "a");
        fprintf(fp, "[%d] RemoveDeletedComm check obj: %p (status=%d)\n", comm_list_call_count, lwe->comm, lwe->status);
        fclose(fp);
#endif
        next_lwe = (SCommWaitElm*) lwe->Suc();
        //if (lwe->status == SCOMM_WAIT_STATUS_IS_DELETED || lwe->status == SCOMM_WAIT_STATUS_DELETE)
        {
#ifdef DEBUG_MH
            FILE *fp = fopen("c:\\klient\\msghan.txt", "a");
            fprintf(fp, "[%d] RemoveDeletedComm found obj: %p\n", comm_list_call_count, lwe->comm);
            fclose(fp);
#endif
            if (lwe->status == SCOMM_WAIT_STATUS_DELETE && lwe->comm && lwe->comm->SafeToDelete())
            {
                lwe->status = SCOMM_WAIT_STATUS_IS_DELETED;
                comm_list_call_count++;
                OP_DELETE(lwe->comm); // delete may cause new calls to HostAddrCache functions
                comm_list_call_count--;
                lwe->comm = 0;
            }
            
            if (comm_list_call_count == 0 && lwe->status == SCOMM_WAIT_STATUS_IS_DELETED)
            {
                lwe->Out();
                OP_DELETE(lwe);
            }
        }
        lwe = next_lwe;
    }
	if(!g_DeletedCommList->Empty() && g_comm_cleaner)
		g_comm_cleaner->SignalWaitElementActivity();
}

SCommWaitElm* SComm::FindInDeletedCommList() const
{
	OP_ASSERT(g_DeletedCommList);
	SCommWaitElm *lwe = (SCommWaitElm*) g_DeletedCommList->First();
	while (lwe)
	{
		if (lwe->comm == this)
			// Found.
			return lwe;
		lwe = (SCommWaitElm*) lwe->Suc();
	}

	// Not found.
	OP_ASSERT(lwe == 0);
	return NULL;
}

BOOL SComm::IsDeleted() const
{
	const SCommWaitElm *lwe = FindInDeletedCommList();
	if (!lwe)
		return FALSE;

	OP_ASSERT(lwe->comm == this);

	// g_DeletedCommList currently only supports deleted items.
	// This assert will warn us if the situation changes.
	// Then we should only return TRUE if the status is
	// one of the following.
	OP_ASSERT(lwe->status == SCOMM_WAIT_STATUS_DELETE ||
	          lwe->status == SCOMM_WAIT_STATUS_IS_DELETED);

	return TRUE;
}

void SComm::MarkAsDeleted()
{
	if (!g_DeletedCommList)
		return;

	// If already present in g_DeletedCommList -
	// do not add the second time.
	if (FindInDeletedCommList())
	{
		// g_DeletedCommList currently only supports deleted items.
		// This assert will warn us if the situation changes.
		// Then the logic should change similarly to the logic
		// in Comm::AddWaitingComm().
		OP_ASSERT(IsDeleted());

		return;
	}

#ifdef DEBUG_MH
	FILE *fp = fopen("c:\\klient\\msghan.txt", "a");
	fprintf(fp, "[%d] SComm::MarkDeleteComm new obj: %p (status=%d)\n", comm_list_call_count, comm, COMM_WAIT_STATUS_DELETE);
	fclose(fp);
#endif

	OP_ASSERT(g_SCommWaitElm_factory);
	SCommWaitElm *lwe = g_SCommWaitElm_factory->Allocate();
	if (lwe)
	{
		lwe->Init(this, SCOMM_WAIT_STATUS_DELETE);
		// Add the object to the deletion queue.
		lwe->Into(g_DeletedCommList);
		if(g_comm_cleaner)
			g_comm_cleaner->SignalWaitElementActivity();
	}
}

void SComm::SafeDestruction(SComm *target)
{
	if(target != NULL)
	{
#ifdef URL_DISABLE_INTERACTION
		target->SetUserInteractionBlocked(TRUE);
#endif //URL_DISABLE_INTERACTION
		target->ChangeParent(NULL);
		if(target->SafeToDelete())
		{
			// Delete only if the object is not already in the deletion queue.
			// Otherwise we'll get double deletion: now and later
			// by the deletion queue processor.
			if(!target->IsDeleted())
				OP_DELETE(target);
		}
		else
			target->MarkAsDeleted();
	}
}

#ifdef URL_DISABLE_INTERACTION
BOOL SComm::GetUserInteractionBlocked() const
{
	return user_interaction_blocked || (parent ? parent->GetUserInteractionBlocked() : FALSE);
}
#endif

#ifdef _EXTERNAL_SSL_SUPPORT_
#ifdef URL_ENABLE_INSERT_TLS
BOOL SComm::InsertTLSConnection(ServerName *_host, unsigned short _port)
{
	return FALSE;
}
#endif
#endif // _EXTERNAL_SSL_SUPPORT_

#ifdef URL_PER_SITE_PROXY_POLICY
BOOL SComm::GetForceSOCKSProxy()
{
    return parent ? parent->GetForceSOCKSProxy() : force_socks_proxy;
}
#endif
