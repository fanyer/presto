// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Patricia Aas
//

#ifndef __FILE_CONTAINER_H__
#define __FILE_CONTAINER_H__

#include "adjunct/desktop_util/handlers/DownloadManager.h"

class FileContainer : 
	public DownloadManager::Container
{
public:

//  -------------------------
//  Public member functions:
//  -------------------------

	FileContainer()	: DownloadManager::Container(), m_executable(TRUE) {}

    FileContainer(OpString * file_name,
				  OpString * file_path,
				  OpString * file_ext,
				  OpBitmap * file_thumbnail);
    
    FileContainer(OpString * file_name,
				  OpString * file_path,
				  OpString * file_ext,
				  Image    &file_thumbnail);

	void Init(OpString * file_name,
			  OpString * file_path,
			  OpString * file_ext,
			  OpBitmap * file_thumbnail);

    void Init(OpString * file_name,
			  OpString * file_path,
			  OpString * file_ext,
			  Image    &file_thumbnail);

	void Empty();

    BOOL IsExecutable() { return m_executable; }
    
    const OpStringC& GetFilePathName() { return m_file_path_name; }
    const OpStringC& GetExtension()    { return m_file_extension; }

	virtual ContainerType GetType() { return CONTAINER_TYPE_FILE; }

private:

//  ------------------------
//  Private member functions:
//  -------------------------

    void Init(OpString * file_path,
			  OpString * file_ext);

//  -------------------------
//  Private member variables:
//  -------------------------

    BOOL      m_executable;
    OpString  m_file_extension;
    OpString  m_file_path_name;
};

#endif //__FILE_CONTAINER_H__
