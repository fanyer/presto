/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef ATTACHMENT_HOLDER_H
#define ATTACHMENT_HOLDER_H

#include "adjunct/m2/src/include/defs.h"

class MessageEncoder;
class SimpleTreeModel;
class Message;

class AttachmentHolder
{
public:
	/** @param model Model to add and remove attachments to, needs to have 2 columns.
	  * Columns are filled in like this: [Suggested filename | Size]
	  */
	AttachmentHolder(SimpleTreeModel& model);

	/** Initialize the attachment holder
	  * @param id M2 ID of message this holds attachment for (can be 0 if unknown)
	  * @param draftsdir Directory where drafts should be written to
	  */
	OP_STATUS Init(message_gid_t id, const OpStringC& draftsdir);

	/** Add an attachment from a local file on the machine
	  * @param file_path Full path to file to add
	  */
	OP_STATUS AddFromFile(const OpStringC& file_path, const OpStringC& suggested_filename = OpStringC());

	/** Add an attachment that is currently in Opera's cache
	  * @param file_path Full path to file in cache
	  * @param suggested_name Name for file to use for display purposes
	  * @param url URL of file in cache, needed for making sure the attachment stays in cache while needed
	  * @param is_inlined - whether the attachment is inlined or not (inlined attachments are not encoded by the MessageEncoder)
	  */
	OP_STATUS AddFromCache(const OpStringC& file_path, const OpStringC& suggested_name, URL& url, BOOL is_inlined);

	/** Save the attachments held in this holder to a draft
	  * @param id M2 ID of message that this holder holds attachments for
	  */
	OP_STATUS Save(message_gid_t id);

	/** Remove an attachment with a certain ID
	  * @param id ID of attachment as seen from model
	  */
	OP_STATUS Remove(int id);

	/** Remove all attachments
	  */
	OP_STATUS RemoveAll();

	/** Encode these attachments so that they can be sent in a message
	  * @param encoder Encoder to use for attachments
	  * @param charset Charset to use when encoding headers for attachments
	  */
	OP_STATUS EncodeForSending(MessageEncoder& encoder, const OpStringC8& charset);

	/** Get an Attachment at a certain id
	  * @param id of the attachment to get
	  * @param context_id that the URL should use
	  * @param suggested_filename
	  * @param attachment_url
	  */
	OP_STATUS		GetAttachmentInfoById(int id, UINT32 context_id, OpString& suggested_filename, URL& attachment_url);

	/** Same as GetAttachmentInfoById, useful when you know the internal position in the model (eg. when you loop through all attachments)
	  */
	OP_STATUS		GetAttachmentInfoByPosition(int position, UINT32 context_id, OpString& suggested_filename, URL& attachment_url);

	/** Get the total count of attachments being held by the AttachmentHolder
	  */
	UINT32			GetCount() { return m_attachments.GetCount(); }

private:

	struct AttachmentInfo
	{
		int m_id;
		OpString m_original_filename;
		OpString m_copy_filename;
		OpString m_suggested_filename;
		OpString8 m_suggested_mime_type;

		AttachmentInfo() : m_id(0) {}
		uni_char* GetFullPath() { return m_copy_filename.HasContent() ? m_copy_filename.CStr() : m_original_filename.CStr(); }
	};

	OP_STATUS AddToModel(int id, const OpStringC& suggested_filename, OpFileLength size, uni_char* full_path);
	OP_STATUS CreateFileCopy(AttachmentInfo* info);
	OP_STATUS Remove(AttachmentInfo* info);

	SimpleTreeModel& m_model;
	OpAutoVector<AttachmentInfo> m_attachments;
	OpAutoVector<URL_InUse> m_cache_holder;
	OpString m_directory;
	message_gid_t m_id;
	int m_next_attachment_id;
};

#endif // ATTACHMENT_HOLDER_H
