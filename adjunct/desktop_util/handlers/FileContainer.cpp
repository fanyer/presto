// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Patricia Aas
//

#include "core/pch.h"
#include "adjunct/desktop_util/handlers/FileContainer.h"
#include "modules/pi/OpSystemInfo.h"

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
FileContainer::FileContainer(OpString * file_name,
							 OpString * file_path,
							 OpString * file_ext,
							 OpBitmap * file_thumbnail)
    : DownloadManager::Container(file_name,
								 file_thumbnail)
{
    Init(file_path, file_ext);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
FileContainer::FileContainer(OpString * file_name,
							 OpString * file_path,
							 OpString * file_ext,
							 Image    &file_thumbnail)
    : DownloadManager::Container(file_name,
								 file_thumbnail)
{
    Init(file_path, file_ext);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void FileContainer::Init(OpString * file_name,
						 OpString * file_path,
						 OpString * file_ext,
						 OpBitmap * file_thumbnail)
{
	InitContainer(file_name, file_thumbnail); //superclass init
	Init(file_path, file_ext);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void FileContainer::Init(OpString * file_name,
						 OpString * file_path,
						 OpString * file_ext,
						 Image    &file_thumbnail)
{
	InitContainer(file_name, file_thumbnail); //superclass init
	Init(file_path, file_ext);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void FileContainer::Empty()
{
	EmptyContainer();
	m_file_path_name.Empty();
	m_file_extension.Empty();
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void FileContainer::Init(OpString * file_path,
						 OpString * file_ext)
{
    OpString name;
    name.Set(GetName().CStr());
    m_executable = g_op_system_info->GetIsExecutable(&name);

    if(file_path)
	{
		m_file_path_name.Set(file_path->CStr());
	}
    
    if(file_ext)
	{
		m_file_extension.Set(file_ext->CStr());
	}
}
