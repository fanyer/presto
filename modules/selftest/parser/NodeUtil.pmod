/* -*- Mode: pike; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

//! Utility functions that operate on Node "objects".  Must be here
//! since a Node is either an object or an array of Nodes. (yes, it's a
//! recursive type. :-))

//#include "ugly-macros.h"
import Parser.Pike;
typedef mixed Node;

int line( Node n )
//! Return the line-number of the node.
{
	if( arrayp( n ) )
	{
		int res;
		foreach( n, Node m )
			if( res = line( m ) )
				return res;
		return 0;
	}
	if( n && n->line )
		return n->line;
}

string trim_fn( string x )
{
// 	if( has_prefix( x, combine_path( __FILE__, "..", "..", "..", ".." ) ) )
// 		x = x[strlen(combine_path( __FILE__, "..", "..", "..", ".." ))+1..];
	return x;
}

array recursively_trim_fn( Node x )
{
// 	foreach( x, Node y ) 
// 		if( arrayp( y ) )
// 			recursively_trim_fn( y );
// 		else if( y->file )
// 			y->file = trim_fn( y->file );
	return x;
}

string file( Node n )
//! Return the file the node comes from or 0 if there was no file
{
	if( arrayp( n ) )
		if( sizeof(n) )
			return file( n[0] );
		else
			return 0;
	if( n && n->file )
		return trim_fn( n->file );
	return 0;
}


string text( Node n )
//! Return the text of the node.
{
	if( arrayp( n ) )
		return map(n,text)*"";
	if( n )
		return n->text+n->trailing_whitespaces;
	return "";
}


Node fix_call( Node n )
{
	if( objectp( n ) )
	{
		if( n->call )
		{
			mixed x = n->call();
			if( arrayp( x ) || objectp(x) )
				return x;
			return Token(x,line(n),file(n));
		}
		return n;
	}
	else
		return map( n, fix_call );
}

Node replace_identifier( Node x, string ident, string new_ident )
{
	if( objectp( x ) )
	{
		if( x->text == ident )
			return Token( new_ident, line(x), file(x), x->trailing_whitespaces );
		return x;
	}
	return map( x, replace_identifier, ident, new_ident );
}

Node replace_identifiers( Node x, mapping(string:Node) idents )
{
	if( objectp( x ) )
	{
		if( Node new = idents[x->text] )
			return new;
		return x;
	}
	return map( x, replace_identifiers, idents );
}

array trim( array what )
{
	return what[1..sizeof(what)-2];
}

Node dup_node( Node n )
{
	if( objectp( n ) )
		return Token( n->text, n->line, n->file, n->trailing_whitespaces );
	if( arrayp(n) )
		return map(n,dup_node);
	return n;
}

Node replace_identifiers_s( Node x, mapping(string:Node) idents )
{
	array res = allocate( sizeof( x ) );
	int concat, i;
	foreach( x, Node q )
	{
		if( arrayp( q ) )
		{
			if( concat )
			{
				array tmp = ({});;
				foreach( trim(replace_identifiers_s(q,idents)), Node z )
				{
					if( z->text != "," )
						tmp += ({ z });
				}
				res[i]=tmp;
				concat = 0;
			}
			else
				res[i]=replace_identifiers_s(q,idents);
		}
		else if( q->text == "CONCAT" )
		{
			concat = 1;
			res[i] = Token("");
		}
		else if( mixed new = idents[q->text] )
		{
			if( stringp(new)||intp(new) )
				res[i]=Token((string)new,0,0,q->trailing_whitespace);
			else
				res[i]=dup_node(new);
		}
		else
			res[i] = q;
		i++;
	}
	return res;
}

Node replace_macro( Node x, string ident, string new_ident )
{
	if( objectp( x ) )
	{
 		if( has_value( x->text, ident ) )
			return Token( replace( x->text, ident, new_ident ), line(x), file(x), x->trailing_whitespaces );
 		return x;
	}
	return map( x, replace_macro, ident, new_ident );
}

static constant groupings = ([ "{":"}", "(":")", "[":"]" ]);
static constant actions   = (["{":1,"}":2,"(":1,")":2,"[":1,"]":2, ]);

Node group(Node tokens)
{
	ADT.Stack stack = ADT.Stack();
	array(Token) ret=({});

	foreach(tokens, Token token)
	{
		switch(actions[token->text])
		{
			case 0: 
				ret+=({token});
				break;
			case 1:
				stack->push(ret);
				ret=({token});
				break;
			case 2:
				if (!sizeof(ret) || !stack->ptr || (groupings[ret[0]->text] != token->text))
					throw( token );
				ret=stack->pop()+({ ret + ({token}) });
		}
	}
	if( stack->ptr )
		throw( ({ret[0]}) );
	return ret;
}
