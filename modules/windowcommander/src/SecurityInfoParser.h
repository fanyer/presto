/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/**
 * @file security_info_parser.h
 *
 * Dialog that is used to display security information (basically certificate details)
 *
 * Currently also contains the parser to extract the security information from the information url.
 */

#ifndef SECURITY_INFO_PARSER_H_
# define SECURITY_INFO_PARSER_H_

# ifdef SECURITY_INFORMATION_PARSER
#  include "adjunct/desktop_util/treemodel/simpletreemodel.h"

// Needed to look up hosts that are loaded from cache
#  include "modules/pi/network/OpHostResolver.h"
#  ifdef _XML_SUPPORT_
#   include "modules/xmlutils/xmlparser.h"
#   include "modules/xmlutils/xmltokenhandler.h"
#  endif // _XML_SUPPORT_
#  include "modules/windowcommander/OpSecurityInfoParser.h"

/**
 * Class that is used to parse the security information that the ssl-module provides in the form of an URL-instance
 * which contains the information in XHTML-form. This class parses that information as XML and extracts the values
 * that are relevant, and stores them in members that are accessible through getters. In addition the class creates
 * a hierarchical representation of the information in a SimpleTreeModel, that contains the security information in
 * a generic form that is used by the SecurityInformationDialog class to display the certificate.
 */
class SecurityInformationParser : public OpSecurityInformationParser
#ifdef _XML_SUPPORT_
	, public XMLTokenHandler
#endif // _XML_SUPPORT_
{
public:
	/** Helper enum used in the parsing to classify tags according to their role in the parsing. */
	enum TagClass
	{
		NEW_NODE_TAG,
		CONTENT_IS_PARENT_HEADING,
		TAG_NAME_DEFINES_PARENT_HEADING,
		IGNORE_TAG,
		MARKER_TAG,							///< A tag that is there to indicate some special meaning in a text block. The parser should treat it as air.
		REFERENCE_TAG,
		BOOLEAN_TAG,						///< A tag that indicates a bool value by its presence, and has no content.
		ORDINARY_TAG
	};
private:
	/**
	 * Class used to represent the nodes in the security information tree. The nodes can iterate over their children to simplify parsing.
	 *
	 * It can also keep track of a nodes location in a treemodel, when that is being built during MakeTreeModel.
	 */
	class SecurityInfoNode
	{
	private:
		OpString                   tag_name; ///< This is the name of the tag that this node originated from. Can't be set after the node has been created.
		OpString                   header;   ///< This is the header of the node, the text that is supposed to appear in the tree view.
		OpString                   text;     ///< The content of this node, the text that is supposed to appear in the edit area. This field contains the text between the tags for this particular node.
		OpVector<SecurityInfoNode> children; ///< List of children of this node.
		SecurityInfoNode*          parent;   ///< Pointer to the parent node.
		INT32                      position; ///< If the node is inserted in the security information tree, this position is set to the position in that tree. Has a negative, illegal value if the node is not in the tree.
		UINT32                     iterator; ///< This variable is used to iterate over the children of the node to simplify traversal. The simple system used here is vulnerable to unexpected behaviour, such as insertions or deletions while iterating.
	public:
		/**
		 * Constructor that creates the node. The only values that are set here are the name of the original tag and a pointer to the parent. The rest of the information is explicitly blanked, and
		 * the iterator is reset.
		 * @param tag_name The name of the tag that this tag originates from.
		 * @param parent A pointer to the parent element. Can be NULL for the root element.
		 */
		                           SecurityInfoNode(const uni_char* tag_name, SecurityInfoNode* parent);
		/**
		 * Destructor. Will iterate over all the children and destroy them all. A destroy call to the root of a tree will
		 * destroy the entire tree.
		 */
		                           ~SecurityInfoNode();
		// This group of functions does what their names indicate.
		void                       AddChild(SecurityInfoNode* child)               {children.Add(child);}
		uni_char*                  GetTagName()                                    {return tag_name.CStr();}
		uni_char*                  GetHeader()                                     {return header.CStr();}
		void                       AppendToHeader(const uni_char* string, int len) {header.Append(string, len);}
		void                       SetHeader(const uni_char* string, int len)      {header.Set(string, len);}
		uni_char*                  GetText()                                       {return text.CStr();}
		void                       AppendToText(const uni_char* string, int len)   {text.Append(string, len);}
		BOOL                       HasChildren()                                   {return children.GetCount() > 0;}
		OpString*                  GetTextPtr()                                    {return &text;}
		OpString*                  GetHeaderPtr()                                  {return &header;}
		SecurityInfoNode*          GetParent()                                     {return parent;}
		INT32                      GetPositionInTree()                             {return position;}
		BOOL                       IsInTree()                                      {return (position >= 0);}

		/** Resets the GetNextChild() function. */
		void                       ResetChildIterator()                            {iterator = 0;}

		/**
		 * Returns the next child in the list of children for this node. Call ResetChildIterator before starting to use this function.
		 * The behaviour will be wrong if ResetChildIterator is called while iterating, or if children are added or removed during the
		 * iteration.
		 *
		 * @return A pointer to the next child if it exists, NULL otherwise.
		 */
		SecurityInfoNode* GetNextChild();

		/**
		 * Function for direct access to a child.
		 *
		 * @param index The index of the child
		 * @return A pointer to the specified child, if it exists, NULL otherwise.
		 */
		SecurityInfoNode* GetChild(unsigned int index);

		/**
		 * This is used to mark a node as inserted into the tree, and what position it is inserted in.
		 * The call is also used on nodes which are not actually inserted on the tree, but have been visited and
		 * found to be irrelevant. The position then indicates the position of the parent.
		 *
		 * @param pos The position that the node has in the tree model.
		 */
		void SetTreePosition(INT32 pos) {position = pos;}
	};

	class ParsingState
	{
	private:
		class TagStack
		{
		private:
			OpVector<OpString> tag_stack;
		public:
			TagStack();
			~TagStack();
			void Init();
			void Push(OpString &tag);
			OpString* Pop(OpString &tag);
			OpString* Top(OpString &tag);
		};
		TagStack m_tag_stack;
		int m_certificate_number;
		int m_protocol_number;
		int m_certificate_servername_number;
		int m_url_number;
		int m_port_number;
		int m_subject_number;
		int m_issuer_number;
		int m_issued_depth;
		int m_expires_depth;
		OpString* m_current_certificate_name;
		OpString m_current_node_text;
	public:
		ParsingState();
		~ParsingState();
		void Init();
		void StartTag(OpString &tag);
		OpString* EndTag(OpString &tag);
		OpString* GetTopTag(OpString &tag);
		BOOL IsURL();
		BOOL IsPort();
		BOOL IsProtocol();
		BOOL IsCertificateServerName();
		BOOL IsSubject();
		BOOL IsIssuer();
		BOOL IsFirstCert();
		BOOL IsIssuedDate();
		BOOL IsExpiredDate();
		TagClass GetTopTagClass();
		TagClass GetTopNonMarkerTagClass();
		void SetCurrentNodeText(OpString &text);
		void AppendCurrentNodeText(OpString &text);
		OpString* GetCurrentNodeText(OpString &text);
		void ClearCurrentNodeText();
		void SetCurrentCertificateNamePtr(OpString* name_ptr);
		OpString* GetCurrentCertificateNamePtr();
	};

	ParsingState* m_state;

	// Variables
	URL								m_security_information_url;         ///< The URL-instance containing the security information.
	SimpleTreeModel*				m_tree_model;
	SimpleTreeModel*				m_validated_tree_model;
	SimpleTreeModel*				m_client_tree_model;

	OpSecurityInformationContainer* m_info_container; ///< A container to hold the security information for the connection. Used for convenience and to reduce the risk of memory leaks.

	// Used for temporary storage during parsing. Really the parsing state machine.
	SecurityInfoNode* m_security_information_tree_root;
	SecurityInfoNode* m_current_sec_info_element;
	SecurityInfoNode* m_server_cert_chain_root; ///< If there is a <servercertchain> in the document, this will be set to it. Otherwise, if there is a <certificate> in the document, it will be set to that. If neither are present, it will be NULL.
	SecurityInfoNode* m_validated_cert_chain_root;
	SecurityInfoNode* m_client_cert_chain_root;
	//OpString* m_current_certificate_name;
	//OpString m_current_node_text;

	// Used for counting specific tags encountered. Used to select the correct parts of certificates.
	//int m_certificate_number;
	//int m_protocol_number;
	//int m_url_number;
	//int m_subject_number;
	//int m_issuer_number;
	//OpVector<OpString> tag_stack; // Helper vector used as a stack during parsing
	/** Helper function used to classify tags during parsing. */
	//TagClass ClassifyTag(const uni_char* tag_name);

	// Implementation of the XMLTokenHandler API
	virtual XMLTokenHandler::Result	HandleToken(XMLToken &token);

	void StartElementHandler(XMLToken &token);
	/** XML Parsing helper. */
	void EndElementHandler(XMLToken &token);
	/** XML Parsing helper */
	void EmptyElementHandler(XMLToken &token);
	/** XML Parsing helper. */
	void CharacterDataHandler(const uni_char *s, int len);
	/**
	 * This function is used to populate a SimpleTreeModel with the data parsed from the XML-document.
	 *
	 * The function edits the tree that was build during parsing in order to do this, so it will only
	 * work once. (Yes, this is not very good design, I'm planning on fixing that. mariusab 20050201)
	 * You access the treemodel externally by using the GetTreeModelPtr() function.
	 *
	 * @return OK if the parse went well, an error code otherwise.
	 *
	 * @see GetTreeModelPtr()
	 */
	OP_STATUS MakeServerTreeModel(SimpleTreeModel &tree_model);
	/**
	 * Fill me with useful information...
	 */
	OP_STATUS MakeClientTreeModel(SimpleTreeModel &tree_model);
	/**
	 * Fill me with useful information...
	 */
	OP_STATUS MakeValidatedTreeModel(SimpleTreeModel &tree_model);
public:
	/**
	 * Constructor
	 */
	SecurityInformationParser();
	/**
	 * @param security_information_url A reference to the URL-object
	 * that holds the security information. NOT a reference to the
	 * URL that the security information describes.
	 */
	OP_STATUS SetSecInfo(URL security_information_url);
	/**
	 * Destructor. Removes the entire underlying data structure.
	 * References to parsed strings will NOT be usable after
	 * destruction of the parser.
	 */
	~SecurityInformationParser();
	/**
	 * This function performs the parsing of the xml-document and
	 * builds the data structure that holds the certificate information.
	 *
	 * @param limited_parse Set to true to make the parser parse the document
	 * in small chunks, and abort as soon as the subject cn and co has been
	 * parsed out.
	 */
	OP_STATUS Parse(BOOL limited_parse = FALSE);
	/**
	 * Perform a limited parsing of the xml document to only parse out
	 * the subject cn and co, so that the security button can be populated.
	 * Parsing will be aborted after the relevant pieces of information
	 * are extracted.
	 *
	 * @param button_text	The button text extracted.
	 * @param isEV			Security mode is EV or not.
	 */
	OP_STATUS LimitedParse(OpString &button_text, BOOL isEV);
	/**
	 * Helper, used to encapsulate the somewhat dirty extraction of the
	 * information that is extra relevant, and must be displayed on
	 * the front tab of the certificate dialog.
	 */
	void CheckForAndExtractSpecialInfo(OpString &current_XML_tag);

	/**
	 * Get a tree model from the parsed information.
	 *
	 * If the tree model has not been created already, then MakeTreeModel will be called, but that will only happen once.
	 * A member pointer will keep track of the model, and it will be deleted on deletion of the parser.
	 *
	 * @return A pointer to a treemodel that represents the contents of the security information. If some error occurs, it returns NULL.
	 *
	 * @see MakeTreeModel()
	 */
	SimpleTreeModel* GetServerTreeModelPtr();
	/**
	 *
	 */
	SimpleTreeModel* GetClientTreeModelPtr();
	/**
	 *
	 */
	SimpleTreeModel* GetValidatedTreeModelPtr();

	OpSecurityInformationContainer* GetSecurityInformationContainterPtr() {return m_info_container;}
};

# endif // SECURITY_INFORMATION_PARSER
#endif /*SECURITY_INFO_PARSER_H_*/
