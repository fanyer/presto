/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CLASS_ATTRIBUTE_H
#define CLASS_ATTRIBUTE_H

#include "modules/logdoc/complex_attr.h"

class HTMLClassNames;

/** Class for representing a unique class name referenced by ClassAttribute instances,
	class selectors, etc. ReferencedHTMLClass objects are stored in HTMLClassNames hash tables. */

class ReferencedHTMLClass
{
private:

	/** The class string. */
	uni_char* m_class;

	/** Pointer back to the hash table where this reference is kept. */
	HTMLClassNames* m_classnames;

	/** Reference count. The object is deleted when the count reaches 0. */
	unsigned int m_ref_count;

	/** Remove this element from the hashtable and delete it. */
	void Delete();

public:

	/** Constructor.

		@param cls_str A heap allocated string representation for this class.
					   This object is responsible for deleting the string.
		@param classnames The hash table that this reference is added to. */

	ReferencedHTMLClass(uni_char* cls_str, HTMLClassNames* classnames)
		: m_class(cls_str), m_classnames(classnames), m_ref_count(0) {}

	/** Destructor. */

	~ReferencedHTMLClass() { OP_ASSERT(m_ref_count == 0); OP_DELETEA(m_class); }

	/** Increment reference count. */

	void Ref() { m_ref_count++; }

	/** Decrement reference count and delete and remove the reference
		from the hash table if the reference count has reached 0. */

	void UnRef() { if (--m_ref_count == 0) Delete(); }

	/** Return the string representation for the class that this reference is for. */

	const uni_char* GetString() const { return m_class; }

	/** If the hash table is deleted, but this reference is still alive,
		reset the hash table pointer to avoid access to the deleted hash table. */

	void PoolDeleted() { m_classnames = NULL; }
};


/** Class for representing HTML CLASS attributes. */

class ClassAttribute : public ComplexAttr
{
public:
	/** Constructor.

		@param string original attribute string.
		@param single_class If the class attribute contains a single class,
							its reference is sent in here. For attributes
							with more classes, the class references are all
							added with the Construct call. */

	ClassAttribute(uni_char* string, ReferencedHTMLClass* single_class) : m_class(single_class), m_string(string) {}

	/** Destructor. Calls UnRef() on all class references,
		deletes class reference array if present, and deletes
		original attribute string. */

	virtual ~ClassAttribute();

	/** Allocate and populate a list of class references taken from the vector argument. */

	OP_STATUS Construct(const OpVector<ReferencedHTMLClass>& classes);

	/** Create a copy of this attribute. */

	virtual OP_STATUS CreateCopy(ComplexAttr **copy_to);

	/** Append the string representation of this attribute to the buffer. */

	virtual OP_STATUS ToString(TempBuffer *buffer) { return buffer->Append(m_string); }

	/** Move this attribute to another document.
		That means getting new class references from the new document. */

	virtual BOOL MoveToOtherDocument(FramesDocument *old_document, FramesDocument *new_document);

	/** Compare with other ClassAttribute. */

	virtual BOOL Equals(ComplexAttr* other) { OP_ASSERT(other); return uni_strcmp(GetAttributeString(), static_cast<ClassAttribute*>(other)->GetAttributeString()) == 0; }

	/** Return the original attribute string. */

	const uni_char* GetAttributeString() const { return m_string; }

	/** Match with reference */

	BOOL MatchClass(const ReferencedHTMLClass* match_ref) const
	{
		if (IsSingleClass())
			return m_class == match_ref;
		else
		{
			ReferencedHTMLClass** cur = GetClassList();
			while (*cur != NULL)
				if (*cur++ == match_ref)
					return TRUE;

			return FALSE;
		}
	}

	/** Match with string */

	BOOL MatchClass(const uni_char* match_string, BOOL case_sensitive) const
	{
		int (*compare_fun)(const uni_char*, const uni_char*);

		if (case_sensitive)
			compare_fun = uni_strcmp;
		else
			compare_fun = uni_stricmp;

		if (IsSingleClass())
		{
			if (m_class)
				return compare_fun(match_string, m_class->GetString()) == 0;
		}
		else
		{
			ReferencedHTMLClass** cur = GetClassList();
			while (*cur != NULL)
				if (compare_fun(match_string, (*cur++)->GetString()) == 0)
					return TRUE;
		}
		return FALSE;
	}

	/** Return the class reference for the nth class. Iterate from 0.
		When you reach the end of the reference list, NULL is returned. */
	const ReferencedHTMLClass* GetClassRef(unsigned int i) const
	{
		if (IsSingleClass())
			return i == 0 ? m_class : NULL;
		else
			return GetClassList()[i];
	}

	/** Same as GetClassRef, but allows out-of-range queries.
		@param i Iterates from 0.
		@return For i from <0, length-1> returns class reference for the ith class.
		    For i out of range returns NULL. */
	const ReferencedHTMLClass* GetClassRefSafe(unsigned int i) const
	{
		if (IsSingleClass())
			return i == 0 ? m_class : NULL;
		else
		{
			ReferencedHTMLClass** cl = GetClassList();
			while(*cl && i--)
				cl++;
			return *cl;
		}
	}


	unsigned int GetLength() const
	{
		if (IsSingleClass())
			return m_class ? 1 : 0;
		else
		{
			ReferencedHTMLClass** class_list = GetClassList();
			unsigned int len = 0;
			while (*class_list++)
				len++;
			return len;
		}
	}

private:

	/** Returns TRUE if this attribute has a single class
		for which we use the m_class pointer in the union. */

	BOOL IsSingleClass() const { return (reinterpret_cast<UINTPTR>(m_class) & 1) == 0; }

	/** Set the class reference array. */

	void SetClassList(ReferencedHTMLClass** class_list) { m_class_list = reinterpret_cast<ReferencedHTMLClass**>(reinterpret_cast<UINTPTR>(class_list) | 1); }

	/** Return the class reference array. Must not be called
		without checking that IsSingleClass is FALSE. */

	ReferencedHTMLClass** GetClassList() const
	{
		OP_ASSERT(!IsSingleClass());
		return reinterpret_cast<ReferencedHTMLClass**>(reinterpret_cast<UINTPTR>(m_class_list) & ~1);
	}

	/** This union either stores a referenced class from the class name hash, or an array of such references. */

	union
	{
		ReferencedHTMLClass* m_class;
		ReferencedHTMLClass** m_class_list;
	};

	/** Original class attribute string. */

	uni_char* m_string;
};

#endif // CLASS_ATTRIBUTE_H
