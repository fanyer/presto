// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// $Id$
//
// Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

// ----------------------------------------------------

#ifndef IDENTITY_H
#define IDENTITY_H

// ----------------------------------------------------

typedef UINT32 identity_id_t;

// this has to be thought about some more
// are Identities rfc822-specific or can they
// be generalized to other backends?

class Identity
{
	// e.g. ee@work.com
	//      ee@home.org
	//      ee@NOSPAMhome.org etc
    
    // Address m_adress;
};

// ----------------------------------------------------

#endif // IDENTITY_H
