/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef TYPEDESCRIPTOR_H
#define TYPEDESCRIPTOR_H

#include "modules/search_engine/SearchUtils.h"

#if defined _DEBUG || defined SELFTEST
#define ESTIMATE_MEMORY_USED_AVAILABLE
#endif

/**
 * @brief Description of the basic properties of a type.
 * @author Pavel Studeny <pavels@opera.com>
 *
 * Basic description of a type used by VectorBase or BTreeBase.
 */
struct TypeDescriptor
{
	typedef CHECK_RESULT(OP_STATUS (* AssignPtr)(void *dst, const void *src));
	typedef void (* DestructPtr)(void *item);
	typedef BOOL (* ComparePtr)(const void *left, const void *right);
#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
	typedef size_t (* EstimateMemoryUsedPtr)(const void *item);
#endif

	TypeDescriptor(size_t type_size, AssignPtr assign, ComparePtr compare, DestructPtr destruct
#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
		, EstimateMemoryUsedPtr estimateMemoryUsed
#endif
		)
	{
		size = type_size;
		size_align32 = (size_t)((type_size+3) & ~3);
		Assign = assign;
		Destruct = destruct;
		Compare = compare;
#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
		EstimateMemoryUsed = estimateMemoryUsed;
#endif
	}

	size_t size;
	size_t size_align32;
	AssignPtr Assign;
	DestructPtr Destruct;
	ComparePtr Compare;
#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
	EstimateMemoryUsedPtr EstimateMemoryUsed;
#endif
};

/**
 * @brief Default TypeDescriptor for non-pointer types. Used for integers etc.
 */
template <typename T> struct DefDescriptor : public TypeDescriptor
{
	DefDescriptor(void) : TypeDescriptor(sizeof(T), &Assign, &Compare, &Destruct
#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
		, &EstimateMemoryUsed
#endif
		) {}

	CHECK_RESULT(static OP_STATUS Assign(void *dst, const void *src))
	{
		*(T *)dst = *(T *)src;

		return OpStatus::OK;
	}

	static void Destruct(void *item)
	{
		((T *)item)->~T();
	}

	static BOOL Compare(const void *left, const void *right)
	{
		return (*(T *)left < *(T *)right);
	}

#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
	static size_t EstimateMemoryUsed(const void *item)
	{
		return sizeof(T);
	}
#endif
};

/**
 * @brief Default TypeDescriptor for pointers.
 * Warning: Do not use pointer to objects on instantiation of template, 
 * that will give memory leaks. Do not use PtrDescriptor<int*>, 
 * do use use PtrDescriptor<int>
 */
template <typename T> struct PtrDescriptor : public TypeDescriptor
{
	PtrDescriptor(void) : TypeDescriptor(sizeof(T *), &Assign, &Compare, &Destruct
#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
		, &EstimateMemoryUsed
#endif
		) {}

	CHECK_RESULT(static OP_STATUS Assign(void *dst, const void *src))
	{
		*(T **)dst = *(T **)src;

		return OpStatus::OK;
	}

	static void Destruct(void *item)
	{
		OP_DELETE(*(T **)item);
	}

	static void DestructArray(void *item)
	{
		OP_DELETEA(*(T **)item);
	}

	static BOOL Compare(const void *left, const void *right)
	{
		return (*(T **)left < *(T **)right);
	}

#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
	static size_t EstimateMemoryUsed(const void *item)
	{
		return sizeof(T*) + (*(const T **)item)->EstimateMemoryUsed() + 2*sizeof(size_t);
	}
#endif
};

#endif  // TYPEDESCRIPTOR_H

