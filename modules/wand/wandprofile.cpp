/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef WAND_SUPPORT

#include "modules/wand/wandmanager.h"

#include "modules/wand/wand_internal.h"
#include "modules/doc/frm_doc.h"
#include "modules/util/opfile/opfile.h"
#include "modules/forms/formvalue.h"
#include "modules/sync/sync_util.h"
#include "modules/libcrypto/include/CryptoBlob.h"

WandProfile::~WandProfile()
{
#ifdef SYNC_HAVE_PASSWORD_MANAGER
	WandManager::SyncBlocker sync_blocker; // don't sync delete events to server 
										   // when shutting down.
#endif // SYNC_HAVE_PASSWORD_MANAGER

	DeleteAllPages();
}

OP_STATUS WandProfile::Save(OpFile &file)
{
	RETURN_IF_ERROR(WriteWandString(file, name));
	RETURN_IF_ERROR(file.WriteBinByte(type));

	RETURN_IF_ERROR(file.WriteBinLong(pages.GetCount()));
	for(UINT32 i = 0; i < pages.GetCount(); i++)
	{
		RETURN_IF_ERROR(pages.Get(i)->Save(file));
	}
	return OpStatus::OK;
}

OP_STATUS WandProfile::Open(OpFile &file, long version)
{
	RETURN_IF_ERROR(ReadWandString(file, name, version));
	BYTE byte;
	OpFileLength bytes_read;
	RETURN_IF_ERROR(file.Read(&byte, 1, &bytes_read));
	if (bytes_read != 1)
		return OpStatus::ERR;
	if (byte >= WAND_TYPE_HIGH_LIMIT)
	{
		return OpStatus::ERR;
	}

	long num_pages = 0;
	OpStatus::Ignore(file.ReadBinLong(num_pages));

	for(int i = 0; i < num_pages; i++)
	{
		WandPage* page = OP_NEW(WandPage, ());
		if (!page ||
			OpStatus::IsError(page->Open(file, version)) ||
			OpStatus::IsError(pages.Add(page)))
		{
			OP_DELETE(page);
			return OpStatus::ERR_NO_MEMORY;
		}
		if (type == WAND_LOG_PROFILE)
			m_wand_manager->OnWandPageAdded(pages.GetCount() - 1);
	}

	return OpStatus::OK;
}


WandPage* WandProfile::FindPage(FramesDocument* doc, INT32 index /* = 0 */)
{
	if (type == WAND_PERSONAL_PROFILE)
	{
		OP_ASSERT(pages.GetCount() == 1); // Personal profiles should always have 1 page. No more no less, unless we have OOM:er or something.
		return (unsigned)index >= pages.GetCount() ? NULL : pages.Get(0); // The only page
	}
	else
	{
		int nr = 0;
		for(UINT32 i = 0; i < pages.GetCount(); i++)
		{
			WandPage* page = pages.Get(i);
			if (page->IsMatching(doc))
			{
				if (nr == index)
					return page;
				nr++;
			}
		}
	}
	return NULL;
}

// Find a page from the same document as new_page orsuitable in some other way.
WandPage* WandProfile::FindPage(WandPage* new_page, INT32 index /* = 0 */)
{
	if (type == WAND_PERSONAL_PROFILE)
	{
		OP_ASSERT(pages.GetCount() == 1); // Personal profiles should always have 1 page. No more no less, unless we have OOM:er or something.
		return (unsigned)index >= pages.GetCount() ? NULL : pages.Get(0); // The only page
	}
	else
	{
		int nr = 0;
		for(UINT32 i = 0; i < pages.GetCount(); i++)
		{
			WandPage* page = pages.Get(i);
			if (page->IsMatching(new_page))
			{
				if (nr == index)
					return page;
				nr++;
			}
		}
	}
	return NULL;
}

void WandProfile::DeletePage(FramesDocument* doc, INT32 index)
{
	if (type == WAND_PERSONAL_PROFILE)
	{
		OP_ASSERT(pages.GetCount() == 1); // Personal profiles should always have 1 page. No more no less.
	}
	else
	{
		int nr = 0;
		for(UINT32 i = 0; i < pages.GetCount(); i++)
		{
			WandPage* page = pages.Get(i);
			if (page->IsMatching(doc))
			{
				if (nr == index)
				{
					if (type == WAND_LOG_PROFILE)
					{
						m_wand_manager->OnWandPageRemoved(i, page);
					}
					/* Note: remove the WandPage from pages after notifying the
					 * listener, the listener may need to access the page once
					 * more... */
					WandPage* removed_page = pages.Remove(i);
					OP_ASSERT(page == removed_page);
					OP_DELETE(removed_page);
					return;
				}
				nr++;
			}
		}
	}
}

void WandProfile::DeletePage(int index)
{
	OP_ASSERT(type == WAND_LOG_PROFILE);
	WandPage* page = pages.Get(index);
	m_wand_manager->OnWandPageRemoved(index, page);
	/* Note: remove the WandPage from pages after notifying the listener, the
	 * listener may need to access the page once more... */
	WandPage* removed_page = pages.Remove(index);
	OP_ASSERT(removed_page == page);
	OP_DELETE(removed_page);
}

void WandProfile::DeleteAllPages()
{
	INT32 count = pages.GetCount();
	for(int i = 0; i < count; i++)
	{
		WandPage* page = pages.Get(0);
		if (type == WAND_LOG_PROFILE)
		{
			m_wand_manager->OnWandPageRemoved(0, page);
		}
		/* Note: remove the WandPage from pages after notifying the listener,
		 * the listener may need to access the page once more... */
		WandPage *removed_page = pages.Remove(0);
		OP_ASSERT(removed_page == page);
		OP_DELETE(removed_page);
	}
}

OP_STATUS WandProfile::Store(WandPage* submitted_page)
{
	OP_ASSERT(submitted_page);
	OP_ASSERT(!submitted_page->GetUrl().IsEmpty());

	OP_STATUS status = pages.Add(submitted_page);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(submitted_page);
		return status;
	}

	if (type == WAND_LOG_PROFILE)
	{
		m_wand_manager->OnWandPageAdded(pages.GetCount() - 1);

#ifdef WAND_EXTERNAL_STORAGE
		if (submitted_page->GetNeverOnThisPage() == FALSE && m_wand_manager->data_storage && submitted_page->CountPasswords() == 1)
		{
			WandDataStorageEntry entry;
			OpString tmppass;
			submitted_page->FindPassword()->Decrypt(tmppass);
			entry.Set(submitted_page->FindUserName(), tmppass.CStr());
			tmppass.Wipe();

			m_wand_manager->data_storage->StoreData(submitted_page->GetUrl(), &entry);
		}
#endif // WAND_EXTERNAL_STORAGE
	}
	return OpStatus::OK;
}

#ifdef WAND_ECOMMERCE_SUPPORT
OP_STATUS WandProfile::StoreECommerce(WandPage* submitted_page /*HTML_Element* form_elm*/)
{
	// FIXME: Move this code into WandPage
	OP_ASSERT(type == WAND_PERSONAL_PROFILE);

	OP_ASSERT(submitted_page);
	for (int i = 0; i < submitted_page->CountObjects(); i++)
	{
		WandObjectInfo* submitted_field = submitted_page->GetObject(i);
		OP_ASSERT(submitted_field);
		const OpString& name = submitted_field->name;
		OP_ASSERT(!name.IsEmpty());

		if (!submitted_field->is_password && WandManager::IsECommerceName(name.CStr()))
		{
			// Compare with the one stored in the global profile
			WandPage* global_page = GetWandPage(0);
			const uni_char* value = submitted_field->value.CStr();
			WandObjectInfo* stored_ecom_info = global_page->FindNameMatch(name.CStr());
			if (!stored_ecom_info || 
				stored_ecom_info->value.IsEmpty() ||
				value && !uni_str_eq(stored_ecom_info->value.CStr(), value))
			{
				BOOL changed = submitted_field->is_changed;
				OP_ASSERT(!submitted_field->is_password);
				RETURN_IF_ERROR(global_page->AddEComObjectInfo(name.CStr(), value, changed));
			}
		}
	} // end for

	return OpStatus::OK;
}
#endif // WAND_ECOMMERCE_SUPPORT

WandObjectInfo* WandProfile::FindMatch(FramesDocument* doc, HTML_Element* he)
{
	OP_ASSERT(doc);
	OP_ASSERT(he);

	// There may be several pages matching the formobjects document. We have to check all since some
	// of them may be "entire server"-matches which actually does/doesn't match with this particular document.
	INT32 index = 0;
	WandPage* page = NULL;
	while((page = FindPage(doc, index)) != NULL)
	{
		HTML_Element* he_form = NULL;
		WandObjectInfo* objinfo = page->FindMatch(doc, he, he_form);
		if (objinfo)
		{
			OP_ASSERT(!page->GetNeverOnThisPage() || !"Should never have been stored");
			return objinfo;
		}
		index++;
	}

	return NULL;
}

#ifdef SYNC_HAVE_PASSWORD_MANAGER

OP_STATUS WandProfile::SyncDataFlush(OpSyncDataItem::DataItemType item_type, BOOL first_sync, BOOL is_dirty)
{
	OP_ASSERT(item_type == OpSyncDataItem::DATAITEM_PM_FORM_AUTH && type == WAND_LOG_PROFILE);

	if(item_type != OpSyncDataItem::DATAITEM_PM_FORM_AUTH || type != WAND_LOG_PROFILE)
		return OpStatus::OK; // Should not happen.

	WandSecurityWrapper security;
	OP_STATUS security_status = security.EnableWithoutWindow();
	RETURN_IF_MEMORY_ERROR(security_status);
	if (security_status == OpStatus::ERR_YIELD)
		return m_wand_manager->SetSuspendedSyncOperation(SuspendedWandOperation::WAND_OPERATION_SYNC_DATA_FLUSH_FORM_NO_WINDOW, first_sync, is_dirty);

	UINT32 count = pages.GetCount();
	unsigned index;
	for (index = 0; index < count; index++)
	{
		OpStatus::Ignore(pages.Get(index)->SyncItem(TRUE, is_dirty));
	}

	return OpStatus::OK;
}

WandSyncItem* WandProfile::FindWandSyncItem(const uni_char *sync_id, int &page_list_index)
{
	for (unsigned idx = 0; idx < pages.GetCount(); idx++)
	{
		WandPage *page = pages.Get(idx);
		if (!page->sync_id.Compare(sync_id))
		{
			page_list_index = idx;
			return page;
		}
	}

	return NULL;
}

WandSyncItem* WandProfile::FindWandSyncItemWithSameUser(WandSyncItem *item, int &index)
{
	OP_ASSERT(item->GetAuthType() == OpSyncDataItem::DATAITEM_PM_FORM_AUTH);
	index = -1;
	if (item->GetAuthType() == OpSyncDataItem::DATAITEM_PM_FORM_AUTH)
	{
		WandPage *page = static_cast<WandPage*>(item);
		WandPage *result = NULL;
		int page_index = 0;
		do
		{
			result = FindPage(page, page_index++);
		} while (result && !result->IsSameFormAndUsername(page, NULL));

		if (result)
			index = pages.Find(result);
		return result;
	}
	return NULL;
}

void WandProfile::DeleteWandSyncItem(int index)
{
	DeletePage(index);
}

OP_STATUS WandProfile::StoreWandSyncItem(WandSyncItem *item)
{

	OP_ASSERT(item->GetAuthType() == OpSyncDataItem::DATAITEM_PM_FORM_AUTH);
	if (item->GetAuthType() == OpSyncDataItem::DATAITEM_PM_FORM_AUTH)
	{
		return Store(static_cast<WandPage*>(item));
	}
	return OpStatus::ERR;
}



void WandProfile::SyncWandItemsSyncStatuses()
{
	for (unsigned index = 0; index < pages.GetCount(); index++)
	{
		OpStatus::Ignore(pages.Get(index)->SyncItem());
	}
}
#endif // SYNC_HAVE_PASSWORD_MANAGER

WandPage* WandProfile::CreatePage(FramesDocument* doc, HTML_Element* he_form)
{
	WandPage* page = OP_NEW(WandPage, ());
	if (page == NULL)
		return NULL;

	if (doc && OpStatus::IsError(page->SetUrl(doc, he_form)) ||
		OpStatus::IsError(pages.Add(page)))
	{
		OP_DELETE(page);
		return NULL;
	}

	return page;
}

#endif // WAND_SUPPORT
