/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_LAYOUT_LAYOUT_POOL_H
#define MODULES_LAYOUT_LAYOUT_POOL_H

class LayoutPool
{
public:
	LayoutPool(unsigned short size);
	~LayoutPool();

	void ConstructL();
	void Destroy();

	void Clean();

	void* New(size_t nbytes);
	void Delete(void* p);

private:
	void** m_pool;
	int m_next;
	int m_size;
};

#endif // !MODULES_LAYOUT_LAYOUT_POOL_H
