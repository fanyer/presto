/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Owner:    Karianne Ekern (karie)
 * @author Co-owner: Adam Minchinton (adamm)
 */

#include "core/pch.h"

#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/quick/models/ServerWhiteList.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"

#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefsfile/prefsentry.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/prefsfile/impl/backend/prefssectioninternal.h"
#include "modules/url/url_man.h"

#define REPOSITORIES_FILENAME    UNI_L("standard_trusted_repositories.ini")


ServerWhiteList &ServerWhiteList::GetInstance()
{
	static ServerWhiteList s_whitelist;
	OP_STATUS status = s_whitelist.Init();
	if (OpStatus::IsError(status))
	{
		OP_ASSERT(!"Server whitelist was not initialized properly");
	}
	return s_whitelist;
}


ServerWhiteList::ServerWhiteList()
	: m_initialized(FALSE)
{
}


ServerWhiteList::~ServerWhiteList()
{
}


OP_STATUS ServerWhiteList::Init()
{
	if (m_initialized)
		return OpStatus::OK;

	OP_STATUS status = Read();
	if (OpStatus::IsSuccess(status))
		m_initialized = TRUE;
	return status;
}


BOOL ServerWhiteList::Find(const OpStringC& repository, OpAutoVector<OpString> & vector)
{
	if (!repository.HasContent())
		return FALSE;

	for (UINT32 i = 0; i < vector.GetCount(); i++)
	{
		if (repository.CompareI(*vector.Get(i)) == 0)
			return TRUE;
	}
	return FALSE;
}


BOOL ServerWhiteList::IsValidEntry(const OpStringC& address)
{
	if (   address.Find(UNI_L("http://"))  != 0
		&& address.Find(UNI_L("https://")) != 0)
	{
		return FALSE;
	}
	if (address.Length() < 9)
	{
		return FALSE;
	}
	return TRUE;
}


BOOL ServerWhiteList::FindMatch(const OpStringC& address)
{
	if (!address.HasContent())
	{
		return FALSE;
	}

	for (UINT32 i = 0; i < m_vector.GetCount(); i++)
	{
		INT32 len = m_vector.Get(i)->Length();
		if (m_vector.Get(i)->Compare(address.CStr(), len) == 0)
		{
			return TRUE;
		}
	}
	return FALSE;
}


OP_STATUS ServerWhiteList::Insert(const OpStringC& address)
{
	if (!address.HasContent())
		return OpStatus::ERR;

	if (!IsValidEntry(address))
	{
		return OpStatus::ERR;
	}

	if (!Find(address, m_vector))
	{
		OpString* s = OP_NEW(OpString, ());
		RETURN_OOM_IF_NULL(s);

		OP_STATUS status = s->Set(address);
		if (OpStatus::IsSuccess(status))
		{
			status = m_vector.Add(s);
		}
		if (OpStatus::IsError(status))
		{
			OP_DELETE(s);
			return status;
		}
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}


BOOL ServerWhiteList::ElementIsEditable(const OpStringC& address)
{
	return !Find(address, m_fixed_vector);
}


BOOL ServerWhiteList::RemoveServer(const OpStringC& address)
{
	if (!address.HasContent())
		return FALSE; 

	for (UINT32 i = 0; i < m_vector.GetCount(); i++)
	{
		if (address.Compare(*m_vector.Get(i)) == 0)
		{
			m_vector.Delete(m_vector.Get(i));
			return TRUE;
		}
	}
	return FALSE;
}


OP_STATUS ServerWhiteList::GetRepositoryFromAddress(OpString& server, const OpStringC& address)
{
	// strip out the query string first, if any
	int query_pos = address.FindFirstOf('?');
	RETURN_IF_ERROR(server.Set(address.CStr(), query_pos));

	int last_slash = server.FindLastOf('/');
	int slash = StringUtils::FindNthOf(1, server, UNI_L("/"));

	if (slash != KNotFound || slash < last_slash)
	{
		if (last_slash < server.Length() - 1)
		{
			if (slash < last_slash)
			{
				RETURN_IF_ERROR(server.Set(server.CStr(), last_slash+1));
			}
		}
	}
	return OpStatus::OK;
}


BOOL ServerWhiteList::AddServer(const OpStringC& address)
{
	if (address.Compare(UNI_L("http://")) == 0)
		return FALSE;

	OpString server;
	RETURN_VALUE_IF_ERROR(GetRepositoryFromAddress(server, address), FALSE);
	RETURN_VALUE_IF_ERROR(Insert(server), FALSE);

	// Make sure the new entry is stored on file
	RETURN_VALUE_IF_ERROR(Write(), FALSE);
	return TRUE;
}


OP_STATUS ServerWhiteList::Read()
{
	m_vector.DeleteAll();
	m_fixed_vector.DeleteAll();

	OpFile fixed_file;
	OpString fixed_filename;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_INI_FOLDER, fixed_filename));

	if (OpStatus::IsSuccess(fixed_filename.Append(REPOSITORIES_FILENAME)) &&
		OpStatus::IsSuccess(fixed_file.Construct(fixed_filename.CStr())))
	{
		//
		// m_vector has to be filled first, because FillVector looks for
		// duplicates in m_fixed_vector
		//
		RETURN_IF_MEMORY_ERROR(FillVector(fixed_file, m_vector));
		RETURN_IF_MEMORY_ERROR(FillVector(fixed_file, m_fixed_vector));
	}

	OpFile local_file;
	RETURN_IF_LEAVE(g_pcfiles->GetFileL(
		PrefsCollectionFiles::TrustedRepositoriesFile, local_file));

	RETURN_IF_MEMORY_ERROR(FillVector(local_file, m_vector));

	return OpStatus::OK;
}


OP_STATUS ServerWhiteList::Write()
{
	OpFile tmp;

	RETURN_IF_LEAVE(g_pcfiles->
			GetFileL(PrefsCollectionFiles::TrustedRepositoriesFile, tmp));

    PrefsFile whitelist_file(PREFS_STD);
	RETURN_IF_LEAVE(whitelist_file.ConstructL());
	RETURN_IF_LEAVE(whitelist_file.SetFileL(&tmp));
	RETURN_IF_LEAVE(whitelist_file.DeleteAllSectionsL());

	BOOL dirty = FALSE;

	for (UINT32 i = 0; i < m_vector.GetCount(); i++)
	{
		BOOL duplicate = Find(*m_vector.Get(i), m_fixed_vector);
		if (!duplicate)
		{
			TRAPD(status, whitelist_file.WriteStringL(
						UNI_L("whitelist"),
						m_vector.Get(i)->CStr(),
						NULL));
			if (OpStatus::IsError(status))
				continue;
			dirty = TRUE;
		}
	}

	if (dirty)
		RETURN_IF_LEAVE(whitelist_file.CommitL());
	else
		OpStatus::Ignore(tmp.Delete(FALSE));

	return OpStatus::OK;
}


SimpleTreeModel* ServerWhiteList::GetModel()
{
	SimpleTreeModel* model = OP_NEW(SimpleTreeModel, ());
	BuildModel(model);
	return model;
}


void ServerWhiteList::BuildModel(SimpleTreeModel* model)
{
	if (!model)
		return;

	OP_ASSERT(!model->GetCount());
	for (UINT32 i = 0; i < m_vector.GetCount(); i++)
	{
		model->AddItem(*m_vector.Get(i));
	}
}


OP_STATUS ServerWhiteList::FillVector(OpFile & file, OpAutoVector<OpString> & vector)
{
	BOOL file_exists = FALSE;
	RETURN_IF_ERROR(file.Exists(file_exists));
	if (file_exists)
	{
		PrefsFile whitelist_file(PREFS_STD);
		RETURN_IF_LEAVE(whitelist_file.ConstructL());
		RETURN_IF_LEAVE(whitelist_file.SetFileL(&file));
		RETURN_IF_LEAVE(whitelist_file.LoadAllL());

		OpAutoPtr<PrefsSection> wlist;
		RETURN_IF_LEAVE(wlist.reset(whitelist_file.ReadSectionL(UNI_L("whitelist"))));

		const PrefsEntry *ientry = wlist.get() ? wlist->Entries() : NULL;
		for(; ientry != NULL; ientry = ientry->Suc())
		{
			OpString *s = OP_NEW(OpString, ());
			RETURN_OOM_IF_NULL(s);
			if (OpStatus::IsError( s->Set(ientry->Key()) ))
			{
				OP_DELETE(s);
				continue;
			}

			BOOL duplicate = Find(*s, m_fixed_vector);
			if (!duplicate)
				if (OpStatus::IsError(vector.Add(s)))
					OP_DELETE(s);
		}
	}
	return OpStatus::OK;
}

