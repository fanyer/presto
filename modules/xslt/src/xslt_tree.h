/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2008 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef XSLT_TREE_H
#define XSLT_TREE_H

#ifdef XSLT_SUPPORT

#include "modules/xslt/src/xslt_outputhandler.h"
#include "modules/xmlutils/xmltreeaccessor.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/xmlutils/xmltokenhandler.h"
#include "modules/xmlutils/xmldocumentinfo.h"
#include "modules/util/opstring.h"
#include "modules/util/OpHashTable.h"

class XSLT_Tree
  : public XMLTreeAccessor,
    public XSLT_OutputHandler,
    public XMLTokenHandler
{
private:
  XSLT_Tree ();

  class TreeNode;
  class Container;
  class Attributes;
  class Element;
  class CharacterData;
  class ProcessingInstruction;

  friend class TreeNode;
  friend class Container;
  friend class Attributes;

  class TreeNode
  {
  public:
    TreeNode () : parent (NULL), nextsibling (NULL), previoussibling (NULL) {}

    XMLTreeAccessor::NodeType type;
    Container *parent;
    TreeNode *nextsibling, *previoussibling;

    void Delete (Element *element);
    void Delete (CharacterData *characterdata);
    void Delete (ProcessingInstruction *pi);
  };

  class Container
    : public TreeNode
  {
  public:
    TreeNode *firstchild, *lastchild;

    ~Container ();
  };

  Container tree_root;
  Container *current;

  class Element
    : public Container
  {
  public:
    XMLCompleteName name;

    class Attribute
    {
    public:
      Attribute () : nextattr (NULL) {}
      XMLCompleteName name;
      OpString value;
      Attribute *nextattr;
    } *firstattr, *lastattr;

    Element () : firstattr (NULL), lastattr (NULL) {}
    ~Element ();
  };

  class CharacterData
    : public TreeNode
  {
  public:
    OpString data;
  };

  class ProcessingInstruction
    : public CharacterData
  {
  public:
    OpString target;
  };

  OpGenericStringHashTable idtable;
  URL documenturl;
  XMLDocumentInformation documentinformation;

  void AddNode (TreeNode *node);
  void AddCharacterDataL (XMLTreeAccessor::NodeType type, const uni_char *data);

  class Attributes : public XMLTreeAccessor::Attributes
  {
  public:
    Attributes() : element(0) {}

    virtual unsigned GetCount ();
    virtual OP_STATUS GetAttribute (unsigned index, XMLCompleteName &name, const uni_char *&value, BOOL &id, BOOL &specified, TempBuffer *buffer);

    void Reset (BOOL ignore_default0, BOOL ignore_nsdeclarations0, Element* attribute_owner) { ignore_default = ignore_default0; ignore_nsdeclarations = ignore_nsdeclarations0; element = attribute_owner; }

  private:
    BOOL ignore_default;
    BOOL ignore_nsdeclarations;
    Element* element;
  };

  Attributes attributes;

  OP_STATUS StartEntity (const URL &url, const XMLDocumentInformation &documentinfo, BOOL entity_reference);
  OP_STATUS StartElement (const XMLCompleteNameN &completename);
  OP_STATUS AddAttribute (const XMLCompleteNameN &completename, const uni_char *value, unsigned value_length, BOOL id);
  OP_STATUS AddCharacterData (XMLTreeAccessor::NodeType nodetype, const uni_char *data, BOOL delete_data);
  OP_STATUS AddProcessingInstruction (const uni_char *target, unsigned target_length, const uni_char *data, unsigned data_length);
  OP_STATUS AddComment (const uni_char *data, unsigned data_length);
  void EndElement ();
  OP_STATUS HandleTokenInternal (XMLToken &token);

#ifdef SELFTEST
  static BOOL CompareNodes (TreeNode *node1, TreeNode *node2);
#endif // SELFTEST

public:
  static XSLT_Tree *MakeL ();
#ifdef SELFTEST
  static OP_STATUS Make (XSLT_Tree *&tree);
#endif // SELFTEST

  virtual ~XSLT_Tree ();

  /* From XMLTreeAccessor: */
  virtual BOOL IsCaseSensitive();
  virtual XMLTreeAccessor::Node *GetRoot ();
  virtual XMLTreeAccessor::Node *GetParent (XMLTreeAccessor::Node *from);
  virtual XMLTreeAccessor::Node *GetFirstChild (XMLTreeAccessor::Node *from);
  virtual XMLTreeAccessor::Node *GetLastChild (XMLTreeAccessor::Node *from);
  virtual XMLTreeAccessor::Node *GetNextSibling (XMLTreeAccessor::Node *from);
  virtual XMLTreeAccessor::Node *GetPreviousSibling (XMLTreeAccessor::Node *from);
  virtual XMLTreeAccessor::NodeType GetNodeType (XMLTreeAccessor::Node *node);
  virtual BOOL IsEmptyText (XMLTreeAccessor::Node *node);
  virtual BOOL IsWhitespaceOnly (XMLTreeAccessor::Node *node);
  virtual void GetName (XMLCompleteName &name, XMLTreeAccessor::Node *node);
  virtual const uni_char *GetPITarget (XMLTreeAccessor::Node *node);
  virtual void GetAttributes (XMLTreeAccessor::Attributes *&attributes, XMLTreeAccessor::Node *node, BOOL ignore_default, BOOL ignore_nsdeclarations);
  virtual OP_STATUS GetData (const uni_char *&data, XMLTreeAccessor::Node *node, TempBuffer *buffer);
  virtual OP_STATUS GetElementById (XMLTreeAccessor::Node *&node, const uni_char *id);
  virtual URL GetDocumentURL ();
  virtual const XMLDocumentInformation *GetDocumentInformation ();

  /* From XSLT_OutputHandler: */
  virtual void StartElementL (const XMLCompleteName &name);
  virtual void AddAttributeL (const XMLCompleteName &name, const uni_char *value, BOOL id, BOOL specified);
  virtual void AddTextL (const uni_char *data, BOOL disable_output_escaping);
  virtual void AddCommentL (const uni_char *data);
  virtual void AddProcessingInstructionL (const uni_char *target, const uni_char *data);
  virtual void EndElementL (const XMLCompleteName &name);
  virtual void EndOutputL ();

  /* From XMLTokenHandler: */
  virtual XMLTokenHandler::Result HandleToken (XMLToken &token);

#ifdef SELFTEST
  static BOOL Compare (XSLT_Tree *tree1, XSLT_Tree *tree2);
#endif // SELFTEST
};

#endif // XSLT_SUPPORT
#endif // XSLT_TREE_H
