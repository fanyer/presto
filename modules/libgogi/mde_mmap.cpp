#include "core/pch.h"

#ifdef MDE_MMAP_MANAGER

#include "modules/libgogi/mde_mmap.h"

MDE_MMapManager::MDE_MMapManager() : first_free_file(NULL), first_mapped_file(NULL)
{}

MDE_MMapManager::~MDE_MMapManager()
{
	while( first_mapped_file )
	{
		MDE_MMAP_File *x = first_mapped_file;
		first_mapped_file = x->next;
		OP_DELETE(x);
	}
	while( first_free_file )
	{
		MDE_MMAP_File *x = first_free_file;
		first_free_file = x->next;
		OP_DELETE(x);
	}
}

OP_STATUS MDE_MMapManager::MapFile(MDE_MMAP_File** fmap, const uni_char* filename)
{
	MDE_MMAP_File *x = first_mapped_file;

	while(x && uni_strcmp(filename, x->filename))
		x = x->next;

	if( !x )
	{
		if( first_free_file )
		{
			x = first_free_file;
			first_free_file = x->next;
		}
		else
		{
			x = OP_NEW(MDE_MMAP_File, ());
			if( !x )
				return OpStatus::ERR;
		}
		x->next = first_mapped_file;
		first_mapped_file = x;
		x->filename = uni_strdup( filename );
		x->refs=0;

		// Make sure it is set to null in case create fails and does not set it to null
		x->filemap = NULL;
		OP_STATUS err = MDE_MMapping::Create(&(x->filemap), filename);
		if (!x->filename || OpStatus::IsError(err))
		{
			if (!x->filename)
				err = OpStatus::ERR_NO_MEMORY;
			UnmapFile(x);
			return err;
		}
	}
	x->refs++;

	*fmap = x;

	return OpStatus::OK;
}

void MDE_MMapManager::UnmapFile(MDE_MMAP_File* fmap)
{
	MDE_MMAP_File *t = first_mapped_file, *p=NULL;
	while( t && t != fmap )
	{
		p = t;
		t = t->next;
	}
	if( !t )
		return; // Already free.
	if( --fmap->refs <  1 )
	{
		if( !p )
			first_mapped_file = fmap->next;
		else
			p->next = fmap->next;
		fmap->next = first_free_file;
		first_free_file = fmap;
		
		::op_free(fmap->filename);
		OP_DELETE(fmap->filemap);
		fmap->filemap = NULL;
		fmap->filename = NULL;
	}
}

#endif // MDE_MMAP_MANAGER
