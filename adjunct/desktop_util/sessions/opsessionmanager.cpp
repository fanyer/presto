/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Markus Johansson
 */
#include "core/pch.h"

#ifdef SESSION_SUPPORT

#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"
#include "adjunct/desktop_util/sessions/opsessionmanager.h"
#include "adjunct/desktop_util/sessions/opsession.h"
#include "adjunct/quick/application/SessionLoadContext.h"

#include "modules/util/opfile/opfile.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/util/filefun.h"
#include "modules/util/opfile/opmemfile.h"

#define SESSION_BACKUP_DELAY	(5 * 60)	// every 5 minutes
#define AUTOSAVE_SESSION_NAME	UNI_L("autosave")
#define DATA_BUFFER_LEN			256

void
OpSessionManager::InitL()
{
	OP_STATUS rc;

	BOOL backup_restored;

	OpStatus::Ignore(g_session_manager->RestoreBackupSessionIfNeeded(backup_restored));

	TRAP(rc, ScanSessionFolderL());
}

void
OpSessionManager::BackupAutosaveSessionL()
{
	OpFile dest; ANCHOR(OpFile, dest);
	LEAVE_IF_ERROR(dest.Construct(AUTOSAVE_SESSION_NAME UNI_L(".win.bak"), OPFILE_SESSION_FOLDER));

	OpFile src; ANCHOR(OpFile, src);
	LEAVE_IF_ERROR(src.Construct(AUTOSAVE_SESSION_NAME UNI_L(".win"), OPFILE_SESSION_FOLDER));

	LEAVE_IF_ERROR(dest.CopyContents(&src, FALSE));

	m_last_backup_time = op_time(NULL);
}

BOOL
OpSessionManager::NeedAutosaveSessionBackup()
{
	time_t time_now = op_time(NULL);

	return (m_last_backup_time == 0) || (m_last_backup_time + SESSION_BACKUP_DELAY < time_now);
}

OP_STATUS
OpSessionManager::RestoreBackupSessionIfNeeded(BOOL& backup_restored)
{
	backup_restored = FALSE;

	OpFile src;
	RETURN_IF_ERROR(src.Construct(AUTOSAVE_SESSION_NAME UNI_L(".win.bak"), OPFILE_SESSION_FOLDER));

	OpFile dest;
	RETURN_IF_ERROR(dest.Construct(AUTOSAVE_SESSION_NAME UNI_L(".win"), OPFILE_SESSION_FOLDER));

	BOOL exists;
	RETURN_IF_ERROR(src.Exists(exists));	// backup
	if(exists)
	{
		// we have a backup file
		RETURN_IF_ERROR(dest.Exists(exists)); // default session
		if(exists)
		{
			bool session_valid = false;

			// we have a default session file
			OpFileLength len, backup_len;
			RETURN_IF_ERROR(dest.GetFileLength(len));
			RETURN_IF_ERROR(src.GetFileLength(backup_len));

			// if file is too small and it cannot be valid
			if(len > 64)
			{
				// open the session and sniff the content
				RETURN_IF_ERROR(dest.Open(OPFILE_READ));
				if(dest.IsOpen())
				{
					OpFileLength bytes_read = 0;
					char data_buffer[DATA_BUFFER_LEN];

					RETURN_IF_ERROR(dest.Read(data_buffer, DATA_BUFFER_LEN - 1, &bytes_read));
					data_buffer[bytes_read] = '\0';

					// go through the buffer to see if we have something that resembles a .ini section
					for(int i = 0; i < bytes_read - 10; i++)
					{
						// first section is [session]
						if(data_buffer[i] == '[' && op_strncmp(&data_buffer[i+1], "session]", 8) == 0)
						{
							session_valid = true;
							break;
						}
					}
					dest.Close();
				}
			}
			if(!session_valid)
			{
				// backup file exists and the main session is empty, copy the backup over
				RETURN_IF_ERROR(dest.CopyContents(&src, FALSE));

				backup_restored = TRUE;
			}
		}
		else
		{
			// we have no main session file but we do have a backup file, us that
			RETURN_IF_ERROR(dest.CopyContents(&src, FALSE));

			backup_restored = TRUE;
		}
	}
	return OpStatus::OK;
}

const OpStringC
OpSessionManager::GetSessionNameL(UINT32 index)
{
	if( index >= GetSessionCount() )
		return 0;
	else
		return *m_sessions.Get(index);
}

OpSession*
OpSessionManager::ReadSessionL(UINT32 index)
{
	if (index >= GetSessionCount() )
		return 0;
	else
		return ReadSessionL(*m_sessions.Get(index));
}

OpSession*
OpSessionManager::ReadSession(UINT32 index)
{
	OpSession* session = NULL;
	TRAPD(err, session = ReadSessionL(index));
	return OpStatus::IsSuccess(err) ? session : NULL;
}

BOOL
OpSessionManager::DeleteSessionL(UINT32 index)
{
	OpFile file; ANCHOR(OpFile, file);
	OpString filename; ANCHOR(OpString, filename);

	if (NULL == m_sessions.Get(index))
	{
		return FALSE;
	}

	filename.SetL(*m_sessions.Get(index));
	filename.AppendL(UNI_L(".win"));

	LEAVE_IF_ERROR(file.Construct(filename.CStr(), OPFILE_SESSION_FOLDER));
	BOOL deleted = OpStatus::IsSuccess(file.Delete());

	m_sessions.Delete(index);

	return deleted;
}

void
OpSessionManager::DeleteAllSessionsL()
{
	while (GetSessionCount())
	{
		DeleteSessionL(0);
	}
}

OpSession*
OpSessionManager::CreateSessionL(const OpStringC& name)
{
	OpFile file; ANCHOR(OpFile, file);
	OpString filename; ANCHOR(OpString, filename);

	filename.SetL(name);
	filename.AppendL(UNI_L(".win"));

	LEAVE_IF_ERROR(file.Construct(filename.CStr(), OPFILE_SESSION_FOLDER));

	OpStackAutoPtr<PrefsFile> prefsfile(CreatePrefsFileL(&file));
	OpStackAutoPtr<OpSession> session(OP_NEW_L(OpSession, ()));

	// we don't want cascading
#ifdef PF_CAP_CASCADE_DISABLE
	prefsfile->SetDisableCascade();
#endif // PF_CAP_CASCADE_DISABLE

	session->InitL(prefsfile.get());
	prefsfile.release();

	return session.release();
}

OpSession*
OpSessionManager::CreateSessionFromFileL(const OpFile* file)
{
	OpStackAutoPtr<PrefsFile> prefsfile(CreatePrefsFileL(file));
	OpStackAutoPtr<OpSession> session(OP_NEW_L(OpSession, ()));
	session->InitL(prefsfile.get());

	prefsfile.release();

	return session.release();
}

OpSession*
OpSessionManager::CreateSessionFromFile(const OpFile* file)
{
	OpSession* session = NULL;
	TRAPD(err, session = CreateSessionFromFileL(file));
	return OpStatus::IsSuccess(err) ? session : NULL;
}

OpSession* OpSessionManager::CreateSessionInMemoryL()
{
	OpStackAutoPtr<OpMemFile> memfile(OP_NEW_L(OpMemFile, ()));

	OpStackAutoPtr<PrefsFile> prefsfile(CreatePrefsFileL(memfile.get()));
	OpStackAutoPtr<OpSession> session(OP_NEW_L(OpSession, ()));
	session->InitL(prefsfile.get(), TRUE);
	prefsfile.release();
	return session.release();
}

void
OpSessionManager::WriteSessionToFileL(OpSession* session, BOOL reset_after_write)
{
	if(m_read_only)
	{
		session->Cancel(); // avoid save in the destructor
		return;
	}
	if(NeedAutosaveSessionBackup())
	{
		// Don't want to propagate this as writing is more important
		TRAPD(err, BackupAutosaveSessionL());
	}
	session->WriteToFileL(reset_after_write);
}

// private functions below

void
OpSessionManager::ScanSessionFolderL()
{
	m_sessions.DeleteAll();

	OpStackAutoPtr<OpFolderLister> lister(OpFile::GetFolderLister(OPFILE_SESSION_FOLDER, UNI_L("*.win")));
	LEAVE_IF_NULL(lister.get());

	while (lister->Next())
	{
		if (!lister->IsFolder())
		{
			OpStackAutoPtr<OpString> path(OP_NEW_L(OpString, ()));
			const uni_char* filename = lister->GetFileName();
			path->SetL(filename, uni_strlen(filename) - 4);
			LEAVE_IF_ERROR(m_sessions.Add(path.get()));
			path.release();
		}
	}
}

OpSession*
OpSessionManager::ReadSessionL(const OpStringC& name)
{
	OpStackAutoPtr<OpSession> session (CreateSessionL(name));
	session->LoadL();
	return session.release();
}

OpSession*
OpSessionManager::ReadSession(const OpStringC& name)
{
	OpSession* session = NULL;
	TRAPD(err, session = ReadSessionL(name));
	return OpStatus::IsSuccess(err) ? session : NULL;
}

UINT32
OpSessionManager::GetClosedSessionCount() const
{
	return m_session_load_context ? m_session_load_context->GetClosedSessionCount() : 0;
}

OpSession*
OpSessionManager::GetClosedSession(UINT32 i) const
{
	return m_session_load_context ? m_session_load_context->GetClosedSession(i) : NULL;
}

void
OpSessionManager::EmptyClosedSessions()
{
	if (m_session_load_context)
		m_session_load_context->EmptyClosedSessions();
}

PrefsFile*
OpSessionManager::CreatePrefsFileL(const OpFileDescriptor* file)
{
	OpStackAutoPtr<OpSessionReader::SessionFileAccessor> accessor(OP_NEW_L(OpSessionReader::SessionFileAccessor, ()));
	OpStackAutoPtr<PrefsFile> prefsfile(OP_NEW_L(PrefsFile, (accessor.release())));

	prefsfile->ConstructL();
	prefsfile->SetFileL(file);

	return prefsfile.release();
}

void
OpSessionManager::AddStringToVectorL(OpVector<OpString>& vector, const OpStringC& string)
{
	OpStackAutoPtr<OpString> copy(OP_NEW_L(OpString, ()));

	LEAVE_IF_ERROR(copy->Set(string));
	LEAVE_IF_ERROR(vector.Add(copy.get()));

	copy.release();
}

void
OpSessionManager::AddValueToVectorL(OpVector<UINT32>& vector, UINT32 value)
{
	UINT32* copy = OP_NEW_L(UINT32, ());
	*copy = value;

	OP_STATUS status = vector.Add(copy);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(copy);
		LEAVE(status);
	}
}

#endif // SESSION_SUPPORT
