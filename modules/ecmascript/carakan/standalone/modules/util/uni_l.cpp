/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef UNI_L_IS_FUNCTION

/* Implement a UNI_L function that hashes on the address of the input and returns a
   unicode string.
   
   This code does not meet the coding standards, but then it's not a part of Opera,
   so who really cares... 
   
   Inside Opera the initialization check would be factored out (and there would be
   a similar shutdown sequence, elsewhere).  The point is merely that this code would
   be fast enough for production use, and if you're not low on memory the memory
   footprint would be unproblematic too.
   */

struct UNIL_Node
{
	const char *s;
	uni_char *u;
	UNIL_Node *next;
};

static UNIL_Node **table;
static int tablesize;
static int population;

inline int
hash(const char *s, int tablesize)
{
	return (int)(((UINTPTR)s >> 3) % tablesize);
}

const uni_char* UNI_L(const char *s)
{
	// Initialize?
	if (table == NULL)
	{
		tablesize = 10;
		table = new (ELeave) UNIL_Node*[tablesize];
		for ( int i=0 ; i < tablesize ; i++ )
			table[i] = NULL;
	}
	
	// Present?
	int h = hash(s, tablesize);
	
	UNIL_Node *p = table[h];
	while (p != NULL)
	{
		if (p->s == s)
			return p->u;
		p = p->next;
	}
	
	// Rehash?
	if (population > tablesize)
	{
		int new_tablesize = tablesize + tablesize / 2;
		UNIL_Node **new_table = new (ELeave) UNIL_Node*[new_tablesize];
		for ( int i=0 ; i < new_tablesize ; i++ )
			new_table[i] = NULL;
		for ( int i=0 ; i < tablesize ; i++ )
		{
			UNIL_Node *p = table[i];
			while (p != NULL)
			{
				UNIL_Node *q = p->next;
				int h = hash(p->s, new_tablesize);
				p->next = new_table[h];
				new_table[h] = p;
				p = q;
			}
		}
		delete [] table;
		tablesize = new_tablesize;
		table = new_table;
	}
	
	// Insert new
	p = new (ELeave) UNIL_Node;
	p->next = table[h];
	p->s = s;
	int len = op_strlen(s);
	p->u = new (ELeave) uni_char[len+1];
	for ( int i=0 ; i <= len ; i++ )
		p->u[i] = s[i];
	table[h] = p;
	population++;
	
	return p->u;
}

#endif
