/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file GadgetSignatureStorage.cpp
 *
 * Implementation of some GadgetSignatureStorage methods.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#include "core/pch.h"

#ifdef SIGNED_GADGET_SUPPORT

#include "modules/libcrypto/include/GadgetSignatureStorage.h"


const GadgetSignature* GadgetSignatureStorage::GetSignatureByFlatIndex(unsigned int index) const
{
	const unsigned int distributor_signature_count = GetDistributorSignatureCount();

	if (index < distributor_signature_count)
		return GetDistributorSignature(index);
	else if (index == distributor_signature_count && GetAuthorSignatureCount())
		return GetAuthorSignature();
	else
		return NULL;
}


GadgetSignature* GadgetSignatureStorage::GetSignatureByFlatIndex(unsigned int index)
{
	// Drop mutability.
	const GadgetSignatureStorage* const_this = this;

	// Do the job.
	const GadgetSignature* const_signature = const_this->GetSignatureByFlatIndex(index);

	// Reclaim mutability.
	GadgetSignature* signature = const_cast <GadgetSignature*> (const_signature);

	return signature;
}

#endif // SIGNED_GADGET_SUPPORT
