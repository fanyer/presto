/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
** psmaas - Patricia Aas
*/

#include "core/pch.h"

#ifdef HISTORY_SUPPORT

#include "modules/history/OpHistorySiteFolder.h"
#include "modules/history/OpHistorySiteSubFolder.h"

/*___________________________________________________________________________*/
/*                         CORE HISTORY MODEL SITE SUB FOLDER                */
/*___________________________________________________________________________*/


/***********************************************************************************
 **
 **
 **
 **	
 **
 ***********************************************************************************/
OP_STATUS HistorySiteSubFolder::GetTitle(OpString & title) const
{
	return GetSiteFolder()->GetTitle(title);
}

/***********************************************************************************
 **
 **
 **
 **	
 **
 ***********************************************************************************/
const OpStringC & HistorySiteSubFolder::GetTitle() const
{
	return GetSiteFolder()->GetTitle();
}

/***********************************************************************************
 **
 **
 **
 **	
 **
 ***********************************************************************************/
const LexSplayKey * HistorySiteSubFolder::GetKey() const
{
	return GetSiteFolder()->GetKey();
}

#endif // HISTORY_SUPPORT
