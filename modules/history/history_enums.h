/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
** psmaas - Patricia Aas
*/

#ifndef HISTORY_ENUMS_H
#define HISTORY_ENUMS_H

//___________________________________________________________________________
// 
//___________________________________________________________________________
enum TimePeriod 
{
    TODAY            = 0, 
    YESTERDAY        = 1, 
    WEEK             = 2,  
    MONTH            = 3,
    OLDER            = 4,
    NUM_TIME_PERIODS = 5 //Do not insert constants after this one 
};

//___________________________________________________________________________
// Used to distinguish different folders from eachother
//___________________________________________________________________________
enum HistoryFolderType 
{
    SITE_FOLDER, 
    SITE_SUB_FOLDER,  
    TIME_FOLDER,
    PREFIX_FOLDER,
    NUM_FOLDER_TYPES//Do not insert constants after this one 
};

//___________________________________________________________________________
// 
//___________________________________________________________________________
enum HistoryProtocol 
{
	OPERA_PROTOCOL = 0,
	FILE_PROTOCOL  = 1,
	FTP_PROTOCOL   = 2,
	HTTP_PROTOCOL  = 3,
	HTTPS_PROTOCOL = 4,
	NUM_PROTOCOLS  = 5, //Do not insert constants after this one
	NONE_PROTOCOL
};

#endif //HISTORY_ENUMS_H
