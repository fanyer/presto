/* -*- Mode: pike; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

mapping(string:Node) code_snipplets = ([]);

static Node recursively_strip_line_statements(Node tokens)
{
	if (arrayp(tokens))
		map (tokens, recursively_strip_line_statements);
	else
	{	
		tokens->line = 0;
		tokens->file = 0;
	}
	
	return tokens;
}

string code_snipplet( string name, mapping|void replaces )
{
#ifdef DEBUG
	if( !code_snipplets[name] )
		error("Undefined code snipplet %O\n", name );
#endif
 	if( replaces && sizeof( replaces ) )
		return simple_reconstitute(.NodeUtil.replace_identifiers_s( code_snipplets[name], replaces ));
 	return simple_reconstitute(code_snipplets[name]);
}


Node code_snipplet_t( string name, mapping|void replaces )
{
#ifdef DEBUG
	if( !code_snipplets[name] )
		error("Undefined code snipplet %O\n", name );
#endif
	if( replaces && sizeof( replaces ) )
		return .NodeUtil.replace_identifiers_s( code_snipplets[name], replaces );
	return code_snipplets[name];
}

Node code_snipplet_stripped_t( string name, mapping|void replaces )
{
	return recursively_strip_line_statements(code_snipplet_t(name, replaces));
}

static void recursively_fix_dollar( Node tokens )
{
	if( arrayp( tokens ) )
		map( tokens, recursively_fix_dollar );
	else 
		tokens->text = replace( tokens->text, "\10000", "$" );
}

void code_snipplet_init()
{
	string file = combine_path( __FILE__, "../../src/code_snipplets.cpp");
	string snipsnap;
	if( !(snipsnap=Stdio.read_file( file ) ) )
	{
		werror("ERROR: Failed to read code snipplet template file \""+file+"\"\n");
		exit( 1 );
	}
	snipsnap -= "\r";
	Node tokens = SPLIT(replace(snipsnap,"$","\10000"),file);
	recursively_fix_dollar( tokens );
	if( Node offending = catch( tokens = .NodeUtil.group( tokens ) ) )
		if( objectp( offending ) )
			SYNTAX( sprintf("Syntax error; stray %O", offending->text ), offending );
		else
			SYNTAX( sprintf("Syntax error; unmatched %O", offending[0]->text ), offending[0] );

	string name;
	int next_is_code, is_ecma;
	foreach( tokens, Node tk )
	{
		if( next_is_code )
		{
			if( tk->text == "ecma" )
			{
				is_ecma = 1;
			}
			else
			{
				if( is_ecma )
				{
					tk->file = 0;
					tk->line = 0; // Tricky trick. Recursively set file and line in the tokens to 0.
				}
				code_snipplets[name] = trim(tk);
				if( !is_ecma )
					code_snipplets[name] += ({Token("\n")});
				next_is_code = 0;
				is_ecma = 0;
			}
		}
		else if( tk->text == ":" )
			next_is_code = 1;
		else if( !arrayp(tk) )
			name = tk->text;
	}
}
