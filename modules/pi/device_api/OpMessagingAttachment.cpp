/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Tomasz Jamroszczak tjamroszczak@opera.com
 *
 */

#include <core/pch.h>

#ifdef PI_MESSAGING

#include "modules/pi/device_api/OpMessaging.h"


OP_STATUS
OpMessaging::Attachment::CopyTo(OpMessaging::Attachment** new_attachment)
{
	OP_ASSERT(new_attachment);
	OpMessaging::Attachment* copy = OP_NEW(OpMessaging::Attachment, ());
	RETURN_OOM_IF_NULL(copy);
	OpAutoPtr<OpMessaging::Attachment> ap_attachment(copy);

	RETURN_IF_ERROR(copy->m_full_filename.Set(m_full_filename));
	RETURN_IF_ERROR(copy->m_id.Set(m_id));
	copy->m_inside_message = m_inside_message;
	RETURN_IF_ERROR(copy->m_mimetype.Set(m_mimetype));
	copy->m_size = m_size;
	RETURN_IF_ERROR(copy->m_suggested_name.Set(m_suggested_name));

	ap_attachment.release();
	*new_attachment = copy;
	return OpStatus::OK;
}

#endif // PI_MESSAGING

