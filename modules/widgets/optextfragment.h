#ifndef OP_TEXT_FRAGMENT_H
#define OP_TEXT_FRAGMENT_H

#include "modules/layout/box/box.h"
#include "modules/layout/bidi/characterdata.h"

#ifndef NEEDS_RISC_ALIGNMENT ///< Packing causes memory corruption.
# pragma pack(1)
#endif

struct OP_TEXT_FRAGMENT {
	WordInfo wi;					///< The WordInfo for this fragment
	union {
		struct {
			unsigned char bidi : 4;	///< The bidi-direction
			unsigned char level : 4;///< The bidi-level
		} packed;
		unsigned char packed_init;
	};
	INT32 start;					///< start, as in WordInfo->start. But as INT32.
	UINT32 nextidx;					///< The index of the next fragment in left-to-right order
};

#ifndef NEEDS_RISC_ALIGNMENT
# pragma pack()
#endif

#endif // OP_TEXT_FRAGMENT_H
