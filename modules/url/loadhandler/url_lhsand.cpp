/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#ifdef DOM_GADGET_FILE_API_SUPPORT
#include "modules/url/url2.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/url/url_tools.h"
#include "modules/cache/url_cs.h"
#include "modules/formats/uri_escape.h"

#include "modules/dochand/win.h"

#include "modules/url/loadhandler/url_lhsand.h"
#include "modules/url/tools/content_detector.h"

void URL_DataStorage::StartLoadingMountPoint(BOOL guess_content_type)
{
	OP_STATUS op_err = OpStatus::OK;
	
	MessageHandler*	messageHandler = GetFirstMessageHandler();

	if (messageHandler == NULL || g_mountPointProtocolManager == NULL)
	{
		SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));

		UnsetListCallbacks();	
		mh_list->Clean();
		return;
	}
		
	ServerName *server_name = (ServerName *) url->GetAttribute(URL::KServerName, NULL);
	const uni_char * OP_MEMORY_VAR temp;

	temp = (server_name ? server_name->UniName() : NULL);
	
	url->SetAttribute(URL::KIsGeneratedByOpera, FALSE);

	OpString temp_path;
	op_err = url->GetAttribute(URL::KUniPath_Escaped,0,temp_path);
	if(OpStatus::IsError(op_err))
	{
		SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
		
		UnsetListCallbacks();
		mh_list->Clean();
		return;
	}
	
	const uni_char *MountPointPath = NULL; 
	
	if (temp) 
		 MountPointPath = g_mountPointProtocolManager->GetMountPointPath(temp, messageHandler->GetWindow()->Id());
		 
	if (temp == NULL || MountPointPath == NULL )
	{	
		SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_FILE_CANNOT_OPEN));
		UnsetListCallbacks();
		mh_list->Clean();
		return;
	}
	
	url->Access(FALSE);
	
	OpString filename;

	if (OpStatus::IsError(op_err = filename.Set(MountPointPath)) ||
		OpStatus::IsError(op_err = filename.Append(temp_path.CStr())))
	{
		if (op_err == OpStatus::ERR_NO_MEMORY)
		{
			g_memory_manager->RaiseCondition(op_err);
			SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_OUT_OF_MEMORY));
		}
		else
		{
			SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_OUT_OF_MEMORY));
		}
		
		UnsetListCallbacks();
		mh_list->Clean();
		return;				
	}	
	
	UriUnescape::ReplaceChars(filename.CStr(), UriUnescape::LocalfileUtf8);
		
	SetAttribute(URL::KHeaderLoaded,TRUE);
	SetAttribute(URL::KMIME_CharSet, NULL);
	storage = Local_File_Storage::Create(url, filename, OPFILE_ABSOLUTE_FOLDER, OPFILE_FLAGS_ASSUME_ZIP | OPFILE_FLAGS_CASE_SENSITIVE_ZIP);

	if(storage == NULL)
	{
		SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_FILE_CANNOT_OPEN));
		UnsetListCallbacks();
		mh_list->Clean();
		return;
	}

	if ((URLContentType) GetAttribute(URL::KContentType) == URL_UNDETERMINED_CONTENT && guess_content_type)
	{
		ContentDetector content_detector(url, TRUE, TRUE);
		op_err = content_detector.DetectContentType();
		if(OpStatus::IsError(op_err))
			goto err;
	}
	
	if(old_storage)
	{
		OP_DELETE(old_storage);
		old_storage = NULL;
	}
	
	op_err = OpStatus::OK;
	switch((URLContentType) GetAttribute(URL::KContentType))
	{
	case URL_HTML_CONTENT:
		op_err = SetAttribute(URL::KMIME_Type, "text/html");
		break;
	case URL_TEXT_CONTENT:
		op_err = SetAttribute(URL::KMIME_Type, "text/plain");
		break;
	default:
		break;
	}
err:
	if(OpStatus::IsError(op_err))
	{
		OP_DELETE(storage);
		storage = NULL;
		SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_FILE_CANNOT_OPEN));
		
		UnsetListCallbacks();
		mh_list->Clean();
		return;
	}	

	BroadcastMessage(MSG_HEADER_LOADED, url->GetID(), 0, MH_LIST_ALL);
	BroadcastMessage(MSG_URL_DATA_LOADED, url->GetID(), 0, MH_LIST_ALL);
	SetAttribute(URL::KLoadStatus,URL_LOADED);

	if ((URLContentType) GetAttribute(URL::KContentType) == URL_UNDETERMINED_CONTENT && guess_content_type)
	{
		ContentDetector content_detector(url, TRUE, TRUE);
		OpStatus::Ignore(content_detector.DetectContentType()); 
	}

	time_t time_now = g_timecache->CurrentTime();
	TRAP(op_err, SetAttributeL(URL::KVLocalTimeLoaded, &time_now));
	OpStatus::Ignore(op_err);

	UnsetListCallbacks();
	mh_list->Clean();	
}

//////////////////////////////////////////////////////////////////////////
// MountPoint protocol
//////////////////////////////////////////////////////////////////////////

MountPoint::MountPoint(unsigned int windowId)
	: m_windowId(windowId)
{
}

MountPoint::~MountPoint()
{
	if (g_mountPointProtocolManager)
		g_mountPointProtocolManager->RemoveMountPoint(this);
}

/* static */ OP_BOOLEAN
MountPoint::Make(MountPoint *&mountPoint, const OpStringC16 &name, unsigned int windowId, const OpStringC16 &mountPointFilePath)
{
	if (g_mountPointProtocolManager == NULL)
	{
		OP_ASSERT(!"in MountPoint::Make() : g_mountPointProtocolManager should not be null here");
		return OpStatus::ERR;
	}
	
	mountPoint = NULL;
	
	if (g_mountPointProtocolManager->GetMountPointPath(name, windowId) != NULL)
	{
		return OpBoolean::IS_TRUE;
	}
	
	OpAutoPtr<MountPoint> tempMountPoint(OP_NEW(MountPoint, (windowId)));
	RETURN_OOM_IF_NULL(tempMountPoint.get());
	
	RETURN_IF_ERROR(tempMountPoint->m_name.Set(name));
	RETURN_IF_ERROR(tempMountPoint->m_mountPointFilePath.Set(mountPointFilePath));

	RETURN_IF_ERROR(g_mountPointProtocolManager->AddMountPoint(tempMountPoint.get()));
	mountPoint = tempMountPoint.release();
	
	return OpBoolean::IS_FALSE;
}

const uni_char *MountPointProtocolManager::GetMountPointPath(const OpStringC16 &name, unsigned int windowId)
{
	UINT32 l = m_mountPoints.GetCount();
	
	MountPoint *mountPoint;
	
	for (UINT32 i = 0; i < l; i++)
	{
		mountPoint = m_mountPoints.Get(i);
		
		if (mountPoint && name.CompareI(mountPoint->m_name) == 0  && mountPoint->m_windowId == windowId )
		{
			return mountPoint->m_mountPointFilePath.CStr();
		}
	}

	return NULL;
}

OP_STATUS 
MountPointProtocolManager::AddMountPoint(MountPoint *mountPoint)
{
	OP_ASSERT(GetMountPointPath(mountPoint->m_name, mountPoint->m_windowId) == NULL);
		
	RETURN_IF_ERROR(m_mountPoints.Add(mountPoint));
	return OpStatus::OK;
}

void
MountPointProtocolManager::RemoveMountPoint(MountPoint *mountPoint)
{	
	OP_ASSERT(m_mountPoints.Find(mountPoint) != -1);
	OpStatus::Ignore(m_mountPoints.RemoveByItem(mountPoint));
}

#endif // DOM_GADGET_FILE_API_SUPPORT
