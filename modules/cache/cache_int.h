/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#ifndef CACHE_INTERNAL_H_
#define CACHE_INTERNAL_H_

enum cache_operation_flags {
	COFLAG_PREFIX_MATCH=1,
	COFLAG_INVALIDATE=2 ///< Invalidate cache objects - don't delete them
};

#if defined (DISK_CACHE_SUPPORT) || defined(CACHE_PURGE)

class FileNameElement : public HashedListElement<FileNameElement>
{
private:

	OpStringS		filename;
	OpString8		linkid;
	OpStringS		pathfilename;
	OpFileLength	file_size;
	OpFileFolder	folder;	// Relative folder

public:

					FileNameElement();
	OP_STATUS		Init(const OpStringC &name, const OpStringC &pname, OpFileFolder file_folder);
	virtual			~FileNameElement(){}

	const OpStringC &PathFileName(){return pathfilename;}

	const OpStringC &FileName(){return filename;}
	const OpFileFolder &Folder(){return folder;}

	void			SetFileSize(OpFileLength len){file_size = len;}
	OpFileLength	GetFileSize(){ return file_size;}

    virtual	const char*
					LinkId() { return linkid.CStr(); }
};

class FileName_Store : public LinkObjectStore
{
#ifdef DEBUG_ENABLE_OPASSERT
	BOOL is_initialized;
#endif

public:
	FileName_Store(unsigned int size) : LinkObjectStore(size, op_strcmp)
	{
#ifdef DEBUG_ENABLE_OPASSERT
		is_initialized = FALSE;
#endif
	}

	virtual				~FileName_Store()
	{
		FileNameElement* file_name = (FileNameElement*) GetFirstLinkObject();
		while(file_name)
		{
			RemoveLinkObject(file_name);
			OP_DELETE(file_name);
			file_name = (FileNameElement *) GetNextLinkObject();
		}
	}

	/**
	 * Second phase constructor. This method must be called prior to using the object,
	 * unless it was created using the Create() method.
	 */
	OP_STATUS           Construct()
	{
		OP_STATUS op_err = LinkObjectStore::Construct();
#ifdef DEBUG_ENABLE_OPASSERT
		is_initialized = OpStatus::IsSuccess(op_err);
#endif
		return op_err;
	}

	/**
	 * Creates and initializes a FileName_Store object.
	 * @param filename_name Set to the created object if successful and to NULL otherwise,
	 *                      DON'T use this as a way to check for errors, check the
	 *                      return value instead!
	 * @param size
	 * @return OP_STATUS - Always check this.
	 */
	static OP_STATUS    Create(FileName_Store **filename_store,
							   unsigned int size);

	FileNameElement*	GetFirstFileName() { OP_ASSERT(is_initialized); return (FileNameElement*) GetFirstLinkObject(); }
	FileNameElement*	GetNextFileName() { OP_ASSERT(is_initialized); return (FileNameElement*) GetNextLinkObject(); }

	void				RemoveFileName(FileNameElement* fname) { OP_ASSERT(is_initialized); RemoveLinkObject(fname); }
	void				AddFileName(FileNameElement* fname) { OP_ASSERT(is_initialized); AddLinkObject(fname); }

	// Was GetFileName(). I don't want this function to be able to create a new object, it just create confusion, and it is not used
	FileNameElement*	RetrieveFileName(const OpStringC &name,const OpStringC &pname/*, BOOL create = TRUE*/);
};
#endif // defined (DISK_CACHE_SUPPORT) || defined(CACHE_PURGE)

#endif // CACHE_INTERNAL_H_
