//
// Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Arjan van Leeuwen (arjanl)
//

#ifndef IMAP_NAMESPACE_H
#define IMAP_NAMESPACE_H

class ImapNamespace
{
public:
	// Types used
	enum NamespaceType
	{
		PERSONAL,
		OTHER,
		SHARED
	};
	
	OP_STATUS		  SetNamespace(const OpStringC8& name, char delimiter)
										{ m_delimiter = delimiter; return m_name.Set(name); }
	const OpStringC8& GetName() 		{ return m_name; }
	char			  GetDelimiter()	{ return m_delimiter; }
			
private:
	OpString8		  m_name;
	char			  m_delimiter;
};

#endif // IMAP_NAMESPACE_H
