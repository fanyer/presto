/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"
#include "adjunct/desktop_util/resources/pi/opdesktopproduct.h"

OpDesktopProduct& OpDesktopProduct::GetInstance()
{
	static OpDesktopProduct s_desktop_product;
	return s_desktop_product;
}

OP_STATUS OpDesktopProduct::Init(OperaInitInfo* pinfo, PrefsFile* operaprefs)
{
	ProductInfo product_info;
	RETURN_IF_ERROR(GetProductInfo(pinfo, operaprefs, product_info));
	m_product_type = product_info.m_product_type;
	RETURN_IF_ERROR(m_package_type.TakeOver(product_info.m_package_type));
	return m_labs_product_name.TakeOver(product_info.m_labs_product_name);
}
