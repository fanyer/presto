/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/xmlparser/xmlcommon.h"
#include "modules/xmlparser/xmldoctype.h"

#ifdef XML_VALIDATING_ELEMENT_CONTENT

extern void **
XMLGrowArray (void **old_array, unsigned count, unsigned &total);

#define GROW_ARRAY(array, type) (array = (type *) XMLGrowArray ((void **) array, array##_count, array##_total))

#ifdef XML_COMPATIBILITY_CONTENT_MODEL_DETERMINISTIC

BOOL
XMLDoctype::Element::IsDeterministic ()
{
  BOOL is_deterministic = TRUE;

  if (content_model == CONTENT_MODEL_children)
    {
      unsigned children_count = content->CountChildren (), child_index;

      if (children_count > 0)
        {
          ContentItem **children = OP_NEWA_L(ContentItem *, children_count * 2), **children_ptr = children, **followers = children + children_count, **followers_ptr;

          followers_ptr = followers;

          is_deterministic = FALSE;

          if (content->AddFollower (followers, followers_ptr, content))
            {
              content->GetChildren (children_ptr);

              for (child_index = 0; child_index < children_count; ++child_index)
                {
                  followers_ptr = followers;

                  if (!children[child_index]->CheckFollowers (followers, followers_ptr))
                    break;
                }

              if (child_index == children_count)
                is_deterministic = TRUE;
            }

          OP_DELETEA(children);
        }
    }

  return is_deterministic;
}

#endif // XML_COMPATIBILITY_CONTENT_MODEL_DETERMINISTIC

BOOL
XMLDoctype::Element::CheckElementContent (const uni_char **children, unsigned children_count, BOOL has_cdata, unsigned &index)
{
  switch (content_model)
    {
    case CONTENT_MODEL_ANY:
      return TRUE;

    case CONTENT_MODEL_EMPTY:
      return !has_cdata && children_count == 0;

    case CONTENT_MODEL_Mixed:
      return content->CheckChildren (children, children_count, index) && index == children_count;

    case CONTENT_MODEL_children:
      return !has_cdata && content->CheckChildren (children, children_count, index) && index == children_count;

    default:
      /* Doesn't happen. */
      return FALSE;
    }
}

BOOL
XMLDoctype::Element::ContentItem::CheckChildren (const uni_char **children, unsigned children_count, unsigned &matched)
{
  unsigned max_repeats = flag == FLAG_RepeatZeroOrMore || flag == FLAG_RepeatOneOrMore ? ~0u : 1, repeats = 0, local_matched = 0;

  matched = 0;

  while (repeats < max_repeats)
    if (!CheckChildrenInner (children + matched, children_count - matched, local_matched))
      break;
    else
      {
        ++repeats;
        if (local_matched == 0)
          break;
        matched += local_matched;
      }

  return repeats > 0 || flag == FLAG_Optional || flag == FLAG_RepeatZeroOrMore;
}

BOOL
XMLDoctype::Element::ContentItem::CheckChildrenInner (const uni_char **children, unsigned children_count, unsigned &matched)
{
  if (type == TYPE_child)
    if (children_count != 0 && uni_strcmp (data.name, children[0]) == 0)
      {
        matched = 1;
        return TRUE;
      }
    else
      return FALSE;
  else
    {
      ContentItem **items = data.group->GetItems ();
      unsigned items_count = data.group->GetItemsCount ();

      if (data.group->GetType () == XMLDoctype::Element::ContentGroup::TYPE_choice)
        {
          BOOL matched_empty = FALSE;

          matched = 0;

          for (unsigned index = 0; index < items_count; ++index)
            if (items[index]->CheckChildren (children, children_count, matched))
              if (matched == 0)
                matched_empty = TRUE;
              else
                return TRUE;

          return matched_empty;
        }
      else
        {
          matched = 0;

          for (unsigned index = 0, local_matched = 0; index < items_count; ++index)
            if (!items[index]->CheckChildren (children + matched, children_count - matched, local_matched))
              return FALSE;
            else
              matched += local_matched;

          return TRUE;
        }
    }
}

#ifdef XML_COMPATIBILITY_CONTENT_MODEL_DETERMINISTIC

unsigned
XMLDoctype::Element::ContentItem::CountChildren ()
{
  if (type == TYPE_child)
    return 1;
  else
    {
      ContentItem **items = data.group->GetItems ();
      unsigned items_count = data.group->GetItemsCount (), children = 0;

      for (unsigned index = 0; index < items_count; ++index)
        children += items[index]->CountChildren ();

      return children;
    }
}

void
XMLDoctype::Element::ContentItem::GetChildren (ContentItem **&children)
{
  if (type == TYPE_child)
    *children++ = this;
  else
    {
      ContentItem **items = data.group->GetItems ();
      unsigned items_count = data.group->GetItemsCount ();

      for (unsigned index = 0; index < items_count; ++index)
        items[index]->GetChildren (children);
    }
}

BOOL
XMLDoctype::Element::ContentItem::CheckFollowers (ContentItem **followers, ContentItem **&followers_ptr)
{
  if (flag == FLAG_RepeatZeroOrMore || flag == FLAG_RepeatOneOrMore)
    if (!AddFollower (followers, followers_ptr, this))
      return FALSE;

  if (parent)
    {
      XMLDoctype::Element::ContentGroup *group = parent->data.group;

      if (group->GetType () == XMLDoctype::Element::ContentGroup::TYPE_seq)
        {
          ContentItem **items = group->GetItems ();
          unsigned items_count = group->GetItemsCount (), items_index = 0;

          while (items[items_index++] != this);

          if (!parent->AddFollowersFromSequence (followers, followers_ptr, items_index))
            return FALSE;

          if (items_index == items_count)
            if (!parent->CheckFollowers (followers, followers_ptr))
              return FALSE;
        }
    }

  return TRUE;
}

BOOL
XMLDoctype::Element::ContentItem::AddFollower (ContentItem **followers, ContentItem **&followers_ptr, ContentItem *follower)
{
  if (follower->type == TYPE_child)
    {
      while (followers != followers_ptr)
        if (*followers == follower)
          return TRUE;
        else if (uni_strcmp (follower->data.name, (*followers)->data.name) == 0)
          return FALSE;
        else
          ++followers;

      *followers_ptr++ = follower;
    }
  else if (follower->data.group->GetType () == XMLDoctype::Element::ContentGroup::TYPE_seq)
    {
      unsigned items_index = 0;

      if (!follower->AddFollowersFromSequence (followers, followers_ptr, items_index))
        return FALSE;
    }
  else
    {
      ContentItem **items = follower->data.group->GetItems ();
      unsigned items_count = follower->data.group->GetItemsCount ();

      for (unsigned items_index = 0; items_index < items_count; ++items_index)
        if (!AddFollower (followers, followers_ptr, items[items_index]))
          return FALSE;
    }

  return TRUE;
}

BOOL
XMLDoctype::Element::ContentItem::AddFollowersFromSequence (ContentItem **followers, ContentItem **&followers_ptr, unsigned &items_index)
{
  ContentItem **items = data.group->GetItems ();
  unsigned items_count = data.group->GetItemsCount ();

  while (items_index < items_count)
    {
      ContentItem *item = items[items_index];

      if (!AddFollower (followers, followers_ptr, item))
        return FALSE;

      if (item->flag == FLAG_None || item->flag == FLAG_RepeatOneOrMore)
        break;

      ++items_index;
    }

  return TRUE;
}

#endif // XML_COMPATIBILITY_CONTENT_MODEL_DETERMINISTIC
#endif // XML_VALIDATING_ELEMENT_CONTENT
