/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _REPOSITORY_ITEM_H
#define _REPOSITORY_ITEM_H

/** Element of array used for a platform specific certificate repository */
struct External_RepositoryItem{
	/** Pointer to binary (DER-Encoded) X509 certificate */
	const unsigned char *certificate;

	/** Length of the certificate */
	size_t certificate_len;
};

#endif // _REPOSITORY_ITEM_H
