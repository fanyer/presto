// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#ifndef ASSOCIATEDFILE_H
#define ASSOCIATEDFILE_H

#ifdef URL_ENABLE_ASSOCIATED_FILES

#include "modules/util/opfile/opfile.h"

class URL_Rep;

class AssociatedFile : public OpFile
{
friend class URL_Rep;
public:
	AssociatedFile(void) : OpFile() {rep = NULL;}
	virtual ~AssociatedFile(void);

protected:
	URL_Rep *rep;

	void SetRep(URL_Rep *url_rep);
};

#endif
#endif  // ASSOCIATEDFILE_H

