#include "core/pch.h"

#include "modules/xmlparser/xmlconfig.h"

#ifdef XML_DUMP_DOCTYPE

#include "modules/xmlparser/xmldoctype.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/tempbuf.h"

static OP_STATUS
XMLWriteToFile (OpFile &file, const uni_char *string)
{
  const uni_char *ptr = string, *ptr_end = ptr + uni_strlen (string);

  /* Oh yeah, really efficient.  But who cares? */
  while (ptr != ptr_end)
    {
      unsigned ch = *ptr++;

      if (ch >= 0xd800 && ch <= 0xdbff)
        {
          if (ptr == ptr_end || *ptr < 0xdc00 || *ptr > 0xdfff)
            return OpStatus::ERR;

          ch = 0x10000 + ((ch - 0xd800) << 10) + (*ptr++ - 0xdc00);
        }

      char buffer[10]; /* ARRAY OK jl 2008-02-07 */

      if (ch == 10 || ch >= 32 && ch < 128)
        {
          buffer[0] = (char) ch;
          buffer[1] = 0;
        }
      else
        op_sprintf (buffer, "&#x%x;", ch);

      RETURN_IF_ERROR (file.Write (buffer, op_strlen (buffer)));
    }

  return OpStatus::OK;
}

static OP_STATUS
XMLWriteToFileEscaped (OpFile &file, const uni_char *string)
{
  if (*string)
    {
      TempBuffer buffer;

      while (*string)
        {
          if (*string == '&')
            RETURN_IF_ERROR (buffer.Append ("&#38;"));
          else if (*string == '<')
            RETURN_IF_ERROR (buffer.Append ("&#60;"));
          else if (*string == '\'')
            RETURN_IF_ERROR (buffer.Append ("&#27;"));
          else
            RETURN_IF_ERROR (buffer.Append (*string));

          ++string;
        }

      return XMLWriteToFile (file, buffer.GetStorage ());
    }
  else
    return OpStatus::OK;
}

#define XML_WRITE(string) RETURN_IF_ERROR (XMLWriteToFile (file, string))
#define XML_WRITEC(string) RETURN_IF_ERROR (XMLWriteToFile (file, UNI_L (string)))

OP_STATUS
XMLDoctype::DumpToFile (const uni_char *filename, const DumpSpecification &specification)
{
  OpFile file;

  RETURN_IF_ERROR (file.Construct (filename));
  RETURN_IF_ERROR (file.Open (OPFILE_WRITE));

  XML_WRITEC ("<?xml version='1.0' encoding='us-ascii'?>\xa");

  if (specification.elements || specification.attributes)
    {
      Element **ptr = elements, **ptr_end = ptr + elements_count;

      while (ptr != ptr_end)
        {
          Element *element = *ptr++;

          if (specification.elements)
            {
              XML_WRITEC ("<!ELEMENT ");
              XML_WRITE (element->GetName ());

              Element::ContentModel contentmodel = element->GetContentModel ();

              if (contentmodel == Element::CONTENT_MODEL_EMPTY)
                XML_WRITEC (" EMPTY>\xa");
              else if (contentmodel == Element::CONTENT_MODEL_ANY || !specification.contentspecs)
                XML_WRITEC (" ANY>\xa");
              else if (contentmodel == Element::CONTENT_MODEL_Mixed)
                {
                  XML_WRITEC (" (#PCDATA");

                  if (element->content && element->content->GetGroup () && element->content->GetGroup ()->GetItemsCount () != 0)
                    {
                      Element::ContentItem **ptr = element->content->GetGroup ()->GetItems (), **ptr_end = ptr + element->content->GetGroup ()->GetItemsCount ();

                      while (ptr != ptr_end)
                        {
                          Element::ContentItem *item = *ptr++;

                          XML_WRITEC ("|");
                          XML_WRITE (item->GetName ());
                        }

                      XML_WRITEC (")*>\xa");
                    }
                  else
                    XML_WRITEC (")>\xa");
                }
              else
                {
                  XML_WRITEC (" ");
                  RETURN_IF_ERROR (DumpItemToFile (file, element->content));
                  XML_WRITEC (">\xa");
                }
            }

          if (specification.attributes)
            {
              Attribute **ptr = element->GetAttributes (), **ptr_end = ptr + element->GetAttributesCount ();
              unsigned included = 0;

              while (ptr != ptr_end)
                {
                  Attribute *attribute = *ptr++;

                  if (specification.allattributes || specification.attributedefaults && attribute->GetDefaultValue () || specification.alltokenizedattributes && attribute->GetType () != Attribute::TYPE_String || specification.idattributes && attribute->GetType () == Attribute::TYPE_Tokenized_ID)
                    ++included;
                }

              if (included)
                {
                  XML_WRITEC ("<!ATTLIST ");
                  XML_WRITE (element->GetName ());
                  XML_WRITEC ("\xa");

                  ptr = element->GetAttributes (), ptr_end = ptr + element->GetAttributesCount ();

                  while (ptr != ptr_end)
                    {
                      Attribute *attribute = *ptr++;

                      if (specification.allattributes || specification.attributedefaults && attribute->GetDefaultValue () || specification.alltokenizedattributes && attribute->GetType () != Attribute::TYPE_String || specification.idattributes && attribute->GetType () == Attribute::TYPE_Tokenized_ID)
                        {
                          XML_WRITEC ("  ");
                          XML_WRITE (attribute->GetAttributeName ());

                          Attribute::Type type = attribute->GetType ();

                          if (!specification.allattributes && (type == Attribute::TYPE_Enumerated_Notation || type == Attribute::TYPE_Enumerated_Enumeration))
                            type = Attribute::TYPE_Tokenized_NMTOKEN;

                          switch (type)
                            {
                            case Attribute::TYPE_String:
                              XML_WRITEC (" CDATA ");
                              break;

                            case Attribute::TYPE_Tokenized_ID:
                              XML_WRITEC (" ID ");
                              break;

                            case Attribute::TYPE_Tokenized_IDREF:
                              XML_WRITEC (" IDREF ");
                              break;

                            case Attribute::TYPE_Tokenized_IDREFS:
                              XML_WRITEC (" IDREFS ");
                              break;

                            case Attribute::TYPE_Tokenized_ENTITY:
                              XML_WRITEC (" ENTITY ");
                              break;

                            case Attribute::TYPE_Tokenized_ENTITIES:
                              XML_WRITEC (" ENTITIES ");
                              break;

                            case Attribute::TYPE_Tokenized_NMTOKEN:
                              XML_WRITEC (" NMTOKEN ");
                              break;

                            case Attribute::TYPE_Tokenized_NMTOKENS:
                              XML_WRITEC (" NMTOKENS ");
                              break;

                            case Attribute::TYPE_Enumerated_Notation:
                              XML_WRITEC (" NOTATION");

                            case Attribute::TYPE_Enumerated_Enumeration:
                              XML_WRITEC (" (");

                              const uni_char **ptr = attribute->GetEnumerators (), **ptr_end = ptr + attribute->GetEnumeratorsCount ();

                              while (ptr != ptr_end)
                                {
                                  XML_WRITE (*ptr++);

                                  if (ptr != ptr_end)
                                    XML_WRITEC ("|");
                                }

                              XML_WRITEC (") ");
                            }

                          if (attribute->GetFlag () == Attribute::FLAG_REQUIRED)
                            XML_WRITEC ("#REQUIRED");
                          else if (attribute->GetFlag () == Attribute::FLAG_IMPLIED || (!specification.attributedefaults && !specification.allattributes))
                            XML_WRITEC ("#IMPLIED");
                          else
                            {
                              if (attribute->GetFlag () == Attribute::FLAG_FIXED)
                                XML_WRITEC ("#FIXED ");

                              XML_WRITEC ("'");
                              RETURN_IF_ERROR (XMLWriteToFileEscaped (file, attribute->GetDefaultValue ()));
                              XML_WRITEC ("'");
                            }

                          if (--included == 0)
                            XML_WRITEC (">\xa");
                          else
                            XML_WRITEC ("\xa");
                        }
                    }
                }
            }
        }
    }

  if (specification.generalentities)
    {
      Entity **ptr = general_entities, **ptr_end = ptr + general_entities_count;

      while (ptr != ptr_end)
        RETURN_IF_ERROR (DumpEntityToFile (file, *ptr++, specification));
    }

  if (specification.parameterentities)
    {
      Entity **ptr = parameter_entities, **ptr_end = ptr + parameter_entities_count;

      while (ptr != ptr_end)
        RETURN_IF_ERROR (DumpEntityToFile (file, *ptr++, specification));
    }

  file.Close ();

  return OpStatus::OK;
}

OP_STATUS
XMLDoctype::DumpItemToFile (OpFile &file, Element::ContentItem *item)
{
  if (item->GetType () == Element::ContentItem::TYPE_child)
    XML_WRITE (item->GetName ());
  else
    {
      XML_WRITEC ("(");

      Element::ContentGroup *group = item->GetGroup ();
      Element::ContentItem **ptr = group->GetItems (), **ptr_end = ptr + group->GetItemsCount ();

      while (ptr != ptr_end)
        {
          RETURN_IF_ERROR (DumpItemToFile (file, *ptr++));

          if (ptr != ptr_end)
            if (group->GetType () == Element::ContentGroup::TYPE_choice)
              XML_WRITEC ("|");
            else
              XML_WRITEC (",");
        }

      XML_WRITEC (")");
    }

  if (item->GetFlag () == Element::ContentItem::FLAG_Optional)
    XML_WRITEC ("?");
  else if (item->GetFlag () == Element::ContentItem::FLAG_RepeatZeroOrMore)
    XML_WRITEC ("*");
  else if (item->GetFlag () == Element::ContentItem::FLAG_RepeatOneOrMore)
    XML_WRITEC ("+");

  return OpStatus::OK;
}

OP_STATUS
XMLDoctype::DumpEntityToFile (OpFile &file, Entity *entity, const DumpSpecification &specification)
{
  Entity::ValueType valuetype = entity->GetValueType ();

  if (valuetype == Entity::VALUE_TYPE_Internal || specification.externalentities && valuetype == Entity::VALUE_TYPE_External_ExternalId || specification.unparsedentities && valuetype == Entity::VALUE_TYPE_External_NDataDecl)
    {
      XML_WRITEC ("<!ENTITY ");

      if (entity->GetType () == Entity::TYPE_Parameter)
        XML_WRITEC ("% ");

      XML_WRITE (entity->GetName ());

      if (valuetype == Entity::VALUE_TYPE_Internal)
        {
          XML_WRITEC (" '");
          RETURN_IF_ERROR (XMLWriteToFileEscaped (file, entity->GetValue ()));
          XML_WRITEC ("'>\xa");
        }
      else
        {
          if (entity->GetPubid ())
            {
              XML_WRITEC (" PUBLIC '");
              XML_WRITE (entity->GetPubid ());
              XML_WRITEC ("' '");
            }
          else
            XML_WRITEC (" SYSTEM '");

          XML_WRITE (entity->GetSystem ());
          XML_WRITEC ("'");

          if (valuetype == Entity::VALUE_TYPE_External_NDataDecl)
            {
              XML_WRITEC (" NDATA ");
              XML_WRITE (entity->GetNDataName ());
            }

          XML_WRITEC (">\xa");
        }
    }

  return OpStatus::OK;
}

#endif // XML_DUMP_DOCTYPE
