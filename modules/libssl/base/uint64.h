/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef HAVE_INT64

#if ULONG_MAX > 0xffffffff
# define UINT64_MASK_32 0xffffffff
# define MASKED_WITH &
# define MASK_WITH_LIMIT(x) x &=  UINT64_MASK_32
#else
# define UINT64_MASK_32
# define MASKED_WITH
# define MASK_WITH_LIMIT(x)
#endif

class uint64 {
private:
    unsigned long high,low;
public:
    uint64(){high=low=0;};
    uint64(unsigned long val ){high=0;low= val MASKED_WITH UINT64_MASK_32;};
	//  uint64(const uint64 &);
	
    uint64 &operator =(unsigned long val){high=0;low= val MASKED_WITH UINT64_MASK_32;return *this;};
	//  uint64 &operator =(const uint64 &);
    
    void inc();
    uint64 &operator ++(){inc();return *this;}; // prefix
    uint64 operator ++(int){uint64 temp(*this); inc(); return temp;};  // postfix
    void dec();
    uint64 &operator --(){dec();return *this;}; // prefix
    uint64 operator --(int){uint64 temp(*this); dec(); return temp;};  // postfix
    
    unsigned long operator &(unsigned long val)const {return low & val;};
    uint64 operator |(unsigned long) const;
    
    uint64 operator &(const uint64 &) const;
    uint64 operator |(const uint64 &) const;
	
    void rot_right(unsigned short);
    void rot_left(unsigned short);
    uint64 operator >>(unsigned short) const;
    uint64 operator <<(unsigned short) const;
    
    int operator ==(const uint64 &) const;
    int operator !=(const uint64 &) const;
};

#endif    
