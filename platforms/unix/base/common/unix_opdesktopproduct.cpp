/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */
#include "core/pch.h"

#include "adjunct/desktop_util/resources/pi/opdesktopproduct.h"
#include "adjunct/desktop_util/resources/ResourceFolders.h"

#include "modules/prefsfile/prefsfile.h"
#include "modules/util/opfile/opfile.h"

/*static*/
OP_STATUS OpDesktopProduct::GetProductInfo(OperaInitInfo* pinfo, PrefsFile*, OpDesktopProduct::ProductInfo& product_info)
{
	OpString filename;
	RETURN_IF_ERROR(ResourceFolders::GetFolder(OPFILE_RESOURCES_FOLDER, pinfo, filename));
	RETURN_IF_ERROR(filename.Append(UNI_L("package-id.ini")));

	OpFile package_id;
	RETURN_IF_ERROR(package_id.Construct(filename));

	PrefsFile package_id_prefs(PREFS_INI);
	RETURN_IF_LEAVE(
		package_id_prefs.ConstructL(); \
		package_id_prefs.SetFileL(&package_id); \
		package_id_prefs.LoadAllL()
	);

	OpStringC product;
	RETURN_IF_LEAVE(product = package_id_prefs.ReadStringL(UNI_L("Package"), UNI_L("Product"), UNI_L("opera")));
	if (product == "opera-next")
	{
		product_info.m_product_type = PRODUCT_TYPE_OPERA_NEXT;
	}
	else if (product.Compare("opera-labs-", 11) == 0)
	{
		product_info.m_product_type = PRODUCT_TYPE_OPERA_LABS;
	}
	else
	{
		product_info.m_product_type = PRODUCT_TYPE_OPERA;
	}

	if (product_info.m_product_type == PRODUCT_TYPE_OPERA_LABS)
	{
		RETURN_IF_LEAVE(package_id_prefs.ReadStringL(UNI_L("Package"), UNI_L("Labs Name"), product_info.m_labs_product_name));
	}
	else
	{
		product_info.m_labs_product_name.Empty();
	}

	RETURN_IF_LEAVE(package_id_prefs.ReadStringL(UNI_L("Package"), UNI_L("Package Type"), product_info.m_package_type, UNI_L("tar")));

	return OpStatus::OK;
}
