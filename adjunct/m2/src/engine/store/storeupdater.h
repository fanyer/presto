// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Arjan van Leeuwen (arjanl)

#ifndef STORE_UPDATER_H
#define STORE_UPDATER_H

#ifdef M2_MERLIN_COMPATIBILITY

#include "adjunct/m2/src/engine/store/store3.h"
#include "adjunct/m2/src/util/blockfile.h"

#include "modules/hardcore/mh/messobj.h"
#include "modules/util/adt/oplisteners.h"
#include "modules/util/opfile/opfile.h"

class ProgressInfo;


class StoreUpdateListener
{
public:
	virtual ~StoreUpdateListener() {}
	virtual void OnStoreUpdateProgressChanged(int progress, int total) = 0;
};


/** @brief Class to update an existing store to the current version
  * @author Arjan van Leeuwen
  *
  * This class can be used to update an existing store to the new format.
  * Currently only updates Store2 to Store3
  */
class StoreUpdater : public MessageObject
{
public:
	/** Constructor
	  */
	StoreUpdater(Store& new_store);

	/** Destructor
	  */
	~StoreUpdater();

	/** Initialize this class - run before running any other functions (except static)
	  */
	OP_STATUS	Init();

	/** Start the update process
	  */
	OP_STATUS	StartUpdate();

	/** Stops the update process
	  */
	void		StopUpdate() { m_continue = FALSE; }

	/** Add a listener for the update process
	  * @param listener Listener to add
	  */
	OP_STATUS	AddListener(StoreUpdateListener* listener) { return m_listeners.Add(listener); }

	/** Remove a listener for the update process
	  * @param listener Listener to remove
	  */
	OP_STATUS	RemoveListener(StoreUpdateListener* listener) { return m_listeners.Remove(listener); }

	/** Check if this store needs an update
	  * @return Whether the store needs to be updated
	  */
	static BOOL NeedsUpdate();

	// From MessageObject
	void		HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

private:
#ifdef OPERA_BIG_ENDIAN
	struct eight_eight_sixteen {
		UINT16 sixteen;
		UINT8  eight2;
		UINT8  eight1;
	} ees;
#else
	struct eight_eight_sixteen {
		UINT8  eight1;
		UINT8  eight2;
		UINT16 sixteen;
	} ees;
#endif

	OP_STATUS UpdateMessage();
	void	  OnProgressChanged();
	void	  OnFinished();
	OP_STATUS WriteLog();
	static OP_STATUS PrepareFromLog(OpFile& file, int& block, BOOL read_only);
	OP_STATUS LogError(OP_STATUS errorval, unsigned message_id, const char* errormsg);

	Store&			m_new_store;
	BlockFile		m_old_store;
	int				m_current_block;
	BOOL			m_continue;
	BOOL			m_errors;
	OpFile			m_update_log;
	OpListeners<StoreUpdateListener> m_listeners;

	static BOOL3	m_needs_update;
};

#endif // M2_MERLIN_COMPATIBILITY
#endif // STORE_UPDATER_H
