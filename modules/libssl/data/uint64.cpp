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


#include "core/pch.h"
#if defined(_NATIVE_SSL_SUPPORT_)
#ifndef HAVE_INT64   
#include "modules/libssl/sslbase.h"

void uint64::inc()
{
	low ++;
	MASK_WITH_LIMIT(low);
	if(!low)
	{
		high ++;
		MASK_WITH_LIMIT(high);
	}  
}

/* Unref YNP
uint64 uint64::operator |(unsigned long val) const
{
uint64 temp(*this);

  temp.low |= (val MASKED_WITH UINT64_MASK_32);
  
	return temp;
	}
*/
/*
uint64 uint64::operator &(const uint64 &val) const
{
uint64 temp(*this);

  temp.low &= val.low;
  temp.high &= val.high;
  
	return temp;
	}
	
	  uint64 uint64::operator |(const uint64 &val) const
	  {
	  uint64 temp(*this);
	  
		temp.low |= val.low;
		temp.high |= val.high;
		
		  return temp;
		  }
*/

void uint64::rot_right(unsigned short step)
{
	if (step >64)
	{
		low = high = 0;
		return;
	}
	
	if(step >= 32)
	{
		low = high >> (step -32);
		high = 0;
	}
	else
	{
		low = ((low >> step) | (high << (32-step)))  MASKED_WITH UINT64_MASK_32;
		high >>= step;
	}
}

uint64 uint64::operator >>(unsigned short step) const
{
	uint64 temp(*this);
	
	temp.rot_right(step);
	return temp;
}


/* Unref YNP
void uint64::rot_left(unsigned short step)
{
if (step >64){
low = high = 0;
return;
}

  if(step >= 32){
  high = (low >> (step -32)) MASKED_WITH UINT64_MASK_32;
  low = 0;
  }else{
  high = ((high << step) | (low >> (32-step)))  MASKED_WITH UINT64_MASK_32;
  low = (low << step)  MASKED_WITH UINT64_MASK_32;
  }
  }
*/

/* Unref YNP
uint64 uint64::operator <<(unsigned short step) const
{
uint64 temp(*this);

  temp.rot_left(step);
  return temp;
  }
*/

/*
int uint64::operator ==(const uint64 &other) const
{
return (high == other.high ? low == other.low : 0);
}
*/
int uint64::operator !=(const uint64 &other) const
{
	return (high != other.high ? 1 : low != other.low);
}


#endif                      
#endif
