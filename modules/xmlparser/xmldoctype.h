/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XMLPARSER_XMLDOCTYPE_H
#define XMLPARSER_XMLDOCTYPE_H

#include "modules/url/url2.h"
#include "modules/util/OpHashTable.h"
#include "modules/util/tempbuf.h"

#include "modules/xmlparser/xmlconfig.h"

#ifdef XML_VALIDATING
# include "modules/xmlutils/xmlvalidator.h"
#endif // XML_VALIDATING

class XMLInternalParser;
class OpFile;

class XMLExternalId
{
public:
  XMLExternalId ();
  ~XMLExternalId ();

  enum Type
    {
      TYPE_None,
      TYPE_SYSTEM,
      TYPE_PUBLIC
    };

  Type type;
  uni_char *pubid;
  uni_char *system;
};

class XMLName
{
public:
  uni_char *name;
  unsigned name_length;
};

class XMLAttributeName
{
public:
  XMLName elemname;
  XMLName attrname;
};

class XMLDoctype
#ifdef XML_VALIDATING
  : public XMLValidator
#endif // XML_VALIDATING
{
public:
#ifdef XML_STORE_ATTRIBUTES
  class Attribute
  {
  public:
    enum Type
      {
        TYPE_Unknown = -1,
        TYPE_String,
        TYPE_Tokenized_ID,
        TYPE_Tokenized_IDREF,
        TYPE_Tokenized_IDREFS,
        TYPE_Tokenized_ENTITY,
        TYPE_Tokenized_ENTITIES,
        TYPE_Tokenized_NMTOKEN,
        TYPE_Tokenized_NMTOKENS,
        TYPE_Enumerated_Notation,
        TYPE_Enumerated_Enumeration
      };

    enum Flag
      {
        FLAG_None,
        FLAG_REQUIRED,
        FLAG_IMPLIED,
        FLAG_FIXED
      };

    Attribute (BOOL declared_in_external_subset);
    ~Attribute ();

    void SetType (Type new_type) { type = new_type; }
    void SetFlag (Flag new_flag) { flag = new_flag; }

    void SetElementName (const uni_char *data, unsigned data_length);
    void SetAttributeName (const uni_char *data, unsigned data_length);
#ifdef XML_ATTRIBUTE_DEFAULT_VALUES
    void SetDefaultValue (const uni_char *value, unsigned value_length);
#endif // XML_ATTRIBUTE_DEFAULT_VALUES
    void AddEnumerator (const uni_char *name, unsigned name_length);

    Type GetType () { return (XMLDoctype::Attribute::Type) type; }
    Flag GetFlag () { return (XMLDoctype::Attribute::Flag) flag; }

    const uni_char *GetElementName () { return name.elemname.name; }
    const uni_char *GetAttributeName () { return name.attrname.name; }
#ifdef XML_ATTRIBUTE_DEFAULT_VALUES
    const uni_char *GetDefaultValue () { return default_value; }
#endif // XML_ATTRIBUTE_DEFAULT_VALUES

    const uni_char **GetEnumerators () { return (const uni_char **) enumerators; }
    unsigned GetEnumeratorsCount () { return enumerators_count; }

    BOOL GetDeclaredInExternalSubset () { return declared_in_external_subset; }

    BOOL Match (const uni_char *element_name, const uni_char *attribute_name);
    BOOL Match (const uni_char *name, unsigned name_length);

  protected:
    friend class XMLDoctype;

    Type type:5;
    Flag flag:3;
    BOOL declared_in_external_subset;

    XMLAttributeName name;

#ifdef XML_ATTRIBUTE_DEFAULT_VALUES
    uni_char *default_value;
#endif // XML_ATTRIBUTE_DEFAULT_VALUES

    uni_char **enumerators;
    unsigned enumerators_count;
    unsigned enumerators_total;
  };

  void AddAttribute (Attribute *element);
  Attribute *GetAttribute (const uni_char *elemname, unsigned elemname_length, const uni_char *attrname, unsigned attrname_length);
#else // XML_STORE_ATTRIBUTES
  class Attribute
  {
  public:
    enum Type
      {
        TYPE_Unknown = -1,
        TYPE_String,
        TYPE_Tokenized_ID,
        TYPE_Tokenized_IDREF,
        TYPE_Tokenized_IDREFS,
        TYPE_Tokenized_ENTITY,
        TYPE_Tokenized_ENTITIES,
        TYPE_Tokenized_NMTOKEN,
        TYPE_Tokenized_NMTOKENS,
        TYPE_Enumerated_Notation,
        TYPE_Enumerated_Enumeration
      };

    enum Flag
      {
        FLAG_None,
        FLAG_REQUIRED,
        FLAG_IMPLIED,
        FLAG_FIXED
      };
  };
#endif // XML_STORE_ATTRIBUTES

#ifdef XML_STORE_ELEMENTS
  class Element
  {
  public:
    enum ContentModel
      {
        CONTENT_MODEL_EMPTY,
        CONTENT_MODEL_ANY,
        CONTENT_MODEL_Mixed,
        CONTENT_MODEL_children
      };

    Element (BOOL declared_in_external_subset);
    ~Element ();

    void SetName (const uni_char *data, unsigned data_length);
    void AddAttribute (Attribute *attribute);

    const uni_char *GetName () { return name.name; }

    Attribute *GetAttribute (const uni_char *name, unsigned name_length);

    Attribute **GetAttributes () { return attributes; }
    unsigned GetAttributesCount () { return attributes_count; }

    BOOL GetDeclaredInExternalSubset () { return declared_in_external_subset; }

    BOOL Match (const uni_char *name, unsigned name_length);

  protected:
    friend class XMLDoctype;

    XMLName name;

    Attribute **attributes;
    unsigned attributes_count;
    unsigned attributes_total;

    BOOL declared_in_external_subset;

#ifdef XML_VALIDATING_ELEMENT_CONTENT
  public:
    void SetContentModel (ContentModel model) { content_model = model; }
    ContentModel GetContentModel () { return content_model; }

    void Open ();
    void AddChild (const uni_char *name, unsigned name_length);
    BOOL HasChild (const uni_char *name, unsigned name_length);

    /* Applies to the current open group. */
    void Close ();
    void SetChoice ();
    void SetSequence ();

    /* Applies to the last item (child or group). */
    void SetOptional ();
    void SetRepeatZeroOrMore ();
    void SetRepeatOneOrMore ();

    /* Checks an element's content against the content specification. */
    BOOL CheckElementContent (const uni_char **children, unsigned children_count, BOOL has_cdata, unsigned &index);

#ifdef XML_COMPATIBILITY_CONTENT_MODEL_DETERMINISTIC
    /* Checks the content specification for determinism. */
    BOOL IsDeterministic ();
#endif // XML_COMPATIBILITY_CONTENT_MODEL_DETERMINISTIC

  protected:
    class ContentItem;
    class ContentGroup;

    friend class ContentItem;
    friend class ContentGroup;

    class ContentItem
    {
    public:
      enum Type
        {
          TYPE_child,
          TYPE_group
        };

      enum Flag
        {
          FLAG_None,
          FLAG_Optional,
          FLAG_RepeatZeroOrMore,
          FLAG_RepeatOneOrMore
        };

      ContentItem ();
      ~ContentItem ();

      void SetFlag (Flag new_flag) { flag = new_flag; }
      void SetParent (ContentItem *new_parent) { parent = new_parent; }

      void SetGroup (ContentGroup *group);
      void SetName (const uni_char *data, unsigned data_length);

      Type GetType () { return type; }
      Flag GetFlag () { return flag; }
      ContentItem *GetParent () { return parent; }

      const uni_char *GetName () { return data.name; }
      ContentGroup *GetGroup () { return data.group; }

      BOOL CheckChildren (const uni_char **children, unsigned children_count, unsigned &matched);
      BOOL CheckChildrenInner (const uni_char **children, unsigned children_count, unsigned &matched);

#ifdef XML_COMPATIBILITY_CONTENT_MODEL_DETERMINISTIC
      unsigned CountChildren ();
      void GetChildren (ContentItem **&children);
      BOOL CheckFollowers (ContentItem **followers, ContentItem **&followers_ptr);
      BOOL AddFollower (ContentItem **followers, ContentItem **&followers_ptr, ContentItem *follower);
      BOOL AddFollowersFromSequence (ContentItem **followers, ContentItem **&followers_ptr, unsigned &items_index);
#endif // XML_COMPATIBILITY_CONTENT_MODEL_DETERMINISTIC

    protected:
      Type type;
      Flag flag;
      ContentItem *parent;

      union
      {
        uni_char *name;
        ContentGroup *group;
      } data;
    };

    class ContentGroup
    {
    public:
      enum Type
        {
          TYPE_Unknown,
          TYPE_choice,
          TYPE_seq
        };

      ContentGroup ();
      ~ContentGroup ();

      void Initialize ();
      void Destroy ();

      void SetType (Type new_type) { type = new_type; }
      void SetItem (ContentItem *new_item) { item = new_item; }
      void AddItem (ContentItem *item);

      Type GetType () { return type; }
      ContentItem **GetItems () { return items; }
      unsigned GetItemsCount () { return items_count; }

    protected:
      Type type;
      ContentItem *item, **items;
      unsigned items_count;
      unsigned items_total;
    };

    ContentModel content_model;
    ContentItem *content, *last_item, *current_group;
#endif // XML_VALIDATING_ELEMENT_CONTENT
  };

  void AddElement (Element *element);
  Element *GetElement (const uni_char *name, unsigned name_length = ~0u);

#ifdef XML_VALIDATING
  BOOL GetElementHasIDAttribute (const uni_char *name, unsigned name_length);
#endif // XML_VALIDATING
#endif // XML_STORE_ELEMENTS

  class Entity
  {
  public:
    enum Type
      {
        TYPE_General,
        TYPE_Parameter
      };

    enum ValueType
      {
        VALUE_TYPE_Unknown,
        VALUE_TYPE_Internal,
        VALUE_TYPE_External_ExternalId,
        VALUE_TYPE_External_NDataDecl
      };

    Entity (BOOL declared_in_external_subset);
    ~Entity ();

    void SetType (Type new_type) { type = new_type; }
    void SetName (const uni_char *value, unsigned value_length);

    void SetValue (const uni_char *value, unsigned value_length, BOOL allocated);
    void SetPubid (uni_char *value);
    void SetSystem (uni_char *value, URL baseurl);
    void SetNDataName (const uni_char *value, unsigned value_length);

    Type GetType () { return type; }
    ValueType GetValueType () { return value_type; }
    BOOL GetDeclaredInExternalSubset () { return declared_in_external_subset; }

    const uni_char *GetName () { return name.name; }

    const uni_char *GetValue () { return value; }
    unsigned GetValueLength () { return value_length; }

    const uni_char *GetPubid () { return external_id.pubid; }
    const uni_char *GetSystem () { return external_id.system; }
    URL GetBaseURL () { return baseurl; }
    const uni_char *GetNDataName () { return ndatadecl_name; }

    BOOL Match (const uni_char *name, unsigned name_length);

  protected:
    friend class XMLDoctype;

    Type type;
    ValueType value_type;
    BOOL declared_in_external_subset;

    XMLName name;
    uni_char *value;
    unsigned value_length;

    XMLExternalId external_id;
    URL baseurl;
    uni_char *ndatadecl_name;
  };

  void AddEntity (Entity *entity);
  void AddEntity (const uni_char *name, const uni_char *replacement_text);
  Entity *GetEntity (Entity::Type type, const uni_char *name, unsigned name_length = ~0u);
  Entity *GetEntity (Entity::Type type, unsigned index);
  unsigned GetEntitiesCount (Entity::Type type);

#ifdef XML_STORE_NOTATIONS
  class Notation
  {
  public:
    Notation ();
    ~Notation ();

    void SetName (const uni_char *value, unsigned value_length);
    void SetPubid (uni_char *value) { pubid = value; }
    void SetSystem (uni_char *value) { system = value; }

    const uni_char *GetName () { return name.name; }
    const uni_char *GetPubid () { return pubid; }
    const uni_char *GetSystem () { return system; }

  protected:
    friend class XMLDoctype;

    XMLName name;
    uni_char *pubid;
    uni_char *system;
  };

  void AddNotation (Notation *notation);
  Notation *GetNotation (const uni_char *name, unsigned name_length = ~0u);
  Notation *GetNotation (unsigned index);
  unsigned GetNotationsCount () { return notations_count; }
#endif // XML_STORE_NOTATIONS

  XMLDoctype ();
  ~XMLDoctype ();

  void SetName (const uni_char *value, unsigned value_length);
  void SetPubid (uni_char *value);
  void SetSystem (uni_char *value);
  BOOL SetExternalSubset (XMLDoctype *external_subset, BOOL read_only);

  void InitEntities ();

  const uni_char *GetName () { return name; }
  const uni_char *GetPubid () { return external_id.pubid; }
  const uni_char *GetSystem () { return external_id.system; }
  XMLDoctype *GetExternalSubset () { return external_subset; }

  TempBuffer *GetInternalSubsetBuffer () { return &internal_subset_buffer; }

#ifdef XML_VALIDATING
  void ValidateDoctype (XMLInternalParser *parser, BOOL &invalid);
#endif // XML_VALIDATING

  void Finish ();
  void Cleanup ();

#ifdef XML_DUMP_DOCTYPE
  class DumpSpecification
    {
    public:
      unsigned elements:1;
      /**< Dump element declarations. */

      unsigned contentspecs:1;
      /**< Dump full content specifications in element declarations.  If
           not, element content will be declared as EMPTY or ANY. */

      unsigned attributes:1;
      /**< Dump attribute declarations. */

      unsigned allattributes:1;
      /**< Dump all declared attributes. */

      unsigned alltokenizedattributes:1;
      /**< Dump all attributes declared to have a tokenized type.  Attributes
           declared with an enumerated type will be declared as NMTOKEN
           instead (unless allattributes is also true.) */

      unsigned attributedefaults:1;
      /**< Dump attribute default values (implicitly forces dumping of all
           attributes that have a declared default value.) */

      unsigned idattributes:1;
      /**< Dump all attributes declared to have ID type. */

      unsigned generalentities:1;
      /**< Dump general entity declarations. */

      unsigned parameterentities:1;
      /**< Dump parameter entity declarations. */

      unsigned externalentities:1;
      /**< Dump external entities. */

      unsigned unparsedentities:1;
      /**< Dump unparsed entities. */
    };

  OP_STATUS DumpToFile (const uni_char *filename, const DumpSpecification &specification);
#endif // XML_DUMP_DOCTYPE

#ifdef XML_VALIDATING
  virtual OP_STATUS MakeValidatingTokenHandler(XMLTokenHandler *&tokenhandler, XMLTokenHandler *secondary, XMLValidator::Listener *listener);
  /**< Creates a token handler that validates the document according
       to this document type.  The secondary token handler will be
       passed unmodified tokens.

       @param tokenhandler Set to the newly created token handler.
       @param secondary Secondary tokenhandler that will receive all
                        tokens this token handler receives.
       @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

  virtual OP_STATUS MakeValidatingSerializer(XMLSerializer *&serializer, XMLValidator::Listener *listener);
  /**< Creates a serializer that "serializes" by validating the
       elements serialized according to this document type.

       @param serializer Set to the created serializer.
       @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */
#endif // XML_VALIDATING

  static XMLDoctype *IncRef (XMLDoctype *doctype);
  static void DecRef (XMLDoctype *doctype);

protected:
  friend class Element;

  void Initialize ();
  void Destroy ();

#ifdef XML_VALIDATING
  BOOL ValidateAttributes (XMLInternalParser *parser, Attribute **ptr, Attribute **ptr_end, BOOL &invalid);
#endif // XML_VALIDATING

#ifdef XML_DUMP_DOCTYPE
  OP_STATUS DumpItemToFile (OpFile &file, Element::ContentItem *item);
  OP_STATUS DumpEntityToFile (OpFile &file, Entity *entity, const DumpSpecification &specification);
#endif // XML_DUMP_DOCTYPE

  uni_char *name;
  XMLExternalId external_id;
  XMLDoctype *external_subset;
  BOOL external_subset_read_only;

#ifdef XML_STORE_ATTRIBUTES
  Attribute **attributes;
  unsigned attributes_count;
  unsigned attributes_total;
  OpHashTable *attributetable;

  Attribute *current_attribute;
#endif // XML_STORE_ATTRIBUTES

#ifdef XML_STORE_ELEMENTS
  Element **elements;
  unsigned elements_count;
  unsigned elements_total;
  OpHashTable *elementtable;

  Element *current_element;
#endif // XML_STORE_ELEMENTS

  Entity **general_entities;
  unsigned general_entities_count;
  unsigned general_entities_total;
  OpHashTable *generalentitytable;

  Entity **parameter_entities;
  unsigned parameter_entities_count;
  unsigned parameter_entities_total;
  OpHashTable *parameterentitytable;

  Entity *current_entity;

#ifdef XML_STORE_NOTATIONS
  Notation **notations;
  unsigned notations_count;
  unsigned notations_total;
  OpHashTable *notationtable;
#endif // XML_STORE_NOTATIONS

  unsigned refcount;

  TempBuffer internal_subset_buffer;

  OpHashFunctions *namehashfunctions, *attributenamehashfunctions;

#ifdef XML_VALIDATING_ELEMENT_CONTENT
  static Element::ContentItem *current_item;
#endif // XML_VALIDATING_ELEMENT_CONTENT
};

#endif // XMLPARSER_XMLDOCTYPE_H
