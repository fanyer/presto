// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#include "core/pch.h"

#ifdef URL_ENABLE_ASSOCIATED_FILES

#include "modules/cache/AssociatedFile.h"

#include "modules/url/url2.h"
#include "modules/url/url_rep.h"

AssociatedFile::~AssociatedFile(void)
{
	if (rep != NULL)
		rep->DecUsed(1);
}

void AssociatedFile::SetRep(URL_Rep *url_rep)
{
	OP_ASSERT(rep == NULL && url_rep!=NULL);

	if(rep)
		rep->DecUsed(1);

	rep = url_rep; 

	if(rep)
		rep->IncUsed(1);
}

#endif

