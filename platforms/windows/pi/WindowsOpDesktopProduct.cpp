/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "adjunct/desktop_util/resources/pi/opdesktopproduct.h"
#include "modules/prefsfile/prefsfile.h"

/*static*/
OP_STATUS OpDesktopProduct::GetProductInfo(OperaInitInfo*, PrefsFile* operaprefs, OpDesktopProduct::ProductInfo& product_info)
{
	const DesktopProductType pref_to_enum_map[] =
	{
		PRODUCT_TYPE_OPERA,
		PRODUCT_TYPE_OPERA_NEXT,
		PRODUCT_TYPE_OPERA_LABS
	};

	int pref;
	RETURN_IF_LEAVE(pref = operaprefs->ReadIntL("System", "Opera Product"));

	//No guarantee that the value of the pref is valid, since it can be changed by users
	if (pref >= (sizeof(pref_to_enum_map)/sizeof(DesktopProductType)) || pref < 0)
	{
		product_info.m_product_type = PRODUCT_TYPE_OPERA;
	}
	else
	{
		product_info.m_product_type = pref_to_enum_map[pref];
	}

	if (product_info.m_product_type == PRODUCT_TYPE_OPERA_LABS)
	{
		RETURN_IF_LEAVE(operaprefs->ReadStringL("System", "Opera Labs Name", product_info.m_labs_product_name));
	}
	else
	{
		product_info.m_labs_product_name.Empty();
	}
	return product_info.m_package_type.Set("exe");
}
