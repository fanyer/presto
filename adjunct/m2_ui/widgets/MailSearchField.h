/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Alexander Remen (alexr)
*/


#ifndef MAILSEARCHFIELD_H
#define MAILSEARCHFIELD_H

#include "adjunct/quick_toolkit/widgets/OpQuickFind.h"

class MailSearchField : public OpQuickFind
					  , public OpPrefsListener
{
public:
			static OP_STATUS		Construct(MailSearchField** obj);
			~MailSearchField();
			
			// == OpWidgetListener ========================
			virtual void			OnChange(OpWidget *widget, BOOL changed_by_mouse);
			virtual void			OnDropdownMenu(OpWidget *widget, BOOL show);

			virtual Type			GetType() {return WIDGET_TYPE_MAIL_SEARCH_FIELD;}

			// == OpPrefsListener ========================
			virtual void			PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue);

			void					IgnoreMatchText(BOOL should_ignore) { m_ignore_match_text = should_ignore; }
private:
			MailSearchField();
			virtual void			MatchText();
			
			enum SpecialItems
			{
			
				CLEAR_ALL_SEARCHES		= 1, // Clear all searches
				SEARCH_IN_CURRENT_VIEW	= 2, // Search in current view
				SEARCH_IN_ALL_MESSAGES	= 3	 // Search in all messages
			};

			BOOL					m_ignore_match_text;
};

#endif // MAILSEARCHFIELD_H