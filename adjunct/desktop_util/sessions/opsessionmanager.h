/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Markus Johansson
*/

#ifndef OP_SESSIONMANAGER_H
#define OP_SESSIONMANAGER_H

#ifdef SESSION_SUPPORT

#include "modules/util/opstring.h"
#include "modules/util/adt/opvector.h"

class OpSession;
class OpFileDescriptor;
class OpFile;
class PrefsFile;
class SessionLoadContext;

class OpSessionManager
{
public:

	OpSessionManager()
		: m_last_backup_time(0)
		, m_read_only(FALSE)
		, m_session_load_context(NULL)
	{}

	/**
	 *
	 */
	void InitL();

	/**
	 * Returns TRUE if the interval since the last autosave session backup
	 * was performed has expired
	 */
	BOOL NeedAutosaveSessionBackup();

	/**
	 *
	 */
	void BackupAutosaveSessionL();

	/**
	 * If Opera crashed on the previous run, check if the default session file is missing or of zero size,
	 * and use the backup session file if that is the case.  Should only be called if the cases where Opera
	 * didn't have a clean shutdown.  
	 */
	OP_STATUS RestoreBackupSessionIfNeeded(BOOL& backup_restored);
	/**
	 * Get number of available sessions.
	 */
	UINT32 GetSessionCount() { return m_sessions.GetCount(); }

	/**
	 * Gets the name of a session.
	 * @param index 0 to GetSessionCount() - 1
	 */
	const OpStringC GetSessionNameL(UINT32 index);

	/**
	 * Reads a session.
	 * @param index 0 to GetSessionCount() - 1
	 */
	OpSession* ReadSessionL(UINT32 index);
	OpSession* ReadSession(UINT32 index);

	/**
	 * Creates a new OpSession object with given name
	 */
	OpSession* CreateSessionL(const OpStringC& name);

	/**
	 * Creates a new OpSession object with given name
	 */
	OpSession* CreateSessionFromFileL(const OpFile* name);
	OpSession* CreateSessionFromFile(const OpFile* name);

	/**
	 * Creates a new OpSession object in memory (not to be saved)
	 */
	OpSession* CreateSessionInMemoryL();

	/**
	 * Writes an OpSession object to file.
	 */
	void WriteSessionToFileL(OpSession* session, BOOL reset_after_write = FALSE);

	/**
	 * Scans the sessions folder for .win-files.
	 * Stores found files in m_sessions.
	 * m_sessions is cleared before searching.
	 */
	void ScanSessionFolderL();

	/**
	 * Reads a session with the given name.
	 */
	OpSession* ReadSessionL(const OpStringC& name);
	OpSession* ReadSession(const OpStringC& name);

	/**
	 * Deletes a session. Session file is deleted as well.
	 * @param index 0 to GetSessionCount() - 1
	 * @return TRUE if session was successfully deleted
	 */
	BOOL DeleteSessionL(UINT32 index);

	/**
	 * Deletes all sessions. Session files are deleted as well.
	 */
	void DeleteAllSessionsL();

	/**
	 * Set all sessions as read-only to avoid data loss. 
	 * No sessions will allowed to be saved in this state. Typically, this might happen
	 * on OOM when opening session tabs on startup. If the incomplete session is then saved, 
	 * data loss will occur. 
	 */
	void SetReadOnly(BOOL read_only) { m_read_only = read_only; }

	/**
	 * Can sessions be written?
	 */
	BOOL IsReadOnly() const { return m_read_only; }

	/**
	 * Set new SessionLoadContext.
	 */
	void SetSessionLoadContext(SessionLoadContext* context) { m_session_load_context = context; }

	/**
	 * Get SessionLoadContext.
	 */
	SessionLoadContext* GetSessionLoadContext() const { return m_session_load_context; }

	/**
	 * Returns number of saved sessions.
	 */
	UINT32 GetClosedSessionCount() const;

	/** 
	 * Get saved session. Returns NULL if session doesnt exist.
	 */
	OpSession* GetClosedSession(UINT32 i) const;

	/**
	 * Remove all saved sessions.
	 */
	void EmptyClosedSessions();

private:

	/**
	 *
	 */
	PrefsFile* CreatePrefsFileL(const OpFileDescriptor* file);

	/**
	 *
	 */
	void AddStringToVectorL(OpVector<OpString>& vector, const OpStringC& string);

	/**
	 *
	 */
	void AddValueToVectorL(OpVector<UINT32>& vector, UINT32 value);

	OpAutoVector<OpString>	m_sessions;
	time_t					m_last_backup_time;		// At which time was the last session backup performed
	BOOL					m_read_only;			// When TRUE, some has happened that might cause data loss if a session is saved. 
	SessionLoadContext*		m_session_load_context; // InputContext for session management
};

#endif // SESSION_SUPPORT
#endif // OP_SESSIONMANAGER_H
