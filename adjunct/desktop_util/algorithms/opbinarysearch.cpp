#include "core/pch.h"

#include "opbinarysearch.h"

//See header-file for code example

OpBinarySearch::OpBinarySearch()
: m_item_count(-1),
  m_confirmed_current_position(-1),
  m_unconfirmed_current_position(-1),
  m_low_limit(-1),
  m_high_limit(-1)
{
}

OpBinarySearch::~OpBinarySearch()
{
}


INT32 OpBinarySearch::GetStartPosition(UINT32 item_count)
{
    m_low_limit = 0;
    m_item_count = (INT32)item_count;
    m_high_limit = m_item_count-1; // 0-indexed
    m_confirmed_current_position = m_unconfirmed_current_position = m_low_limit>m_high_limit ? -1 : m_item_count/2;
    return m_confirmed_current_position;
}


INT32 OpBinarySearch::TooHigh()
{
    if ((m_item_count==-1) || (m_confirmed_current_position==-1))
        return -1; //Not initialized yet. Remember to call GetStartPosition!

    m_high_limit = m_confirmed_current_position-1;
    if (m_high_limit<m_low_limit)
        return -1;

    m_unconfirmed_current_position = -1;
    m_confirmed_current_position = (m_low_limit+m_high_limit)/2;
    return m_confirmed_current_position<=m_high_limit?m_confirmed_current_position:(m_high_limit>=m_low_limit?m_high_limit:-1);
}


INT32 OpBinarySearch::TooLow()
{
    if ((m_item_count==-1) || (m_confirmed_current_position==-1))
        return -1; //Not initialized yet. Remember to call GetStartPosition!

    m_low_limit = m_unconfirmed_current_position!=-1?m_unconfirmed_current_position+1:m_confirmed_current_position+1;
    if (m_low_limit>m_high_limit)
        return -1;

    m_unconfirmed_current_position = -1;
    m_confirmed_current_position = (m_low_limit+m_high_limit+1)/2;
    return m_confirmed_current_position>=m_low_limit?m_confirmed_current_position:(m_low_limit<=m_high_limit?m_low_limit:-1);
}


INT32 OpBinarySearch::Unknown()
{
    if ((m_item_count==-1) || (m_confirmed_current_position==-1) || (m_confirmed_current_position>m_high_limit))
        return -1;

    if (m_unconfirmed_current_position>=m_high_limit || (m_confirmed_current_position==m_high_limit && m_unconfirmed_current_position==-1))
    {
        return TooHigh();
    }

    m_unconfirmed_current_position = m_unconfirmed_current_position==-1?m_confirmed_current_position:m_unconfirmed_current_position;
    return ++m_unconfirmed_current_position;
}
