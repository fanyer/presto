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
#include "modules/xmlparser/xmlinternalparser.h"

#include "modules/util/OpHashTable.h"
#include "modules/util/tempbuf.h"

#if 0
# include "modules/newdebug/src/profiler.h"
# define MARKUP_PROFILE_DOCTYPE(id) OP_PROFILE(id)
#else
# define MARKUP_PROFILE_DOCTYPE(id)
#endif

unsigned
XMLLength (const uni_char *string, unsigned string_length)
{
  if (string_length == ~0u)
    return uni_strlen (string);
  else
    return string_length;
}

static UINT32
XMLHashString (const uni_char *string, unsigned string_length)
{
  UINT32 hash = string_length;

  while (string_length-- > 0)
    hash = hash + hash + hash + string[string_length];

  return hash;
}

static BOOL
XMLNameCompare (const XMLName &name1, const XMLName &name2)
{
  return name1.name_length == name2.name_length && op_memcmp (name1.name, name2.name, UNICODE_SIZE (name1.name_length)) == 0;
}

class XMLNameHashFunctions
  : public OpHashFunctions
{
public:
  virtual UINT32 Hash (const void* key);
  virtual BOOL KeysAreEqual (const void* key1, const void* key2);
};

/* virtual */ UINT32
XMLNameHashFunctions::Hash (const void* key)
{
  const XMLName *name = (const XMLName *) key;
  return XMLHashString (name->name, name->name_length);
}

/* virtual */ BOOL
XMLNameHashFunctions::KeysAreEqual (const void* key1, const void* key2)
{
  const XMLName *name1 = (const XMLName *) key1;
  const XMLName *name2 = (const XMLName *) key2;

  return XMLNameCompare (*name1, *name2);
}

class XMLAttributeNameHashFunctions
  : public OpHashFunctions
{
public:
  virtual UINT32 Hash (const void* key);
  virtual BOOL KeysAreEqual (const void* key1, const void* key2);
};

/* virtual */ UINT32
XMLAttributeNameHashFunctions::Hash (const void* key)
{
  const XMLAttributeName *name = (const XMLAttributeName *) key;
  return XMLHashString (name->elemname.name, name->elemname.name_length) ^ XMLHashString (name->attrname.name, name->attrname.name_length);
}

/* virtual */ BOOL
XMLAttributeNameHashFunctions::KeysAreEqual (const void* key1, const void* key2)
{
  const XMLAttributeName *name1 = (const XMLAttributeName *) key1;
  const XMLAttributeName *name2 = (const XMLAttributeName *) key2;

  return XMLNameCompare (name1->elemname, name2->elemname) && XMLNameCompare (name1->attrname, name2->attrname);
}

void **
XMLGrowArray (void **old_array, unsigned count, unsigned &total)
{
  if (count == total)
    {
      unsigned new_total = total ? total + total : 8;

      void **new_array = OP_NEWA_L(void *, new_total);

      op_memcpy (new_array, old_array, count * sizeof old_array[0]);
      OP_DELETEA(old_array);

      total = new_total;
      return new_array;
    }
  else
    return old_array;
}

#define GROW_ARRAY(array, type) (array = (type *) XMLGrowArray ((void **) array, array##_count, array##_total))

static void *
XML_GetFromTable (OpHashTable *table, const uni_char *name, unsigned name_length)
{
  if (table)
    {
      XMLName n;

      n.name = (uni_char *) name;
      n.name_length = XMLLength (name, name_length);

      void *data;

      if (table->GetData (&n, &data) == OpStatus::OK)
        return data;
    }

  return 0;
}

static void
XML_AddToTable (OpHashTable *&table, OpHashFunctions *hashfunctions, void *name, void *data)
{
  if (!table)
    table = OP_NEW_L(OpHashTable, (hashfunctions));

  table->AddL (name, data);
}

XMLExternalId::XMLExternalId ()
  : pubid (0),
    system (0)
{
}

XMLExternalId::~XMLExternalId ()
{
  OP_DELETEA(pubid);
  OP_DELETEA(system);
}

#ifdef XML_STORE_ATTRIBUTES

XMLDoctype::Attribute::Attribute (BOOL declared_in_external_subset)
  : type (TYPE_Unknown),
    flag (FLAG_None),
    declared_in_external_subset (declared_in_external_subset),
#ifdef XML_ATTRIBUTE_DEFAULT_VALUES
    default_value (0),
#endif // XML_ATTRIBUTE_DEFAULT_VALUES
    enumerators (0),
    enumerators_count (0),
    enumerators_total (0)
{
  name.elemname.name = 0;
  name.attrname.name = 0;

  XML_OBJECT_CREATED (XMLDoctypeAttribute);
}

XMLDoctype::Attribute::~Attribute ()
{
  OP_DELETEA(name.elemname.name);
  OP_DELETEA(name.attrname.name);

#ifdef XML_ATTRIBUTE_DEFAULT_VALUES
  OP_DELETEA(default_value);
#endif // XML_ATTRIBUTE_DEFAULT_VALUES

  for (unsigned index = 0; index < enumerators_count; ++index)
    OP_DELETEA(enumerators[index]);

  OP_DELETEA(enumerators);

  XML_OBJECT_DESTROYED (XMLDoctypeAttribute);
}

void
XMLDoctype::Attribute::SetElementName (const uni_char *elemname, unsigned elemname_length)
{
  XMLInternalParser::CopyString (name.elemname.name, elemname, elemname_length);
  name.elemname.name_length = elemname_length;
}

void
XMLDoctype::Attribute::SetAttributeName (const uni_char *attrname, unsigned attrname_length)
{
  XMLInternalParser::CopyString (name.attrname.name, attrname, attrname_length);
  name.attrname.name_length = attrname_length;
}

#ifdef XML_ATTRIBUTE_DEFAULT_VALUES
void
XMLDoctype::Attribute::SetDefaultValue (const uni_char *value, unsigned value_length)
{
  XMLInternalParser::CopyString (default_value, value, value_length);
}
#endif // XML_ATTRIBUTE_DEFAULT_VALUES

void
XMLDoctype::Attribute::AddEnumerator (const uni_char *name, unsigned name_length)
{
  GROW_ARRAY (enumerators, uni_char *);
  XMLInternalParser::CopyString (enumerators[enumerators_count++], name, name_length);
}

BOOL
XMLDoctype::Attribute::Match (const uni_char *elem_name, const uni_char *attr_name)
{
  return XMLInternalParser::CompareStrings (elem_name, name.elemname.name) && XMLInternalParser::CompareStrings (attr_name, name.attrname.name);
}

BOOL
XMLDoctype::Attribute::Match (const uni_char *other_name, unsigned other_name_length)
{
  return name.attrname.name_length == other_name_length && op_memcmp (name.attrname.name, other_name, UNICODE_SIZE (other_name_length)) == 0;
}

void
XMLDoctype::AddAttribute (Attribute *attribute)
{
  void *data;

  if (attributetable && attributetable->GetData (&attribute->name, &data) == OpStatus::OK)
    {
      OP_DELETE(attribute);
      return;
    }

  if (external_subset && !external_subset_read_only)
    external_subset->AddAttribute (attribute);
  else
    {
      current_attribute = attribute;

#ifdef XML_STORE_ELEMENTS
      if (Element *element = GetElement (attribute->GetElementName ()))
        element->AddAttribute (attribute);
      else
#endif // XML_STORE_ELEMENTS
        {
          GROW_ARRAY (attributes, Attribute *);
          attributes[attributes_count++] = attribute;
        }

      current_attribute = 0;

      if (!attributetable)
        attributetable = OP_NEW_L(OpHashTable, (attributenamehashfunctions));

      attributetable->AddL (&attribute->name, attribute);
    }
}

XMLDoctype::Attribute *
XMLDoctype::GetAttribute (const uni_char *elemname, unsigned elemname_length, const uni_char *attrname, unsigned attrname_length)
{
  if (attributetable)
    {
      XMLAttributeName name;

      name.elemname.name = (uni_char *) elemname;
      name.elemname.name_length = elemname_length;
      name.attrname.name = (uni_char *) attrname;
      name.attrname.name_length = attrname_length;

      void *attribute;

      if (attributetable->GetData (&name, &attribute) == OpStatus::OK)
        return (Attribute *) attribute;
    }

#if 0
  return external_subset ? external_subset->GetAttribute (elemname, elemname_length, attrname, attrname_length) : 0;
#else // 0
  return 0;
#endif // 0
}

#endif // XML_STORE_ATTRIBUTES

#ifdef XML_STORE_ELEMENTS

XMLDoctype::Element::Element (BOOL declared_in_external_subset)
  : attributes (0),
    attributes_count (0),
    attributes_total (0),
    declared_in_external_subset (declared_in_external_subset)
{
  name.name = 0;

#ifdef XML_VALIDATING_ELEMENT_CONTENT
  content = 0;
#endif // XML_VALIDATING_ELEMENT_CONTENT

  XML_OBJECT_CREATED (XMLDoctypeElement);
}

XMLDoctype::Element::~Element ()
{
  OP_DELETEA(name.name);

  for (unsigned index = 0; index < attributes_count; ++index)
    OP_DELETE(attributes[index]);
  OP_DELETEA(attributes);

  XML_OBJECT_DESTROYED (XMLDoctypeElement);
}

void
XMLDoctype::Element::SetName (const uni_char *new_name, unsigned new_name_length)
{
  XMLInternalParser::CopyString (name.name, new_name, new_name_length);
  name.name_length = new_name_length;
}

void
XMLDoctype::Element::AddAttribute (Attribute *attribute)
{
  GROW_ARRAY (attributes, Attribute *);
  attributes[attributes_count++] = attribute;
}

XMLDoctype::Attribute *
XMLDoctype::Element::GetAttribute (const uni_char *name, unsigned name_length)
{
  for (unsigned index = 0; index < attributes_count; ++index)
    if (attributes[index]->Match (name, name_length))
      return attributes[index];

  return 0;
}

BOOL
XMLDoctype::Element::Match (const uni_char *other_name, unsigned other_name_length)
{
  return name.name_length == other_name_length && op_memcmp (name.name, other_name, UNICODE_SIZE (other_name_length)) == 0;
}

#ifdef XML_VALIDATING_ELEMENT_CONTENT

void
XMLDoctype::Element::Open ()
{
  ContentItem *item = XMLDoctype::current_item = OP_NEW_L(ContentItem, ());
  ContentGroup *group = OP_NEW_L(ContentGroup, ());

  if (content_model == CONTENT_MODEL_Mixed)
    group->SetType (ContentGroup::TYPE_choice);

  item->SetGroup (group);

  if (!content)
    content = item;
  else
    current_group->GetGroup ()->AddItem (item);

  last_item = 0;
  current_group = item;

  XMLDoctype::current_item = 0;
}

void
XMLDoctype::Element::AddChild (const uni_char *name, unsigned name_length)
{
  ContentItem *item = XMLDoctype::current_item = OP_NEW_L(ContentItem, ());

  item->SetName (name, name_length);
  current_group->GetGroup ()->AddItem (item);

  last_item = item;
  XMLDoctype::current_item = 0;
}

BOOL
XMLDoctype::Element::HasChild (const uni_char *name, unsigned name_length)
{
  ContentItem **items = current_group->GetGroup ()->GetItems ();
  unsigned items_count = current_group->GetGroup ()->GetItemsCount ();

  for (unsigned index = 0; index < items_count; ++index)
    if (XMLInternalParser::CompareStrings (name, name_length, items[index]->GetName ()))
      return TRUE;

  return FALSE;
}

void
XMLDoctype::Element::Close ()
{
  if (current_group->GetGroup ()->GetType () == ContentGroup::TYPE_Unknown)
    SetSequence ();

  last_item = current_group;
  current_group = current_group->GetParent ();
}

void
XMLDoctype::Element::SetChoice ()
{
  current_group->GetGroup ()->SetType (ContentGroup::TYPE_choice);
}

void
XMLDoctype::Element::SetSequence ()
{
  current_group->GetGroup ()->SetType (ContentGroup::TYPE_seq);
}

void
XMLDoctype::Element::SetOptional ()
{
  last_item->SetFlag (ContentItem::FLAG_Optional);
}

void
XMLDoctype::Element::SetRepeatZeroOrMore ()
{
  last_item->SetFlag (ContentItem::FLAG_RepeatZeroOrMore);
}

void
XMLDoctype::Element::SetRepeatOneOrMore ()
{
  last_item->SetFlag (ContentItem::FLAG_RepeatOneOrMore);
}

XMLDoctype::Element::ContentItem::ContentItem ()
  : flag (FLAG_None),
    parent (0)
{
}

XMLDoctype::Element::ContentItem::~ContentItem ()
{
  if (type == TYPE_child)
    OP_DELETEA(data.name);
  else
    OP_DELETE(data.group);
}

void
XMLDoctype::Element::ContentItem::SetGroup (ContentGroup *group)
{
  type = TYPE_group;
  data.group = group;
  group->SetItem (this);
}

void
XMLDoctype::Element::ContentItem::SetName (const uni_char *name, unsigned name_length)
{
  type = TYPE_child;
  XMLInternalParser::CopyString (data.name, name, name_length);
}

XMLDoctype::Element::ContentGroup::ContentGroup ()
{
  Initialize ();
}

XMLDoctype::Element::ContentGroup::~ContentGroup ()
{
  Destroy ();
}

void
XMLDoctype::Element::ContentGroup::Initialize ()
{
  op_memset (this, 0, sizeof *this);
}

void
XMLDoctype::Element::ContentGroup::Destroy ()
{
  for (unsigned index = 0; index < items_count; ++index)
    OP_DELETE(items[index]);
  OP_DELETEA(items);
}

void
XMLDoctype::Element::ContentGroup::AddItem (ContentItem *new_item)
{
  GROW_ARRAY (items, ContentItem *);
  items[items_count++] = new_item;
  new_item->SetParent (item);
}

#endif // XML_VALIDATING_ELEMENT_CONTENT

void
XMLDoctype::AddElement (Element *element)
{
  OP_ASSERT (!GetElement (element->GetName ()));

  if (external_subset && !external_subset_read_only)
    external_subset->AddElement (element);
  else
    {
      current_element = element;

      GROW_ARRAY (elements, Element *);
      elements[elements_count++] = element;

      current_element = 0;

      XML_AddToTable (elementtable, namehashfunctions, &element->name, element);

#ifdef XML_STORE_ATTRIBUTES
      unsigned index = 0;
      while (index < attributes_count)
        if (XMLInternalParser::CompareStrings (attributes[index]->GetElementName (), element->GetName ()))
          {
            element->AddAttribute (attributes[index]);
            attributes[index] = index != --attributes_count ? attributes[attributes_count] : 0;
          }
        else
          ++index;
#endif // XML_STORE_ATTRIBUTES
    }
}

XMLDoctype::Element *
XMLDoctype::GetElement (const uni_char *name, unsigned name_length)
{
  if (Element *element = (Element *) XML_GetFromTable (elementtable, name, XMLLength (name, name_length)))
    return element;
  else
    return external_subset ? external_subset->GetElement (name, name_length) : 0;
}

#ifdef XML_VALIDATING

BOOL
XMLDoctype::GetElementHasIDAttribute (const uni_char *name, unsigned name_length)
{
  Attribute **ptr, **ptr_end;

  if (Element *element = GetElement (name, name_length))
    {
      ptr = element->GetAttributes ();
      ptr_end = ptr + element->GetAttributesCount ();
    }
  else
    {
      ptr = attributes;
      ptr_end = ptr + attributes_count;
    }

  while (ptr != ptr_end)
    {
      if (XMLInternalParser::CompareStrings (name, name_length, (*ptr)->GetElementName ()))
        if ((*ptr)->GetType () == Attribute::TYPE_Tokenized_ID)
          return TRUE;

      ++ptr;
    }

  return FALSE;
}

#endif // XML_VALIDATING
#endif // XML_STORE_ELEMENTS

XMLDoctype::Entity::Entity (BOOL declared_in_external_subset)
  : type (TYPE_General),
    value_type (VALUE_TYPE_Unknown),
    declared_in_external_subset (declared_in_external_subset),
    value (0)
{
  name.name = 0;

  XML_OBJECT_CREATED (XMLDoctypeEntity);
}

XMLDoctype::Entity::~Entity ()
{
  OP_DELETEA(name.name);
  OP_DELETEA(value);

  if (value_type == VALUE_TYPE_External_NDataDecl)
    OP_DELETEA(ndatadecl_name);

  XML_OBJECT_DESTROYED (XMLDoctypeEntity);
}

void
XMLDoctype::Entity::SetName (const uni_char *new_name, unsigned new_name_length)
{
  XMLInternalParser::CopyString (name.name, new_name, new_name_length);
  name.name_length = new_name_length;
}

void
XMLDoctype::Entity::SetValue (const uni_char *new_value, unsigned new_value_length, BOOL allocated)
{
  if (value)
    OP_DELETEA(value);

  if (value_type == VALUE_TYPE_Unknown)
    value_type = VALUE_TYPE_Internal;

  if (allocated)
    value = (uni_char *) new_value;
  else
    XMLInternalParser::CopyString (value, new_value, new_value_length);
  value_length = new_value_length;
}

void
XMLDoctype::Entity::SetPubid (uni_char *value)
{
  value_type = VALUE_TYPE_External_ExternalId;
  external_id.type = XMLExternalId::TYPE_PUBLIC;
  external_id.pubid = value;
}

void
XMLDoctype::Entity::SetSystem (uni_char *value, URL new_baseurl)
{
  if (value_type != VALUE_TYPE_External_ExternalId)
    {
      external_id.type = XMLExternalId::TYPE_SYSTEM;
      value_type = VALUE_TYPE_External_ExternalId;
    }

  external_id.system = value;

  baseurl = new_baseurl;
}

void
XMLDoctype::Entity::SetNDataName (const uni_char *value, unsigned value_length)
{
  value_type = VALUE_TYPE_External_NDataDecl;
  XMLInternalParser::CopyString (ndatadecl_name, value, value_length);
}

BOOL
XMLDoctype::Entity::Match (const uni_char *other_name, unsigned other_name_length)
{
  return name.name_length == other_name_length && op_memcmp (name.name, other_name, UNICODE_SIZE (other_name_length)) == 0;
}

void
XMLDoctype::AddEntity (Entity *entity)
{
  Entity ***entities_ptr;
  unsigned *entities_count_ptr, *entities_total_ptr;
  OpHashTable **entitytable_ptr;

  if (GetEntity (entity->GetType (), entity->GetName ()))
    {
      OP_DELETE(entity);
      return;
    }

  if (external_subset && !external_subset_read_only)
    external_subset->AddEntity (entity);
  else
    {
      if (entity->GetType () == Entity::TYPE_General)
        {
          entities_ptr = &general_entities;
          entities_count_ptr = &general_entities_count;
          entities_total_ptr = &general_entities_total;
          entitytable_ptr = &generalentitytable;
        }
      else
        {
          entities_ptr = &parameter_entities;
          entities_count_ptr = &parameter_entities_count;
          entities_total_ptr = &parameter_entities_total;
          entitytable_ptr = &parameterentitytable;
        }

      Entity **&entities = *entities_ptr;
      unsigned &entities_count = *entities_count_ptr;
      unsigned &entities_total = *entities_total_ptr;
      OpHashTable *&entitytable = *entitytable_ptr;

      GROW_ARRAY (entities, Entity *);
      XML_AddToTable (entitytable, namehashfunctions, &entity->name, entity);

      entities[entities_count++] = entity;
    }
}

void
XMLDoctype::AddEntity (const uni_char *name, const uni_char *replacement_text)
{
  Entity *entity = current_entity = OP_NEW_L(Entity, (FALSE));
  entity->SetName (name, uni_strlen (name));
  entity->SetValue (replacement_text, uni_strlen (replacement_text), FALSE);
  AddEntity (entity);
  current_entity = 0;
}

XMLDoctype::Entity *
XMLDoctype::GetEntity (Entity::Type type, const uni_char *name, unsigned name_length)
{
  MARKUP_PROFILE_DOCTYPE("MDa:GetEntity");
  if (Entity *entity = (Entity *) XML_GetFromTable (type == Entity::TYPE_General ? generalentitytable : parameterentitytable, name, XMLLength (name, name_length)))
    return entity;
  else
    return external_subset ? external_subset->GetEntity (type, name, name_length) : 0;
}

XMLDoctype::Entity *
XMLDoctype::GetEntity (Entity::Type type, unsigned index)
{
  Entity **entities;
  unsigned entities_count;

  if (type == Entity::TYPE_General)
    {
      entities = general_entities;
      entities_count = general_entities_count;
    }
  else
    {
      entities = parameter_entities;
      entities_count = parameter_entities_count;
    }

  if (index < entities_count)
    return entities[index];
  else
    return 0;
}

unsigned
XMLDoctype::GetEntitiesCount (Entity::Type type)
{
  if (type == Entity::TYPE_General)
    return general_entities_count;
  else
    return parameter_entities_count;
}

#ifdef XML_STORE_NOTATIONS

XMLDoctype::Notation::Notation ()
  : pubid (0),
    system (0)
{
  name.name = 0;
  name.name_length = 0;

  XML_OBJECT_CREATED (XMLDoctypeNotation);
}

XMLDoctype::Notation::~Notation ()
{
  OP_DELETEA(name.name);
  OP_DELETEA(pubid);
  OP_DELETEA(system);

  XML_OBJECT_DESTROYED (XMLDoctypeNotation);
}

void
XMLDoctype::Notation::SetName (const uni_char *value, unsigned value_length)
{
  XMLInternalParser::CopyString (name.name, value, value_length);
  name.name_length = value_length;
}

void
XMLDoctype::AddNotation (Notation *notation)
{
  if (GetNotation (notation->GetName ()))
    {
      OP_DELETE(notation);
      return;
    }

  if (external_subset && !external_subset_read_only)
    external_subset->AddNotation (notation);
  else
    {
      GROW_ARRAY (notations, Notation *);
      notations[notations_count++] = notation;

      XML_AddToTable (notationtable, namehashfunctions, &notation->name, notation);
    }
}

XMLDoctype::Notation *
XMLDoctype::GetNotation (const uni_char *name, unsigned name_length)
{
  if (Notation *notation = (Notation *) XML_GetFromTable (notationtable, name, XMLLength (name, name_length)))
    return notation;
  else
    return external_subset ? external_subset->GetNotation (name, name_length) : 0;
}

XMLDoctype::Notation *
XMLDoctype::GetNotation (unsigned index)
{
  if (index < notations_count)
    return notations[index];
  else
    return 0;
}

#endif // XML_STORE_NOTATIONS

XMLDoctype::XMLDoctype ()
{
  Initialize ();

  XML_OBJECT_CREATED (XMLDoctype);
}

XMLDoctype::~XMLDoctype ()
{
  Destroy ();

  XML_OBJECT_DESTROYED (XMLDoctype);
}

void
XMLDoctype::SetName (const uni_char *value, unsigned value_length)
{
  OP_DELETEA (name);

  XMLInternalParser::CopyString (name, value, value_length);
}

void
XMLDoctype::SetPubid (uni_char *value)
{
  OP_DELETEA (external_id.pubid);

  external_id.type = XMLExternalId::TYPE_PUBLIC;
  external_id.pubid = value;
}

void
XMLDoctype::SetSystem (uni_char *value)
{
  OP_DELETEA (external_id.system);

  if (external_id.type == XMLExternalId::TYPE_None)
    external_id.type = XMLExternalId::TYPE_SYSTEM;

  external_id.system = value;
}

BOOL
XMLDoctype::SetExternalSubset (XMLDoctype *new_external_subset, BOOL read_only)
{
#ifdef XML_STORE_ATTRIBUTES
  for (unsigned attrindex = 0; attrindex < attributes_count; ++attrindex)
    if (new_external_subset->GetElement (attributes[attrindex]->GetElementName ()))
      /* An attribute is declared in the internal subset that affects
         an element declared in the external subset.  Don't use the
         cached external subset. */
      return FALSE;
#endif // XML_STORE_ATTRIBUTES

#ifdef XML_STORE_ELEMENTS
  for (unsigned elemindex = 0; elemindex < elements_count; ++elemindex)
    if (new_external_subset->GetElement (elements[elemindex]->GetName ()))
      /* An element type declaration in the internal subset conflicts
         with one in the external subset.  Don't use the cached
         external subset. */
      return FALSE;
#endif // XML_STORE_ELEMENTS

  external_subset = new_external_subset;
  external_subset_read_only = read_only;

  OP_ASSERT (FALSE);

  //if (external_id.pubid)
  //  external_subset->SetPubid (external_id.pubid);
  //if (external_id.system)
  //  external_subset->SetSystem (external_id.system);

  external_subset->external_id.type = external_id.type;
  return TRUE;
}

void
XMLDoctype::InitEntities ()
{
  namehashfunctions = OP_NEW_L(XMLNameHashFunctions, ());
  attributenamehashfunctions = OP_NEW_L(XMLAttributeNameHashFunctions, ());

  AddEntity (UNI_L ("amp"), UNI_L ("&#38;"));
  AddEntity (UNI_L ("lt"), UNI_L ("&#60;"));
  AddEntity (UNI_L ("gt"), UNI_L (">"));
  AddEntity (UNI_L ("apos"), UNI_L ("'"));
  AddEntity (UNI_L ("quot"), UNI_L ("\""));
}

#ifdef XML_VALIDATING

void
XMLDoctype::ValidateDoctype (XMLInternalParser *parser, BOOL &invalid)
{
  Element **elem_ptr = elements, **elem_ptr_end = elem_ptr + elements_count;

  while (elem_ptr != elem_ptr_end)
    {
      Attribute **attr_ptr = (*elem_ptr)->GetAttributes (), **attr_ptr_end = attr_ptr + (*elem_ptr)->GetAttributesCount ();

      if (!ValidateAttributes (parser, attr_ptr, attr_ptr_end, invalid))
        return;

      ++elem_ptr;
    }

  Entity **ent_ptr = general_entities, **ent_ptr_end = ent_ptr + general_entities_count;

  while (ent_ptr != ent_ptr_end)
    {
      Entity *entity = *ent_ptr;

#ifdef XML_STORE_NOTATIONS
      if (entity->GetValueType () == Entity::VALUE_TYPE_External_NDataDecl)
        if (!GetNotation (entity->GetNDataName ()))
          {
            invalid = TRUE;

            BOOL fatal = parser->SetLastError (XMLInternalParser::VALIDITY_ERROR_Undeclared_Notation);

            parser->SetLastErrorData (entity->GetNDataName (), ~0u);

            if (fatal)
              return;
          }
#endif // XML_STORE_NOTATIONS

      ++ent_ptr;
    }
}

#endif // XML_VALIDATING

void
XMLDoctype::Finish ()
{
  while (attributes_count != 0)
    {
      Attribute *attribute = attributes[0];
      const uni_char *elemname = attribute->GetElementName ();

      current_element = OP_NEW_L(Element, (FALSE));
      current_element->SetName (elemname, uni_strlen (elemname));

      AddElement (current_element);
    }
}

void
XMLDoctype::Cleanup ()
{
#ifdef XML_STORE_ATTRIBUTES
  OP_DELETE(current_attribute);
  current_attribute = 0;
#endif // XML_STORE_ATTRIBUTES

#ifdef XML_STORE_ELEMENTS
  OP_DELETE(current_element);
  current_element = 0;
#endif // XML_STORE_ELEMENTS

#ifdef XML_VALIDATING_ELEMENT_CONTENT
  OP_DELETE(current_item);
  current_item = 0;
#endif // XML_VALIDATING_ELEMENT_CONTENT

  OP_DELETE(current_entity);
  current_entity = 0;
}

#ifdef XML_VALIDATING

/* virtual */ OP_STATUS
XMLDoctype::MakeValidatingTokenHandler (XMLTokenHandler *&tokenhandler, XMLTokenHandler *secondary, XMLValidator::Listener *listener)
{
  return OpStatus::ERR;
}

/* virtual */ OP_STATUS
XMLDoctype::MakeValidatingSerializer (XMLSerializer *&serializer, XMLValidator::Listener *listener)
{
  return OpStatus::ERR;
}

#endif // XML_VALIDATING

/* static */ XMLDoctype *
XMLDoctype::IncRef (XMLDoctype *doctype)
{
  if (doctype)
    ++doctype->refcount;
  return doctype;
}

/* static */ void
XMLDoctype::DecRef (XMLDoctype *doctype)
{
  if (doctype && --doctype->refcount == 0)
    OP_DELETE(doctype);
}

void
XMLDoctype::Initialize ()
{
  op_memset (this, 0, sizeof *this);
  refcount = 1;
  namehashfunctions = 0;
  attributenamehashfunctions = 0;
}

void
XMLDoctype::Destroy ()
{
  Cleanup ();

  unsigned index;

  OP_DELETEA(name);
#ifdef XML_STORE_ATTRIBUTES
  OP_DELETE(attributetable);
  for (index = 0; index < attributes_count; ++index)
    OP_DELETE(attributes[index]);
  OP_DELETEA(attributes);
#endif // XML_STORE_ATTRIBUTES
#ifdef XML_STORE_ELEMENTS
  OP_DELETE(elementtable);
  for (index = 0; index < elements_count; ++index)
    OP_DELETE(elements[index]);
  OP_DELETEA(elements);
#endif // XML_STORE_ELEMENTS
  OP_DELETE(generalentitytable);
  for (index = 0; index < general_entities_count; ++index)
    OP_DELETE(general_entities[index]);
  OP_DELETEA(general_entities);
  OP_DELETE(parameterentitytable);
  for (index = 0; index < parameter_entities_count; ++index)
    OP_DELETE(parameter_entities[index]);
  OP_DELETEA(parameter_entities);
#ifdef XML_STORE_NOTATIONS
  OP_DELETE(notationtable);
  for (index = 0; index < notations_count; ++index)
    OP_DELETE(notations[index]);
  OP_DELETEA(notations);
#endif // XML_STORE_NOTATIONS

  OP_DELETE(namehashfunctions);
  OP_DELETE(attributenamehashfunctions);
}

#ifdef XML_VALIDATING

BOOL
XMLDoctype::ValidateAttributes (XMLInternalParser *parser, Attribute **attr_ptr, Attribute **attr_ptr_end, BOOL &invalid)
{
  while (attr_ptr != attr_ptr_end)
    {
      Attribute *attribute = *attr_ptr;

#ifdef XML_STORE_NOTATIONS
      if (attribute->GetType () == Attribute::TYPE_Enumerated_Notation)
        {
          const uni_char **enum_ptr = attribute->GetEnumerators (), **enum_ptr_end = enum_ptr + attribute->GetEnumeratorsCount ();

          while (enum_ptr != enum_ptr_end)
            {
              if (!GetNotation (*enum_ptr))
                {
                  invalid = TRUE;

                  BOOL fatal = parser->SetLastError (XMLInternalParser::VALIDITY_ERROR_Undeclared_Notation);
                  parser->SetLastErrorData (*enum_ptr, ~0u);

                  if (fatal)
                    return FALSE;
                }

              ++enum_ptr;
            }
        }
#endif // XML_STORE_NOTATIONS

      ++attr_ptr;
    }

  return TRUE;
}

#endif // XML_VALIDATING

#ifdef XML_VALIDATING_ELEMENT_CONTENT
XMLDoctype::Element::ContentItem *XMLDoctype::current_item = 0;
#endif // XML_VALIDATING_ELEMENT_CONTENT
