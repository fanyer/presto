#include "core/pch.h"

#ifdef M2_SUPPORT

#include "modules/util/simset.h"
#include <ctype.h>

#include "nntprange.h"
#include "nntpmodule.h"


NNTPRangeItem::NNTPRangeItem()
: m_from(-1),
  m_to(-1)
{
}

NNTPRange::NNTPRange()
: m_availablerange_from(-1),
  m_availablerange_to(-1)
{
}


NNTPRange::~NNTPRange()
{
    range_list.Clear();
}


OP_STATUS NNTPRange::SetReadRange(const OpStringC8& string)
{
    range_list.Clear();
    return AddRange(string);
}


void NNTPRange::SetAvailableRange(INT32 from, INT32 to)
{
    m_availablerange_from = MIN(from,to);
    m_availablerange_to = MAX(from, to);
}


OP_STATUS NNTPRange::SetAvailableRange(const OpStringC8& available_range)
{
    OP_STATUS ret;
    INT32 from=0, to=0;
    char* range_ptr = (char*)(available_range.CStr());
    if (range_ptr)
    {
        if ((ret=ParseNextRange(range_ptr, from, to)) != OpStatus::OK)
            return ret;
    }
    SetAvailableRange(from, to);
    return OpStatus::OK;
}


OP_STATUS NNTPRange::GetReadRange(OpString8& string) const
{
    OP_STATUS ret;
    string.Empty();
    char temp_string[22];
    NNTPRangeItem* item = (NNTPRangeItem*)(range_list.First());
    while (item)
    {
        if (!string.IsEmpty() && (ret=string.Append(","))!=OpStatus::OK)
            return ret;

        if (item->m_from<0 || item->m_to<0)
        {
            temp_string[0] = 0;
        }
        else
        {
            if (item->m_to <= item->m_from)
            {
                sprintf(temp_string, "%d", item->m_from);
            }
            else
            {
                sprintf(temp_string, "%d-%d", item->m_from, item->m_to);
            }
        }

        if ((ret=string.Append(temp_string)) != OpStatus::OK)
            return ret;

        item = (NNTPRangeItem*)(item->Suc());
    }
    return OpStatus::OK;
}

BOOL NNTPRange::IsRead(INT32 index) const
{
    NNTPRangeItem* item = (NNTPRangeItem*)(range_list.First());
    while (item)
    {
        if (item->m_from<=index && item->m_to>=index)
        {
            return TRUE;
        }
        else if (item->m_from > index)
        {
            return FALSE;
        }

        item = (NNTPRangeItem*)(item->Suc());
    }

    return FALSE;
}


INT32 NNTPRange::GetUnreadCount() const
{
    if (m_availablerange_from<0 || m_availablerange_to<0)
        return 0;

    INT32 unread = m_availablerange_to - m_availablerange_from + 1;
    NNTPRangeItem* item = (NNTPRangeItem*)(range_list.First());
    while (item)
    {
        unread -= (item->m_to - item->m_from + 1);
        item = (NNTPRangeItem*)(item->Suc());
    }

    return max(0, unread);
}


OP_STATUS NNTPRange::GetNextUnreadRange(INT32& low_limit, BOOL optimize, BOOL only_one_item,
                                        OpString8& unread_string, OpString8& optimized_string,
										INT32* count, INT32* max_to) const
{
    INT32 from, to;
    unread_string.Empty();
    optimized_string.Empty();
	
	if (count)
		*count = 0;
	if (max_to)
		*max_to = 0;
	
    if (low_limit>m_availablerange_to || m_availablerange_from<0 || m_availablerange_to<0)
        return OpStatus::OK;

    from = max(low_limit, m_availablerange_from);
    to = m_availablerange_to;

    NNTPRangeItem* item = (NNTPRangeItem*)(range_list.First());
    while (item && item->m_to<from) //Skip items before requested range
    {
        item = (NNTPRangeItem*)(item->Suc());
    }

    while (item && item->m_from<=to) //Search for read ranges within the unread range
    {
        if (!optimize || ((item->m_to-item->m_from)>=10) || (item->m_from<=from && item->m_to>=to))
        {
            if (item->m_from <= from) //Cut away from the start of the range. Still need to find the end of the range
            {
                from = item->m_to+1;
            }
            else
            {
                to = max(from, item->m_from-1); //Cut away from the end of the range. We have a final range, break out
                break;
            }
        }
        else //We optimize away this range. Add it to the optimized_string
        {
            OP_STATUS ret;
            if (!optimized_string.IsEmpty() && ((ret=optimized_string.Append(",")) != OpStatus::OK) )
                return ret;

            char temp_string[22];
            if (item->m_from<0 || item->m_to<0)
            {
                temp_string[0] = 0;
            }
            else
            {
                if (item->m_to<=item->m_from)
                {
                    sprintf(temp_string, "%d", item->m_from);
                }
                else
                {
                    sprintf(temp_string, "%d-%d", item->m_from, item->m_to);
                }
            }

            if ((ret=optimized_string.Append(temp_string)) != OpStatus::OK)
                return ret;
        }
        item = (NNTPRangeItem*)(item->Suc());
    }

    if (from > m_availablerange_to) //No range available
        return OpStatus::OK;
	
	if (max_to)
		*max_to = to;
	
	if (count)
	{
		*count = (to - from) + 1;
		return OpStatus::OK;
	}

    char temp_string[22]; //Generate string
    if (from<0 || to<0)
    {
        temp_string[0] = 0;
    }
    else
    {
        if (to<=from || only_one_item)
        {
            sprintf(temp_string, "%d", from);
            low_limit = ++from;
        }
        else
        {
            sprintf(temp_string, "%d-%d", from, to);
            low_limit = ++to;
        }
    }
	
    return unread_string.Set(temp_string);
}


INT32 NNTPRange::GetNextUnreadCount(INT32& max_to) const
{
	INT32 low_limit = 0;
	INT32 count;
	OpString8 unread_string, optimized_string;
	
	if (GetNextUnreadRange(low_limit, FALSE, FALSE, unread_string, optimized_string, &count, &max_to) == OpStatus::OK)
		return count;
	else
		return 0;
}


OP_STATUS NNTPRange::AddRange(const OpStringC8& string)
{
    OP_STATUS ret;
    INT32 from, to;
    char* range_ptr = (char*)(string.CStr());
    while (range_ptr)
    {
        if ( (ParseNextRange(range_ptr, from, to) == OpStatus::OK) &&
             ((ret=AddRange(from, to)) != OpStatus::OK) )
        {
                return ret;
        }
    }
    return OpStatus::OK;
}

OP_STATUS NNTPRange::AddRange(INT32 from, INT32 to)
{
    if (from<0 || to<0) //No range to add
        return OpStatus::OK;

//#pragma PRAGMAMSG("FG: Just to track down that strange bug in .newsrc-file  [20020909]")
    OP_ASSERT(from<500000 && to<500000);

    if (from > to)
    {
        INT32 tmp_to = to;
        to = from;
        from = tmp_to;
    }

    if (range_list.Empty()) //Optimize handling of empty list
    {
        NNTPRangeItem* item = OP_NEW(NNTPRangeItem, ());
        if (!item)
            return OpStatus::ERR_NO_MEMORY;

        item->m_from = from;
        item->m_to = to;
        item->Into(&range_list);
        return OpStatus::OK;
    }

    //Skip unrelated entries (item should never become NULL)
    NNTPRangeItem* item = (NNTPRangeItem*)(range_list.First());
    NNTPRangeItem* tmp_item;
    while (item && ((item->m_to+1) < from))
    {
        tmp_item = (NNTPRangeItem*)(item->Suc());
        if (!tmp_item || (tmp_item->m_to+1)>=from)
            break;
        item = tmp_item;
    }

    NNTPRangeItem* current_item;
    if ((item->m_to+1)==from)
    {
        current_item = item;
        current_item->m_to = to;
    }
    else
    {
        current_item = OP_NEW(NNTPRangeItem, ());
        if (!current_item)
            return OpStatus::ERR_NO_MEMORY;

        current_item->m_from = from;
        current_item->m_to = to;
        if (item->m_to < from)
        {
            current_item->Follow(item);
        }
        else
        {
            current_item->Precede(item);
        }
    }

    item = (NNTPRangeItem*)(current_item->Suc());
    while (item && item->m_from<=(to+1))
    {
        if (item->m_from<from)
        {
            current_item->m_from = item->m_from;
        }

        if (item->m_to>to)
        {
            current_item->m_to = item->m_to;
        }

        tmp_item = item;
        item->Out();
        item = (NNTPRangeItem*)(current_item->Suc());
        OP_DELETE(tmp_item);
    }

    return OpStatus::OK;
}


OP_STATUS NNTPRange::ParseNextRange(char*& range_ptr, INT32& from, INT32& to)
{
    from = to = -1;
    if (!range_ptr)
        return OpStatus::ERR_OUT_OF_RANGE;

    while (*range_ptr && (*range_ptr<'0' || *range_ptr>'9')) //Skip any garbage in front of range
        range_ptr++;

    if (!*range_ptr)
    {
        range_ptr = NULL;
        return OpStatus::ERR_OUT_OF_RANGE;
    }

    from = 0;
    while (*range_ptr>='0' && *range_ptr<='9')
    {
        from = from*10+(*range_ptr-'0');
        range_ptr++;
    }

    BOOL is_range=FALSE;
    while (*range_ptr && (*range_ptr<'0' || *range_ptr>'9'))
    {
        if ((*range_ptr) == '-')
        {
            is_range = TRUE;
        }
        else if ((*range_ptr) == ',')
        {
            is_range = FALSE;
            range_ptr++;
            break;
        }
        range_ptr++;
    }

    if (!is_range)
    {
        to = from;
    }
    else
    {
        to = 0;
        while (*range_ptr>='0' && *range_ptr<='9')
        {
            to = to*10+(*range_ptr-'0');
            range_ptr++;
        }

        while (*range_ptr && (*range_ptr<'0' || *range_ptr>'9'))
        {
            if ((*range_ptr) == ',')
            {
                range_ptr++;
                break;
            }
            range_ptr++;
        }
    }

    if (range_ptr && !*range_ptr)
        range_ptr = NULL;

    return OpStatus::OK;
}

#endif //M2_SUPPORT
