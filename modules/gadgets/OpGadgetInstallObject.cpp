/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
*
* Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
*
* This file is part of the Opera web browser.	It may not be distributed
* under any circumstances.
*/

#include "core/pch.h"

#ifdef GADGETS_INSTALLER_SUPPORT

#include "modules/gadgets/OpGadgetManager.h"
#include "modules/gadgets/OpGadgetInstallObject.h"
#include "modules/util/filefun.h"

/***********************************************************************************
**
**	OpGadgetInstallObject
**
***********************************************************************************/

/*static*/ OP_STATUS
OpGadgetInstallObject::Make(OpGadgetInstallObject *&new_obj, GadgetClass type)
{
	new_obj = OP_NEW(OpGadgetInstallObject, (type));
	if (!new_obj)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError(new_obj->Construct()))
	{
		OP_DELETE(new_obj);
		new_obj = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

OpGadgetInstallObject::OpGadgetInstallObject(GadgetClass type)
	: m_is_installed(FALSE)
	, m_klass(NULL)
	, m_gadget(NULL)
	, m_type(type)
{
}

OP_STATUS
OpGadgetInstallObject::Construct()
{
	switch (m_type)
	{
	case GADGET_CLASS_UNITE:
#ifdef WEBSERVER_SUPPORT
		RETURN_IF_ERROR(m_filename.Set((UNI_L("Unite_Unnamed.ua"))));
		break;
#else // WEBSERVER_SUPPORT
		return OpStatus::ERR;
#endif // WEBSERVER_SUPPORT
	case GADGET_CLASS_EXTENSION:
#ifdef EXTENSION_SUPPORT
		RETURN_IF_ERROR(m_filename.Set((UNI_L("Extension_Unnamed.oex"))));
		break;
#else // EXTENSION_SUPPORT
		return OpStatus::ERR;
#endif // EXTENSION_SUPPORT
	case GADGET_CLASS_WIDGET:
		RETURN_IF_ERROR(m_filename.Set((UNI_L("Widget_Unnamed.wgt"))));
		break;
	default:
		OP_ASSERT(!"Unknown or unsupported widget type, should not happen");
		return OpStatus::ERR;
	}
	RETURN_IF_ERROR(CreateUniqueFilename(m_path, OPFILE_TEMP_FOLDER, m_filename));
	RETURN_IF_ERROR(m_file.Construct(m_path));

	return OpStatus::OK;
}

OpGadgetInstallObject::~OpGadgetInstallObject()
{
	if (!m_is_installed)
		OpStatus::Ignore(m_file.Delete());
	Out();
}

OP_STATUS
OpGadgetInstallObject::Open()
{
	OP_ASSERT(!m_file.IsOpen());
	return m_file.Open(OPFILE_WRITE);
}

OP_STATUS
OpGadgetInstallObject::AppendData(const ByteBuffer &data)
{
	OP_ASSERT(m_file.IsOpen() && m_file.IsWritable());
	unsigned chunks = data.GetChunkCount();
	for (unsigned i = 0; i < chunks; ++i)
	{
		unsigned nbytes;
		char *chunk = data.GetChunk(i, &nbytes);
		RETURN_IF_ERROR(m_file.Write(chunk, nbytes));
	}
	return OpStatus::OK;
}

OP_STATUS
OpGadgetInstallObject::Close()
{
	OP_ASSERT(m_file.IsOpen());
	return m_file.Close();
}

BOOL
OpGadgetInstallObject::IsOpen() const
{
	return m_file.IsOpen();
}

/*static*/
OP_STATUS
OpGadgetInstallObject::SanitizeFilename(OpString &filename, const uni_char *alternative_name)
{
	int length = filename.Length();
	uni_char *tmp = OP_NEWA(uni_char, length+1); // +1 for '\0'
	RETURN_OOM_IF_NULL(tmp);
	int j = 0;
	for (int i = 0; i < length; ++i)
	{
		uni_char c = filename[i];
		if ((c >= 'a' && c <= 'z') ||
			(c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9') ||
			c == '(' || c == ')' ||
			c == '_' || c == '-' || c == ' ' || c == '.'|| c == ',')
			tmp[j++] = c;
	}
	tmp[j] = '\0';
	if (j == 0 ||
		uni_str_eq(tmp, ".") ||
		uni_str_eq(tmp, ".."))
	{
		OP_DELETEA(tmp);
		RETURN_IF_ERROR(filename.Set(alternative_name));
	}
	else
	{
		filename.TakeOver(tmp);
	}
	return OpStatus::OK;
}

OP_STATUS
OpGadgetInstallObject::Update()
{
	// Get the real name of the widget and use that for a better filename
	URLContentType content_type;
	RETURN_IF_ERROR(GetUrlType(content_type));
	OpString unused;
	OpAutoPtr<OpGadgetClass> klass(g_gadget_manager->CreateClassWithPath(m_path, content_type, NULL, unused));
	RETURN_OOM_IF_NULL(klass.get());

	// Suggested filename is based on gadget name
	OpString new_filename, new_abs_path;
	RETURN_IF_ERROR(klass->GetGadgetName(new_filename));
	RETURN_IF_ERROR(SanitizeFilename(new_filename, UNI_L("Widget")));
	if (m_type == GADGET_CLASS_EXTENSION)
	{
		RETURN_IF_ERROR(new_filename.Append(".oex"));
	}
	else if (m_type == GADGET_CLASS_UNITE)
	{
		RETURN_IF_ERROR(new_filename.Append(".ua"));
	}
	else // if (m_type == GADGET_MAJOR_TYPE_WIDGET)
	{
		RETURN_IF_ERROR(new_filename.Insert(0, "Widget_"));
		RETURN_IF_ERROR(new_filename.Append(".wgt"));
	}
	klass.reset();

	// Correct filename to ensure a unique name
	RETURN_IF_ERROR(CreateUniqueFilename(new_abs_path, OPFILE_GADGET_FOLDER, new_filename));

	// Copy the file to the gadget folder and update file object
	OpFile new_file;
	RETURN_IF_ERROR(new_file.Construct(new_abs_path));
	OP_STATUS status = new_file.CopyContents(&m_file, TRUE);
	if (OpStatus::IsSuccess(status))
	{
		status = m_file.Delete();
		if (OpStatus::IsSuccess(status))
			status = m_file.Copy(&new_file);
	}
	if (OpStatus::IsError(status))
	{
		OpStatus::Ignore(new_file.Delete());
		return status;
	}

	RETURN_IF_ERROR(m_filename.Set(new_filename));
	RETURN_IF_ERROR(m_path.Set(new_abs_path));

	return OpStatus::OK;
}

BOOL
OpGadgetInstallObject::IsValidWidget()
{
	OP_ASSERT(!m_file.IsOpen());

	return g_gadget_manager->IsThisAGadgetPath(m_path);
}

OP_STATUS
OpGadgetInstallObject::GetUrlType(URLContentType &url_type) const
{
	return ToUrlType(url_type, m_type);
}

/*static*/
OP_STATUS
OpGadgetInstallObject::ToUrlType(URLContentType &url_type, GadgetClass type)
{
	switch (type)
	{
	case GADGET_CLASS_UNITE:
#ifdef WEBSERVER_SUPPORT
		url_type = URL_UNITESERVICE_INSTALL_CONTENT;
		break;
#else // WEBSERVER_SUPPORT
		return OpStatus::ERR;
#endif // WEBSERVER_SUPPORT
	case GADGET_CLASS_EXTENSION:
#ifdef EXTENSION_SUPPORT
		url_type = URL_EXTENSION_INSTALL_CONTENT;
		break;
#else // EXTENSION_SUPPORT
		return OpStatus::ERR;
#endif // EXTENSION_SUPPORT
	case GADGET_CLASS_WIDGET:
		url_type = URL_GADGET_INSTALL_CONTENT;
		break;
	default:
		OP_ASSERT(!"Unknown or unsupported widget type, should not happen");
		return OpStatus::ERR;
	}
	return OpStatus::OK;
}

#endif // GADGETS_INSTALLER_SUPPORT
