/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * Morten Rolland, mortenro@opera.com
 */

#ifndef MEMORY_GROUP_H
#define MEMORY_GROUP_H

class OpMemGroup
{
public:
	OpMemGroup(void);
	~OpMemGroup(void);

	void* NewGRO(size_t size);
	void* NewGRO(size_t size, const char* file, int line, const char* obj);

	void* NewGRO_L(size_t size);
	void* NewGRO_L(size_t size, const char* file, int line, const char* obj);

	void ReleaseAll(void);

private:
	void Reset(void)
	{
		all = 0;
		primary_ptr = 0;
		secondary_ptr = 0;
		primary_size = 0;
		secondary_size = 0;
	}

	void* all; ///< All allocations in this group chained together

	char* primary_ptr;	///< Primary next allocation address
	char* secondary_ptr;	///< Secondary next allocation address

	unsigned int primary_size; ///< Free bytes in primary allocation segment
	unsigned int secondary_size;	///< Free bytes in secondary allocations segment
};

#endif // MEMORY_GROUP_H
