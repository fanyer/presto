/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Include file for OpMemUpperTen object
 * 
 * You must include this file if you want to use the OpMemUpperTen object
 * used to rank allocation sites.
 *
 * \author Morten Rolland, mortenro@opera.com
 */

#ifndef MEMORY_UPPERTEN_H
#define MEMORY_UPPERTEN_H

class OpMemUpperTen
{
public:
	OpMemUpperTen(int size = 10);
	~OpMemUpperTen(void);

	void Add(UINTPTR ptr, size_t arg2, int arg3);
	void Show(const char* title, const char* format);

private:
	int size;
	int total_bytes;
	int total_count;
	BOOL valid;
	UINTPTR* ptr1;
	size_t* arg2;
	int* arg3;
};

#endif // MEMORY_UPPERTEN_H
