/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/extensions/ExtensionUtils.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/desktop_util/file_utils/FileUtils.h"

#include "modules/display/pixelscaler.h"
#include "modules/gadgets/OpGadget.h"
#include "modules/gadgets/OpGadgetManager.h"
#include "modules/util/path.h"
#include "modules/util/zipload.h"
#include "modules/skin/OpSkinElement.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/libgogi/pi_impl/mde_bitmap_window.h"

static OpString GetLocaleString(Str::LocaleString id)
{
	OpString string;
	OpStatus::Ignore(g_languageManager->GetString(id, string));
    
	return string;
}

OP_STATUS ExtensionUtils::IsExtensionPath(const OpStringC& some_path, 
		BOOL& is_extension_path)
{
	is_extension_path = FALSE;

	OpString candidate_path;
	RETURN_IF_ERROR(candidate_path.Set(some_path));
	if (OpStringC(GADGET_CONFIGURATION_FILE) == OpPathGetFileName(candidate_path))
	{
		OpPathRemoveFileName(candidate_path);
	}
	is_extension_path = g_gadget_manager->IsThisAnExtensionGadgetPath(candidate_path);
	
	return OpStatus::OK;
}

OP_STATUS ExtensionUtils::IsDevModeExtension(OpGadgetClass& gclass, BOOL& in_dev_mode)
{
	in_dev_mode = FALSE;

	OpString gadget_path;
	RETURN_IF_ERROR(gclass.GetGadgetPath(gadget_path));
	RETURN_IF_ERROR(FileUtils::IsDirectory(gadget_path, in_dev_mode));

	return OpStatus::OK;
}


OP_STATUS ExtensionUtils::IsDevModeExtension(const OpStringC& wuid, BOOL& in_dev_mode)
{
	in_dev_mode = FALSE;
	OpGadget* gadget = g_gadget_manager->FindGadget(wuid);
	return gadget ? IsDevModeExtension(*gadget->GetClass(), in_dev_mode) : OpStatus::ERR;
}

OP_STATUS ExtensionUtils::FindNormalExtension(const OpStringC& gid, OpGadget** gadget, UINT& starting_pos)
{
	if (gadget)
		*gadget = NULL;

	for (; starting_pos < g_gadget_manager->NumGadgets(); starting_pos++)
	{
		OpGadget* gadget_tmp = g_gadget_manager->GetGadget(starting_pos);
		if (gadget_tmp->IsExtension())
		{
			BOOL in_dev_mode = FALSE;
			RETURN_IF_ERROR(IsDevModeExtension(*gadget_tmp->GetClass(), in_dev_mode));
			if (!in_dev_mode && gid == gadget_tmp->GetGadgetId())
			{
				if (gadget)
					*gadget = gadget_tmp;
				break;
			}
		}
	}
	starting_pos++;

	return OpStatus::OK;
}

OP_STATUS ExtensionUtils::IsExtensionInstalled(const OpStringC& path, BOOL &result)
{
	OpGadgetClass* gclass = NULL;
	// g_gadget_manager->CreateGadgetClass returns error if file given in parameter path is not a valid widget
	if (OpStatus::IsError(g_gadget_manager->CreateGadgetClass(&gclass, path, URL_EXTENSION_INSTALL_CONTENT)))
	{
		result = FALSE;
		return OpStatus::OK;
	}
	OpStringC widget_id(gclass->GetAttribute(WIDGET_ATTR_ID));
	UINT starting_pos = 0;
	OpGadget* gadget = NULL;
	RETURN_IF_ERROR(ExtensionUtils::FindNormalExtension(widget_id, &gadget, starting_pos));
	result = gadget != NULL;
	return OpStatus::OK;
}

OP_STATUS ExtensionUtils::IsExtensionInPackage(const OpStringC& path, BOOL &result)
{
	OpFile file;
	RETURN_IF_ERROR(file.Construct(path.CStr()));
	OpString package_ext_path;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_EXTENSION_CUSTOM_PACKAGE_FOLDER, package_ext_path));
	RETURN_IF_ERROR(package_ext_path.Append(file.GetName()));
	OpFile package_file;
	RETURN_IF_ERROR(file.Construct(package_ext_path.CStr()));
	RETURN_IF_ERROR(file.Exists(result));
	return OpStatus::OK;
}

OP_STATUS ExtensionUtils::HasExtensionUserJSIncludes(OpGadgetClass* gclass,
		BOOL& has_userjs_includes)
{
	has_userjs_includes = FALSE;

	OpFolderLister* lister = NULL;
	RETURN_IF_ERROR(OpZipFolderLister::Create(&lister));
	OpAutoPtr<OpFolderLister> lister_anchor(lister);

	OpString includes_path; 
	RETURN_IF_ERROR(
			OpPathDirFileCombine(includes_path, OpStringC(gclass->GetGadgetPath()),
				OpStringC(UNI_L("includes"))));

	if (OpStatus::IsError(lister->Construct(includes_path, UNI_L("*.js")))) {
		// probably not a zip but an unpacked extension
		OpFolderLister::Create(&lister);
		lister_anchor.reset(lister);
		RETURN_IF_ERROR(lister->Construct(includes_path, UNI_L("*.js")));
	}
	has_userjs_includes = lister->Next();

	return OpStatus::OK;
}

OP_STATUS ExtensionUtils::GetGenericExtensionIcon(Image& icon, unsigned int size)
{
	OpSkinElement* element = NULL;

	if (size <= 16)
	{
		element = g_skin_manager->GetSkinElement("Extension");
	}
	else
	{
		OpString8 el_name;
		RETURN_IF_ERROR(el_name.AppendFormat("Extension %d", size));
		element = g_skin_manager->GetSkinElement(el_name.CStr());
	}

	if (element)
	{
		icon = element->GetImage(SKINSTATE_FOCUSED);
		return !icon.IsEmpty() ? OpStatus::OK : OpStatus::ERR;
	}
	else
	{
		return OpStatus::ERR;
	}
}

ExtensionUtils::AccessWindowType ExtensionUtils::GetAccessWindowType(OpGadgetClass *gclass)
{
	BOOL has_userjs_includes = FALSE;
	RETURN_VALUE_IF_ERROR(ExtensionUtils::HasExtensionUserJSIncludes(gclass, has_userjs_includes), TYPE_SIMPLE);

	return has_userjs_includes ? TYPE_CHECKBOXES : TYPE_SIMPLE;
}

OP_STATUS ExtensionUtils::GetAccessLevelString(OpGadgetClass *gclass,
                                               OpString& access_list)
{
    BOOL has_userjs_includes;
    BOOL has_access_list;
    
    return ExtensionUtils::GetAccessLevel(gclass, has_access_list, access_list,
                                          has_userjs_includes);
}

OP_STATUS ExtensionUtils::GetAccessLevel(OpGadgetClass *gclass, BOOL& has_access_list, 
                                         OpString& access_list, BOOL& has_userjs_includes)
{

	has_access_list = FALSE;
	OpGadgetAccess* access = gclass->GetFirstAccess();
		
	if (access && access->Suc() == NULL)
	{
		has_access_list = TRUE;
		if (uni_strncmp(access->Name(), UNI_L("*"), 1) == 0)
		{
			access_list.Append(GetLocaleString(Str::D_INSTALL_EXTENSION_ACCESS_WEBPAGES).CStr());
		}
		else
		{
			access_list.AppendFormat(GetLocaleString(Str::D_INSTALL_EXTENSION_ACCESS_SINGLE_SITE).CStr(), access->Name());
		}
	}
	else 
	{
		if (!access)
		{
			access_list.Append(GetLocaleString(Str::D_INSTALL_EXTENSION_ACCESS_NONE).CStr());
		}
		else 
		{
			has_access_list = TRUE;
			access_list.Append(GetLocaleString(Str::D_INSTALL_EXTENSION_ACCESS_WEBPAGES).CStr());
		}		
	}
	
	has_userjs_includes = FALSE;
	RETURN_IF_ERROR(ExtensionUtils::HasExtensionUserJSIncludes(gclass, has_userjs_includes));

	return OpStatus::OK;    
}

bool ExtensionUtils::RequestsSpeedDialWindow(const OpStringC& wuid)
{
	OpGadget* gadget = g_gadget_manager->FindGadget(wuid);
	if (!gadget)
		return false;

	return RequestsSpeedDialWindow(*gadget->GetClass());
}

OP_STATUS ExtensionUtils::CreateExtensionClass(const OpStringC& path,OpGadgetClass** extension_class)
{
	RETURN_VALUE_IF_NULL(extension_class,OpStatus::ERR);
	OpFile upd_file;
	RETURN_IF_ERROR(upd_file.Construct(path));
	BOOL exists;
	RETURN_IF_ERROR(upd_file.Exists(exists));
	if (!exists)
	  return OpStatus::ERR;

	OpString error_reason;	
	*extension_class = g_gadget_manager->CreateClassWithPath(path.CStr(), URL_EXTENSION_INSTALL_CONTENT, NULL, error_reason);
	RETURN_VALUE_IF_NULL(*extension_class, OpStatus::ERR);

	return OpStatus::OK;
}

bool ExtensionUtils::RequestsSpeedDialWindow(const OpGadgetClass& gadget_class)
{
	OpGadgetClass& mutable_class(const_cast<OpGadgetClass&>(gadget_class));
	return mutable_class.IsExtension()
			&& mutable_class.IsFeatureRequested("opera:speeddial")
			&& mutable_class.GetDefaultMode() == WINDOW_VIEW_MODE_MINIMIZED;
}

BitmapOpWindow* ExtensionUtils::GetExtensionBitmapWindow(const OpStringC& wuid)
{
	if (!RequestsSpeedDialWindow(wuid))
		return NULL;

	OpGadget* gadget = g_gadget_manager->FindGadget(wuid);
	return gadget && gadget->GetWindow()
			? static_cast<BitmapOpWindow*>(gadget->GetWindow()->GetOpWindow())
			: NULL;
}

bool ExtensionUtils::IsExtensionRunning(const OpStringC& wuid)
{
	OpGadget* gadget = g_gadget_manager->FindGadget(wuid);
	return gadget
			&& gadget->IsExtension()
			&& gadget->IsRunning();
}

OP_STATUS ExtensionUtils::GetExtensionIcon(Image& img,
						   const OpGadgetClass* gadget_class,
						   UINT32 desired_width,
						   UINT32 desired_height)
{
	FallbackExtensionIconProvider fallback_provider(desired_width);
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	// try to scale size
	INT32 scale = static_cast<DesktopOpSystemInfo*>(g_op_system_info)->GetMaximumHiresScaleFactor();
	PixelScaler scaler(scale);
	desired_width = TO_DEVICE_PIXEL(scaler, desired_width);
	desired_height = TO_DEVICE_PIXEL(scaler, desired_height);
#endif
	return GadgetUtils::GetGadgetIcon(img, gadget_class, desired_width, desired_height, 0, 0, TRUE, FALSE, fallback_provider);
}