/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef LINK_OBJECT_STORE_H
#define LINK_OBJECT_STORE_H

class HashedLink;

/** A doubly-linked and hashed list. */
class LinkObjectStore
{
public:

  					LinkObjectStore(unsigned int size, int (*cmpr)(const char *, const char *)=NULL);
	OP_STATUS		Construct();
  	virtual			~LinkObjectStore();

	unsigned long	LinkObjectCount() { return count; }

	HashedLink*		GetLinkObject(const char* id, unsigned int *ret_index=NULL);
	void			AddLinkObject(HashedLink* obj, unsigned int *pre_index=NULL);
	void			RemoveLinkObject(HashedLink* obj);

	HashedLink*		GetFirstLinkObject();
	HashedLink*		GetNextLinkObject();

	static unsigned short GetHashIdx(const char* str, unsigned short max_idx, unsigned int &full_index);

private:
		// dummy copy constructor

	LinkObjectStore(const LinkObjectStore&);
	LinkObjectStore& operator=(const LinkObjectStore&);

protected:
	Head*			store_array;
  	unsigned int	store_size;

  	unsigned int	last_idx;
  	HashedLink*		last_link_obj;

  	unsigned long	count;
	int				(*compare)(const char *, const char *);
};

#endif // LINK_OBJECT_STORE_H
