#ifndef MDE_MMAP_H
#define MDE_MMAP_H

#ifdef MDE_USE_MMAP

/** MDE mmap mapping from a file to a memory address. This interface must be 
 * implmented by the platform in order to use mmap support in libgogi. */
class MDE_MMapping
{
public:
	/** Create a mapping between a file and a memory address.
	 * @param m a pointer to store the mapping object in.
	 * @param file the file to map to an address.
	 * @returns OK on success, ERR_FILE_NOT_FOUND if there is not file at a given path or other error otherwise. */
	static OP_STATUS Create(MDE_MMapping** m, const uni_char* file);
	/** Destroy the mapping. This destructor must unmap the memory mapping
	 * for the file. */
	virtual ~MDE_MMapping(){}

	/** @returns the address of the mapping this object represents. */
	virtual void* GetAddress() = 0;
	/** @returns the size of the mapping this object represents. */
	virtual int GetSize() = 0;
};

#ifdef MDE_MMAP_MANAGER
struct MDE_MMAP_File
{
    MDE_MMAP_File *next;
    uni_char *filename;
    MDE_MMapping* filemap;
    int refs;

	MDE_MMAP_File() : next(NULL), filename(NULL), filemap(NULL), refs(0)
	{}
    ~MDE_MMAP_File()
	{
		OP_DELETE(filemap);
		::op_free(filename);
	}
};

class MDE_MMapManager
{
public:
	MDE_MMapManager();
	~MDE_MMapManager();

	OP_STATUS MapFile(MDE_MMAP_File** fmap, const uni_char* filename);
	void UnmapFile(MDE_MMAP_File* fmap);
private:
	struct MDE_MMAP_File *first_free_file;
	struct MDE_MMAP_File *first_mapped_file;
};
#endif // MDE_MMAP_MANAGER

#endif // MDE_USE_MMAP

#endif // MDE_MMAP_H

