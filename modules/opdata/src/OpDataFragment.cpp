/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#include "modules/opdata/src/OpDataFragment.h"


class CopyListHelper : public OpDataFragment::TraverseHelper
{
public:
	CopyListHelper(size_t first_offset, size_t last_length)
		: m_last(NULL)
		, m_first_offset(first_offset)
		, m_last_length(last_length)
		, m_new_first(NULL)
		, m_new_last(NULL)
		, m_done(false)
	{}

	/// Copy the given fragment and append it to our copied fragments list.
	virtual bool ProcessFragment(const OpDataFragment *frag)
	{
		OP_ASSERT(m_last);
		OP_ASSERT(m_first_offset < frag->Length());
		size_t offset = frag->Offset() + m_first_offset;
		size_t length = frag->Length() - m_first_offset;
		if (frag == m_last)
		{
			OP_ASSERT(m_last_length <= frag->Length());
			length = m_last_length - m_first_offset;
		}
		m_first_offset = 0;

		OpDataFragment *copy = OpDataFragment::Create(frag->Chunk(), offset, length);
		if (!copy) // OOM
		{
			OP_ASSERT(!m_done);
			return true;
		}

		copy->Chunk()->Use();

		if (!m_new_first) // this is the first fragment
			m_new_first = copy;
		else // this is not the first fragment
		{
			OP_ASSERT(m_new_last);
			m_new_last->SetNext(copy);
		}

		m_new_last = copy;

		if (frag == m_last) // this is the last fragment
		{
			m_done = true;
			return true;
		}
		return false;
	}

	/// Register the last source fragment.
	virtual void SetTraverseProperties(const OpDataFragment *, const OpDataFragment *last, unsigned int)
	{
		m_last = last;
	}

	/// Return true if we've successfully finished copying.
	bool Done(void) const { return m_done; }

	/// Return the first fragment in the copied fragment list
	OpDataFragment *First(void) const { return m_new_first; }

	/// Return the first fragment in the copied fragment list
	OpDataFragment *Last(void) const { return m_new_last; }

private:
	const OpDataFragment *m_last;
	size_t m_first_offset;
	size_t m_last_length;
	OpDataFragment *m_new_first;
	OpDataFragment *m_new_last;
	bool m_done;
};

/* static */ OP_STATUS
OpDataFragment::CopyList(
	const OpDataFragment *first,
	const OpDataFragment *last,
	size_t first_offset,
	size_t last_length,
	OpDataFragment **new_first,
	OpDataFragment **new_last)
{
	OP_ASSERT(first && last);
	OP_ASSERT(first_offset < first->Length());
	OP_ASSERT(last_length && last_length <= last->Length());
	OP_ASSERT(first != last || last_length > first_offset);

	// first, try to copy fragments
	CopyListHelper helper(first_offset, last_length);
	first->Traverse(&helper, last);
	if (!helper.Done()) // OOM
	{
		// release new fragments
		if (helper.First())
			helper.First()->Traverse(NULL, helper.Last(),
				TRAVERSE_ACTION_RELEASE | TRAVERSE_INCLUDE_LAST);
		return OpStatus::ERR_NO_MEMORY;
	}

	// success; update out-params
	*new_first = helper.First();
	*new_last = helper.Last();
	return OpStatus::OK;
}

class ListsOverlapHelper : public OpDataFragment::TraverseHelper
{
public:
	ListsOverlapHelper(const OpDataFragment *frag) : m_frag(frag) , m_result(false) {}

	/// Copy the given fragment and append it to our copied fragments list.
	virtual bool ProcessFragment(const OpDataFragment *frag)
	{
		if (!m_result)
			m_result = frag == m_frag;
		return m_result;
	}

	/// Register the last source fragment.
	virtual void SetTraverseProperties(const OpDataFragment *, const OpDataFragment *last, unsigned int)
	{
		if (last == m_frag)
			m_result = true;
	}

	/// Return true if m_frag has been found
	bool Result(void) const { return m_result; }
private:
	const OpDataFragment *m_frag;
	bool m_result;
};

/* static */ bool
OpDataFragment::ListsOverlap(
	const OpDataFragment *a_first,
	const OpDataFragment *a_last,
	const OpDataFragment *b_first,
	const OpDataFragment *b_last)
{
	ListsOverlapHelper a_helper(b_last);
	a_first->Traverse(&a_helper, a_last);
	if (a_helper.Result())
		return true;
	ListsOverlapHelper b_helper(a_last);
	b_first->Traverse(&b_helper, b_last);
	return b_helper.Result();
}

OpDataFragment *
OpDataFragment::Traverse(
	TraverseHelper *helper,
	OpDataFragment *last /* = NULL */,
	unsigned int flags /* = 0 */)
{
	bool act_use = (flags & TRAVERSE_ACTION_USE) != 0;
	bool act_release = (flags & TRAVERSE_ACTION_RELEASE) != 0;
	bool skip_first = (flags & TRAVERSE_EXCLUDE_FIRST) != 0;
	bool do_last = (flags & TRAVERSE_INCLUDE_LAST) != 0;
	OP_ASSERT(!(act_use && act_release) || !"Cannot _both_ USE and RELEASE fragment");

	if (helper)
		helper->SetTraverseProperties(this, last, flags);
	OpDataFragment *frag = this, *next;
	while (frag)
	{
		if ((helper && helper->ProcessFragment(frag)) || frag == last)
			break;
		next = frag->Next();
		if (!skip_first || frag != this)
		{
			if (act_use)
				frag->Use();
			else if (act_release)
				frag->Release();
		}
		frag = next;
	}
	if (frag && do_last)
	{
		if (act_use)
			frag->Use();
		else if (act_release)
			frag->Release();
	}
	return frag;
}

const OpDataFragment *
OpDataFragment::Traverse(
	TraverseHelper *helper,
	const OpDataFragment *last /* = NULL */,
	unsigned int flags /* = 0 */) const
{
	OP_ASSERT((flags & (TRAVERSE_ACTION_USE | TRAVERSE_ACTION_RELEASE)) == 0 || !"Cannot USE or RELEASE fragments in const mode");
	OP_ASSERT(helper);

	helper->SetTraverseProperties(this, last, flags);
	const OpDataFragment *frag = this;
	while (frag)
	{
		if ((helper && helper->ProcessFragment(frag)) || frag == last)
			break;
		frag = frag->Next();
	}
	return frag;
}

class LookupHelper : public OpDataFragment::TraverseHelper
{
public:
	LookupHelper(size_t *index, bool prefer_end)
		: m_index(index), m_prefer_end(prefer_end)
	{}

	/// Traverse until m_index is found within the given fragment.
	virtual bool ProcessFragment(const OpDataFragment *frag)
	{
		if (*m_index < frag->Length() ||
		    (m_prefer_end && *m_index == frag->Length()))
			return true;
		*m_index -= frag->Length();
		return false;
	}

private:
	size_t *m_index;
	const bool m_prefer_end;
};

OpDataFragment *
OpDataFragment::Lookup(size_t *index, unsigned int flags /* = 0 */)
{
	LookupHelper helper(index, (flags & LOOKUP_PREFER_LENGTH) != 0);
	return Traverse(&helper, NULL, flags);
}
