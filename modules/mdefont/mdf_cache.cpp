/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */

#include "core/pch.h"

#ifdef MDEFONT_MODULE
#include "modules/mdefont/mdefont.h"

////////// Font face cache //////////
#ifdef MDF_FONT_FACE_CACHE
MDF_FontFaceCache::~MDF_FontFaceCache()
{
    for (int i = 0; i < m_cache_used; i++)
    {
        OP_DELETE(m_cache_array[i]);
    }
}
OP_STATUS MDF_FontFaceCache::GetFontFace(MDF_FontFace** face, const uni_char* font_name, int index)
{
    int last_used = 0;
    for (int i = 0; i < m_cache_used; i++)
    {
        if (m_cache_array[i]->IsEqual(font_name, index))
        {
            m_cache_array[i]->m_last_used = m_next_timestamp++;
            *face = m_cache_array[i];

            // move found element first
            m_cache_array[i] = m_cache_array[0];
            m_cache_array[0] = *face;

            return OpStatus::OK;
        }

        if (m_cache_array[i]->m_last_used <	m_cache_array[last_used]->m_last_used)
        {
            last_used = i;
        }
    }

    if (m_cache_used < m_cache_size)
    {
        last_used = m_cache_used;
    }

    if (last_used == m_cache_used)
    {
        m_cache_array[last_used] = NULL;
        ++m_cache_used;
    }
    OP_STATUS status = m_engine->LoadFontFace(&m_cache_array[last_used], font_name, index);
    if (OpStatus::IsError(status))
    {
        OP_DELETE(m_cache_array[last_used]);
        m_cache_array[last_used] = m_cache_array[m_cache_used-1];
        --m_cache_used;
    }
    else
    {
        m_cache_array[last_used]->m_last_used = m_next_timestamp++;
        *face = m_cache_array[last_used];
    }

    return status;
}
#endif // MDF_FONT_FACE_CACHE



////////// Two-level cache base implementation //////////
# ifdef MDF_TL_EMPTY
#  error MDF_TL_EMPTY already defined
# endif // MDF_TL_EMPTY
# define MDF_TL_EMPTY m_cache_size
# ifdef MDF_TL_HASH
#  error MDF_TL_HASH already defined
# endif // MDF_TL_HASH
# define MDF_TL_HASH(val) ((val) % m_hash_size)
MDF_TwoLevelCache::MDF_TwoLevelCache(size_t cache_size, size_t hash_size,
                                     MDF_TwoLevelCacheEntry* slots, unsigned short* hashes,
									 MDF_TwoLevelCompareFunction* compfunc/* = 0*/)
    : m_cache_size(cache_size), m_hash_size(hash_size)
    , m_slots(slots), m_hashes(hashes)
	, m_h1(hashes), m_h2(hashes + hash_size)
    , m_free_slots(0), m_cache_fill(0)
	, m_compfunc(compfunc), m_swaps(0)
{
	OP_ASSERT(cache_size % 2 == 0); // cache is divided in two
    OP_ASSERT(cache_size < USHRT_MAX);
    OP_ASSERT(hash_size < USHRT_MAX);

    int i;
    for (i = 0; i < m_cache_size-1; ++i)
        m_slots[i].m_next = i+1;
    m_slots[m_cache_size-1].m_next = MDF_TL_EMPTY;
    for (i = 0; i < m_hash_size; ++i)
        m_h1[i] = m_h2[i] = MDF_TL_EMPTY;
}
MDF_TwoLevelCache::~MDF_TwoLevelCache()
{
	// unfortunately, it has to be left to the inheriting class to
	// call Shutdown since when this destructor is called the object
	// is no longer of the inheriting type and thus Free is pure
	// virtual
	OP_ASSERT(m_cache_fill == 0);
	OP_DELETEA(m_slots);
	OP_DELETEA(m_hashes);
}
void MDF_TwoLevelCache::Shutdown()
{
	if (m_cache_fill)
	{
		Clear(m_h1);
		Clear(m_h2);
		m_cache_fill = 0;
	}
}
OP_STATUS MDF_TwoLevelCache::GetData(unsigned short& slot, UINT32 id, MDE_FONT* font, const void* args/* = 0*/)
{
    UINT32 hv = MDF_TL_HASH(id);

    slot = Slot(id, hv, args);
    if (slot != MDF_TL_EMPTY)
	{
		OP_ASSERT(m_slots[slot].m_id == id);
        return OpStatus::OK;
	}

    slot = m_free_slots;
    m_slots[slot].m_id = id;

    RETURN_IF_ERROR(LoadData(slot, id, font, args));
    Into(slot, hv);

    return OpStatus::OK;
}
unsigned short MDF_TwoLevelCache::Slot(UINT32 id, unsigned short hash, const void* args/* = 0*/)
{
	OP_ASSERT(hash == MDF_TL_HASH(id));
    unsigned short* nextp;
    unsigned short res = FindData(m_h1, hash, id, &nextp, args);
    if (res != MDF_TL_EMPTY) // L1 cache
	{
		OP_ASSERT(m_slots[res].m_id == id);
        return res;
	}

    res = FindData(m_h2, hash, id, &nextp, args);
    if (res != MDF_TL_EMPTY) // L2 cache
    {
		OP_ASSERT(m_slots[res].m_id == id);
        // Remove res from h2 so it is not destroyed if we need to swap caches
        *nextp = m_slots[res].m_next;
        if (m_cache_fill >= (m_cache_size >> 1))
        {
            SwapCache();
        }
        // Move res to h1
        m_slots[res].m_next = m_h1[hash];
        m_h1[hash] = res;
        ++m_cache_fill;
		return res;
    }

	// not in any cache
    if (m_cache_fill >= (m_cache_size >> 1))
        SwapCache();
    return MDF_TL_EMPTY;
}
void MDF_TwoLevelCache::Into(unsigned short slot, UINT32 hash)
{
	OP_ASSERT(MDF_TL_HASH(m_slots[slot].m_id) == hash);
    m_free_slots = m_slots[slot].m_next;
    m_slots[slot].m_next = m_h1[hash];
    m_h1[hash] = slot;
    ++m_cache_fill;
}
unsigned short MDF_TwoLevelCache::FindData(unsigned short* hash, int hash_val, UINT32 id, unsigned short** nextp, const void* args/* = 0*/)
{
    *nextp = &hash[hash_val];
    unsigned short res = hash[hash_val];
    while (res != MDF_TL_EMPTY)
    {
        if (id == m_slots[res].m_id && (!m_compfunc || (*m_compfunc)(this, res, args)))
            return res;
        *nextp = &m_slots[res].m_next;
        res = m_slots[res].m_next;
    }
    return MDF_TL_EMPTY;
}
void MDF_TwoLevelCache::SwapCache()
{
    // Clear h2
    Clear(m_h2);
    // swap h1 and h2
    unsigned short* tmph = m_h1;
    m_h1 = m_h2;
    m_h2 = tmph;
    m_cache_fill = 0;
	++m_swaps;
}
void MDF_TwoLevelCache::Clear(unsigned short* hash)
{
    for (int i = 0; i < m_hash_size; ++i)
    {
        while (hash[i] != MDF_TL_EMPTY)
        {
            short next = m_slots[hash[i]].m_next;
            m_slots[hash[i]].m_next = m_free_slots;
            Free(hash[i]);
            m_free_slots = hash[i];
            hash[i] = next;
        }
    }
}
void MDF_TwoLevelCache::SoftClear()
{
	int i;
	for (i = 0; i < m_cache_size-1; ++i)
		m_slots[i].m_next = i+1;
	m_slots[m_cache_size-1].m_next = MDF_TL_EMPTY;
	for (i = 0; i < m_hash_size; ++i)
		m_h1[i] = m_h2[i] = MDF_TL_EMPTY;
	m_free_slots = 0;
	m_cache_fill = 0;
}
////////// Glyph cache //////////
#  ifdef MDF_FONT_GLYPH_CACHE
BOOL GlyphMatch(MDF_TwoLevelCache* cache, unsigned short slot, const void* args)
{
	OP_ASSERT(cache);
	OP_ASSERT(args);
	return ((MDF_FontGlyphCache*)cache)->m_glyphs[slot].IsIndex() == ((MDF_FontGlyphCache::Args*)args)->m_isIndex;
}

// static
OP_STATUS MDF_FontGlyphCache::Create(MDF_FontGlyphCache** cache, MDF_FontEngine* engine)
{
	return Create(cache, MDF_GLYPH_CACHE_SIZE, engine);
}

OP_STATUS MDF_FontGlyphCache::Create(MDF_FontGlyphCache** cache, int cache_size, MDF_FontEngine* engine)
{
    OP_ASSERT(cache);
	// size above have historically meant size for one level
    const size_t size = 2*cache_size;
    OpAutoArray<MDF_TwoLevelCacheEntry> slots(OP_NEWA(MDF_TwoLevelCacheEntry, size));
    if (!slots.get())
        return OpStatus::ERR_NO_MEMORY;
    OpAutoArray<unsigned short> hashes(OP_NEWA(unsigned short, 2*MDF_GLYPH_CACHE_HASH_SIZE));
    if (!hashes.get())
        return OpStatus::ERR_NO_MEMORY;
	OpAutoArray<MDF_FontGlyph> glyphs(OP_NEWA(MDF_FontGlyph, size));
	if (!glyphs.get())
		return OpStatus::ERR_NO_MEMORY;
    MDF_FontGlyphCache* c = OP_NEW(MDF_FontGlyphCache, (engine,
														size, MDF_GLYPH_CACHE_HASH_SIZE,
														slots.get(), hashes.get(),
														glyphs.get()));
    if (!c)
        return OpStatus::ERR_NO_MEMORY;
    *cache = c;
	slots.release();
	hashes.release();
	glyphs.release();
    return OpStatus::OK;
}
MDF_FontGlyphCache::MDF_FontGlyphCache(MDF_FontEngine* engine,
									   size_t cache_size, size_t hash_size,
									   MDF_TwoLevelCacheEntry* slots, unsigned short* hashes,
									   MDF_FontGlyph* glyphs)
	: MDF_TwoLevelCache(cache_size, hash_size, slots, hashes, GlyphMatch)
	, m_engine(engine)
	, m_glyphs(glyphs)
{
    for (size_t i = 0; i < m_cache_size; ++i)
        m_glyphs[i].buffer_data = NULL;
}
OP_STATUS MDF_FontGlyphCache::GetChar(MDF_FontGlyph*& glyph, UINT32 gid, MDE_FONT* font, BOOL mustRender, BOOL isIndex/* = FALSE*/)
{
    const Args args(mustRender, isIndex);
	unsigned short slot;
    RETURN_IF_ERROR(GetData(slot, gid, font, &args));

	glyph = m_glyphs + slot;

    // If the glyph in the cache is not rendered, try to render it
    if (mustRender && !glyph->Rendered())
    {
        OP_STATUS status = m_engine->LoadFontGlyph(glyph, font, mustRender);
        if (OpStatus::IsError(status))
        {
            glyph->flags &= ~MDF_FontGlyph::GlyphRendered;
            return status;
        }
        glyph->flags |= MDF_FontGlyph::GlyphRendered;
    }
    return OpStatus::OK;
}
BOOL MDF_FontGlyphCache::IsFull() const
{
	return m_swaps > 1;
}
/** Copy from cache1 to cache2.
*/
OP_STATUS MDF_FontGlyphCache::MoveCache(MDF_FontGlyphCache* cache2, MDF_FontGlyphCache* cache1)
{
	OP_ASSERT(cache2->m_hash_size == cache1->m_hash_size);
	unsigned short cache2_size = cache2->m_cache_size;
	unsigned short cache1_size = cache1->m_cache_size;
	OP_ASSERT(cache2_size > cache1_size);
	int i = 0;
	unsigned short slots_to_copy = MIN(cache2_size, cache1_size);
	for (i = 0; i < slots_to_copy; ++i)
	{
		cache2->m_glyphs[i] = cache1->m_glyphs[i];
		cache2->m_slots[i] = cache1->m_slots[i];
		if (cache1->m_slots[i].m_next == cache1_size)
		{
			// Since caches have different sizes, they have different values for MDF_TL_EMPTY.
			cache2->m_slots[i].m_next = cache2_size;
		}
	}
	for (i = 0; i < cache2->m_hash_size; ++i)
	{
		cache2->m_h1[i] = cache1->m_h1[i];
		if (cache1->m_h1[i] == cache1_size)
		{
			cache2->m_h1[i] = cache2_size;
		}
		cache2->m_h2[i] = cache1->m_h2[i];
		if (cache1->m_h2[i] == cache1_size)
		{
			cache2->m_h2[i] = cache2_size;
		}
	}
	cache2->m_cache_fill = cache1->m_cache_fill;
	cache2->m_free_slots = cache1->m_free_slots;

	// Add the new slots to the list of free slots.
	unsigned short slot = cache2->m_free_slots;
	while (cache2->m_slots[slot].m_next != cache2_size)
	{
		slot = cache2->m_slots[slot].m_next;
	}
	slot = cache2->m_slots[slot].m_next = cache1_size;
	while (slot < cache2_size - 1)
	{
        cache2->m_slots[slot].m_next = slot+1;
		++slot;
	}
	cache2->m_slots[cache2_size-1].m_next = cache2_size;

	// Clear old cache (without calling Free) before deleting it,
	// to prevent the glyphs in it from being freed.
	cache1->SoftClear();
    return OpStatus::OK;
}
unsigned short MDF_FontGlyphCache::GetSize() const
{
	return m_cache_size / 2;
}
OP_STATUS MDF_FontGlyphCache::LoadData(unsigned short slot, UINT32 id, MDE_FONT* font, const void* _args)
{
    OP_ASSERT(_args);
    MDF_FontGlyph* glyph = m_glyphs + slot;
    const Args& args = *(Args*)_args;
	glyph->c = id;
    // Clear before since LoadFontGlyph is required to set MDF_GLYPH_RENDERED if rendering when mustRender is FALSE
    glyph->flags = 0;
    if (args.m_isIndex)
        glyph->flags |= MDF_FontGlyph::GlyphIsIndex;
    RETURN_IF_ERROR(m_engine->LoadFontGlyph(glyph, font, args.m_mustRender));
	OP_ASSERT(!args.m_mustRender || glyph->Rendered()); // boldification may force rendering regardless
	OP_ASSERT(args.m_isIndex == glyph->IsIndex());
    return OpStatus::OK;
}
void MDF_FontGlyphCache::Free(unsigned short slot)
{
	m_engine->FreeFontGlyph(m_glyphs + slot);
}
unsigned short MDF_FontGlyphCache::GetFill() const
{
	return m_cache_fill;
}

#  endif // MDF_FONT_GLYPH_CACHE



////////// Advance cache //////////
#  ifdef MDF_FONT_ADVANCE_CACHE
OP_STATUS MDF_AdvanceCache::Create(MDF_AdvanceCache** advance_cache, MDE_FONT* font, unsigned short size, unsigned short hash_size)
{
    OP_ASSERT(advance_cache);
    OP_ASSERT(font);
    OP_ASSERT(size % 2 == 0);

    OpAutoArray<MDF_TwoLevelCacheEntry> entries(OP_NEWA(MDF_TwoLevelCacheEntry, size));
    if (!entries.get())
        return OpStatus::ERR_NO_MEMORY;
    OpAutoArray<unsigned short> hash(OP_NEWA(unsigned short, 2 * hash_size));
    if (!hash.get())
        return OpStatus::ERR_NO_MEMORY;
	OpAutoArray<short> advances(OP_NEWA(short, size));
	if (!advances.get())
		return OpStatus::ERR_NO_MEMORY;
    MDF_AdvanceCache* ac = OP_NEW(MDF_AdvanceCache, (font, size, hash_size, entries.get(), hash.get(), advances.get()));
    if (!ac)
        return OpStatus::ERR_NO_MEMORY;
    *advance_cache = ac;
	entries.release();
	hash.release();
	advances.release();
    return OpStatus::OK;
}
OP_STATUS MDF_AdvanceCache::Advance(UnicodePoint ucp, short& advance)
{
    const unsigned short hv = MDF_TL_HASH(ucp);
    // not using GetData since fetching glyph inserts advance into cache, which'd cause all kinds of havoc
    unsigned short slot = Slot(ucp, hv);
	// gid not in cache
    if (slot == MDF_TL_EMPTY)
    {
        MDF_GlyphHandle g(m_font);
        // fetching glyph will automatically insert advance in cache through call to Set
		short tmp_advance;
        RETURN_IF_ERROR(g.Advance(tmp_advance, ucp, FALSE));
        slot = Slot(ucp, hv);
		// not enabled, since it's patently stupid to have an advance cache
		// smaller than the glyph cache
#ifdef MDF_FONT_GLYPH_CACHE
		// this means advance is not recorded in advance cache but
		// glyph is present in glyph cache. yes, this can actually
		// happen.
		if (slot == MDF_TL_EMPTY)
		{
			Set(ucp, g.Glyph().advance);
			slot = Slot(ucp, hv);
		}
#endif // MDF_FONT_GLYPH_CACHE
        OP_ASSERT(slot != MDF_TL_EMPTY);
        OP_ASSERT(tmp_advance == m_advances[slot]);
    }
    advance = m_advances[slot];
    return OpStatus::OK;
}
void MDF_AdvanceCache::Set(UnicodePoint ucp, short advance)
{
	unsigned short slot;
#ifdef DEBUG_ENABLE_OPASSERT
	OP_STATUS status =
#endif // DEBUG_ENABLE_OPASSERT
		GetData(slot, ucp, m_font);
	OP_ASSERT(OpStatus::IsSuccess(status)); // should always succeed since LoadData doesn't do anything
    m_advances[slot] = advance;
}
OP_STATUS MDF_AdvanceCache::LoadData(unsigned short slot, UINT32 id, MDE_FONT* font, const void* args/* = 0*/)
{
	return OpStatus::OK; // just return OK so Set can use GetData to get slot
}
#  endif // MDF_FONT_ADVANCE_CACHE

# undef MDF_TL_EMPTY
# undef MDF_TL_HASH



#endif // MDEFONT_MODULE
