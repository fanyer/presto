// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// $Id$
//
// Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#ifndef PART_H
#define PART_H

/**
 * use mimedec-structures here
 */

class Part 
{
public:
	
	MimeHeader* GetMime();
    
    Header* GetHeader();
    
	char*   GetContent();
	Part*   GetPart(UINT32 i);
	
};

#endif // PART_H
