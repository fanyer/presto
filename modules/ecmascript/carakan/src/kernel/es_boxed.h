/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  1999 - 2006
 *
 * Data types allocated on the garbage collected heap.
 *
 * @author Lars T Hansen
 */

#ifndef ES_BOXED_H
#define ES_BOXED_H

/* The data allocated on the GC heap are POD-structs in the sense of
   the C++ standard; see excerpts from the standard below.  They have
   no non-static methods at all.  The POD restriction makes their layout
   relatively transparent; in particular it ensures that the first
   element of each struct is at offset 0 in the struct, always, and that
   the address of a union of the structs is the address of each struct.

   These conditions are important for the garbage collector.  During the
   sweep phase of GC, the collector scans the heap from one end to the
   other, inspecting the header of each object.  It needs to find that
   header at the start of the object.

   The consequence of using POD-structs is that we cannot use standard
   C++ subclassing (because there are no guarantees in the standard about
   how a compound class will be laid out, not even for a single inheritance
   hierarchy without virtual methods).  Instead, we emulate subclassing
   by giving groups of types the same initial prefix and using explicit
   casts among these types when we need to cast to a "base class".

   We also cannot use normal constructors; instead, Initialize() functions
   are defined that are called as part of the object construction.

   All of this is slightly messy, but not very bad.

   The hierarchy of types:

   ES_Boxed
     ES_Boxed_Array
	 ES_Compiled_Code
	 ES_Free
	 ES_Latent_Arguments
     ES_Object
	   ES_Array
	   ES_Foreign_Object
	     ES_Foreign_Function
		 ES_Global_Object
	   ES_Function
	   ES_RegExp
	 ES_Property_Object
     ES_Rib
	   ES_Lexical_Rib
	   ES_Reified_Rib
     //ES_Double [ES_DOUBLES_ON_THE_HEAP - obsolete; see comment in es_double.h]
     JString
     JStringStorage

   Although the term "POD-struct" seems to imply that "struct" has to be
   used, this is not so -- so I use "class" everywhere, with all members
   public, to reduce problems of backward compatibility to systems where
   ES_Object is declared as a class.



   C++ standard section 9

 4 A POD-struct is an aggregate class that has no non-static data members of
   type pointer to member, non-POD-struct, non-POD-union (or array of such
   types) or reference, and has no user-defined copy assignment operator and
   no user-defined destructor. Similarly, a POD-union is an aggregate union
   that has no non-static data members of type pointer to member, non-POD-struct,
   non-POD-union (or array of such types) or reference, and has no user-defined
   copy assignment operator and no user-defined destructor. A POD class is a
   class that is either a POD-struct or a POD-union.


   C++ standard section 9.2

14 Two POD-struct (clause 9) types are layout-compatible
   if they have the same number of members, and corresponding
   members (in order) have layout-compatible types (3.9).

15 Two POD-union (clause 9) types are layout-compatible
   if they have the same number of members, and corresponding
   members (in any order) have layout-compatible types (3.9).

16 If a POD-union contains two or more POD-structs
   that share a common initial sequence, and if the POD-union
   object currently contains one of these POD-structs,
   it is permitted to inspect the common initial part
   of any of them. Two POD-structs share a common initial
   sequence if corresponding members have layout-compatible
   types (and, for bitfields, the same widths) for a sequence
   of one or more initial members.

17 A pointer to a POD-struct object, suitably converted using a
   reinterpret_cast, points to its initial member (or if that
   member is a bitfield, then to the unit in which it resides)
   and vice versa. [Note: There might therefore be unnamed
   padding within a POD-struct object, but not at its beginning,
   as necessary to achieve appropriate alignment. ]

*/

#ifdef ES_HEAP_DEBUGGER
extern unsigned *g_ecmaLiveObjects;
#define HEAP_DEBUG_LIVENESS_FOR(heap) g_ecmaLiveObjects = (heap)->live_objects
#else
#define HEAP_DEBUG_LIVENESS_FOR(heap) (void)0
#endif // ES_HEAP_DEBUGGER

class ES_Boxed;

inline unsigned int ObjectSize(const ES_Boxed *b);

class ES_Boxed
{

public:
    ES_Header hdr;
#if defined(ES_MOVABLE_OBJECTS) && defined(_DEBUG)
    unsigned int trace_number;
#endif // defined(ES_MOVABLE_OBJECTS) && defined(_DEBUG)

    inline unsigned Bits() const
    {
        return hdr.header & ES_Header::MASK_BITS;
    }

    inline unsigned Size() const
    {
        return hdr.header >> ES_Header::SIZE_SHIFT;
    }

    inline void SetSize(unsigned size)
    {
        OP_ASSERT(size <= UINT16_MAX);
        hdr.header = (hdr.header & ES_Header::MASK_BITS) | (size << ES_Header::SIZE_SHIFT);
    }

    inline void SetHeader(unsigned size, unsigned bits)
    {
        OP_ASSERT(bits <= UINT16_MAX);
        OP_ASSERT(size <= UINT16_MAX);
        hdr.header = (size << ES_Header::SIZE_SHIFT) | bits;
    }

    inline void ClearHeader()
    {
        hdr.header = 0;
    }

    inline void
    InitHeader(unsigned size)
    {
#ifdef _DEBUG
        op_memset(this, 0xfe, size);
#endif // _DEBUG

        hdr.header = (size < UINT16_MAX ? size : UINT16_MAX) << ES_Header::SIZE_SHIFT;

#ifdef _DEBUG
        InitGCTag(GCTAG_UNINITIALIZED);
#endif // _DEBUG
    }

    inline GC_Tag
    GCTag() const
    {
        return (GC_Tag) (hdr.header & ES_Header::MASK_GCTAG);
    }

    inline void
    InitGCTag(GC_Tag n)
    {
        ChangeGCTag(n);
    }

    inline void InitGCTag(GC_Tag n, BOOL need_destroy);

    inline void
    ChangeGCTag(GC_Tag n)
    {
#ifdef ES_HEAP_DEBUGGER
        if (GCTag() != GCTAG_free && n != GCTAG_free)
            --g_ecmaLiveObjects[GCTag()];
#endif // ES_HEAP_DEBUGGER

        hdr.header = (hdr.header & ~ES_Header::MASK_GCTAG) | n;

#ifdef ES_HEAP_DEBUGGER
        if (n != GCTAG_free)
            ++g_ecmaLiveObjects[n];
#endif // ES_HEAP_DEBUGGER
    }

    inline void SetNeedDestroy();

    /** @return non-zero if this box is special property */
    inline BOOL
    IsSpecial()
    {
        unsigned gctag = GCTag();
        return gctag >= GCTAG_FIRST_SPECIAL && gctag <= GCTAG_LAST_SPECIAL;
    }

    inline BOOL
    IsClass()
    {
        unsigned gctag = GCTag();
        return gctag >= GCTAG_FIRST_CLASS && gctag <= GCTAG_LAST_CLASS;
    }

    inline BOOL
    IsBuiltinString() const
    {
        return (hdr.header & ES_Header::MASK_IS_BUILTIN_STRING) != 0;
    }

    inline void
    SetBuiltinString()
    {
        hdr.header |= ES_Header::MASK_IS_BUILTIN_STRING;
    }

    inline BOOL
    IsAllocatedOnLargePage() const
    {
        return (hdr.header & ES_Header::MASK_ON_LARGE_PAGE) != 0;
    }

    inline void
    SetAllocatedOnLargePage()
    {
        hdr.header |= ES_Header::MASK_ON_LARGE_PAGE;
    }

    inline BOOL
    IsLargeObject() const
    {
        return ObjectSize(this) >= LARGE_OBJECT_LIMIT;
    }

    inline void
    SplitAllocation(ES_Boxed *next)
    {
        OP_ASSERT(ObjectSize(this) != UINT16_MAX);

        /* Make sure 'next' is actually included in this allocation. */
        OP_ASSERT(reinterpret_cast<char *>(this) + ObjectSize(this) > reinterpret_cast<char *>(next));

        /* Make sure 'next' is properly aligned. */
        OP_ASSERT(ES_IS_POINTER_PAGE_ALIGNED(next));
        OP_ASSERT(ES_IS_SIZE_PAGE_ALIGNED(ObjectSize(this)));

        unsigned new_size = reinterpret_cast<char *>(next) - reinterpret_cast<char *>(this);
        OP_ASSERT((Size() - new_size) < UINT16_MAX);
        next->SetHeader(Size() - new_size, 0);
#if defined(ES_MOVABLE_OBJECTS) && defined(_DEBUG)
        next->trace_number = trace_number;
#endif // defined(ES_MOVABLE_OBJECTS) && defined(_DEBUG)
        OP_ASSERT(new_size < UINT16_MAX);
        SetSize(new_size);

        OP_ASSERT(ES_IS_SIZE_PAGE_ALIGNED(ObjectSize(next)));
        OP_ASSERT(ES_IS_SIZE_PAGE_ALIGNED(ObjectSize(this)));
    }

    inline unsigned GetPageOffset();

    inline ES_PageHeader *GetPage();
    inline const ES_PageHeader *GetPage() const;
};

#if 0
// Hack to kill "dereferencing type-punned pointer will break strict-aliasing
// rules" warning from GCC.  Might be better ways.
template<class T> static inline ES_Boxed *&ManglePointerIntoBoxed(T **x)
{
    return *reinterpret_cast<ES_Boxed **>(x);
}
template<class T> static inline ES_Boxed *&ManglePointerIntoBoxed(T ***x)
{
    return *reinterpret_cast<void **>(x);
}

#define CAST_TO_BOXED_REF(b) ManglePointerIntoBoxed(&b)
#endif // 0

#define CAST_TO_BOXED(b) static_cast<ES_Boxed*>(b)

/* Operations on ES_Boxed */

/** @return size of the object (including the size of the header), in bytes,  */
inline unsigned int
ObjectSize(const ES_Boxed *b)
{
	unsigned size = b->Size();
	return size != UINT16_MAX ? size : b->GetPage()->GetSize();
}

#endif // ES_BOXED_H
