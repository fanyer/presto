// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2010 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Øyvind Østlund
//

#include "core/pch.h"

#include "OperaInstallLog.h"

#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/xmlnames.h"

#include "platforms/windows/installer/RegistryKey.h"
#include "platforms/windows/windows_ui/res/#buildnr.rci"


//
// Class ShortCut
//

/* static */
const uni_char* ShortCut::TYPE[] =
			{
				// Array should be in sync with the enum DesktopShortcutInfo::Destination
				UNI_L("SC_DEST_NONE"),				// There is no support for shortcuts on this platform.
				UNI_L("SC_DEST_DESKTOP"),			// Desktop, available on Windows.
				UNI_L("SC_DEST_COMMON_DESKTOP"),	// Common desktop, available on Windows.
				UNI_L("SC_DEST_QUICK_LAUNCH"),		// Quick launch bar, available on Windows.
				UNI_L("SC_DEST_MENU"),				// Applications menu, available on Windows, UNIX.
				UNI_L("SC_DEST_COMMON_MENU"),		// Common applications menu, available on Windows.
				UNI_L("SC_DEST_TEMP"),				// Temp folder to create shortcut temporary.
			};

/* static */
BOOL ShortCut::IsValidDest(int dest)
{ 
	if (dest > DesktopShortcutInfo::SC_DEST_NONE && dest < DesktopShortcutInfo::SC_DESTINATION_TYPE_COUNT)
		return TRUE;

	OP_ASSERT(!"This is not a valid destination.");
	return FALSE; 
}


/* static */
int ShortCut::DestStr2DestNum(const uni_char* dest)
{
	if (dest)
	{
		int i;

		for (i = 0; i < DesktopShortcutInfo::SC_DESTINATION_TYPE_COUNT; i++)
		{
			if (uni_strcmp(dest, TYPE[i]) == 0)
				return i; 
		}
	}

	OP_ASSERT(!"This is an invalid type");
	return DesktopShortcutInfo::SC_DEST_NONE;
}

/* static */
const uni_char* ShortCut::DestNum2DestStr(int dest)
{
	if (IsValidDest(dest))
		return TYPE[dest];

	return TYPE[DesktopShortcutInfo::SC_DEST_NONE];
}


OP_STATUS ShortCut::SetPath(const uni_char* path)
{
	return m_path.Set(path);
}

OP_STATUS ShortCut::SetDestination(DesktopShortcutInfo::Destination destination)
{
	if (IsValidDest(destination))
	{
		m_destination = destination;
		return OpStatus::OK;
	}

	return OpStatus::ERR_OUT_OF_RANGE;
}

OP_STATUS ShortCut::SetDestination(const uni_char* destination)
{
	if (destination && *destination)
	{
		int dest = DestStr2DestNum(destination);
		
		if (!IsValidDest(dest))
			return OpStatus::ERR_OUT_OF_RANGE;

		m_destination = (DesktopShortcutInfo::Destination)dest;
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

//
// Class OperaInstallLog
//
OperaInstallLog::OperaInstallLog(): m_reg_index(0)
								   ,m_shortcut_index(0)
								   ,m_file_index(0)
{

}

OperaInstallLog::~OperaInstallLog()
{
	Close();
	
	m_registry.DeleteAll();
	m_shortcuts.DeleteAll();
	m_files.DeleteAll();
}

const OperaInstaller::Settings* OperaInstallLog::GetInstallMode() const
{
	return &m_mode;
}

const uni_char* OperaInstallLog::GetInstallPath() const
{
	return m_path.CStr();
}

const uni_char* OperaInstallLog::GetRegistryHive() const
{
	return m_hive.CStr();
}
	
const RegistryKey* OperaInstallLog::GetRegistryKey() const
{
	if (m_reg_index < m_registry.GetCount())
		return m_registry.Get(m_reg_index++);
	else
		return NULL;
}

const ShortCut* OperaInstallLog::GetShortcut() const
{
	if (m_shortcut_index < m_shortcuts.GetCount())
		return m_shortcuts.Get(m_shortcut_index++);
	else
		return NULL;
}

const uni_char* OperaInstallLog::GetFile() const
{
	if (m_file_index < m_files.GetCount())
		return m_files.Get(m_file_index++)->CStr();
	else
		return NULL;
}

void OperaInstallLog::SetInstallMode(OperaInstaller::Settings& install_mode)
{
	m_mode.all_users = install_mode.all_users;
	m_mode.copy_only = install_mode.copy_only;
	m_mode.desktop_shortcut = install_mode.desktop_shortcut;
	m_mode.launch_opera = install_mode.launch_opera;
	m_mode.quick_launch_shortcut = install_mode.quick_launch_shortcut;
	m_mode.set_default_browser = install_mode.set_default_browser;
	m_mode.single_profile = install_mode.single_profile;
	m_mode.start_menu_shortcut = install_mode.start_menu_shortcut;
	m_mode.pinned_shortcut = install_mode.pinned_shortcut;
}

OP_STATUS OperaInstallLog::SetInstallPath(const uni_char* install_path)
{
	return m_path.Set(install_path);
}

OP_STATUS OperaInstallLog::SetRegistryHive(const uni_char* reg_hive)
{
	return m_hive.Set(reg_hive);
}
	
OP_STATUS OperaInstallLog::SetRegistryKey(const RegistryKey& reg_key)
{
	OpAutoPtr<RegistryKey> reg(OP_NEW(RegistryKey, ()));
	RETURN_OOM_IF_NULL(reg.get());
	RETURN_IF_ERROR(reg->Copy(reg_key));
	RETURN_IF_ERROR(m_registry.Add(reg.get()));

	reg.release();
	return OpStatus::OK;
}

OP_STATUS OperaInstallLog::SetShortcut(const ShortCut& short_cut)
{
	OpAutoPtr<ShortCut> s (OP_NEW(ShortCut, ()));
	RETURN_OOM_IF_NULL(s.get());
	RETURN_IF_ERROR(s->SetPath(short_cut.GetPath()));
	RETURN_IF_ERROR(s->SetDestination(short_cut.GetDestination()));
	RETURN_IF_ERROR(m_shortcuts.Add(s.get()));

	s.release();
	return OpStatus::OK;
}

OP_STATUS OperaInstallLog::SetFile(const uni_char* file)
{
	OpString* f = OP_NEW(OpString, ());
	RETURN_OOM_IF_NULL(f);
	
	OP_STATUS status = f->Set(file);

	if (OpStatus::IsSuccess(status))
		status = m_files.Add(f);

	return status;
}

OP_STATUS OperaInstallLog::Parse()
{
	Open(OPFILE_READ | OPFILE_COMMIT);

	// Check if the file exists, return err if not.
	BOOL exists;
	RETURN_IF_ERROR(Exists(exists));
	
	if (!exists)
		return OpStatus::ERR_FILE_NOT_FOUND;

	const XMLExpandedName log_elm(UNI_L("install_log"));
	const XMLExpandedName path_elm(UNI_L("path"));
	const XMLExpandedName files_elm(UNI_L("files"));
	const XMLExpandedName registry_elm(UNI_L("registry"));
	const XMLExpandedName shortcut_elm(UNI_L("shortcut"));
	
	OP_STATUS status = OpStatus::OK;
	XMLFragment frag;
	RETURN_IF_ERROR(frag.Parse(this));
	RETURN_IF_ERROR(ParseLogElement(frag));
	
	while(OpStatus::IsSuccess(status) && frag.EnterAnyElement())
	{
		const XMLCompleteName& elm_name = frag.GetElementName();
	
		// if <install_log>
		if (elm_name == log_elm)
			status = ParseLogElement(frag);
			
		// if <path>
		else if (elm_name == path_elm)
			status = ParsePathElement(frag);
			
		// if <files>
		else if (elm_name == files_elm)
			status = ParseFilesElement(frag);
	
		// if <registry>
		else if (elm_name == registry_elm)
			status = ParseRegistryElement(frag);
			
		// if <shortcut>
		else if (elm_name == shortcut_elm)
			status = ParseShortcutElement(frag);
			
		// if unknown tag
		else
		{
			// If this assert fires, either the log format is wrong
			// or there is a new element that is not implemented yet
			OP_ASSERT(!"This element is not known to us");
		}

	}

	// </install_log>
	frag.LeaveElement();

	if (OpStatus::IsSuccess(status))
		status = Close();
	else
		OpStatus::Ignore(Close());

	return status;
}

OP_STATUS OperaInstallLog::ParseLogElement(XMLFragment& frag)
{
	const XMLExpandedName copy_only_att(UNI_L("copy_only"));
	const XMLExpandedName all_users_att(UNI_L("all_users"));
	const uni_char* attribute;
	
	// copy_only=
	attribute = frag.GetAttribute(copy_only_att);
	
	if (attribute)
		m_mode.copy_only = uni_stricmp(attribute, UNI_L("true")) == 0 ? TRUE : FALSE;
		
	// all_users=
	attribute = frag.GetAttribute(all_users_att);
	
	if (attribute)		
		m_mode.all_users = uni_stricmp(attribute, UNI_L("true")) == 0 ? TRUE : FALSE;
	
	return OpStatus::OK;
}

OP_STATUS OperaInstallLog::ParsePathElement(XMLFragment& frag)
{
	RETURN_IF_ERROR(m_path.Set(frag.GetText()));
	frag.LeaveElement();
	
	return OpStatus::OK;
}

OP_STATUS OperaInstallLog::ParseFilesElement(XMLFragment& frag)
{
	const XMLExpandedName file_elm(UNI_L("file"));
		
	while(frag.EnterAnyElement())
	{
		const XMLCompleteName element = frag.GetElementName();
		const uni_char* text = frag.GetText();
		
		if (element == file_elm)
		{
			OpAutoPtr<OpString> path(OP_NEW(OpString, ()));
			RETURN_OOM_IF_NULL(path.get());
			
			RETURN_IF_ERROR(path->Set(text));
			RETURN_IF_ERROR(m_files.Add(path.get()));
			path.release();
		}
		else
		{
			// If this assert fires, either the log format is wrong
			// or there is a new element that is not implemented yet
			OP_ASSERT(!"This element is not known to us");
		}
		
		frag.LeaveElement();
	}
	
	frag.LeaveElement();
	
	return OpStatus::OK;
}
	
OP_STATUS OperaInstallLog::ParseRegistryElement(XMLFragment& frag)
{

	const XMLExpandedName hive_att(UNI_L("hive"));
	const XMLCompleteName key_elm(UNI_L("key"));
	const XMLCompleteName value_elm(UNI_L("value"));
	const XMLCompleteName path_att(UNI_L("path"));
	const XMLCompleteName clean_att(UNI_L("clean"));
	const XMLCompleteName name_att(UNI_L("name"));
	const XMLCompleteName type_att(UNI_L("type"));
	
	RegistryKey reg_key;
	RETURN_IF_ERROR(m_hive.Set(frag.GetAttribute(hive_att)));
		
	// <key>
	while (frag.EnterElement(key_elm))
	{

		// path=
		RETURN_IF_ERROR(reg_key.SetKeyPath(frag.GetAttribute(path_att)));
		
		// clean=
		RETURN_IF_ERROR(reg_key.SetCleanKey(frag.GetAttribute(clean_att)));

		// Special case where there is no value element.
		if (!frag.HasMoreElements())
		{
			RETURN_IF_ERROR(reg_key.SetValueData(NULL));
			RETURN_IF_ERROR(reg_key.SetValueName(NULL));
			reg_key.SetValueType(REG_SZ);

			// Add the new registry key to the list		
			OpAutoPtr<RegistryKey> reg(OP_NEW(RegistryKey, ()));
			RETURN_OOM_IF_NULL(reg.get());
			RETURN_IF_ERROR(reg->Copy(reg_key));
			RETURN_IF_ERROR(m_registry.Add(reg.get()));
			reg.release();
		}
		else
		{		
			while (frag.EnterElement(value_elm))
			{
				// name=
				RETURN_IF_ERROR(reg_key.SetValueName(frag.GetAttribute(name_att)));
				
				// type=
				RETURN_IF_ERROR(reg_key.SetValueType(frag.GetAttribute(type_att)));
				
				// value=
				RETURN_IF_ERROR(reg_key.SetValueData(frag.GetText()));

				// Add the new registry key to the list		
				OpAutoPtr<RegistryKey> reg(OP_NEW(RegistryKey, ()));
				RETURN_OOM_IF_NULL(reg.get());
				RETURN_IF_ERROR(reg->Copy(reg_key));
				RETURN_IF_ERROR(m_registry.Add(reg.get()));
				reg.release();
				frag.LeaveElement();

			}
			
		}	
		frag.LeaveElement();
	} // (while)
	
	frag.LeaveElement();
	
	return OpStatus::OK;
}
	
OP_STATUS OperaInstallLog::ParseShortcutElement(XMLFragment& frag)
{
	
	const XMLExpandedName shortcut_elm(UNI_L("shortcut"));
	const XMLExpandedName type_elm(UNI_L("type"));
		
	do
	{
		const XMLCompleteName element = frag.GetElementName();
		const uni_char* text = frag.GetText();
		
		if (element == shortcut_elm)
		{
			// Copy the ShortCut to our vector.
			OpAutoPtr<ShortCut> shortcut(OP_NEW(ShortCut, ()));
			RETURN_OOM_IF_NULL(shortcut.get());	
			RETURN_IF_ERROR(shortcut->SetPath(text));
			RETURN_IF_ERROR(m_shortcuts.Add(shortcut.get()));
			
			// Try to get the type attribute if it is there
			const uni_char* type = frag.GetAttribute(type_elm);
			if (type)
				shortcut->SetDestination(type);

			shortcut.release();
		}
		else
		{
			// If this assert fires, either the log format is wrong
			// or there is a new element that is not implemented yet
			OP_ASSERT(!"This element is not known to us");
		}
		
		frag.LeaveElement();
		
	}while (frag.EnterAnyElement());

	return OpStatus::OK;
}

OP_STATUS OperaInstallLog::SaveToFile()
{

	const XMLExpandedName log_elm(UNI_L("install_log"));
	const XMLExpandedName path_elm(UNI_L("path"));
	const XMLExpandedName copy_only_att(UNI_L("copy_only"));
	const XMLExpandedName all_users_att(UNI_L("all_users"));
	const XMLExpandedName version_att(UNI_L("version"));
	const XMLExpandedName pinned_to_taskbar_att(UNI_L("pinned"));

	XMLFragment frag;
	
	// <install_log>
	RETURN_IF_ERROR(frag.OpenElement(log_elm));
	RETURN_IF_ERROR(frag.SetAttribute(version_att, VER_NUM_STR_UNI UNI_L(".") UNI_L(VER_BUILD_NUMBER_STR)));
	RETURN_IF_ERROR(frag.SetAttribute(copy_only_att, m_mode.copy_only == TRUE ? UNI_L("True") : UNI_L("False")));
	RETURN_IF_ERROR(frag.SetAttribute(all_users_att, m_mode.all_users == TRUE ? UNI_L("True") : UNI_L("False")));
	RETURN_IF_ERROR(frag.SetAttribute(pinned_to_taskbar_att, m_mode.pinned_shortcut == TRUE ? UNI_L("True") : UNI_L("False")));
	
	// <path>
	RETURN_IF_ERROR(frag.OpenElement(path_elm));
	RETURN_IF_ERROR(frag.AddText(m_path.CStr()));
	frag.CloseElement();
	
	// <files>
	if (m_files.GetCount() > 0)
		RETURN_IF_ERROR(SaveFileList(frag));
	
	// We only save registry keys, and shortcuts
	// if NOT copy only is chosen.
	if (!m_mode.copy_only)
	{
		// <registry>
		if (m_registry.GetCount() > 0)
			RETURN_IF_ERROR(SaveRegistryList(frag));

		// <shorcut>
		if (m_shortcuts.GetCount() > 0)
			RETURN_IF_ERROR(SaveShortcutList(frag));

	}
	
	ByteBuffer buffer;
	unsigned int bytes = 0;
	UINT32 chunk;
	
	RETURN_IF_ERROR(Open(OPFILE_WRITE));
	
	RETURN_IF_ERROR(frag.GetEncodedXML(buffer, TRUE, "utf-8", TRUE));
	
	for(chunk = 0; chunk < buffer.GetChunkCount(); chunk++)
	{
		char *chunk_ptr = buffer.GetChunk(chunk, &bytes);
		if(chunk_ptr)
			RETURN_IF_ERROR(Write(chunk_ptr, bytes));
	}
	
	RETURN_IF_ERROR(Close());
	
	return OpStatus::OK;
}

/*
  Example:

    <file>
      program\netscape.exe
    </file>
*/
OP_STATUS OperaInstallLog::SaveFileList(XMLFragment& frag)
{
	const XMLExpandedName files_elm(UNI_L("files"));
	const XMLExpandedName file_elm(UNI_L("file"));

	// <files>
	RETURN_IF_ERROR(frag.OpenElement(files_elm));
	
	// <file>
	UINT i;
	UINT count = m_files.GetCount();
	for (i = 0; i < count; i++)
	{
		RETURN_IF_ERROR(frag.OpenElement(file_elm));
		RETURN_IF_ERROR(frag.AddText(m_files.Get(i)->CStr()));
		frag.CloseElement();
	}
	
	// </files>
	frag.CloseElement();

	return OpStatus::OK;

}

/*
  Example:

    <key path="Software\Classes\Opera.Protocol" clean="1">
      <value name="EditFlags" type="REG_DWORD">
        00000002
	  </value>
	</key>
*/
OP_STATUS OperaInstallLog::SaveRegistryList(XMLFragment& frag)
{

	const XMLExpandedName registry_elm(UNI_L("registry"));
	const XMLExpandedName hive_att(UNI_L("hive"));
	const XMLCompleteName key_elm(UNI_L("key"));
	const XMLCompleteName value_elm(UNI_L("value"));
	const XMLCompleteName clean_att(UNI_L("clean"));
	const XMLCompleteName path_att(UNI_L("path"));
	const XMLCompleteName name_att(UNI_L("name"));
	const XMLCompleteName type_att(UNI_L("type"));

	// <registry>
	RETURN_IF_ERROR(frag.OpenElement(registry_elm));
	RETURN_IF_ERROR(frag.SetAttribute(hive_att, m_hive.CStr()));

	// <key>
	UINT32 i = 0;
	while (m_registry.Get(i))
	{
		RegistryKey* reg = m_registry.Get(i);

		RETURN_IF_ERROR(frag.OpenElement(key_elm));
		RETURN_IF_ERROR(frag.SetAttribute(path_att, reg->GetKeyPath()));
		OpString clean;
		RETURN_IF_ERROR(clean.AppendFormat(UNI_L("%u"), reg->GetCleanKey()));
		RETURN_IF_ERROR(frag.SetAttribute(clean_att, clean.CStr()));
		
		// <value>
		if (reg->HasValue())
		{
			do
			{
				OpString type;
				reg->GetValueType(type);
				RETURN_IF_ERROR(frag.OpenElement(value_elm));
				if (reg->GetValueName())
				{
					RETURN_IF_ERROR(frag.SetAttribute(name_att, reg->GetValueName()));
					RETURN_IF_ERROR(frag.SetAttribute(type_att, type.CStr()));
				}
				RETURN_IF_ERROR(frag.AddText(reg->GetValueData()));

				// </value>
				frag.CloseElement();

				reg = m_registry.Get(++i);
			}while (reg && uni_stricmp(m_registry.Get(i-1)->GetKeyPath(), reg->GetKeyPath()) == 0);
		}
		else
			++i;
	
		// </key>
		frag.CloseElement();	
	}
	
	// </registry>
	frag.CloseElement();

	return OpStatus::OK;

}

/*
  Example:

	<shortcut type="SC_DEST_QUICK_LAUNCH">
		%ALLUSERSPROFILE%\Desktop\Opera.lnkSC_DEST_NONE
	</shortcut>
*/
OP_STATUS OperaInstallLog::SaveShortcutList(XMLFragment& frag)
{

	const XMLExpandedName shortcut_elm(UNI_L("shortcut"));
	const XMLExpandedName type_elm(UNI_L("type"));

	UINT i;
	UINT32 count = m_shortcuts.GetCount();
	for (i = 0; i < count; i++)
	{
		// <shorcut>
		RETURN_IF_ERROR(frag.OpenElement(shortcut_elm));

		// type=
		DesktopShortcutInfo::Destination dest = m_shortcuts.Get(i)->GetDestination();

		if (dest != DesktopShortcutInfo::SC_DEST_NONE)
			RETURN_IF_ERROR(frag.SetAttribute(type_elm, ShortCut::TYPE[dest]));

		// "text" (path)
		RETURN_IF_ERROR(frag.AddText(m_shortcuts.Get(i)->GetPath()));

		// </shortcut>
		frag.CloseElement();
	}

	return OpStatus::OK;

}