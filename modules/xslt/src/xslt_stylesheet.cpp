/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_stylesheet.h"
#include "modules/xslt/src/xslt_template.h"
#include "modules/xslt/src/xslt_simple.h"
#include "modules/xslt/src/xslt_key.h"
#include "modules/xslt/src/xslt_decimalformat.h"
#include "modules/xslt/src/xslt_importorinclude.h"
#include "modules/xslt/src/xslt_output.h"
#include "modules/xslt/src/xslt_outputhandler.h"
#include "modules/xslt/src/xslt_attributeset.h"
#include "modules/xslt/src/xslt_variable.h"
#include "modules/xslt/src/xslt_striporpreservespace.h"
#include "modules/xslt/src/xslt_namespacealias.h"
#include "modules/xslt/src/xslt_program.h"
#include "modules/xslt/src/xslt_engine.h"
#include "modules/xslt/src/xslt_utils.h"
#include "modules/xslt/src/xslt_transformation.h"
#include "modules/xslt/src/xslt_compiler.h"

#include "modules/xmlutils/xmltokenhandler.h"
#include "modules/xpath/xpath.h"

#include "modules/util/adt/opvector.h"

#include "modules/debug/debug.h"

class XSLT_Engine;

class XSLT_TemplatePatterns
{
public:
  static BOOL InsertL (XSLT_TemplatePatterns **existing, XSLT_Template *tmplate, XPathNodeProfile *nodeprofile);
  static BOOL InsertL (XSLT_TemplatePatterns **existing, XSLT_Template *tmplate, XPathNodeProfile *nodeprofile, float priority, XPathPattern **patterns, unsigned patterns_count, BOOL copy_array);

  XSLT_TemplatePatterns ();
  ~XSLT_TemplatePatterns ();

  XSLT_Template *tmplate;
  float priority;
  XPathPattern **patterns;
  unsigned patterns_count;
  BOOL owns_patterns;

  XSLT_TemplatePatterns *next;
};

/* static */ BOOL
XSLT_TemplatePatterns::InsertL (XSLT_TemplatePatterns **existing, XSLT_Template *tmplate, XPathNodeProfile *nodeprofile)
{
  XPathPattern **patterns = tmplate->GetPatterns ();
  unsigned patterns_count = tmplate->GetPatternsCount ();

  if (patterns_count != 0)
    if (tmplate->HasPriority ())
      return InsertL (existing, tmplate, nodeprofile, tmplate->GetPriority (), patterns, patterns_count, FALSE);
    else
      {
        XPathPattern **array = OP_NEWA (XPathPattern *, patterns_count);
        if (!array)
          {
            OP_DELETE (*existing);
            LEAVE (OpStatus::ERR_NO_MEMORY);
          }
        ANCHOR_ARRAY (XPathPattern *, array);

        float highest = patterns[0]->GetPriority ();

        for (unsigned index = 1; index < patterns_count; ++index)
          {
            float priority = patterns[index]->GetPriority ();
            if (priority > highest)
              highest = priority;
          }

        float second_highest = highest;
        unsigned inserted = 0;
        BOOL was_added = FALSE;

        while (inserted != patterns_count)
          {
            XPathPattern **ptr = array;

            for (unsigned index = 0; index < patterns_count; ++index)
              {
                float priority = patterns[index]->GetPriority ();
                if (highest == priority)
                  *ptr++ = patterns[index];
                else if (priority < highest && second_highest == highest || priority > second_highest)
                  second_highest = priority;
              }

            unsigned count = ptr - array;

            if (InsertL (existing, tmplate, nodeprofile, highest, array, count, TRUE))
              was_added = TRUE;

            inserted += count;
            highest = second_highest;
          }

        return was_added;
      }

  return FALSE;
}

/* static */ BOOL
XSLT_TemplatePatterns::InsertL (XSLT_TemplatePatterns **existing, XSLT_Template *tmplate, XPathNodeProfile *nodeprofile, float priority, XPathPattern **patterns, unsigned patterns_count, BOOL copy_array)
{
  unsigned actual_patterns_count = patterns_count;

#ifdef XSLT_USE_NODE_PROFILES
  if (nodeprofile)
    for (unsigned index = 0; index < patterns_count; ++index)
      if (patterns[index]->GetProfileVerdict (nodeprofile) == XPathPattern::PATTERN_DOES_NOT_MATCH_PROFILED_NODES)
        --actual_patterns_count;
#endif // XSLT_USE_NODE_PROFILES

  if (actual_patterns_count != 0)
    {
      XSLT_TemplatePatterns *template_patterns = OP_NEW (XSLT_TemplatePatterns, ());
      if (!template_patterns)
        {
          OP_DELETE (*existing);
          LEAVE (OpStatus::ERR_NO_MEMORY);
        }

      template_patterns->tmplate = tmplate;
      template_patterns->priority = priority;

      if (copy_array || actual_patterns_count < patterns_count)
        {
          if (!(template_patterns->patterns = OP_NEWA (XPathPattern *, actual_patterns_count)))
            {
              OP_DELETE (*existing);
              OP_DELETE (template_patterns);
              LEAVE (OpStatus::ERR_NO_MEMORY);
            }

#ifdef XSLT_USE_NODE_PROFILES
          if (!nodeprofile)
            op_memcpy (template_patterns->patterns, patterns, patterns_count * sizeof patterns[0]);
          else
            for (unsigned rindex = 0, windex = 0; rindex < patterns_count; ++rindex)
              if (patterns[rindex]->GetProfileVerdict (nodeprofile) != XPathPattern::PATTERN_DOES_NOT_MATCH_PROFILED_NODES)
                template_patterns->patterns[windex++] = patterns[rindex];
#else // XSLT_USE_NODE_PROFILES
          op_memcpy (template_patterns->patterns, patterns, patterns_count * sizeof patterns[0]);
#endif // XSLT_USE_NODE_PROFILES

          template_patterns->owns_patterns = TRUE;
        }
      else
        template_patterns->patterns = patterns;

      while (*existing && (*existing)->tmplate->GetImportPrecedence () > tmplate->GetImportPrecedence ())
        existing = &(*existing)->next;

      while (*existing && (*existing)->tmplate->GetImportPrecedence () == tmplate->GetImportPrecedence () && (*existing)->priority > priority)
        existing = &(*existing)->next;

      template_patterns->patterns_count = actual_patterns_count;
      template_patterns->next = *existing;

      *existing = template_patterns;
      return TRUE;
    }
  else
    return FALSE;
}

XSLT_TemplatePatterns::XSLT_TemplatePatterns ()
  : owns_patterns (FALSE),
    next (0)
{
}

XSLT_TemplatePatterns::~XSLT_TemplatePatterns ()
{
  if (owns_patterns)
    OP_DELETEA (patterns);
  OP_DELETE (next);
}

class XSLT_ApplyTemplatesProgram
  : public XSLT_Program
{
private:
  BOOL has_mode;
  XMLExpandedName mode;
  XSLT_Import *imported_into;
  XPathNodeProfile *nodeprofile;
  BOOL is_compiled;
  XSLT_Program *actual_program;

public:
  XSLT_ApplyTemplatesProgram (const XMLExpandedName *mode0, XSLT_Import *imported_into, XPathNodeProfile *nodeprofile)
    : XSLT_Program (TYPE_APPLY_TEMPLATES),
      has_mode (mode0 != 0),
      imported_into (imported_into),
      nodeprofile (nodeprofile),
      is_compiled (FALSE),
      actual_program (0)
  {
    if (has_mode)
      mode = *mode0;

#ifdef XSLT_PROGRAM_DUMP_SUPPORT
    if (has_mode)
      {
        OpString8 mode_description;
        OpStatus::Ignore(mode_description.SetUTF8FromUTF16(mode.GetLocalPart()));
        OpStatus::Ignore(program_description.AppendFormat("apply-templates for mode %s", mode_description.CStr()));
      }
    else
      OpStatus::Ignore(program_description.Set("apply-templates"));
#endif // XSLT_PROGRAM_DUMP_SUPPORT
  }

  void SetIsCompiled () { is_compiled = TRUE; }

  const XMLExpandedName *GetMode () { return has_mode ? &mode : NULL; }
  XSLT_Import *GetImportedInto () { return imported_into; }
  XPathNodeProfile *GetNodeProfile () { return nodeprofile; }
  BOOL GetIsCompiled () { return is_compiled; }

  void SetActualProgram (XSLT_Program *program) { actual_program = program; }
  XSLT_Program *GetActualProgram () { return actual_program; }
};

void
XSLT_Stylesheet::Free (XSLT_Stylesheet *stylesheet)
{
  XSLT_StylesheetImpl *impl = static_cast<XSLT_StylesheetImpl *> (stylesheet);
  OP_DELETE (impl);
}

/* virtual */
XSLT_Stylesheet::~XSLT_Stylesheet ()
{
}

/* virtual */ XSLT_Element *
XSLT_StylesheetElement::StartElementL (XSLT_StylesheetParserImpl *parser, XSLT_ElementType type, const XMLCompleteNameN &completename, BOOL &ignore_element)
{
  XSLT_Element *child = 0;

  switch (type)
    {
    case XSLTE_DECIMAL_FORMAT:
      child = OP_NEW_L (XSLT_DecimalFormat, ());
      break;

    case XSLTE_ATTRIBUTE_SET:
      child = OP_NEW_L (XSLT_AttributeSet, ());
      break;

    case XSLTE_IMPORT:
      if (!import->import_allowed)
        {
          parser->SignalErrorL ("xsl:import must appear before all other top-level elements");
          break;
        }

    case XSLTE_INCLUDE:
      child = OP_NEW_L (XSLT_ImportOrInclude, ());
      break;

    case XSLTE_KEY:
      child = OP_NEW_L (XSLT_Key, ());
      break;

    case XSLTE_NAMESPACE_ALIAS:
      child = OP_NEW_L (XSLT_NamespaceAlias, (import->xmlversion));
      break;

    case XSLTE_OUTPUT:
      child = OP_NEW_L (XSLT_Output, ());
      break;

    case XSLTE_PARAM:
    case XSLTE_VARIABLE:
      child = OP_NEW_L (XSLT_Variable, ());
      break;

    case XSLTE_PRESERVE_SPACE:
    case XSLTE_STRIP_SPACE:
      child = OP_NEW_L (XSLT_StripOrPreserveSpace, ());
      break;

    case XSLTE_TEMPLATE:
      child = OP_NEW_L (XSLT_Template, (import->GetNonInclude (), import->import_precedence));
      break;

    case XSLTE_UNRECOGNIZED:
      if (import->version != XSLT_VERSION_FUTURE)
        parser->SignalErrorL ("unrecognized top-level element");
      break;

    case XSLTE_OTHER:
      if (completename.GetUri () == 0)
        parser->SignalErrorL ("non-xslt top-level element with null namespace uri");
      break;

    default:
      parser->SignalErrorL ("invalid top-level element");
    }

  if (!child || type != XSLTE_IMPORT)
    import->import_allowed = FALSE;

  if (child)
    {
      child->SetType (type);
      child->SetParent (this);

#if defined XSLT_ERRORS && defined XML_ERRORS
      child->SetLocation (parser->GetCurrentLocation ());
#endif // XSLT_ERRORS && XML_ERRORS
    }
  else
    ignore_element = TRUE;

  return child;
}

/* virtual */ BOOL
XSLT_StylesheetElement::EndElementL (XSLT_StylesheetParserImpl *parser)
{
  return FALSE;
}

/* virtual */ void
XSLT_StylesheetElement::AddAttributeL (XSLT_StylesheetParserImpl *parser, XSLT_AttributeType type, const XMLCompleteNameN &name, const uni_char *value, unsigned value_length)
{
  switch (type)
    {
    case XSLTA_EXCLUDE_RESULT_PREFIXES:
      parser->SetStringL (excluded_namespaces, name, value, value_length);
      break;

    case XSLTA_EXTENSION_ELEMENT_PREFIXES:
      parser->SetStringL (extension_namespaces, name, value, value_length);
      break;

    case XSLTA_VERSION:
      if (value_length == 3 && uni_strncmp (value, "1.0", 3) == 0)
        import->version = XSLT_VERSION_1_0;
      else
        import->version = XSLT_VERSION_FUTURE;
      break;

    case XSLTA_ID:
    case XSLTA_OTHER:
      break;

    case XSLTA_NO_MORE_ATTRIBUTES:
      if (import->version == XSLT_VERSION_NONE)
        parser->SignalErrorL ("missing required version attribute");
      excluded_namespaces.FinishL (parser, this);
      extension_namespaces.FinishL (parser, this);
      break;

    default:
      XSLT_Element::AddAttributeL (parser, type, name, value, value_length);
    }
}

/* virtual */
XSLT_StylesheetElement::~XSLT_StylesheetElement ()
{
  OP_DELETE (import);
}

XSLT_StylesheetImpl::WhiteSpaceControl::WhiteSpaceControl (XSLT_Import *import, unsigned index)
  : index (index),
    import_precedence (import->import_precedence),
    next (0)
{
}

XSLT_StylesheetImpl::WhiteSpaceControl::~WhiteSpaceControl ()
{
  OP_DELETE (next);
}

BOOL
XSLT_StylesheetImpl::WhiteSpaceControl::FindMatch (unsigned &import_index, unsigned &index, float &priority, const XMLExpandedNameN &name)
{
  BOOL matches_this = TRUE;

  const uni_char *nametest_uri = nametest.GetUri (), *name_uri = name.GetUri ();
  unsigned nametest_uri_length = nametest_uri ? uni_strlen (nametest_uri) : 0, name_uri_length = name.GetUriLength ();

  const uni_char *nametest_localpart = nametest.GetLocalPart (), *name_localpart = name.GetLocalPart ();
  unsigned nametest_localpart_length = uni_strlen (nametest_localpart), name_localpart_length = name.GetLocalPartLength ();

  if (nametest_uri_length != name_uri_length || (name_uri && nametest_uri && uni_strncmp (nametest_uri, name_uri, name_uri_length) != 0))
    matches_this = FALSE;
  else if ((nametest_localpart_length != 1 || *nametest_localpart != '*') && (nametest_localpart_length != name_localpart_length || uni_strncmp (nametest_localpart, name_localpart, name_localpart_length) != 0))
    matches_this = FALSE;

  BOOL found_other = FALSE;
  float this_priority = 0.f;

  if (matches_this)
    if (nametest_localpart_length == 1 && *nametest_localpart == '*')
      if (nametest_uri)
        this_priority = -0.25f;
      else
        this_priority = -0.5;
    else
      this_priority = 0;

  if (next)
    {
      found_other = next->FindMatch (import_index, index, priority, name);

      if (matches_this && found_other)
        if (import_index > import_precedence || import_index == import_precedence && priority < this_priority || priority == this_priority && index < this->index)
          found_other = FALSE;
    }

  if (matches_this && !found_other)
    {
      import_index = import_precedence;
      index = this->index;
      priority = this_priority;
    }

  return matches_this || found_other;
}

/* static */ void
XSLT_StylesheetImpl::WhiteSpaceControl::Push (WhiteSpaceControl *&existing, WhiteSpaceControl *wsc)
{
  WhiteSpaceControl **pointer = &existing;

  while (*pointer)
    pointer = &(*pointer)->next;

  *pointer = wsc;
}

/* virtual */ void
XSLT_StylesheetImpl::NameTable::Delete (void *data)
{
  XMLExpandedName *name = static_cast<XMLExpandedName *> (data);
  OP_DELETE (name);
}

/* static */ void
XSLT_StylesheetImpl::CDATASectionElement::PushL (CDATASectionElement *&existing, const XMLExpandedNameN &name)
{
  CDATASectionElement **pointer = &existing;

  while (*pointer)
    {
      if ((*pointer)->name == name)
        return;

      pointer = &(*pointer)->next;
    }

  CDATASectionElement *element = OP_NEW_L (CDATASectionElement, ());

  if (OpStatus::IsMemoryError (element->name.Set (name)))
    {
      OP_DELETE (element);
      LEAVE (OpStatus::ERR_NO_MEMORY);
    }

  *pointer = element;
}

XSLT_StylesheetImpl::NamespaceAlias::NamespaceAlias ()
  : next (0),
    stylesheet_prefix (0),
    stylesheet_uri (0),
    result_prefix (0),
    result_uri (0)
{
}

XSLT_StylesheetImpl::NamespaceAlias::~NamespaceAlias ()
{
  OP_DELETEA (stylesheet_prefix);
  OP_DELETEA (stylesheet_uri);
  OP_DELETEA (result_prefix);
  OP_DELETEA (result_uri);
  OP_DELETE (next);
}

void
XSLT_StylesheetImpl::NamespaceAlias::ProcessL (XMLCompleteName &name)
{
  if (name.GetUri ())
    if (uni_strcmp (name.GetUri (), stylesheet_uri) == 0)
      {
        LEAVE_IF_ERROR (name.SetUri (result_uri));
        LEAVE_IF_ERROR (name.SetPrefix (result_prefix));
      }
    else if (next)
      next->ProcessL (name);
}

/* static */ void
XSLT_StylesheetImpl::NamespaceAlias::Push (NamespaceAlias *&existing, NamespaceAlias *alias)
{
  NamespaceAlias **pointer = &existing;

  while (*pointer)
    pointer = &(*pointer)->next;

  *pointer = alias;
}

XSLT_StylesheetImpl::XSLT_StylesheetImpl ()
  : elements (0),
    elements_count (0),
    elements_total (0),
    templates (0),
    templates_count (0),
    templates_total (0),
    keys (0),
    attribute_sets (0),
    attribute_sets_used (0),
    output_method (XSLT_OUTPUT_DEFAULT),
    decimal_formats (0),
    excluded_namespaces (XSLT_NamespaceCollection::TYPE_EXCLUDED_RESULT_PREFIXES),
    extension_namespaces (XSLT_NamespaceCollection::TYPE_EXTENSION_ELEMENT_PREFIXES),
#ifdef XSLT_USE_NODE_PROFILES
    text_nodeprofile (0),
    comment_nodeprofile (0),
#endif // XSLT_USE_NODE_PROFILES
    root_program (0),
    last_variable_in_root (0),
    currently_compiled_global_variable (0),
    compilation_complete (FALSE),
    compilation_started_once (FALSE),
    strip_space (0),
    preserve_space (0),
    whitespace_control_index (0),
    strip_space_table (&xmlexpandedname_hash_functions),
    preserve_space_table (&xmlexpandedname_hash_functions),
    cdata_section_elements (0),
    namespace_aliases (0)
{
  output_specification.method = OutputSpecification::METHOD_UNKNOWN;
  output_specification.version = UNI_L ("1.0");
  output_specification.encoding = UNI_L ("UTF-8");
  output_specification.omit_xml_declaration = FALSE;
  output_specification.standalone = XMLSTANDALONE_NONE;
  output_specification.doctype_public_id = NULL;
  output_specification.doctype_system_id = NULL;
  output_specification.media_type = UNI_L ("text/xml");
}

XSLT_StylesheetImpl::~XSLT_StylesheetImpl ()
{
  unsigned index;

  for (index = 0; index < elements_count; ++index)
    OP_DELETE (elements[index]);
  OP_DELETEA (elements);
  for (index = 0; index < templates_count; ++index)
    OP_DELETE (templates[index]);
  OP_DELETEA (templates);

  OP_DELETE (keys);
  OP_DELETE (attribute_sets);
  OP_DELETE (decimal_formats);

  while (last_variable_in_root)
    {
      XSLT_Variable* next = last_variable_in_root->GetPrevious();
      OP_DELETE (last_variable_in_root);
      last_variable_in_root = next;
    }

  for (index = 0; index < apply_templates_programs.GetCount (); ++index)
    {
      XSLT_ApplyTemplatesProgram* program = apply_templates_programs.Get (index);
      OP_DELETE (program);
    }

#ifdef XSLT_USE_NODE_PROFILES
  XPathNodeProfile::Free (text_nodeprofile);
  XPathNodeProfile::Free (comment_nodeprofile);
#endif // XSLT_USE_NODE_PROFILES

  strip_space_table.DeleteAll ();
  preserve_space_table.DeleteAll ();

  OP_DELETE (strip_space);
  OP_DELETE (preserve_space);
  OP_DELETE (cdata_section_elements);
  OP_DELETE (namespace_aliases);
}

/* virtual */ const XSLT_Stylesheet::OutputSpecification *
XSLT_StylesheetImpl::GetOutputSpecification ()
{
  return &output_specification;
}

/* virtual */ BOOL
XSLT_StylesheetImpl::ShouldStripWhitespaceIn (const XMLExpandedName &name)
{
  if (!name.GetLocalPart ())
    return HasStripSpaceElements();
  else if (strip_space_table.Contains (&name))
    return TRUE;
  else if (preserve_space_table.Contains (&name))
    return FALSE;
  else
    {
      BOOL should_strip = StripWhiteSpace (name);

      OpHashTable *table = should_strip ? &strip_space_table : &preserve_space_table;
      XMLExpandedName *copy = OP_NEW (XMLExpandedName, ());
      if (!copy || OpStatus::IsMemoryError (copy->Set (name)) || OpStatus::IsMemoryError (table->Add (copy, copy)))
        OP_DELETE (copy);

      return should_strip;
    }
}

OP_STATUS
XSLT_StylesheetImpl::Finish (XSLT_StylesheetParserImpl *parser)
{
  TRAPD (status, FinishL (parser));
  return status;
}

void
XSLT_StylesheetImpl::AddStylesheetElementL (XSLT_StylesheetElement *element)
{
  OpStackAutoPtr<XSLT_Element> anchor (element);

  void **elements0 = reinterpret_cast<void **> (elements);
  XSLT_Utils::GrowArrayL (&elements0, elements_count, elements_count + 1, elements_total);
  elements = reinterpret_cast<XSLT_StylesheetElement **> (elements0);

  anchor.release ();
  elements[elements_count++] = element;
}

void
XSLT_StylesheetImpl::AddTemplateL (XSLT_Template *tmplate)
{
  OpStackAutoPtr<XSLT_Element> anchor (tmplate);

  void **templates0 = reinterpret_cast<void **> (templates);
  XSLT_Utils::GrowArrayL (&templates0, templates_count, templates_count + 1, templates_total);
  templates = reinterpret_cast<XSLT_Template **> (templates0);

  anchor.release ();
  templates[templates_count++] = tmplate;
}

void
XSLT_StylesheetImpl::AddKey (XSLT_Key *key)
{
  XSLT_Key::Push (keys, key);
}

XSLT_Key *
XSLT_StylesheetImpl::FindKey (const XMLExpandedName &name)
{
  return keys ? keys->Find (name) : 0;
}

void
XSLT_StylesheetImpl::AddAttributeSet (XSLT_AttributeSet *attribute_set)
{
  XSLT_AttributeSet::Push (attribute_sets, attribute_set);
}

XSLT_AttributeSet *
XSLT_StylesheetImpl::FindAttributeSet (const XMLExpandedName &name)
{
  return attribute_sets ? attribute_sets->Find (name) : 0;
}

void
XSLT_StylesheetImpl::AddVariable (XSLT_Variable *variable)
{
  variable->SetPrevious (last_variable_in_root);
  last_variable_in_root = variable;
}

void
XSLT_StylesheetImpl::SetOutputMethod (XSLT_OutputMethod new_output_method)
{
  output_method = new_output_method;

  switch (new_output_method)
    {
    case XSLT_OUTPUT_XML:
      output_specification.method = OutputSpecification::METHOD_XML;
      break;

    case XSLT_OUTPUT_HTML:
      output_specification.method = OutputSpecification::METHOD_HTML;
      break;

    case XSLT_OUTPUT_TEXT:
      output_specification.method = OutputSpecification::METHOD_TEXT;
    }
}

void
XSLT_StylesheetImpl::AddWhiteSpaceControlL (XSLT_Import *import, const XMLCompleteNameN &nametest, BOOL strip)
{
  WhiteSpaceControl *wsc = OP_NEW_L (WhiteSpaceControl, (import, ++whitespace_control_index));

  if (strip)
    WhiteSpaceControl::Push (strip_space, wsc);
  else
    WhiteSpaceControl::Push (preserve_space, wsc);

  wsc->nametest.SetL (nametest);
}

BOOL
XSLT_StylesheetImpl::StripWhiteSpace (const XMLExpandedName &name)
{
  unsigned strip_import_index = 0;
  unsigned strip_index = 0;
  float strip_priority = 0.f;

  BOOL should_strip = strip_space && strip_space->FindMatch (strip_import_index, strip_index, strip_priority, name);

  if (should_strip)
    {
      unsigned preserve_import_index = 0;
      unsigned preserve_index = 0;
      float preserve_priority = 0.f;

      BOOL should_preserve = preserve_space && preserve_space->FindMatch (preserve_import_index, preserve_index, preserve_priority, name);

      if (should_preserve)
        if (strip_import_index == preserve_import_index && strip_priority == preserve_priority)
          /* FIXME: Issue warning here. */
          should_strip = preserve_index < strip_index;
        else if (preserve_import_index < strip_import_index || preserve_import_index == strip_import_index && preserve_priority > strip_priority)
          should_strip = FALSE;
    }

  return should_strip;
}

void
XSLT_StylesheetImpl::AddCDATASectionElementL (const XMLCompleteNameN &name)
{
  CDATASectionElement::PushL (cdata_section_elements, name);
}

BOOL
XSLT_StylesheetImpl::UseCDATASection (const XMLExpandedName &name)
{
  if (cdata_section_elements)
    return cdata_section_elements->Find (name);
  else
    return FALSE;
}

void
XSLT_StylesheetImpl::AddNamespaceAliasL (XSLT_Import *import, const uni_char *stylesheet_prefix, const uni_char *stylesheet_uri, const uni_char *result_prefix, const uni_char *result_uri)
{
  NamespaceAlias *alias = OP_NEW_L (NamespaceAlias, ());

  NamespaceAlias::Push (namespace_aliases, alias);

  if (stylesheet_prefix)
    LEAVE_IF_ERROR (UniSetStr (alias->stylesheet_prefix, stylesheet_prefix));
  LEAVE_IF_ERROR (UniSetStr (alias->stylesheet_uri, stylesheet_uri));

  if (result_prefix)
    LEAVE_IF_ERROR (UniSetStr (alias->result_prefix, result_prefix));
  LEAVE_IF_ERROR (UniSetStr (alias->result_uri, result_uri));
}

void
XSLT_StylesheetImpl::ProcessNamespaceAliasesL (XMLCompleteName &name)
{
  if (namespace_aliases)
    namespace_aliases->ProcessL (name);
}

XSLT_DecimalFormatData *
XSLT_StylesheetImpl::AddDecimalFormat ()
{
  return XSLT_DecimalFormatData::PushL (decimal_formats);
}

XSLT_DecimalFormatData *
XSLT_StylesheetImpl::FindDecimalFormat (const XMLExpandedName &name)
{
  return XSLT_DecimalFormatData::Find (decimal_formats, name);
}

XSLT_DecimalFormatData *
XSLT_StylesheetImpl::FindDefaultDecimalFormat ()
{
  return XSLT_DecimalFormatData::FindDefault (decimal_formats);
}

void XSLT_StylesheetImpl::CompileTemplatesL (XSLT_StylesheetParserImpl *parser)
{
  if (!compilation_started_once)
    {
      compilation_started_once = TRUE;

      unsigned index;

      for (index = 0; index < templates_count; ++index)
        {
          XSLT_Template *tmplate = templates[index];
          tmplate->CompileTemplateL (parser, this);
        }

      for (index = 0; index < apply_templates_programs.GetCount (); ++index)
        {
          XSLT_ApplyTemplatesProgram *program = apply_templates_programs.Get (index);
          CompileApplyTemplatesProgramL (*program, parser);
        }

      compilation_complete = TRUE;
    }

  if (!compilation_complete)
    LEAVE (OpStatus::ERR);
}

static BOOL
XSLT_IsImportedInto (XSLT_Import *import, XSLT_Template *tmplate)
{
  XSLT_Import *template_import = tmplate->GetImport ()->parent;

  while (template_import)
    if (template_import == import)
      return TRUE;
    else
      template_import = template_import->parent;

  return FALSE;
}

static XSLT_ApplyTemplatesProgram *
XSLT_FindApplyTemplatesProgram (const OpVector<XSLT_ApplyTemplatesProgram> &programs, const XMLExpandedName *mode, const XSLT_Import *imported_into, XPathNodeProfile *nodeprofile)
{
  for (unsigned index = 0; index < programs.GetCount (); ++index)
    {
      XSLT_ApplyTemplatesProgram *program = programs.Get (index);
      if (program->GetImportedInto () == imported_into && !mode == !program->GetMode() && (!mode || *mode == *program->GetMode ()) && program->GetNodeProfile () == nodeprofile)
        return program;
    }

  return 0;
}

XSLT_Program *
XSLT_StylesheetImpl::GetApplyTemplatesProgramL (const XMLExpandedName *mode, XSLT_Import *imported_into, XPathNodeProfile *nodeprofile, BOOL compile, XSLT_MessageHandler *messagehandler)
{
  XSLT_ApplyTemplatesProgram *apply_templates_program = XSLT_FindApplyTemplatesProgram (apply_templates_programs, mode, imported_into, nodeprofile);

  if (!apply_templates_program)
    {
      apply_templates_program = OP_NEW_L (XSLT_ApplyTemplatesProgram, (mode, imported_into, nodeprofile));

      if (mode && OpStatus::IsMemoryError (apply_templates_program->SetMode (*mode)) ||
          OpStatus::IsMemoryError (apply_templates_programs.Add (apply_templates_program)))
        {
          OP_DELETE (apply_templates_program);
          LEAVE(OpStatus::ERR_NO_MEMORY);
        }
    }

  if (compile)
    CompileApplyTemplatesProgramL (*apply_templates_program, messagehandler);

  if (nodeprofile)
    if (XSLT_Program *actual_program = apply_templates_program->GetActualProgram ())
      return actual_program;

  return apply_templates_program;
}

#ifdef XSLT_USE_NODE_PROFILES

XSLT_Program *
XSLT_StylesheetImpl::GetApplyTemplatesProgramL (const XMLExpandedName *mode, XPathNode::Type nodetype, BOOL compile, XSLT_MessageHandler *messagehandler)
{
  OP_ASSERT (nodetype == XPathNode::TEXT_NODE || nodetype == XPathNode::COMMENT_NODE);

  XPathNodeProfile *&nodeprofile = nodetype == XPathNode::TEXT_NODE ? text_nodeprofile : comment_nodeprofile;

  if (!nodeprofile)
    LEAVE_IF_ERROR (XPathNodeProfile::Make (nodeprofile, nodetype));

  return GetApplyTemplatesProgramL (mode, 0, nodeprofile, compile, messagehandler);
}

#endif // XSLT_USE_NODE_PROFILES

void
XSLT_StylesheetImpl::CompileApplyTemplatesProgramL (XSLT_ApplyTemplatesProgram &apply_templates_program, XSLT_MessageHandler *messagehandler)
{
  if (!apply_templates_program.GetIsCompiled ())
    {
      XSLT_Compiler *compiler = OP_NEW_L (XSLT_Compiler, (this, messagehandler));
      OpStackAutoPtr<XSLT_Compiler> compiler_anchor (compiler);

      const XMLExpandedName *mode = apply_templates_program.GetMode ();
      XSLT_Import *imported_into = apply_templates_program.GetImportedInto ();

#ifdef XSLT_USE_NODE_PROFILES
      XPathNodeProfile *nodeprofile = apply_templates_program.GetNodeProfile ();
#else // XSLT_USE_NODE_PROFILES
      XPathNodeProfile *nodeprofile = 0;
#endif // XSLT_USE_NODE_PROFILES

      XSLT_TemplatePatterns *template_patterns = 0;

      for (unsigned index = 0; index < templates_count; ++index)
        {
          XSLT_Template *tmplate = templates[index];

          /* If we're compiling a program that applies templates in a certain mode,
             only insert templates with the correct 'mode' attribute; otherwise
             insert only templates without a 'mode' attribute.  But always insert
             the built-in templates (those with no import) since they should
             magically be available in every mode. */
          if (!(mode ? tmplate->HasMode () && *mode == tmplate->GetMode () : !tmplate->HasMode ()))
            continue;

          /* If we're compiling an apply templates program used by a 'apply-imports'
             element, then only insert templates imported into the stylesheet in
             which the 'apply-imports' element occured. */
          if (imported_into && !XSLT_IsImportedInto (imported_into, tmplate))
            continue;

          XSLT_TemplatePatterns::InsertL (&template_patterns, tmplate, nodeprofile);
        }

      OpStackAutoPtr<XSLT_TemplatePatterns> template_patterns_anchor (template_patterns);

      XSLT_TemplatePatterns *iterator = template_patterns;
      while (iterator)
        {
          XSLT_Template *tmplate = iterator->tmplate;
          BOOL known_match = FALSE;

#ifdef XSLT_USE_NODE_PROFILES
          if (nodeprofile)
            {
              XPathPattern **patterns = iterator->patterns;
              unsigned patterns_count = iterator->patterns_count, pattern_index;

              for (pattern_index = 0; pattern_index < patterns_count; ++pattern_index)
                if (patterns[pattern_index]->GetProfileVerdict (nodeprofile) == XPathPattern::PATTERN_WILL_MATCH_PROFILED_NODES)
                  break;

              if (pattern_index < patterns_count)
                {
                  /* We've found a pattern that is sure to match.  No need to
                     do any more matching; just apply this template. */
                  known_match = TRUE;

                  if (iterator == template_patterns)
                    /* First template: this means this program will just call
                       another program and return.  Callers might as well not
                       use it. */
                    apply_templates_program.SetActualProgram (tmplate->GetProgram ());
                }
            }
#endif // XSLT_USE_NODE_PROFILES

          unsigned destination = 0;

          if (!known_match)
            {
              XSLT_ADD_INSTRUCTION_WITH_ARGUMENT_EXTERNAL (XSLT_Instruction::IC_MATCH_PATTERNS, compiler->AddPatternsL (iterator->patterns, iterator->patterns_count), 0);
              destination = XSLT_ADD_JUMP_INSTRUCTION_EXTERNAL (XSLT_Instruction::IC_JUMP_IF_FALSE, 0);
            }

          XSLT_ADD_INSTRUCTION_WITH_ARGUMENT_EXTERNAL (XSLT_Instruction::IC_CALL_PROGRAM_ON_NODE, compiler->AddProgramL (tmplate->GetProgram ()), 0);
          XSLT_ADD_INSTRUCTION_EXTERNAL (XSLT_Instruction::IC_RETURN, 0);

          if (!known_match)
            {
              compiler->SetJumpDestination (destination);
              iterator = iterator->next;
            }
          else
            break;
        }

      XSLT_ADD_INSTRUCTION_WITH_ARGUMENT_EXTERNAL (XSLT_Instruction::IC_APPLY_BUILTIN_TEMPLATE, 0, 0);

      compiler->FinishL (&apply_templates_program);
      apply_templates_program.SetIsCompiled ();
    }
}

void
XSLT_StylesheetImpl::CompileRootProgramL (XSLT_StylesheetParserImpl *parser)
{
  root_program = GetApplyTemplatesProgramL (0, 0, 0, TRUE);
}

void
XSLT_StylesheetImpl::FinishL (XSLT_StylesheetParserImpl *parser)
{
  CompileTemplatesL (parser);
  CompileRootProgramL (parser);
}

void
XSLT_StylesheetImpl::RenumberImportPrecedence (unsigned old_index, unsigned new_index)
{
  for (unsigned index = 0; index < templates_count; ++index)
    {
      XSLT_Template *tmplate = templates[index];
      if (tmplate->GetImportPrecedence () == old_index)
        tmplate->SetImportPrecedence (new_index);
    }
}

/* virtual */ OP_STATUS
XSLT_StylesheetImpl::StartTransformation (Transformation *&transformation, const Input &input, OutputForm outputform)
{
  XSLT_TransformationImpl *impl = OP_NEW (XSLT_TransformationImpl, (this, input, outputform));
  if (!impl)
    return OpStatus::ERR_NO_MEMORY;
  OpStackAutoPtr<XSLT_TransformationImpl> transformation_anchor (impl);

  TRAPD (status, impl->StartTransformationL ());
  RETURN_IF_ERROR (status);

  transformation_anchor.release();

  transformation = impl;
  return OpStatus::OK;
}

/* static */ OP_STATUS
XSLT_Stylesheet::StopTransformation (Transformation *transformation)
{
  XSLT_TransformationImpl *impl = static_cast<XSLT_TransformationImpl *> (transformation);
  OP_DELETE (impl);
  return OpStatus::OK;
}

XSLT_Stylesheet::Input::Input ()
  : tree (0),
    node (0),
    parameters (0),
    parameters_count (0)
{
}

XSLT_Stylesheet::Input::~Input ()
{
  for (unsigned index = 0; index < parameters_count; ++index)
    OP_DELETE (parameters[index].value);
  OP_DELETEA (parameters);
}

XSLT_Stylesheet::Input::Parameter::Parameter ()
  : value (0)
{
}

/* static */ OP_STATUS
XSLT_Stylesheet::Input::Parameter::Value::MakeNumber (Value *&result0, double value)
{
  XSLT_ParameterValue *result = OP_NEW (XSLT_ParameterValue, ());
  if (!result)
    return OpStatus::ERR_NO_MEMORY;
  result->type = XSLT_ParameterValue::NUMBER;
  result->number = value;
  result0 = result;
  return OpStatus::OK;
}

/* static */ OP_STATUS
XSLT_Stylesheet::Input::Parameter::Value::MakeBoolean (Value *&result0, BOOL value)
{
  XSLT_ParameterValue *result = OP_NEW (XSLT_ParameterValue, ());
  if (!result)
    return OpStatus::ERR_NO_MEMORY;
  result->type = XSLT_ParameterValue::BOOLEAN;
  result->boolean = value;
  result0 = result;
  return OpStatus::OK;
}

/* static */ OP_STATUS
XSLT_Stylesheet::Input::Parameter::Value::MakeString (Value *&result0, const uni_char *value)
{
  XSLT_ParameterValue *result = OP_NEW (XSLT_ParameterValue, ());
  if (!result)
    return OpStatus::ERR_NO_MEMORY;
  result->type = XSLT_ParameterValue::STRING;
  if (OpStatus::IsError (result->string.Set (value)))
    {
      OP_DELETE (result);
      return OpStatus::ERR_NO_MEMORY;
    }
  result0 = result;
  return OpStatus::OK;
}

/* static */ OP_STATUS
XSLT_Stylesheet::Input::Parameter::Value::MakeNode (Value *&result, XPathNode *node)
{
  RETURN_IF_ERROR (MakeNodeList (result));
  return result->AddNodeToList (node);
}

/* static */ OP_STATUS
XSLT_Stylesheet::Input::Parameter::Value::MakeNodeList (Value *&result0)
{
  XSLT_ParameterValue *result = OP_NEW (XSLT_ParameterValue, ());
  if (!result)
    return OpStatus::ERR_NO_MEMORY;
  result->type = XSLT_ParameterValue::NODELIST;
  result0 = result;
  return OpStatus::OK;
}

/* virtual */ OP_STATUS
XSLT_ParameterValue::AddNodeToList (XPathNode *node)
{
  return nodelist.Add (node);
}

XSLT_ParameterValue *
XSLT_ParameterValue::CopyL ()
{
  XSLT_ParameterValue *copy = OP_NEW_L (XSLT_ParameterValue, ());
  OP_STATUS status = OpStatus::OK;

  switch (copy->type = type)
    {
    case STRING:
      status = copy->string.Set (string);
      break;

    case NUMBER:
      copy->number = number;
      break;

    case BOOLEAN:
      copy->boolean = boolean;
      break;

    case NODELIST:
      status = copy->nodelist.AddAll (nodelist);
      break;
    }

  if (OpStatus::IsError (status))
    {
      copy->type = NONE;
      OP_DELETE (copy);

      LEAVE (OpStatus::ERR_NO_MEMORY);
    }

  return copy;
}

XSLT_VariableValue *
XSLT_ParameterValue::MakeVariableValueL ()
{
  switch (type)
    {
    case STRING:
      return XSLT_VariableValue::MakeL (string.CStr ());

    case NUMBER:
      return XSLT_VariableValue::MakeL (number);

    case BOOLEAN:
      return XSLT_VariableValue::MakeL (boolean);

    case NODELIST:
      return XSLT_VariableValue::MakeL (XSLT_NodeList::CopyL (nodelist));
    }

  LEAVE (OpStatus::ERR);
  return 0;
}

#endif // XSLT_SUPPORT
