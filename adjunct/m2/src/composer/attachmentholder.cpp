/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/composer/attachmentholder.h"

#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"
#include "adjunct/m2/src/composer/messageencoder.h"
#include "modules/util/opfile/opfile.h"
#include "modules/url/url_tools.h"
#include "modules/mime/mime_module.h"

AttachmentHolder::AttachmentHolder(SimpleTreeModel& model)
  : m_model(model)
  , m_id(0)
  , m_next_attachment_id(1)
{
}


OP_STATUS AttachmentHolder::Init(message_gid_t id, const OpStringC& draftsdir)
{
	m_id = id;
	RETURN_IF_ERROR(m_directory.Set(draftsdir));

	if (!m_id)
		return OpStatus::OK;

	OpString path;
	RETURN_IF_ERROR(path.AppendFormat(UNI_L("%s%c%d"), m_directory.CStr(), PATHSEPCHAR, m_id));

	OpFolderLister* lister;
	RETURN_IF_ERROR(OpFolderLister::Create(&lister));
	OpAutoPtr<OpFolderLister> lister_holder(lister);
	RETURN_IF_ERROR(lister->Construct(path.CStr(), UNI_L("opr*")));

	while (lister->Next())
	{
		OpAutoPtr<AttachmentInfo> info (OP_NEW(AttachmentInfo, ()));
		if (!info.get())
			return OpStatus::ERR_NO_MEMORY;

		RETURN_IF_ERROR(info->m_copy_filename.Set(lister->GetFullPath()));

		const uni_char* filename = lister->GetFileName();
		if (uni_sscanf(filename, UNI_L("opr%d"), &info->m_id) < 1)
			continue;

		filename += 3;
		while ('0' <= *filename && *filename <= '9')
			filename++;
		if (*filename++ != '-')
			continue;

		RETURN_IF_ERROR(info->m_suggested_filename.Set(filename));

		RETURN_IF_ERROR(m_attachments.Add(info.get()));

		m_next_attachment_id = max(m_next_attachment_id, info->m_id + 1);

		RETURN_IF_ERROR(AddToModel(info->m_id, filename, lister->GetFileLength(), info->GetFullPath()));

		info.release();
	}

	return OpStatus::OK;
}


OP_STATUS AttachmentHolder::AddFromFile(const OpStringC& file_path, const OpStringC& suggested_filename)
{
	OpAutoPtr<AttachmentInfo> info (OP_NEW(AttachmentInfo, ()));
	if (!info.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(info->m_original_filename.Set(file_path.CStr()));

	OpFile file;
	RETURN_IF_ERROR(file.Construct(file_path.CStr()));
	OpFileInfo::Mode mode;
	RETURN_IF_ERROR(file.GetMode(mode));
	if (mode == OpFileInfo::DIRECTORY || mode == OpFileInfo::NOT_REGULAR)
		return OpStatus::ERR;
	RETURN_IF_ERROR(info->m_suggested_filename.Set(suggested_filename.IsEmpty() ? file.GetName() : suggested_filename.CStr()));

	info->m_id = m_next_attachment_id++;

	OpFileLength size;
	RETURN_IF_ERROR(file.GetFileLength(size));
	RETURN_IF_ERROR(AddToModel(info->m_id, info->m_suggested_filename, size, info->GetFullPath()));

	RETURN_IF_ERROR(m_attachments.Add(info.get()));
	info.release();

	return OpStatus::OK;
}


OP_STATUS AttachmentHolder::AddFromCache(const OpStringC& file_path, const OpStringC& suggested_name, URL& url, BOOL is_inlined)
{
	OpAutoPtr<URL_InUse> inuse(OP_NEW(URL_InUse, (url)));
	if (!inuse.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(m_cache_holder.Add(inuse.get()));
	inuse.release();

	// inlined attachments are not encoded by the message encoder, saving that they're in use is enough
	if (is_inlined)
	{
		return OpStatus::OK;
	}

	OpAutoPtr<AttachmentInfo> info (OP_NEW(AttachmentInfo, ()));
	if (!info.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(info->m_original_filename.Set(file_path.CStr()));
	RETURN_IF_ERROR(info->m_suggested_filename.Set(suggested_name));
	RETURN_IF_ERROR(info->m_suggested_mime_type.Set(url.GetAttribute(URL::KMIME_Type).CStr()));

	OpFile file;
	RETURN_IF_ERROR(file.Construct(file_path.CStr()));

	info->m_id = m_next_attachment_id++;

	OpFileLength size;
	RETURN_IF_ERROR(file.GetFileLength(size));
	RETURN_IF_ERROR(AddToModel(info->m_id, info->m_suggested_filename, size, info->GetFullPath()));

	RETURN_IF_ERROR(m_attachments.Add(info.get()));
	info.release();

	return OpStatus::OK;
}


OP_STATUS AttachmentHolder::AddToModel(int id, const OpStringC& suggested_filename, OpFileLength size, uni_char* full_path)
{
	INT32 pos = m_model.AddItem(suggested_filename, 0, 0, -1, 0, id);

	OpString formatted_size;
	RETURN_IF_ERROR(FormatByteSize(formatted_size, size));

	m_model.SetItemData(pos, 1, formatted_size.CStr(), 0, size);

	return OpStatus::OK;
}


OP_STATUS AttachmentHolder::Save(message_gid_t id)
{
	m_id = id;

	if (!m_id)
		return OpStatus::ERR;

	unsigned i;
	OpVector<AttachmentInfo> failed;
	for (i = 0; i < m_attachments.GetCount(); i++)
	{
		AttachmentInfo* attachment = m_attachments.Get(i);
		if (attachment->m_copy_filename.IsEmpty())
		{
			if (OpStatus::IsError(CreateFileCopy(attachment)))
				RETURN_IF_ERROR(failed.Add(attachment));
		}
	}

	// All files have been copied now, only hold on to URL_CONTENT_ID urls (which don't have associated files)
	for (int i = m_cache_holder.GetCount() - 1; i >= 0; i--)
	{
		URL_InUse* url = m_cache_holder.Get(i);
		if (url->GetURL().GetAttribute(URL::KType) != URL_CONTENT_ID && !g_mime_module.HasContentID(url->GetURL()))
		{
			m_cache_holder.Remove(i);
			OP_DELETE(url);
		}
	}

	if (failed.GetCount() > 0)
	{
		for (i = 0; i < failed.GetCount(); i++)
			OpStatus::Ignore(Remove(failed.Get(i))); // Removes file from UI list as well
		
		// We couldn't add one or more attachments, most likely explanation is no access to the file
		return OpStatus::ERR_NO_ACCESS;
	}

	return OpStatus::OK;
}


OP_STATUS AttachmentHolder::CreateFileCopy(AttachmentInfo* info)
{
	OpString copy_path;
	RETURN_IF_ERROR(copy_path.AppendFormat(UNI_L("%s%c%d%copr%d-%s"), m_directory.CStr(), PATHSEPCHAR, m_id, PATHSEPCHAR, info->m_id, info->m_suggested_filename.CStr()));

	OpFile copy;
	RETURN_IF_ERROR(copy.Construct(copy_path.CStr()));

	OpFile file;
	RETURN_IF_ERROR(file.Construct(info->m_original_filename.CStr()));
	RETURN_IF_ERROR(copy.CopyContents(&file, TRUE));

	return info->m_copy_filename.Set(copy.GetFullPath());
}


OP_STATUS AttachmentHolder::Remove(int id)
{
	for (unsigned i = 0; i < m_attachments.GetCount(); i++)
	{
		AttachmentInfo* info = m_attachments.Get(i);

		if (info->m_id == id)
		{
			return Remove(info);
		}
	}

	return OpStatus::OK;
}


OP_STATUS AttachmentHolder::RemoveAll()
{
	m_attachments.DeleteAll();
	m_model.DeleteAll();
	m_cache_holder.DeleteAll();

	if (!m_id)
		return OpStatus::OK;

	OpString draftsdir_path;
	RETURN_IF_ERROR(draftsdir_path.AppendFormat(UNI_L("%s%c%d"), m_directory.CStr(), PATHSEPCHAR, m_id));

	OpFile draftsdir;
	RETURN_IF_ERROR(draftsdir.Construct(draftsdir_path));
	return draftsdir.Delete();
}


OP_STATUS AttachmentHolder::Remove(AttachmentInfo* info)
{
	if (info->m_copy_filename.HasContent())
	{
		OpFile file;
		RETURN_IF_ERROR(file.Construct(info->m_copy_filename.CStr()));
		RETURN_IF_ERROR(file.Delete());
	}
	RETURN_IF_ERROR(m_attachments.RemoveByItem(info));

	SimpleTreeModelItem* item = m_model.GetItemByID(info->m_id);
	if (item)
		m_model.Delete(item->GetIndex());

	OP_DELETE(info);

	return OpStatus::OK;
}


OP_STATUS AttachmentHolder::EncodeForSending(MessageEncoder& encoder, const OpStringC8& charset)
{
	for (unsigned i = 0; i < m_attachments.GetCount(); i++)
	{
		AttachmentInfo* attachment = m_attachments.Get(i);
		OpStringC filename (attachment->GetFullPath());

		RETURN_IF_ERROR(encoder.AddAttachment(filename, attachment->m_suggested_filename, charset, attachment->m_suggested_mime_type));
	}

	return OpStatus::OK;
}

OP_STATUS AttachmentHolder::GetAttachmentInfoById(int id, UINT32 context_id, OpString& suggested_filename, URL& attachment_url)
{
	for (unsigned i = 0; i < m_attachments.GetCount(); i++)
	{
		if (m_attachments.Get(i)->m_id == id)
		{
			return GetAttachmentInfoByPosition(i, context_id, suggested_filename, attachment_url);
		}
	}
	
	return OpStatus::ERR;
}

OP_STATUS AttachmentHolder::GetAttachmentInfoByPosition(int position, UINT32 context_id, OpString& suggested_filename, URL& attachment_url)
{
	OpString escaped_filename, resolved_path;
	AttachmentInfo* attachment = m_attachments.Get(position);
	RETURN_IF_ERROR(suggested_filename.Set(attachment->m_suggested_filename.CStr()));
	RETURN_IF_LEAVE(g_url_api->ResolveUrlNameL(attachment->GetFullPath(), resolved_path));
	escaped_filename.Reserve(resolved_path.Length()*3+1);	//worst case
	EscapeFileURL(escaped_filename.CStr(), resolved_path.CStr());
	attachment_url = g_url_api->GetURL(escaped_filename, context_id);
	attachment_url.QuickLoad(TRUE);
	return OpStatus::OK;
}

#endif // M2_SUPPORT
