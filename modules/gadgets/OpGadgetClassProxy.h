/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_GADGETS_OPGADGETCLASSPROXY_H
#define MODULES_GADGETS_OPGADGETCLASSPROXY_H

#ifdef GADGET_SUPPORT

#include "modules/util/opautoptr.h"

class OpGadgetClass;
class OpGadgetUpdateInfo;

/** An installed OpGadgetClass or information required to install one.
 *
 * This class represents either an existing GadgetClass or one that should
 * be installed. The reason for such a proxy is to allow downloading gadgets
 * via the gadget update mechanism.
 */
class OpGadgetClassProxy
{
public:
	/** Create an OpGadgetClassProxy that represents an installed gadget class. */
	OpGadgetClassProxy(OpGadgetClass* gadget_class);

	/** Create an OpGadgetClassProxy that represents a not-yet-installed gadget class.
	 *
	 * @param update_description_document_url URL to the Update Description Document which contains,
	 * among other information, the source URL from which the gadget can be
	 * downloaded.
	 * @param user_data Pointer to arbitrary user data that can be used to identify
	 * the installation if there are several gadgets installed from the same @a
	 * update_description_document_url.
	 */
	OpGadgetClassProxy(const uni_char* update_description_document_url,
					   const void* const user_data);

	/** @retval true if this represents an installed gadget class. GetGadgetClass()
	 * may be used to access it.
	 * @retval false if this represents a not-yet-installed gadget class. Use
	 * GetDownloadUrl() and GetUserData() instead of GetGadgetClass().
	 */
	bool IsGadgetClassInstalled() const { return m_gadget_class != NULL; }

	/** @return Pointer to represented OpGadgetClass. May be NULL. */
	OpGadgetClass* GetGadgetClass() const { return m_gadget_class; }

	/** Get Update Description Document URL.
	 *
	 * If the OpGadgetClass is present, returns its Update Description Document URL.
	 * Otherwise, returns a pointer to a local Update Description Document URL.
	 *
	 * @return Pointer to Update Description Document URL. May be NULL. */
	const uni_char* GetUpdateDescriptionDocumentUrl() const;

	/** @return Pointer to arbitrary user data given when at creation. */
	const void* GetUserData() const { return m_user_data; }

	/** Get update information.
	 *
	 * If the OpGadgetClass is present, returns its update information.
	 * Otherwise, returns a pointer to a local OpGadgetUpdateInfo object.
	 *
	 * @return Pointer to OpGadgetUpdateInfo. May be NULL.
	 */
	OpGadgetUpdateInfo* GetUpdateInfo() const;

	/** Set update information.
	 *
	 * If the OpGadgetClass is present, call its SetUpdateInfo().
	 * Otherwise, set the local OpGadgetUpdateInfo.
	 */
	void SetUpdateInfo(OpGadgetUpdateInfo* update_info);
private:
	OpGadgetClass* m_gadget_class;
	const uni_char* m_update_description_document_url;
	const void* m_user_data;
	OpAutoPtr<OpGadgetUpdateInfo> m_update_info;
};

#endif // GADGET_SUPPORT
#endif // MODULES_GADGETS_OPGADGETCLASSPROXY_H
