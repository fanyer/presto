/* -*- Mode: pike; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

// Utility functions.

constant TOKEN_VERSION = 5;
import Parser.Pike;

#include "ugly-macros.h"

typedef mixed Node;

class CallClass
{
  string file;
  int line;
  string call();
}

mapping cache_stat = ([]);
Stdio.Stat cache_file_stat( string fn )
{
	return cache_stat[fn] || (cache_stat[fn]=file_stat(fn));
}


Token get_identifier( string X, Token tk, Token token )
{
  if(!tk||arrayp(tk)||!is_identifier(tk))
    SYNTAX("Expected identifier after '"+(X)+"', got "+sprintf("%O",(arrayp(tk)?tk[0]:tk||token)->text),
	   (arrayp(tk)?tk[0]:tk||token));
  return tk;
}

static int is_identifier( Node t )
{
  if( arrayp(t) )
    return 0;
  if( sizeof(t->text/""-
     "ab_cde1234567890fghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"/"" ) )
    return 0;
  if( t->text[0] >= '0' && t->text[0] <= '9' )
    return 0;
  return 1;
}

string recursively_add_constants( Node n )
{
    if( arrayp( n ) )
		return map( n, recursively_add_constants ) * "";
    if( is_identifier(n) )
		return
			"#if !defined("+n->text+") && !constant("+n->text+")\n"+
			"#define "+n->text+" 0\n"+
			"#endif\n";
    return "";
}

mixed parse( string type, Node n, int noerr )
{
	if( arrayp(n) && !sizeof(n) )
		throw("Expected "+type+", got empty expression\n");

	class Handler
	{
		void compile_error(){};
		void compile_warning(){};
	};
	catch 
	{
		string code = (arrayp(n)?simple_reconstitute(n):n->text), pre = "";
		if( type == "bool" )
		{
			type = "mixed";
			pre = recursively_add_constants( n );
		}
		return compile_string( pre+"\n"+type+" foo(){ return "+code+"; }", 0,
							   noerr?Handler():0 )()->foo();
	};
	if( !noerr )
		SYNTAX(sprintf("Expected "+type+", got %O",
					   (arrayp(n)?simple_reconstitute(n):n->text)),
			   (arrayp(n)?(sizeof(n)?n[0]:0):n) );
	return noerr != 1 ? noerr : 0;
}

string parse_string( Node n, int|void noerr )
{
  return (string)parse( "string", n, noerr );
}

int parse_int( Node n, int|void noerr )
{
  return (int)parse( "int", n, noerr );
}

float parse_float( Node n, int|void noerr )
{
  return (float)parse( "int|float", n, noerr );
}

array(Token) tokenize(array s, string file)
{
    int line=1, size_change;
    string t;
    for(int e=0;e<sizeof( s );e++)
    {
		if( (t = s[e])[0] == '#' )
			if( (sscanf(t,"#%*[ \t\14]%d%*[ \t\14]\"%s\"", line,file) == 4) ||
				(sscanf(t,"#%*[ \t\14]line%*[ \t\14]%d%*[ \t\14]\"%s\"",line,file)==5))
			{
				size_change=1;
				s[e] = 0;
				continue;
			}
	
		s[e]=Token(t,line,file,"");
		line += String.count( t, "\n" );
    }
    if( size_change ) return s-({0});
    return s;
}

// Generate a shared uni_char[] string from the given (constant) string.
string TS_STRING( object g, string x, mixed ... args )
{
	x += "\0";
	if( sizeof( args ) ) x = sprintf( x,@args );

	int offset;
	if( zero_type(offset = g->string_already[x]) )
	{
		offset = strlen( g->global_strings );
		g->global_strings += x;
		g->string_already[x] = offset;
	}
	return "(_TS_STRINGS_+"+offset+")";
}

// Generate a shared const char[] string from the given (constant) string.
string TS_CSTRING( object g, string x, mixed ... args )
{
	x+="\0";
	if( sizeof( args ) ) x = sprintf( x,@args );

	int offset;
	if( zero_type(offset = g->cstring_already[x]) )
	{
		offset = strlen( g->global_cstrings );
		g->global_cstrings += x;
		g->cstring_already[x] = offset;
	}
	return "reinterpret_cast<const char*>(_TS_CSTRINGS_+"+offset+")";
}

// Add parantheses to the string if needed to add the string to a
// 'complex' (currently only ifdef) expression. Used but the
// require-code.
string paranthesize_complex( string x )
{
	if( strlen( x )
		&& (has_value( x, "&&" ) || has_value( x, "||" )) 
		&& x[0] != '(' )
		return "("+x+")";
	return x;
}

// Build a moderately optimized '#if X || Y || Z', where X Y and Z are
// all somewhat complex expressions (only supports defined or
// undefined preprocessor tokens, and negative (must not be) or
// positive (must be) conditions)
string make_require_or( array(multiset|object|mapping) requires )
{
	string defs = "";
	mapping have_reqs = ([]);

	if( !sizeof( requires ) )
		return "";

	foreach( requires, multiset req )
		if( !sizeof(req) )
			return "";

	foreach( requires, multiset reqs )
	{
		if( !sizeof( reqs ) )
			continue;
		string tmp = "";
		foreach( sort((array)reqs), string def ) 
		{
			if( strlen( tmp ) )
				tmp += " && ";
			tmp += def;
		}
		if( !have_reqs[tmp]++ ) 
		{
			if( strlen( defs ) )
				defs += " || ";
			tmp = paranthesize_complex( tmp );
			defs += tmp;
		}
	}
	return defs;
}

// Basically the same as make_require_or, but only handles one basic
// list of requirements, subtracting things already required by the
// parent (group or similar)
array(string) make_require( mapping|object options, mapping|object parent, string name )
{
	string pre="",post="";
	multiset req = (multiset)options->require;
	if( parent && parent->require )
		req -= parent->require;
	if( options->disabled && (!parent || !parent->disabled) )
		return ({"#if 0 /* DISABLED */\n",  "#endif\n" });

	if( sizeof( req ) )
	{
		string defs = "";
		foreach( sort((array)req), string def )
		{
			if( sizeof( defs ) )
				defs += " && ";
			defs += def;
		}
		pre = "#if "+defs+"\n";
		post = "#endif // "+defs+"\n";
		if( name )
			pre += "#define "+name+"_DEFINED 1\n";
		else if( name )
			return ({ "#if "+name+"_DEFINED", "#endif // "+name });
	}
	return ({ pre, post });
}

// Split "toplevel" of the (c-like) expression in code on 'token'.
// split_one_level("a && b && (c || (d && q))", "&&" ) will return
// ({ "a", "b", "(c || (d && q))" })
array(string) split_one_level( string code, string token )
{
	catch 
	{
		Node z = group(SPLIT( code, "-" ));
		int soff;
		array(Node) res = ({});
		for( int i = 0; i<sizeof( z ); i++ )
		{
			if( objectp(z[i]) && z[i]->text == token )
			{
				res += ({z[soff..i-1]});
				soff = i+1;
			}
		}
		res += ({z[soff..]});
		return map(map( res, simple_reconstitute ),String.trim_all_whites);
 	};
	return ({code});
}


// return a name that is safe to use as a class, function or variable
// name.
constant safe = (multiset)((array)"_0123456789abcdefghijklmnopqrstuvwxyz");
constant really_safe = (multiset)((array)"_abcdefghijklmnopqrstuvwxyz");

string safe_name( string name, int|void nolimit )
{
	string g=replace(lower_case(name)," ","_");
	string f=filter( g,really_safe);
	if ( strlen( f ) != 0 )
	{
		string h=filter( g,safe );
		if ( f[0] == h[0] )
			f = h;
	}
	if( !nolimit && strlen( f ) > 21 )
		return f[..5]+"_"+hash(name)->digits(32)+"_"+f[strlen(f)-10..];
	return f;
}


// Removes the first and last element of an array. 
array trim( array what )
{
	return what[1..sizeof(what)-2];
}


// Parse a list (recursively)
//
// Given '{ item:value, item number 2, {sub1,sub2}}'
// ({  
//    ({ "item", "value" }), 
//    ({ "itemnumber2", "itemnumber2" }), 
//    ({ 
//        ({"sub1","sub1"}), 
//        ({"sub2","sub2"}) 
//     }) 
// })
array do_group( array values, int|void lvl, mapping|void start )
{
	if( !sizeof( values ) )
		return ({});

	if( sizeof( values ) && arrayp( values[0] ) )
	{
		array res = ({});
		foreach( values, mixed q )
			if( arrayp( q ) )
			{
				if( start )
					start->tokens[start->index++] = q[0];
				res += ({ do_group( trim(q),lvl+1 ) });
			}
		return res;
	}
	array res = ({});
	string val;
	string mval = "";
	int st;
	foreach( values, Node x )
	{
		if( x == "," )
		{
			if( val )
			{
				res += ({ ({ String.trim_all_whites(val), mval }) });
				val = mval = 0;
				st = 0;
			}
		}
		else if( x == ":" )
		{
			st++;
			mval = "";
		}
		else if( st )
		{
			mval += parse_string( x );
		}
		else
		{
			if( val )
			{
				if( arrayp( x ) )
					val += simple_reconstitute( x );
				else
					val += simple_reconstitute( ({x}) );
				mval = val;
			}
			else
			{
				if( arrayp( x ) )
					val = simple_reconstitute( x );
				else
					 val = simple_reconstitute( ({x}) );
				mval = String.trim_all_whites(val);
			}
		}
	}
	if( val )
		res += ({ ({ String.trim_all_whites(val), mval }) });
	
	return res;
}

// Returns 1 if x _might_ be a type. Used to find the start and end of
// function declarations
int is_likely_type( Node x )
{
	if( !x || !objectp(x) ) return 0;
	if( arrayp( x ) )
	{
		if( x[0] != "(" && x[0] != "<" )
			return 0;
// 		for( int i = 1; i<sizeof(x)-1; i++ )
// 			if( !is_likely_type( x[i] ) )
// 				return 0;
		return 1;
	}
	if( x == "*" || x== "&" || x == "::" || x == ":" || x == "," )
		return 1;
	if( safe_name( (string)x,1 ) != lower_case((string)x) || 
		(<'0','1','2','3','4','5','6','7','8','9','.'>)[(int)x[0]])
		return 0;
	return 1;
}
