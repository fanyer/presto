#ifndef NNTPRANGE_H
#define NNTPRANGE_H

#include "modules/util/opstring.h"
# include "modules/util/simset.h"

#include "adjunct/m2/src/include/defs.h"

class NntpBackend;

class NNTPRangeItem : public Link
{
public:
    INT32 m_from;
    INT32 m_to;
    NNTPRangeItem();
private:
	NNTPRangeItem(const NNTPRangeItem&);
	NNTPRangeItem& operator =(const NNTPRangeItem&);
};


class NNTPRange
{
private:
    Head range_list;
    INT32 m_availablerange_from;
    INT32 m_availablerange_to;

public:
	NNTPRange();
    ~NNTPRange();

    OP_STATUS SetReadRange(const OpStringC8& string);
    void      SetAvailableRange(INT32 from, INT32 to);
    OP_STATUS SetAvailableRange(const OpStringC8& available_range);
    void      GetAvailableRange(UINT32& from, UINT32& to) {from=m_availablerange_from; to=m_availablerange_to;}

    INT32   GetUnreadCount() const;

    OP_STATUS GetReadRange(OpString8& string) const;

    BOOL      IsUnread(INT32 index) const {return !IsRead(index);}
    BOOL      IsRead(INT32 index) const;
    
    /* Find the first range where from>=low_limit. low_limit will be returned as to+1, and string will be empty if no range is found.
	  if count is non-NULL, the strings won't be assigned, and count will contain the number of items in the range.
	*/
    OP_STATUS GetNextUnreadRange(INT32& low_limit, BOOL optimize, BOOL only_one_item, OpString8& unread_string, OpString8& optimized_string, INT32* count = NULL, INT32* max_to = NULL) const;
	INT32	  GetNextUnreadCount(INT32& max_to) const;

    OP_STATUS AddRange(const OpStringC8& string);

private:
    OP_STATUS ParseNextRange(char*& range_ptr, INT32& from, INT32& to);
    OP_STATUS AddRange(INT32 from, INT32 to);

	NNTPRange(const NNTPRange&);
	NNTPRange& operator =(const NNTPRange&);
};

#endif // NNTPRANGE_H
