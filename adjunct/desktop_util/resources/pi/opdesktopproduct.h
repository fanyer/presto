/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OPDESKTOPPRODUCT_H
#define OPDESKTOPPRODUCT_H

class OperaInitInfo;
class PrefsFile;

#define g_desktop_product (&OpDesktopProduct::GetInstance())

enum DesktopProductType
{
	PRODUCT_TYPE_OPERA,
	PRODUCT_TYPE_OPERA_NEXT,
	PRODUCT_TYPE_OPERA_LABS
};

class OpDesktopProduct
{
public:

	static OpDesktopProduct& GetInstance();

	OpDesktopProduct() : m_product_type(PRODUCT_TYPE_OPERA) {}

	/**
	 * Initializes product information.
	 *
	 * @param pinfo pointer to the OperaInitInfo
	 * @param operaprefs prefs reader initialized with global and user's operaprefs.ini
	 *
	 * @return OK on success, ERR_NO_MEMORY on OOM
	 */
	OP_STATUS Init(OperaInitInfo* pinfo, PrefsFile* operaprefs);

	/**
	 * Returns which product this is.
	 */
	DesktopProductType GetProductType() const { return m_product_type; }

	/**
	 * Returns the type of Opera package (e.g. "tar", "exe").
	 */
	OpStringC GetPackageType() const { return m_package_type; }

	/**
	 * Returns the name of this labs product (e.g. "Hardware Acceleration").
	 */
	OpStringC GetLabsProductName() const { return m_labs_product_name; }


	/**
	 * Information about Opera product.
	 */
	struct ProductInfo
	{
		ProductInfo() : m_product_type(PRODUCT_TYPE_OPERA) {}
		DesktopProductType m_product_type; //< type of Opera product, mandatory
		OpString m_labs_product_name; //< name of labs product (e.g. "Hardware Acceleration"), can be empty
		OpString m_package_type; //< type of Opera package (e.g. "tar", "exe"), can be empty
	};

	/**
	 * Initializes ProductInfo structure.
	 * Implementaion of this function is provided by platform code.
	 *
	 * @param pinfo pointer to the OperaInitInfo object that can be used in calls to ResourceFolders::GetFolder
	 * @param operaprefs prefs reader initialized with global and user's operaprefs.ini
	 * @param product_info struct to be filled with product information
	 *
	 * @return OK on success, ERR_NO_MEMORY on OOM
	 */
	static OP_STATUS GetProductInfo(OperaInitInfo* pinfo, PrefsFile* operaprefs, ProductInfo& product_info);

private:
	DesktopProductType m_product_type; //< type of Opera product
	OpString m_labs_product_name;      //< name of labs product
	OpString m_package_type;           //< type of Opera package
};

#endif // OPDESKTOPPRODUCT_H
