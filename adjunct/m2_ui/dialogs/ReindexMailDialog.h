/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef REINDEXMAILDIALOG_H
#define REINDEXMAILDIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#ifdef M2_SUPPORT

#include "adjunct/m2/src/engine/listeners.h"

class ProgressInfo;

class ReindexMailDialog : public Dialog , public EngineListener
{
public:
						ReindexMailDialog();
						~ReindexMailDialog();

	Type				GetType()				{return DIALOG_TYPE_REINDEX_MAIL;}
	const char*			GetWindowName()			{return "Reindex Mail Dialog";}
	BOOL				GetModality()			{return FALSE;}
	const uni_char*		GetOkText();

	void				OnInit();
	void				OnCancel();

	// MessageEngine::EngineListener
	void				OnProgressChanged(const ProgressInfo& progress, const OpStringC& progress_text) {};
	void				OnImporterProgressChanged(const Importer* importer, const OpStringC& infoMsg, INT32 current, INT32 total, BOOL simple = FALSE) {};
	void				OnImporterFinished(const Importer* importer, const OpStringC& infoMsg) {};
	void				OnIndexChanged(UINT32 index_id) {};
	void				OnActiveAccountChanged() {};
	void				OnReindexingProgressChanged(INT32 progress, INT32 total);
	void				OnNewMailArrived() {}

	BOOL				OnInputAction(OpInputAction* action);
private:
	OpString			m_ok_text;
	BOOL				m_paused;
	double				m_start_tick;
	INT32				m_start_progress;
};

#endif //M2_SUPPORT
#endif //REINDEXMAILDIALOG_H
