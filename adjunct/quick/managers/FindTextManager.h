/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _FIND_TEXT_MANAGER_H
#define _FIND_TEXT_MANAGER_H

#include "modules/hardcore/timer/optimer.h"

class DesktopWindow;

class FindTextManager : public OpTimerListener
{
public:
	FindTextManager();
	virtual	~FindTextManager();

	OP_STATUS Init(DesktopWindow* parent_window);

	void ShowFindTextDialog();
	void ShowNotFoundMsg();

	void SetFlags(BOOL forward, BOOL match_case, BOOL whole_word);
	void SetSearchText(const uni_char* search_text);

	OpString& GetSearchText()
	{ return m_text; }

	BOOL GetForwardFlag()
	{ return m_forward; }

	BOOL GetMatchCaseFlag()
	{ return m_match_case; }

	BOOL GetWholeWordFlag()
	{ return m_whole_word; }

	BOOL GetOnlyLinksFlag()
	{ return m_only_links; }

	/** Resets the flag responsible for forcing search only for links. */
	void ResetOnlyLinksFlag() { m_only_links = FALSE; }

	enum FIND_TYPE {
		FIND_TYPE_ALL,
		FIND_TYPE_LINKS,
		FIND_TYPE_WORDS
	};
	void SetFindType(FIND_TYPE type);
	void SetMatchCase(BOOL match_case);

	void Search();
	void SearchAgain(BOOL forward, BOOL search_inline);
	void SearchInline(const uni_char* text);

	void OnDialogKilled();

	void StartTimedInlineFind(BOOL only_links);
	void EndTimedInlineFind(BOOL close_with_fade);

	OpWidget*				GetTargetWidget();

	enum FIND_MODE {
		FIND_MODE_NORMAL,
		FIND_MODE_TIMED
	};
	FIND_MODE GetMode() { return m_mode; }

	/** Update timer until automatic close. Should be called upon user activity in the search field. */
	void					UpdateTimer();

	/** Stop timer for automatic close. */
	void					StopTimer() { m_timer->Stop(); }

	// OpTimerListener

	virtual void			OnTimeOut(OpTimer* timer);

protected:
	BOOL DoSearch(BOOL forward, BOOL incremental);

private:
	void					StartTimer() {m_timer->Stop(); m_timer->Start(3000);}

	OpString				m_text;
	BOOL					m_forward;
	BOOL					m_match_case;
	BOOL					m_whole_word;
	BOOL					m_only_links;
	FIND_MODE				m_mode;

	DesktopWindow*			m_parent_window;

	BOOL					m_dialog_present;
	OpTimer*				m_timer;
};

#endif //_FIND_TEXT_MANAGER_H
