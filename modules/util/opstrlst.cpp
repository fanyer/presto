/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef USE_UTIL_OPSTRING_LIST

#include "modules/util/opstrlst.h"

OpString_list::~OpString_list()
{
	Clear();
}

void OpString_list::Clear()
{
	if (vectorlist)
	{
		for (unsigned long i = 0; i < length; i ++)
			OP_DELETE(vectorlist[i]);
		OP_DELETEA(vectorlist);
		vectorlist = NULL;
	}
	alloclength = 0;
	length = 0;
}

OP_STATUS OpString_list::Resize(unsigned long nlength)
{
	OpString **temp,*item;
	unsigned long i,newalloclength;
	
	if (nlength != length)
	{
		if(nlength > alloclength)
		{
			newalloclength = nlength+32;
			temp = OP_NEWA(OpString *, newalloclength);
			if(temp == NULL)
			{
				return OpStatus::ERR_NO_MEMORY;
			}
			for(i=0;i<length;i++)
				temp[i] = vectorlist[i];
			for(;i<newalloclength;i++)
				temp[i] = NULL;
	
			if(length)
				OP_DELETEA(vectorlist);
			vectorlist = temp;
			alloclength = newalloclength;
		}
		if(length <nlength)
		{
			for(i=length;i<nlength;i++)
			{
				vectorlist[i] = item = OP_NEW(OpString, ());
				if(item == NULL)
				{
					for(;i>length;)
					{
						i--;
						OP_DELETE(vectorlist[i]);
						vectorlist[i] = NULL;
					}
	
					return OpStatus::ERR_NO_MEMORY;
				}
			}
		}
		else
		{
			for(i = nlength;i<length;i++)
			{
				OP_DELETE(vectorlist[i]);
				vectorlist[i] = NULL;
			}
			if(nlength == 0)
			{
				OP_DELETEA(vectorlist);
				vectorlist = NULL;
				alloclength = 0;
			}
		}
		length = nlength;
	}
	return OpStatus::OK;
}

OpString &OpString_list::Item(unsigned long index)
{
	if(index >= length)
	{
        return *g_empty_opstring;
	}
	return *vectorlist[index];
}

const OpString &OpString_list::Item(unsigned long index) const
{
	if(index >= length)
	{
	   	return *g_empty_opstring;
 	}
	return *vectorlist[index];
}

#endif // USE_UTIL_OPSTRING_LIST
