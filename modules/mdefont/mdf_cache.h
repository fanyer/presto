/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */

#ifndef MDF_CACHE_H
#define MDF_CACHE_H

struct MDE_FONT;

#ifdef MDF_FONT_FACE_CACHE
/** An entry in the font face cache. Extend this class to included the state
 * needed to keep track of a font face. */
class MDF_FontFace
{
public:
	virtual ~MDF_FontFace(){}

	/** Compare this font face to the given name and index.
	 * @return zero if the name and index does not identify this face,
	 * non-zero otherwise. */
	virtual int IsEqual(const uni_char* font_name, int index) const = 0;

	unsigned int m_last_used;
};

/** A helping class to keep track of font faces. This cache can be used 
 * to make sure the font engine does not keep more fonts open than is 
 * needed. */
class MDF_FontFaceCache
{
public:
	MDF_FontFaceCache(MDF_FontEngine* engine) : m_engine(engine)
											  , m_cache_size(MDF_FONT_FACE_CACHE_SIZE), m_cache_used(0), m_next_timestamp(0)
	{}
	~MDF_FontFaceCache();

	/** Get a font face from the cache. If the face is not in the cache it will be loaded
	 * from m_engine.
	 * @param face variable which will contain the face if the function returns OpStatus::OK.
	 * @param font_name name of the font to load.
	 * @param index index fo the face in the font file to load. This is required to support collection fonts. */
	OP_STATUS GetFontFace(MDF_FontFace** face, const uni_char* font_name, int index);
private:
	MDF_FontEngine* m_engine;
	int m_cache_size;
	int m_cache_used;
	MDF_FontFace* m_cache_array[MDF_FONT_FACE_CACHE_SIZE];
	int m_next_timestamp;
};
#endif // MDF_FONT_FACE_CACHE




typedef BOOL (MDF_TwoLevelCompareFunction)(class MDF_TwoLevelCache*, unsigned short slot, const void* data);
/**
   general implementation of a two-level cache: storage is split over
   two hashes, a hit in first hash does nothing but fetch data. a hit
   in second hash moves entry to first. when first is full second cache
   is flushed and the hashes are swapped.

   the cache in itself doesn't hold any data, instead GetData and
   LoadData use slot index which can be used to denote storage.
*/
class MDF_TwoLevelCache
{
public:
	virtual ~MDF_TwoLevelCache(); // inheriting destructor _must_ call Shutdown!

protected:
	/**
	   entry in two-level cache - holds no data, just cache/hash logic
	*/
	struct MDF_TwoLevelCacheEntry
	{
		unsigned short m_next;
		UINT32 m_id;
	};

	/**
	   @param size the total capacity of the cache - should be divisible by two, since hash is two-level
	   @param hash_size the number of buckets (per level)
	   @param slots storage - half is used in first level, half in second level
	   @param hashes should be twice hash_size, since hash is two-level
	   @param compfunc aditional comparison in FindData (id is checked by base implementation)
	*/
	MDF_TwoLevelCache(size_t cache_size, size_t hash_size,
					  MDF_TwoLevelCacheEntry* slots, unsigned short* hashes,
					  MDF_TwoLevelCompareFunction* compfunc = 0);

	void Shutdown(); // inheriting destructor _must_ call this method!

	/**
	   loads entry into data - inheriting class should implement. slot
	   can be used to denote location.
	*/
	virtual OP_STATUS LoadData(unsigned short slot, UINT32 id, MDE_FONT* font, const void* args = 0) = 0;
	/**
	   called from Clear - any clean-up to be made when slot is
	   removed from cache should be done here.
	 */
	virtual void Free(unsigned short slot) = 0;
	/**
	   finds slot for entry with id - calls LoadData if necessary, and
	   might empty secondary cache. slot can be used to denote
	   storage.
	 */
	OP_STATUS GetData(unsigned short& slot, UINT32 id, MDE_FONT* font, const void* args = 0);

	/** computes hash value for id, in range[0, m_hash_size[ */
	/** finds slot in either cache, and moves/swaps when necessary - returns MDF_TL_EMPTY if slot is not present */
	unsigned short Slot(UINT32 id, unsigned short hash, const void* args = 0);
	/** puts slot with id first in bucket hash */
	void Into(unsigned short id, UINT32 hash);
	/**
	 * Find a data entry in the hash.
	 * @param hash the hash to search for the glyph in.
	 * @param hash_val the hash value of the caracter to find.
	 * @param id the id of the entry to find.
	 * @param nextp will contain a pointer to the next-pointer pointing to the
	 * found entry if an entry is found. 
	 * @param args arguments to pass on to inheriting implementations
	 * @return the index of the glyph if found, MDF_TL_EMPTY otherwise.
	 */
	unsigned short FindData(unsigned short* hash, int hash_val, UINT32 id, unsigned short** nextp, const void* args = 0);
	/** Swap the primary and secondary cache. */
	void SwapCache();
	/** Clear a cache. 
	 * @param hash the hash to clear. */
	void Clear(unsigned short* hash);
	/** Clear a cache without deleting its contents.
	 */
	void SoftClear();

	const unsigned short m_cache_size; ///< number of slots in cache - divided on two hashes
	const unsigned short m_hash_size;  ///< number of buckets per hash

	MDF_TwoLevelCacheEntry* const m_slots; ///< cache slots - divided on two hashes
	unsigned short* const m_hashes; ///< hashes
	unsigned short *m_h1, *m_h2; ///< active pointers to hashes - m_h1 is current first-level, m_h2 is current second-level

	unsigned short m_free_slots; ///< index of first free slot in cache
	unsigned short m_cache_fill; ///< number of entries in current first-level hash

	MDF_TwoLevelCompareFunction* const m_compfunc; ///< auxiliary comparisons when looking for match

	unsigned short m_swaps; ///< number of times the two hashes have been swapped.
};



# ifdef MDF_FONT_GLYPH_CACHE
/** An entry in the glyph cache representing a glyph. */
struct MDF_FontGlyph
{
	enum { GlyphRendered = 1, GlyphIsIndex = 2 };
	BOOL IsIndex();
	BOOL Rendered();

    struct MDF_GLYPH_BUFFER *buffer_data;  //  4
    short bitmap_left;                     //  6
    short bitmap_top;                      //  8
    short advance;                         // 10
    short flags;		                   // 12
	UINT32 c;                            // 16
};
inline BOOL MDF_FontGlyph::IsIndex()  { return (flags & GlyphIsIndex)  == GlyphIsIndex; }
inline BOOL MDF_FontGlyph::Rendered() { return (flags & GlyphRendered) == GlyphRendered; }
// compares index-ness of entries since glyph cache can contain glyphs with unicode point and glyph index id:s
BOOL GlyphMatch(MDF_TwoLevelCache* cache, unsigned short slot, const void* args);
class MDF_FontGlyphCache : public MDF_TwoLevelCache
{
public:
	// comparison function needs access to members
	friend BOOL GlyphMatch(MDF_TwoLevelCache* cache, unsigned short slot, const void* args);

	~MDF_FontGlyphCache() { Shutdown(); OP_DELETEA(m_glyphs); }
	static OP_STATUS Create(MDF_FontGlyphCache** cache, MDF_FontEngine* engine);
	static OP_STATUS Create(MDF_FontGlyphCache** cache, int size, MDF_FontEngine* engine);
	MDF_TwoLevelCacheEntry& Get(unsigned short slot);
	OP_STATUS GetChar(MDF_FontGlyph*& glyph, UINT32 gid, MDE_FONT* font, BOOL mustRender, BOOL isIndex = FALSE);
	static OP_STATUS MoveCache(MDF_FontGlyphCache* cache2, MDF_FontGlyphCache* cache1);
	BOOL IsFull() const;
	unsigned short GetSize() const;
	unsigned short GetFill() const;

protected:
	MDF_FontGlyphCache(MDF_FontEngine* engine,
					   size_t cache_size, size_t hash_size,
					   MDF_TwoLevelCacheEntry* slots, unsigned short* hashes,
					   MDF_FontGlyph* glyphs);

	virtual OP_STATUS LoadData(unsigned short slot, UINT32 id, MDE_FONT* font, const void* args);
	virtual void Free(unsigned short slot);

private:
	struct Args
	{
        Args(BOOL mustRender, BOOL isIndex) : m_mustRender(mustRender), m_isIndex(isIndex) {}
		BOOL m_mustRender;
		BOOL m_isIndex;
	};

	MDF_FontEngine* m_engine;
	MDF_FontGlyph* const m_glyphs;
};
# endif // MDF_FONT_GLYPH_CACHE




# ifdef MDF_FONT_ADVANCE_CACHE
/**
   the advance cache stores the advance for glyphs, identified by
   unicode codepoint.
 */
class MDF_AdvanceCache : public MDF_TwoLevelCache
{
public:
	~MDF_AdvanceCache() { Shutdown(); OP_DELETEA(m_advances); }
	static OP_STATUS Create(MDF_AdvanceCache** advance_cache, MDE_FONT* font, unsigned short size, unsigned short hash_size);
    /**
       get advance for glyph ucp. looks up advance and inserts in
       cache on miss.
    */
    OP_STATUS Advance(UnicodePoint ucp, short& advance);
    /**
       set advance for glyph ucp. on miss, inserts new entry, replacing oldest
       if needed.
	*/
    void Set(UnicodePoint ucp, short advance);

protected:
    MDF_AdvanceCache(MDE_FONT* font,
                     size_t cache_size, size_t hash_size,
                     MDF_TwoLevelCacheEntry* slots, unsigned short* hashes,
					 short* advances)
        : MDF_TwoLevelCache(cache_size, hash_size, slots, hashes)
		, m_font(font)
		, m_advances(advances)
	{}

	virtual void Free(unsigned short slot) { }
	virtual OP_STATUS LoadData(unsigned short slot, UINT32 id, MDE_FONT* font, const void* args = 0);

private:
	MDE_FONT* const m_font;
	short* const m_advances;
};
# endif // MDF_FONT_ADVANCE_CACHE

#endif // MDF_CACHE_H
