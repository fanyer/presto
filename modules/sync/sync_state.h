/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef __SYNC_STATE_H__
#define __SYNC_STATE_H__

#include "modules/prefs/prefsmanager/collections/pc_sync.h"

////////////////////////////////////////////////////////////////////////

enum OpSyncSupports
{
	SYNC_SUPPORTS_BOOKMARK,			///< Supports bookmarks
	SYNC_SUPPORTS_CONTACT,			///< Supports contact
	SYNC_SUPPORTS_ENCRYPTION,		///< Supports encryption
	SYNC_SUPPORTS_EXTENSION,		///< Supports extension
	SYNC_SUPPORTS_FEED,				///< Supports feed
	SYNC_SUPPORTS_NOTE,				///< Supports note item
	SYNC_SUPPORTS_PASSWORD_MANAGER,	///< Supports password manager form and http authentication
	SYNC_SUPPORTS_SEARCHES,			///< Supports searches
	SYNC_SUPPORTS_SPEEDDIAL,		///< Supports speed dial item
	SYNC_SUPPORTS_SPEEDDIAL_2,		///< Supports speed dial 2 item
	SYNC_SUPPORTS_TYPED_HISTORY,	///< Supports typed history
	SYNC_SUPPORTS_URLFILTER,		///< Supports url filter
	SYNC_SUPPORTS_MAX
};

#ifdef _DEBUG
inline Debug& operator<<(Debug& d, enum OpSyncSupports s)
{
# define CASE_ITEM_2_STRING(x) case x: return d << # x
	switch (s) {
		CASE_ITEM_2_STRING(SYNC_SUPPORTS_BOOKMARK);
		CASE_ITEM_2_STRING(SYNC_SUPPORTS_CONTACT);
		CASE_ITEM_2_STRING(SYNC_SUPPORTS_ENCRYPTION);
		CASE_ITEM_2_STRING(SYNC_SUPPORTS_EXTENSION);
		CASE_ITEM_2_STRING(SYNC_SUPPORTS_FEED);
		CASE_ITEM_2_STRING(SYNC_SUPPORTS_NOTE);
		CASE_ITEM_2_STRING(SYNC_SUPPORTS_PASSWORD_MANAGER);
		CASE_ITEM_2_STRING(SYNC_SUPPORTS_SEARCHES);
		CASE_ITEM_2_STRING(SYNC_SUPPORTS_SPEEDDIAL);
		CASE_ITEM_2_STRING(SYNC_SUPPORTS_SPEEDDIAL_2);
		CASE_ITEM_2_STRING(SYNC_SUPPORTS_TYPED_HISTORY);
		CASE_ITEM_2_STRING(SYNC_SUPPORTS_URLFILTER);
		CASE_ITEM_2_STRING(SYNC_SUPPORTS_MAX);
	default: return d << "OpSyncSupports(unknown:" << static_cast<int>(s) << ")";
	}
# undef CASE_ITEM_2_STRING
}
#endif // _DEBUG

/**
 * This class stores in a bit field the enabled/disabled state for each
 * OpSyncSupports type. Calling SetSupports() can enable/disable a single type.
 * Calling HasSupports() shows if a single type is enabled.
 */
class SyncSupportsState
{
private:
	unsigned long m_flag;
	inline unsigned int ToFlag(OpSyncSupports supports) const {
		return 0x0001 << static_cast<unsigned int>(supports);
	}
public:
	/** Initialises an instance where all support types are disabled. */
	SyncSupportsState() : m_flag(0) {}

	/** Resets the instance such that all support types are disabled. */
	void Clear() { m_flag = 0; }

	/** Enables or disables the specified supports type. */
	void SetSupports(OpSyncSupports supports, bool enable) {
		OP_ASSERT(supports < SYNC_SUPPORTS_MAX);
		if (enable)
			m_flag |= ToFlag(supports);
		else
			m_flag &= ~(ToFlag(supports));
	}

	/** Returns true if the specified supports type is enabled. */
	bool HasSupports(OpSyncSupports supports) const {
		return ((m_flag & ToFlag(supports)) ? true : false);
	}

	/** Returns true if any supports type is enabled. */
	bool HasAnySupports() const { return m_flag != 0; }
};

#ifdef _DEBUG
inline Debug& operator<<(Debug& d, const SyncSupportsState& state)
{
	d << "SyncSupportsState(";
	if (state.HasAnySupports())
	{
		bool first = true;
		for (unsigned int i = 0; i < SYNC_SUPPORTS_MAX; ++i)
		{
			OpSyncSupports supports = static_cast<OpSyncSupports>(i);
			if (state.HasSupports(supports))
			{
				if (!first) d << ";";
				d << supports;
				first = false;
			}
		}
	}
	else
		d << "empty";
	return d << ")";
}
#endif // _DEBUG


////////////////////////////////////////////////////////////////////////

class OpSyncState
{
public:
	OpSyncState();

	/** Get/Set the main sync state */
	const OpStringC& GetSyncState() const { return m_sync_state; }
	OP_STATUS GetSyncState(OpString& state) const { return state.Set(m_sync_state); }
	OP_STATUS SetSyncState(const OpStringC& state) { return m_sync_state.Set(state); }

	/** Get/Set the sync state for a particular type */
	const OpStringC& GetSyncState(OpSyncSupports supports) const {
		return m_sync_state_per_item[supports];
	}
	OP_STATUS GetSyncState(OpString& state, OpSyncSupports supports) const {
		return state.Set(m_sync_state_per_item[supports]);
	}
	OP_STATUS SetSyncState(const OpStringC& state, OpSyncSupports supports) {
		m_sync_state_item_persistent[supports] = SYNC_STATE_PERSISTENT;
		return m_sync_state_per_item[supports].Set(state);
	}
	OP_STATUS ResetSyncState(OpSyncSupports supports) {
		m_sync_state_item_persistent[supports] = SYNC_STATE_RESET;
		return m_sync_state_per_item[supports].Set(UNI_L("0"));
	}

	/**
	 * Changes the persistence of the sync-state. If The sync-state of a
	 * supports type is not persistent, then WritePrefs() will not write the
	 * sync-state to the prefs and ReadPrefs() will not overwrite the sync-state
	 * with the value from the prefs.
	 *
	 * By default SetSyncState() sets a persistent sync-state for a supports
	 * type. But calling SetSyncStatePersistent() with argument persistent=FALSE
	 * before calling WritePrefs() makes this state not persistent.
	 */
	void SetSyncStatePersistent(OpSyncSupports supports, BOOL persistent) {
		m_sync_state_item_persistent[supports] = persistent ? SYNC_STATE_PERSISTENT : SYNC_STATE_NOT_PERSISTENT;
	}

	/** Checks if the sync state has been used at all */
	BOOL IsDefaultValue(OpSyncSupports supports) const {
		return m_sync_state_per_item[supports].IsEmpty() || m_sync_state_per_item[supports] == UNI_L("0");
	}

	/** Checks if the sync state for a particular type matches the
	 * main sync state */
	BOOL IsOutOfSync(OpSyncSupports supports) const {
		return (m_sync_state != m_sync_state_per_item[supports]);
	}

	/** Get/Set if the state is dirty. */
	void SetDirty(BOOL dirty) { m_dirty = dirty; }
	BOOL GetDirty() const { return m_dirty; }

	/**
	 * Copies the sync-state from the specified src to this instance.
	 */
	OpSyncState& operator=(const OpSyncState& src);

	/**
	 * Updates the sync-state of this instance with the syn-state from the
	 * specified src. This basically copies all sync-states from src to this
	 * instance with the exception of the sync-states that were reset.
	 * @see ResetSyncState().
	 */
	OP_STATUS Update(const OpSyncState& src);

	/** Reads and writes between the prefs and internal variables for all the
	 * sync states. */
	OP_STATUS ReadPrefs();
	OP_STATUS WritePrefs() const;

public:
	/**
	 * Converts the integer prefs enum that enables the support for syncing a
	 * special type to the corresponding OpSyncSupports enum.
	 * Thus each single support can be enabled or disabled.
	 * @return The OpSyncSupports that is associated to the specified pref or
	 *  SYNC_SUPPORTS_MAX if the pref defines a different or unsupported type.
	 * @see Supports2IntPref()
	 */
	static OpSyncSupports EnablePref2Supports(int pref);
	/**
	 * Converts an OpSyncSupports to the corresponding integer pref enum that is
	 * used to enable resp. disable the corresponding support.
	 * @return The associated integer pref enum or
	 *  PrefsCollectionSync::DummyLastIntegerPref if support for the specified
	 *  OpSyncSupports enum is not compiled in.
	 * @see IntPref2Supports()
	 */
	static PrefsCollectionSync::integerpref Supports2EnablePref(OpSyncSupports);
	/**
	 * Converts an OpSyncSupports to the corresponding string pref enum that is
	 * used to store the sync state of the specified sync support type.
	 * @return The associated string pref enum or
	 *  PrefsCollectionSync::DummyLastStringPref if support for the specified
	 *  OpSyncSupports enum is not compiled in.
	 * @see Supports2IntPref()
	 */
	static PrefsCollectionSync::stringpref Supports2SyncStatePref(OpSyncSupports supports);

private:
	BOOL IsSyncStatePersistent(OpSyncSupports supports) const {
		return m_sync_state_item_persistent[supports] != SYNC_STATE_NOT_PERSISTENT;
	}

	OpString m_sync_state;
	OpString m_sync_state_per_item[SYNC_SUPPORTS_MAX];
	/**
	 * This enum is used to identify how to handle the m_sync_state_per_item in
	 * a call to WritePrefs(), ReadPrefs() or Update().
	 */
	enum SyncStatePersistent {
		/** The sync-state shall not be written to the prefs-file by
		 * WritePrefs() and the current value shall not be overwritten by
		 * ReadPrefs().
		 * @see SetSyncStatePersistent() */
		SYNC_STATE_NOT_PERSISTENT = 0,
		/** The sync-state shall be written to the prefs-file by WritePrefs()
		 * and ReadPrefs() will update the current value with the value read
		 * from the prefs-file.
		 * This is the default state.
		 * @see SetSyncStatePersistent() */
		SYNC_STATE_PERSISTENT = 1,
		/** The sync-state was reset to its default state and the sync-state
		 * shall not be overwritten by the sync-state specified as argument to
		 * Update(). This state is persistent, i.e. WritePrefs() will write the
		 * sync-state to disk and ReadPrefs() will overwrite the current value
		 * with the corresponding value from the prefs-file.
		 * @see ResetSyncState(). */
		SYNC_STATE_RESET = 2
	};
	enum SyncStatePersistent m_sync_state_item_persistent[SYNC_SUPPORTS_MAX];
	BOOL m_dirty;
};

#endif // __SYNC_STATE_H__
