/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef WANDENUMS_H
#define WANDENUMS_H

#ifdef WAND_SUPPORT

/**
 * Different choices for the UI regarding a wand enquiry.
 */
enum WAND_ACTION
{
	WAND_STORE = 0,						///< Store for this exact url
	WAND_STORE_ENTIRE_SERVER,			///< Store for all urls on this domain
	WAND_DONT_STORE,					///< Don't store anything
	WAND_NEVER_STORE_ON_THIS_PAGE,		///< Don't store any info and never do it again in this exact url.
	WAND_NEVER_STORE_ON_ENTIRE_SERVER	///< Don't store any info and never do it again for all urls on this domain.
};

#ifdef WAND_ECOMMERCE_SUPPORT
/**
 * How to treat found eCommerce data in a page.
 */
enum WAND_ECOMMERCE_ACTION {
	/**
	 * Don't store the found eCommerce stuff.
	 */
	WAND_DONT_STORE_ECOMMERCE_VALUES,
	/**
	 * Store the found eCommerce stuff in
	 * the personal profile.
	 */
	WAND_STORE_ECOMMERCE_VALUES
};
#endif // WAND_ECOMMERCE_SUPPORT

#endif // WAND_SUPPORT

#endif // WANDENUMS_H
