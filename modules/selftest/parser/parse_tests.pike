/* -*- Mode: pike; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
//#define DEBUG
#charset utf-8

// Check for pike version, and add implementations for some "new"
// things (pike 7.4 and newer) used in this file when running with
// older pikes.
#include "pike-compat.h"

// The SYNTAX(), ERROR() and C-parser macros.
#include "ugly-macros.h"

#ifdef __NT__
#  define lower_on_nt lower_case
#  define normalize_path_on_nt normalize_path
#else
#  define lower_on_nt
#  define normalize_path_on_nt
#endif

void abort(int err)
{
	throw(err);
}

bool quiet = false;
bool write_deps = false;

// The main source directory for Opera.  Calculated using the filename
// for this file.
constant prefix = lower_on_nt(combine_path( __FILE__, "..", "..", "..", ".." ));
// Well. Guess.
string outroot = prefix;
string cachedir = combine_path( outroot, "modules", "selftest", "parser", "cache" );
string outputdir = combine_path( outroot, "modules", "selftest", "generated" );
string module_filter_file = combine_path( prefix, "modules", "selftest", "selftest.modules" );
string test_files_listing = combine_path( outroot, "modules", "selftest", "selftest_data" );

string common_deps
	= combine_path( prefix, "modules/selftest/parser/parse_tests.pike" )
	+ " " + combine_path( prefix, "modules/selftest/parser/utilities.pike" )
	+ " " + combine_path( prefix, "modules/selftest/parser/code_snipplets.pike" )
	+ " " + combine_path( prefix, "modules/selftest/parser/NodeUtil.pmod" )
	+ " " + combine_path( prefix, "modules/selftest/parser/ugly-macros.h" )
	+ " " + combine_path( prefix, "modules/selftest/parser/pike-compat.h" )
	+ " " + combine_path( prefix, "modules/selftest/src/code_snipplets.cpp" )
	+ " " + combine_path( prefix, "modules/selftest/src/grouptemplate.cpp" );

// The ID:s for the next item.
int test_num, group_num, table_num;

// The generated data.
mapping(string:Table) tables = ([]);
mapping(string:Group) groups = ([]);
mapping depends = ([]);
mapping colls=([]);
int changed_testcase_count = 0;
array(string) global_test_files = ({});


void reset()
{
	test_num = group_num = table_num = 0;
	tables = ([]);
	colls = ([]);
	groups = ([]);
}


#include "utilities.pike"
#include "code_snipplets.pike"

// Used to keep names unique.
int name_collision( string name, mixed x )
{
	if( colls[name] )
		return colls[name]!=x;
	colls[name]=x;
	return 0;
}

// A table, optionally with types. Useable from foreach and iterate
class Table
{
	array(string)        types;
	protected array(array(string)) table;
	protected Node                 error_token;
	protected int                  do_define;
	private mapping              table_tokens;
	private array(string)		 requires = ({});
	Group group;
	int line;
	multiset require = (<>);

	string _sprintf( )
	{
		return sprintf("Table(%s:%d)", group?(group->file/"/")[-1]:"?", line );
	}


	int `>( Table x ) {	return line > x->line; }
	int `<( Table x ) {	return line < x->line; }

	string c_name()
	{
		return "ST_table_"+line;
	}

	string get_type( string arg, array(string) names )
	{
		return types[search(names,arg)];
	}

	void create( array(string) _types, Node _table )
	{
		types = _types;
		error_token = _table[0];
		line = .NodeUtil.line(_table[0]);

		// preparse for #ifdefs
		Regexp.SimpleRegexp ifdefre = Regexp.SimpleRegexp("#[ \t]*elif|if|endif|else");
		string aa = "";
		foreach(_table, array|mixed element)
		{
			if (!arrayp(element))	// ignore actual elements
			{
				if (ifdefre->match(element->text))
				{
					aa += element->text;
				}
			}
			else
			{
				requires += ({aa});
				aa = "";
			}
		}
		// In case there are an "endif" at the end of the table.
		requires += ({aa});

		// The actual parse
		mapping starts = ([
			"index":0,
			"tokens":([])
		]);
		table = do_group( trim( _table ), 0, starts );

		if (sizeof(table) == 0)
			WARN("Table is empty.", error_token);
		table_tokens = starts->tokens;

		// Check table dimensions
		int sz = (types ? sizeof(types) : 0), nm;
		foreach( table, array qq )
		{
			nm++;
			if( !sz )
				sz = sizeof(qq);
			else if( sizeof( qq ) != sz )
			{
				WARN("All elements in the table are not of equal size.", error_token );
				WARN(sprintf("Element %d is not of size %O, but instead %d (%{%O,%})", nm, sz, sizeof(qq),qq),error_token );
			}
		}
	}

	string ifdef()
	{
		if( sizeof(group->require) )
			return "#ifdef "+c_name()+"_DEFINED\n";
		return "";
	}

	Node define( )
	{
		string pre,post;
#ifdef DEBUG
		werror("Calling define for %s do_define = %d\n", c_name(), do_define);
#endif
		if( !do_define || !sizeof(table))
			return "";

		[pre,post] = make_require( group, 0, c_name() );
		array res  = ({Token("\n"),TOKEN(pre+"\n"),Token("int " + c_name()+"_size;\n"),Token("struct {\n")});
		for( int i = 0; i<sizeof( types ); i++ )
			res += ({ Token(" "+types[i]+" v"+i+";\n",line, .NodeUtil.file(error_token)) });
		if( sizeof(table) == 0 )
			WARN("Table is empty", error_token);
		return ({res, Token("}\n "),Token(c_name()+"["+sizeof(table)+"];\n"),Token(post)});
	}

	string initialize()
	{
		if( !do_define || !sizeof(table))
			return "";

		if( !group )
			ERROR("Table outside group used for iterate. Move the table into the group where it's used.", error_token );

		string res = ifdef();
		string end = strlen(res)?"#endif":"";
		res = "\n"+res;
		Token last;
		int i;

		res += c_name()+"_size=0;\n";
		foreach( table, array vv )
		{
			if( table_tokens[i] )
			{
				if(table_tokens[i]->line > 0)
					if( !last ||  table_tokens[i]->file != last->file ||table_tokens[i]->line != last->line+1 )
						res += sprintf("#line %d %O\n", table_tokens[i]->line, .NodeUtil.file(table_tokens[i])  );
				last = table_tokens[i];
			}

			if(sizeof(requires[i]))
				res += requires[i];

			for( int j=0; j<sizeof(vv); j++ )
				res += c_name()+"["+c_name()+"_size"+"].v"+j+"="+vv[j][0]+"; ";
			res += c_name()+"_size++;\n";

			i++;
		}

		// In case we have an endif at the end of the table
		if(sizeof(requires[i]))
			res += requires[i];

		return res+end;
	}

	Node iterate( array idents, Node code )
	{
		if (!sizeof(table))
			return "";

		do_define = 1;
		if( !types )
			SYNTAX( "This table lacks types, it's not possible to use it as a struct.", error_token );

		if( sizeof( idents ) > sizeof( types ) )
			SYNTAX(sprintf("Too many identifiers for table, only %d elements available, need %d\n",
						   sizeof(types),sizeof(idents)), code[0] );
		mapping reps = ([]);
		for( int i = 0; i<sizeof(idents); i++ )
			reps[idents[i]] = Token(c_name()+"[__iter__].v"+i);
		code = .NodeUtil.replace_identifiers( code, reps );
// 			code = .NodeUtil.replace_identifier( code, idents[i], c_name()+"[__iter__].v"+i );
		return ({
			({
				TOKEN("{"),
				TOKEN("OP_MEMORY_VAR int __iter__; for( __iter__ = 0; __iter__<"+c_name()+"_size"+"; __iter__++ ) \n"),
				code,
				TOKEN("}")
			}),
		});
	}

	array(array(string)) get_table()
	{
		return table;
	}

	bool need_path_prefix()
	{ return false; }
}

class FileListTable
{
	inherit Table;

	Node define()
	{
		foreach( table, array vv )
		{
			for( int j=0; j<sizeof(vv); j++ )
			{
				string path = vv[j][0];
				path = path[1..sizeof(path)-2];
				global_test_files += ({  combine_path(dirname(error_token->file) - (prefix + "/"), path ) });
			}
		}

		return ::define();
	}

	Node iterate( array idents, Node code )
	{
		if (!sizeof(table))
			return "";

		do_define = 1;
		if ( !types )
			SYNTAX( "This table lacks types, it's not possible to use it as a struct.", error_token );

		if ( sizeof( idents ) > 1 )
			SYNTAX(sprintf("Too many identifiers for table, only %d elements available, need %d\n",
						   sizeof(types),sizeof(idents)), code[0] );

		mapping reps = ([ idents[0] : Token("ST_make_path(" + c_name() + "[__iter__].v0)")]);
		code = .NodeUtil.replace_identifiers( code, reps );
		return ({
			({
				TOKEN("{"),
				TOKEN("OP_MEMORY_VAR int __iter__; for( __iter__ = 0; __iter__<"+c_name()+"_size"+"; __iter__++ ) \n"),
				code,
				TOKEN("}")
			})
		});
	}

	bool need_path_prefix()
	{ return true; }
}

// A single verify(X), verify_status or verify_success in the code.
class CVerify
{
	int    line;
	string file;
	Node test;
	int invert;
	Group group;

	enum VerifyTypes {
		//normal
		VERIFY = 0,
		//verify_success
		VERIFY_SUCC = 1,
		//verify_status
		VERIFY_STAT = 2,
		//verify_string
		VERIFY_STR = 3,
		//verify_not_oom
		VERIFY_NOOM = 4,
		//verify_files_equal
		VERIFY_FILES_EQUAL = 5,
		//like TRAPD(status, statement); verify_success(status);
		VERIFY_TRAP = 6
	};
	VerifyTypes verify_type;

	void create( Node _test, int _invert, string tkn )
	{
		invert = _invert;
		test = trim( _test );
		switch(tkn)
		{
		case STR_VERIFY_SUCC:
			verify_type = VERIFY_SUCC;
			break;
		case STR_VERIFY_STAT:
			verify_type = VERIFY_STAT;
			break;
		case STR_VERIFY_STR:
			verify_type = VERIFY_STR;
			break;
		case STR_VERIFY_NOOM:
			verify_type = VERIFY_NOOM;
			break;
		case STR_VERIFY_FILES_EQUAL:
			verify_type = VERIFY_FILES_EQUAL;
			break;
		case STR_VERIFY_TRAP:
			verify_type = VERIFY_TRAP;
			break;
		default:
			verify_type = VERIFY;
			break;
		}
	}

	array split_test( )
	{
		int i=-1,j;
		for( j = 0; j<sizeof( test ); j++ )
		{
			if( test[j]->text == "||" || test[j]->text == "&&" )
			{
				i=sizeof(test);
				break;
			}
			if( (< "===","==","!==","!=",">=","<=",">","<" >)[ test[j]->text ] )
				i=j;
		}
		if( i == -1 )
 			i=j-1;
		int|string lhs=String.trim_all_whites(simple_reconstitute(test[..i-1]));
		int|string rhs=String.trim_all_whites(simple_reconstitute(test[i+1..]));
		string operator;
		if( strlen( rhs ) )
			operator = test[i]->text;

		return ({ lhs, rhs, operator });
	}

	array english_for( string operator )
	{
		switch( operator )
		{
			case "===":
			case "==": return ({"'%s' should be '%s'",0});
			case "!===":
 			case "!=": return ({"'%s' should not be '%s'",1});
			case ">=": return ({"'%s' should be equal or bigger than '%s'",0});
			case "<=": return ({"'%s' should be equal or lesser than '%s'",0});
			case ">":  return ({"'%s' should be bigger than '%s'",0});
			case "<":  return ({"'%s' should be smaller than '%s'",0});
		}
	}

	Node call()
	{
		if (verify_type == VERIFY_SUCC ||
			verify_type == VERIFY_STAT ||
			verify_type == VERIFY_TRAP ||
			verify_type == VERIFY_NOOM)
		{
			return call_stat_or_succ();
		}

		if (verify_type == VERIFY_STR)
			return call_str();

		if (verify_type == VERIFY_FILES_EQUAL)
			return call_files_equal();

		Node _test = test;
		int i,j;

		array arguments = ({});
		for( j=0,i=0; i<sizeof(_test); i++ )
		{
			if( _test[i] == "," ) {
				arguments += ({ _test[j..i-1] });
				j = i+1;
				i++;
			}
		}
		arguments += ({ _test[j..] });

		test = /*({ Token("(")})+*/arguments[0]/*+({ Token(")") })*/;

		string user_message = "";
		switch( sizeof( arguments ) )
		{
		case 1: break;
		case 2: user_message = ", " + simple_reconstitute(arguments[1]); break;
		default:
			ERROR("Too many arguments to " STR_VERIFY ", expected 1 or 2", test );
			break;
		}


		Node res = ({});
		int|string rhs, lhs, operator;
		line = .NodeUtil.line( test );
		file = .NodeUtil.file( test );
		if( invert )
			return
				({ Token(" if( !(",line,file),
				   test,
				   Token(") ) {",line,file),
				   code_snipplet_t( "test_passed" ),
				   Token(" return 1; }"), });

		string report;
		int already_know; // can the user guess what the values are?
		bool is_integer_test;

		[lhs,rhs,operator] = split_test();
		if( !strlen( rhs ) )
		{
			report = String.trim_all_whites(simple_reconstitute(test)) + " should be true";
			res += ({Token("do { if(!(",line,file), test, Token(")) ",line,file)});
		}
		else
		{
			if((string)(int)lhs == lhs || (string)(int)rhs == rhs )
			{
				/**
				 * There are two if() checks here.
				 *
				 * The first always fails because TS_skip_group is false when this
				 * code executes. However, the compiler does not know that and assumes
				 * that the right side of the expression will run and as such, compiles it
				 * and issues warnings if necessary. For example
				 *   verify(var == 1);
				 * where var has a pointer type, for instance, will cause a compilation
				 * error which would be hidden due to the casts of lhs and rhs to big_int_t.
				 * Because the operands to the verify call are evaluated once to set lhs
				 * and rhs, they cannot run a second time, in case one of the operands is
				 * an expression with side effects.
				 *
				 * The second 'if' is the real test.
				 */
				is_integer_test = true;
				res += ({Token("do {big_int_t lhs = (big_int_t)(" + lhs + "), rhs = (big_int_t)(" + rhs + ");if(TS_skip_group&&(",line,file),
						test,
						Token(")){(void)0;}if(!(lhs " + operator + " rhs))",line,file)});
			}
			else
			{
				res += ({Token("do { if(!(" + lhs + operator + rhs + "))",line,file)});
			}

			[report,already_know] = english_for( operator );
			if( intp( rhs ) )
				report = replace( report, ([ "be '%s'":"be %O", "'%s'":"%O"]) );
			report = sprintf( report, lhs, rhs );
		}

		mapping base = (["FILE":TS_CSTRING(group,.NodeUtil.file(test)),
						 "LINE":(string).NodeUtil.line(test),
						 "ARGS":""
					   ]);
		if (is_integer_test)
		{
			base->REASON=TS_CSTRING(group, replace(report,"%","%%") + ". The values are %d and %d" + replace(user_message,"%","%%"));
			base->ARGS = ",lhs,rhs";
			res += code_snipplet_stripped_t( "test_failed", base );
		}
		else
		{
			base->REASON=TS_CSTRING(group, replace(report + user_message,"%","%%"));
			res += code_snipplet_stripped_t( "test_failed", base );
		}
		res = res[0..sizeof(res)-2];	// Strip tailing linebreak
		if (.NodeUtil.line(test) > 0)
			res += ({Token("} while(0);", .NodeUtil.line(test),.NodeUtil.file(test))});
		else
			res += ({Token("} while(0);")});
		res += ({Token("\n")});

		return res;
	}

	Node call_stat_or_succ()
	{
		Node _test = test;
		int i,j;
		string report = "";
		array precalc_expr;

		array arguments = ({});
		for( j=0,i=0; i<sizeof(_test); i++ )
		{
			if( _test[i] == "," ) {
				arguments += ({ _test[j..i-1] });
				j = i+1;
				i++;
			}
		}
		arguments += ({ _test[j..] });

		string user_message = "";
		if (verify_type == VERIFY_SUCC)
		{
			switch( sizeof( arguments ) )
			{
			case 1: break;
			case 2: user_message = ", " + simple_reconstitute(arguments[1]); break;
			default:
				SYNTAX("Too many arguments to " STR_VERIFY_SUCC ", expected 1 or 2", test );
				break;
			}

			line = .NodeUtil.line( arguments[0] );
			file = .NodeUtil.file( arguments[0] );

			precalc_expr = ({ Token("OP_STATUS _st_status_temp = ",line,file)})
				+ arguments[0] + ({ Token(";",line,file) });

			report = sprintf("'%s' should have succeeded" + user_message, String.trim_all_whites(simple_reconstitute(test)) );
			test = ({ Token("OpStatus::IsSuccess(_st_status_temp)") });
		}
		else if (verify_type == VERIFY_TRAP)
		{
			switch( sizeof( arguments ) )
			{
			case 1: break;
			case 2: user_message = ", " + simple_reconstitute(arguments[1]); break;
			default:
				SYNTAX("Too many arguments to " STR_VERIFY_TRAP ", expected 1 or 2", test );
				break;
			}

			line = .NodeUtil.line( arguments[0] );
			file = .NodeUtil.file( arguments[0] );

			precalc_expr = ({ Token("TRAPD(_st_status_temp, ",line,file)})
				+ arguments[0] + ({ Token(");",line,file) });

			report = sprintf("'%s' should have succeeded" + user_message, String.trim_all_whites(simple_reconstitute(test)) );
			test = ({ Token("OpStatus::IsSuccess(_st_status_temp)") });
		}
		else if (verify_type == VERIFY_STAT)
		{
			switch( sizeof( arguments ) )
			{
			case 1:
				SYNTAX("Too few arguments to " STR_VERIFY_STAT ", expected 2 or 3", test );
				break;
			case 2: break;
			case 3: user_message = ", " + simple_reconstitute(arguments[2]); break;
			default:
				SYNTAX("Too many arguments to " STR_VERIFY_STAT ", expected 2 or 3", test );
				break;
			}

			int line0 = .NodeUtil.line( arguments[0] );
			string file0 = .NodeUtil.file( arguments[0] );

			int line1 = .NodeUtil.line( arguments[1] );
			string file1 = .NodeUtil.file( arguments[1] );

			precalc_expr = ({ Token("OP_STATUS _st_status_temp_0 = ",line,file)})
				+ arguments[0]
				+ ({ Token(", _st_status_temp = ",line1,file1)})
				+ arguments[1] + ({ Token(";",line1,file1) });

			report = sprintf("'%s' should have returned '%s'" + user_message, String.trim_all_whites(simple_reconstitute(arguments[1])), simple_reconstitute(arguments[0]) );

			test = ({ Token("(_st_status_temp_0 == _st_status_temp)") });

			line = line1;
			file = file1;
		}
		else //if (verify_type == VERIFY_NOOM)
		{
			switch( sizeof( arguments ) )
			{
			case 1: break;
			case 2: user_message = ", " + simple_reconstitute(arguments[1]); break;
			default:
				SYNTAX("Too many arguments to " STR_VERIFY_NOOM ", expected 1 or 2", test );
				break;
			}

			precalc_expr = ({ Token("OP_STATUS _st_status_temp = OpStatus::OK;") });

			report = sprintf("'%s' returned NULL" + user_message, String.trim_all_whites(simple_reconstitute(test)) );

			line = .NodeUtil.line( arguments[0] );
			file = .NodeUtil.file( arguments[0] );

			test = ({ Token("OpStatus::IsSuccess(_st_status_temp = ((",line,file)})
				+ arguments[0]
				+ ({ Token(")!=NULL ? OpStatus::OK : OpStatus::ERR_NO_MEMORY))",line,file) });
		}

		Node res = ({});
		if( invert )
			return
				({	Token(" do { ")}) +
					precalc_expr +
				({	Token(" if( !(\n"),
					test,
					Token(") ) {"),
					code_snipplet_t( "test_passed" ),
					Token(" return 1; } }while(0);") });

		res += ({Token("do { ")}) + precalc_expr + ({Token("if( !(")}) + test + ({Token(")) ")});


		mapping base = (["FILE":TS_CSTRING(group,file),
						 "LINE":(string)line,
						 "ARG":"_st_status_temp"
					   ]);

		base->REASON=TS_CSTRING(group,replace(report,"%","%%"));
		res += code_snipplet_stripped_t( "test_failed_status", base );

		res = res[0..sizeof(res)-2];	// Strip tailing linebreak
		res +=  ({Token("} while(0);", .NodeUtil.line(_test),.NodeUtil.file(_test)),Token("\n")});

		return res;
	}

	Node call_str()
	{
		Node _test = test;
		int i,j;
		string report = "";

		array arguments = ({});
		for( j=0,i=0; i<sizeof(_test); i++ )
		{
			if( _test[i] == "," ) {
				arguments += ({ _test[j..i-1] });
				j = i+1;
				i++;
			}
		}
		arguments += ({ _test[j..] });

		string user_message = "";
		switch( sizeof( arguments ) )
		{
		case 1:
			SYNTAX("Too few arguments to " STR_VERIFY_STR ", expected 2 or 3", test );
			break;
		case 2: break;
		case 3: user_message = ", " + simple_reconstitute(arguments[2]); break;
		default:
			SYNTAX("Too many arguments to " STR_VERIFY_STR ", expected 2 or 3", test );
			break;
		}

		report = sprintf("%s differs %s", String.trim_all_whites(simple_reconstitute(arguments[0])), simple_reconstitute(arguments[1]) );

		test = ({ Token("") });
		line = .NodeUtil.line( _test );
		file = .NodeUtil.file( test );

		Node res = ({});
		if( invert )
			return
				({	Token(" do { StringCompareTest _str_cmp_test(")}) +
					(simple_reconstitute(arguments[0]) == "NULL" ? ({Token("(const char*)")}) + arguments[0] : arguments[0]) +
					({Token(", ")}) +
					(simple_reconstitute(arguments[1]) == "NULL" ? ({Token("(const char*)")}) + arguments[1] : arguments[1]) +
					({Token("); if(_str_cmp_test.m_result != 0) {"),
					code_snipplet_t( "test_passed" ),
					Token(" return 1; } }while(0);") });

		res += ({Token("do { StringCompareTest _str_cmp_test(")}) +
			(simple_reconstitute(arguments[0]) == "NULL" ? ({Token("(const char*)")}) + arguments[0] : arguments[0]) +
			({Token(", ")}) +
			(simple_reconstitute(arguments[1]) == "NULL" ? ({Token("(const char*)")}) + arguments[1] : arguments[1]) +
			({Token("); if(_str_cmp_test.m_result != 0) ")});


		mapping base = (["FILE":TS_CSTRING(group,file),
						 "LINE":(string)line,
						 "ARGS":",_str_cmp_test.m_lhs, _str_cmp_test.m_rhs"
					   ]);

		base->REASON=TS_CSTRING(group,replace(report,"%","%%") + ": \"%s\" != \"%s\"" + user_message);
		res += code_snipplet_stripped_t( "test_failed", base );

		res = res[0..sizeof(res)-2];	// Strip tailing linebreak
		res +=  ({Token("} while(0);", .NodeUtil.line(_test),.NodeUtil.file(_test)),Token("\n")});

		return res;
	}

	Node call_files_equal()
	{
		Node _test = test;
		int i,j;
		string report = "";

		array arguments = ({});
		for( j=0,i=0; i<sizeof(_test); i++ )
		{
			if( _test[i] == "," ) {
				arguments += ({ _test[j..i-1] });
				j = i+1;
				i++;
			}
		}
		arguments += ({ _test[j..] });

		string user_message = "";
		switch( sizeof( arguments ) )
		{
		case 1:
			SYNTAX("Too few arguments to " STR_VERIFY_FILES_EQUAL ", expected 2 or 3", test );
			break;
		case 2: break;
		case 3: user_message = ", " + simple_reconstitute(arguments[2]); break;
		default:
			SYNTAX("Too many arguments to " STR_VERIFY_FILES_EQUAL ", expected 2 or 3", test );
			break;
		}

		line = .NodeUtil.line( _test );
		file = .NodeUtil.file( _test );

		Node res = ({});

		res += ({Token("do { FileComparisonTest file_cmp_test("), arguments[0], Token(", "),
			arguments[1], Token("); "),
			Token("OP_BOOLEAN result = file_cmp_test.CompareBinary();")});

		if ( invert )
			return res + ({Token("if (result != OpBoolean::IS_TRUE) {"),
					code_snipplet_t( "test_passed" ),
					Token(" return 1; } }while(0);") });

		mapping error_base = (["FILE":TS_CSTRING(group,file),
							"LINE":(string)line,
							"ARG":"result",
							"REASON":"NULL"]);

		res += ({Token("if (OpStatus::IsError(result)) {"),
			code_snipplet_stripped_t("test_failed_status", error_base),
			Token(" } ")});
		res += ({Token("else if (result == OpBoolean::IS_FALSE) ")});


		mapping base = (["FILE":TS_CSTRING(group,file),
						 "LINE":(string)line,
						 "ARGS":", file_cmp_test.ReferenceFilenameUTF8(), file_cmp_test.TestedFilenameUTF8()"
					   ]);

		base->REASON=TS_CSTRING(group,"expected contents of \"%s\" differs from contents of \"%s\" " + user_message);
		res += code_snipplet_stripped_t( "test_failed", base );

		res = res[0..sizeof(res)-2];	// Strip tailing linebreak

		res += ({Token(" } while(0);", .NodeUtil.line(_test),.NodeUtil.file(_test)),Token("\n")});

		return res;
	}
}

class JSVerify
{
	inherit CVerify;
	int    line;
	string file;

	void create( Node _test, int _invert, string tkn )
	{
		if ( tkn == STR_VERIFY_SUCC || tkn == STR_VERIFY_STAT || tkn == STR_VERIFY_STR || tkn == STR_VERIFY_NOOM || tkn == STR_VERIFY_TRAP )
			SYNTAX( tkn + " is only supported in C++ selftests",  _test );

		test = trim(_test);
		invert = _invert;
	}
	string call()
	{
		line = .NodeUtil.line( test );
		file = .NodeUtil.file( test );

		if (verify_type == VERIFY_FILES_EQUAL)
			return call_files_equal();

		if( invert )
		{
			string ts = String.trim_all_whites(simple_reconstitute( test ));
			return code_snipplet( "test_js_passed_if_not", (["CONDITION":ts]) );
		}

		int|string lhs, rhs, operator, report, already_know;
		[lhs,rhs,operator] = split_test();
		if( operator )
		{
			[report,already_know] = english_for( operator );

			report = sprintf( report, lhs, rhs );

			if( !already_know )
			{
				report += ". The value was '";
			}
			return code_snipplet( "test_js_verify_base",
								  (["LINE":(string)line, "LHS":lhs, "RHS":rhs, "OPERATOR":operator,
									"REPORT":sprintf("%O%s",report,already_know?"":"+_TS_+\"'.\"")]) );
		}
		string ts = String.trim_all_whites(simple_reconstitute( test ));
		return code_snipplet( "test_js_failed_if_not", (["CONDITION":ts,"REPORT":sprintf("%O+%O", String.trim_all_whites(ts-"\n")," should be true."),"LINE":(string)line]) );
	}

	string call_files_equal()
	{
		Node _test = test;

		int i,j;
		array arguments = ({});
		for( j=0,i=0; i<sizeof(_test); i++ )
		{
			if( _test[i] == "," ) {
				arguments += ({ _test[j..i-1] });
				j = i+1;
				i++;
			}
		}
		arguments += ({ _test[j..] });

		if ( invert )
			return "if (!ST_binary_compare_files(" + arguments[0] + ", " + arguments[1] + ")) { ST_passed(); return; }";

		return code_snipplet( "test_js_verify_files_equal", (["LHS":arguments[0],"RHS":arguments[1],"REPORT":"Files are not equal","LINE":(string)line]) );
	}
}


class Test( string name, Node code, mapping options )
{
	Group    group = current_group;
	int         id = test_num++;
	program Verify = CVerify;
	Node   finally = 0;
	Table    multi = 0;

	array(string) multiargs;

	string _c_name;
	string c_name()
	{
		if( _c_name )
			return _c_name;
		string post = "";
		int postno = 0;
		while( name_collision( (_c_name = "ST_"+safe_name(name)+post),this ) )
			post = sprintf("%d",postno++);
		return _c_name;
	}

	string describe_require_fail()
	{
		string req = String.implode_nicely( sort(indices(options->require)) );
		string res = "";
		if( sizeof( req ) )
			res += " "+req;
		return "(dep: "+res+")";
	}

	string declare()
	{
		string base = "	int "+c_name()+"(); ";
		if( multi )
			base += "	int "+c_name()+"_(char *testname,"+make_multiargs()+");";
 		if( !options->disabled && options->leakcheck )
 			base += "int __low_"+c_name()+"();";
		return base;
	}

	string call()
	{
 		if( !options->disabled && options->leakcheck )
 			return "__low_"+c_name();
		return c_name();
	}

	Node rec_create_verify( Node n )
	{
		for( int i = 0; i<sizeof( n ); i++ )
		{
			if( objectp(n[i]) )
			{
				if( n[i]->text == STR_VERIFY ||
					n[i]->text == STR_VERIFY_SUCC ||
					n[i]->text == STR_VERIFY_STAT ||
					n[i]->text == STR_VERIFY_STR ||
					n[i]->text == STR_VERIFY_NOOM ||
					n[i]->text == STR_VERIFY_FILES_EQUAL ||
					n[i]->text == STR_VERIFY_TRAP )
				{
					if( !arrayp( n[i+1] ) )
						SYNTAX("Expected () after " + n[i]->text + "\n", n[i+1] );
					if( n[i+2] != ";" )
						SYNTAX("Expected ';' after " + n[i]->text + simple_reconstitute(n[i+1]) + "\n", n[i+2] );

					n[i] = Verify( n[i+1], options->fails, n[i]->text);
					n[i]->group = group;
					n[i+1] = "";
					n[i+2] = Token(n[i+2]->trailing_whitespaces);
					i += 2;
				}
				else if( n[i]->text == "assert" )
					SYNTAX("Please don't use assert, use " STR_VERIFY " instead",  n[i] );
			}
			else
				n[i] = rec_create_verify( n[i] );
		}
		return n;
	}

	string make_multicall( )
	{
		string testnamearg, pre, post;
		array(string) testnameargs = ({});
		array code;

		while( sscanf( name, "%s$(%[^)])%s", pre, testnamearg, post ) == 3 )
		{
			if( search( multiargs, testnamearg ) == -1 )
				SYNTAX( "Unknown iterator argument "+testnamearg+"\n", this->code);
			switch( (multi->get_type( testnamearg,multiargs )-" ")-"const" )
			{
				case "char": case "int": case "short": case "uni_char": case "long":
					name = pre+"\0d"+post;
					testnameargs += ({"(int)"+testnamearg});
					break;
				case "unsignedchar":case "unsignedint":case "unsignedlong":
					name = pre+"\0u"+post;
					testnameargs += ({"(unsigned int)"+testnamearg});
					break;
				case "unsignedchar*":case "char*":
					name = pre+"\0s"+post;
					testnameargs += ({"ST_trim_fn((const char *)"+testnamearg+")"});
					break;
				case "uni_char*":
					name = pre+"\0s"+post;
					testnameargs += ({"ST_down("+testnamearg+")"});
					break;
				default:
					SYNTAX("Can not format test name for multitests from "+multi->get_type( testnamearg,multiargs ), this->code);
			}
		}

		name = replace( name, ([ "\0":"%", "%":"%%" ]) );

		if( !sizeof( testnameargs ) )
			WARN("The one running the testsuite probably wants to be able to separate multi-tests\n"
				 "Add a $(testargument) to the test, where testargument is the name of one of the iterator arguments\n", this->code);
		if( !sizeof( multiargs ) )
			SYNTAX("Need at least one argument sent to the test\n", this->code);

		code = code_snipplet_t( "multi_loop",
							  ([ "TESTFUNC":c_name(),
								 "FORMAT":sprintf("%O",name),
								 "FORMATARGS":SPLIT(testnameargs*",",0),
								 "TESTARGS":map(map(multiargs,Token),aggregate)*({Token(",")}),
							  ]) );
		return multi->iterate( multiargs, code);
	}

	string make_multiargs( )
	{
		string res = "";
		int i;
		foreach( multi->types, string tt  )
		{
			if( i < sizeof( multiargs ) )
				res += tt+"  "+multiargs[i]+",";
			i++;
		}
		return res[..strlen(res)-2];
	}

	string _sprintf( )
	{
		return sprintf("Test(%O [%s:%d])", name,
					   (((string).NodeUtil.file(code))/"/")[-1], .NodeUtil.line(code) );
	}

	Node define()
	{
		mixed pre, postdef, failed,predefs, post;
		array res;
		[predefs,postdef] = make_require( options, group, c_name() );

        if( options->multi )
			[multi,multiargs]=options->multi;

		pre =({TOKEN( "//\n//   "+group->name+" : '"+name+"'.\n//\n")});
		post = ({});
		res = ({ TOKEN("{") });

		if( options->require_init )
			res += SKIPP( "!g_selftest.suite->IsOperaInitialized()", "requires initialization");

		if( options->require_uninit )
			res += SKIPP( "g_selftest.suite->IsOperaInitialized()", "opera was initialized");

		foreach( options->required_ok||({}), string tn )
		{
			if( has_value( tn, "->" ) )
			{
				WARN("Inter-group dependencies not fully supported", code);
				group->add_dependency( (tn/"->")[0] );
			}
			else if( !group->find_test( tn ) )
				WARN(sprintf("Can not find test %O, %O will never be run", tn,name), code);
			res += SKIPP( sprintf("!g_selftest.utils->TestWasOk(%O)",tn), sprintf("%O not OK", tn));
		}

		foreach( options->required_fail||({}), string tn )
		{
			if( has_value( tn, "->" ) )
			{
				WARN("Inter-group dependencies not fully supported", code);
				group->add_dependency( (tn/"->")[0] );
			}
			else if( !group->find_test( tn ) )
				WARN(sprintf("Can not find test %O, %O will never be run", tn,name), code);

			res += SKIPP( sprintf("g_selftest.utils->TestWasOk(%O)",tn),sprintf("%O not failed", tn));
		}

		// Expand files to generated paths.
		if (mappingp(options->testfiles))
			code = .NodeUtil.replace_identifiers(code, options->testfiles);

		if( !options->async )
			code = rec_create_verify( code );
		if( sizeof( code ) > 2 )
			code = trim(.NodeUtil.fix_call( code ));
		else if( sizeof( code ) )
		{
			code = ({ code[0] });
			code[0]->text = "";
		}
		else
		{
			code = ({ Token("",group->line,group->file,0) });
		}

		if( options->delay_pre )
			res += code_snipplet_t( "delay_pre", (["DELAY":(int)(options->delay_pre*1000)]) );


		mapping data =([ "NAME":(multi?"testname":TS_CSTRING(group,name)),
						 "MANUAL":(string)!!options->manual
					  ]);

		if( options->timer )
			data->TIMER_START = "g_selftest.utils->timerStart();";
		else
			data->TIMER_START = "g_selftest.utils->timerReset();";

		if( options->repeat != 1 )
		{
			data->REPEAT_START = "  for( int __=0;__<"+options->repeat+";__++) {";
			data->REPEAT_END   = "}";
		}
		else
			data->REPEAT_START = data->REPEAT_END = "";

		if( !options->disabled )
			data->BODY = ({Token("\n"),code,Token("\n")});
		else
			data->BODY = ({ Token("\"Disabled, no code here, move along\"") });

		mapping base = ([
			"FILE":TS_CSTRING(group,.NodeUtil.file( code )),
			"LINE":(string).NodeUtil.line( code ),
			"REASON":"\"This test should have failed\"",
			"ARGS":"",
		]);


		int rv = options->async ? -2 : 0;
		array fcode = ({});

		if( options->delay_post )
		{
			fcode += code_snipplet_t( "delay_post", (["DELAY":(int)(options->delay_post*1000)]));
			rv = -2;
		}

		if( !options->async )
			if( options->fails )
				fcode += code_snipplet_t("test_failed",base);
			else
				fcode += code_snipplet_t("test_passed");

		if( options->manual )
			fcode += code_snipplet_t( "test_ask_user_ok", base+(["QUERY":TS_STRING(group,options->manual)]));

		failed = Token("  return "+rv+";");
		if( !options->disabled )
		{
			if( finally )
			{
				failed = ({Token("do {\n"),finally,Token("\n;"),failed,Token(" } while(0);")});
				fcode += finally;
			}
			if( !rv )
				fcode += ({	Token("\n;return 1;\n") });
			else
				fcode += ({	Token("\n;return "+rv+";\n") });
			data->FINALLY = ({Token("\n"),fcode});
		}
		else
			data->FINALLY = ({});
		data->FAIL = options->fails?"TRUE":"FALSE";

		res += code_snipplet_t( "test_base", data );

		res += ({ Token("\n;return 1;") }); // else, the test was not run, always return 1
		res += ({ Token("}\n") }); // end of function

		if( options->leakcheck )
		{
			mapping leakcheck_data = ([
				"GROUPNAME"		: group->c_name(),
				"LOW_TESTNAME"	: "__low_" + c_name(),
				"TESTNAME"		: c_name()
			]);
			post = code_snipplet_t( "test_leakcheck", leakcheck_data ) + post;
		}
		if( strlen(predefs) )
		{
			pre = ({TOKEN(predefs)})+pre;
			array other;
			if (options->leakcheck && !options->disabled)
				other = ({TOKEN("int "+group->c_name()+"::"+"__low_"+c_name()+"() {")});
			else
				other = ({TOKEN("int "+group->c_name()+"::"+c_name()+"() {")});
			if( options->disabled )
				other += code_snipplet_t( "test_skipped", (["NAME":TS_CSTRING(group,name),"REASON":TS_CSTRING(group,"(disabled)")]));
			else
				other += code_snipplet_t( "test_skipped", (["NAME":TS_CSTRING(group,name),"REASON":TS_CSTRING(group,describe_require_fail())]));
			other += ({TOKEN("}")});
			post = ({post,TOKEN("#else\n"),other,Token("\n"),TOKEN(postdef)});
		}
		if( multi )
			res=({
				pre,
				TOKEN("	int "+group->c_name()+"::"+c_name()+"()\n"),
				Token("{"),
				make_multicall(),
				TOKEN("return 1;\n"),
				Token("}"),
				Token("	int "+group->c_name()+"::"+c_name()+"_(char *testname,"+make_multiargs()+")\n"),
				res,
				post
			});
		else
			res = ({
				pre,
				TOKEN("	int "+group->c_name()+"::"+c_name()+"()\n"),
				res,
				post
			});
		return .NodeUtil.replace_identifiers( res, (["FAILED":failed]) );
	}
}

class JSTest
{
	inherit Test;
	constant is_ecma = 1;
	program Verify = JSVerify;

	string _sprintf( )
	{
		return sprintf("JSTest(%O [%s:%d])", name,
					   (((string).NodeUtil.file(code))/"/")[-1], .NodeUtil.line(code) );
	}

	string call()
	{
		return c_name();
	}

	Node define()
	{
		string pre = "", post = "", predefs="", postdef="";
		array res;
		if( code )
			code = rec_create_verify( code );
		[predefs,postdef] = make_require( options, group, c_name() );

		res = ({ Token("{\n") });
		pre = "//\n//   "+group->name+" : "+name+" .\n//\n";
		if( options->multi )
			ERROR("multi not yet supported for javascript tests", code );

		pre += sprintf("#line %d %O\n",.NodeUtil.line(code),.NodeUtil.file(code));

		code = trim(.NodeUtil.fix_call( code ));

		if ( options->async )
			code = add_trace_functions( code );

		res += SKIPP( "!g_selftest.suite->IsOperaInitialized()", "requires initialization");// C++

		foreach( options->required_ok||({}), string tn )
			res += SKIPP( sprintf("!g_selftest.utils->TestWasOk(%O)",tn), sprintf("%O not OK", tn));

		foreach( options->required_fail||({}), string tn )
			res += SKIPP( sprintf("g_selftest.utils->TestWasOk(%O)",tn),sprintf("%O not failed", tn));

		mapping data =([
			"NAME":(multi?"testname":TS_CSTRING(group,name)),
			"MANUAL":(string)!!options->manual
		]);

		if( options->repeat != 1 )
		{
			data->REPEAT_START = "  for( int __=0;__<%d;__++) {";
			data->REPEAT_END   = "}";
		}
		else
			data->REPEAT_START = data->REPEAT_END = "";

		if( !options->async )
			if( options->fails )
				code += code_snipplet_t( "test_js_failed", ([ "LINE":(string).NodeUtil.line(code),"REPORT":"\"This test should have failed\""]));
			else
				code += code_snipplet_t( "test_js_passed" );

		int prog_length = 1;
		string prog;
		if( options->disabled )
			prog = "disabled";
		else
			prog = code_snipplet( "test_js_base", ([
									  "CODE":simple_reconstitute( code ),
									  "FILE":sprintf("%O",.NodeUtil.file(code)),
									  "FAIL":options->fails?"true":"false",
									  "FINALLY":"",
									  "GROUPNAME":sprintf("%O",group->name),
									  "TESTNAME":sprintf("%O",name)
								  ]));
		string result = TS_STRING(group,prog);
		data->SCRIPTLINENUMBER = (string).NodeUtil.line(code);
		data->PROG = result;
		data->FAIL = options->fails?"TRUE":"FALSE";
		data->ALLOWEXCEPTIONS = options->allow_exceptions?"TRUE":"FALSE";
		data->ASYNC = options->async?"TRUE":"FALSE";
		if( options->manual )
		{
			data->QUERY = TS_CSTRING(group,options->manual);
			data->FINALLY = code_snipplet_t( "test_ask_user_ok", data );
		}
		res += code_snipplet_t( "test_js_base_cc", data );
		res += ({ Token( " return 1; " ) });
		res += ({ Token("}\n") });

		if (options->delay_pre || options->delay_post)
			werror("Delays are not supported in ecmascript tests.\n");

		if( strlen(predefs) )
		{
			pre = predefs+"\n"+pre;
			string other;
			other = "int "+group->c_name()+"::"+c_name()+"() {";
			if( options->disabled )
				other += code_snipplet( "test_skipped", (["NAME":TS_CSTRING(group,name),"REASON":TS_CSTRING(group,"(disabled)")]));
			else
				other += code_snipplet( "test_skipped", (["NAME":TS_CSTRING(group,name),"REASON":TS_CSTRING(group,describe_require_fail())]));
			other += "}";
			post = post+"#else\n"+other+"\n"+postdef;
		}
		return ({TOKEN(pre),TOKEN("    int "+group->c_name()+"::"+c_name()+"()\n"),res,TOKEN(post)});
	}

	string check_selftest_snippet = code_snipplet("test_js_check_fn");

	/**
	 * This function locates all function declarations and
	 * expressions and adds a call to
	 * window.$check_selftest$(group_name,test_name) which
	 * checks if the current ecmascript thread belongs to a
	 * test that is not the current one executing.
	 * This happens if a test still has listeners setup after
	 * failing or timeouts.
	 * If the check fails, the current ecmascript thread is
	 * aborted, else selftest modules adds a listener on this
	 * thread to check when it abrts due to OOM or other
	 * condition so it can proceed with the following tests.
	 *
	 * To be used only in async tests
	 */

	Node add_trace_functions( Node n ){

		if (!arrayp(n))
			return n;

		/**
		 * 0 - nothing relevant
		 * 1 - found function keyword
		 * 2 - found function arguments
		 */
		int state = 0;

		for( int i = 0; i<sizeof( n ); i++ )
		{
			bool handled_array = false;
			switch (state)
			{
			case 0:

				if( objectp(n[i]) && n[i]->text == "function")
				{
					state = 1;
				}
				break;

			case 1:

				if( objectp(n[i]) && i<(sizeof( n )-1))
					//skip name
					i++;
				else if(i>=(sizeof( n )-1))
				{
					state = 0;
					break;
				}

				if( arrayp( n[i] ) )
				{
					handled_array = true;
					n[i] = add_trace_functions( n[i] );
					state = 2;
				}
				else
					state = 0;

				break;

			case 2:

				n[i] = add_trace_functions_body(n[i]);
				handled_array = true;
				state = 0;

				break;
			}

			if ( !handled_array)
				n[i] = add_trace_functions( n[i] );
		}
		return n;
	}
	Node add_trace_functions_body( Node n ){
		if( objectp(n[0]) && n[0]->text == "{" && sizeof(n) > 1)
		{
			return n[0..0] + ({Token(check_selftest_snippet)}) + add_trace_functions(n[1..(sizeof(n)-1)]);
		}
		else
			return add_trace_functions(n);
	}
}

class HTML
{
	inherit Test;
	string html_code;
	string name = "html";
	string type = "text/html";
	constant is_ecma = 1;

	protected void create( string _html_code, mapping _options )
	{
		html_code=_html_code;
		options=_options;
		type = _options->type;
		name = (_options->type/"/")[-1];
	}

	Node define()
	{
		string pre="", post="";
		mapping data = ([
			"CLASS":group->c_name(),
			"TEST":c_name(),
			"TYPE":TS_CSTRING(group,type),
			"DATA":TS_CSTRING(group,html_code),
			"URL":TS_CSTRING(group,options->url),
			"DOCUMENTBLOCKLINENUMBER": options->document_block_line_number
		]);

		[pre,post] = make_require( options, group, c_name() );
		return ({Token(pre),code_snipplet("test_html_base_cc",data),Token(post)});
	}
}

class TestCompare
{
	inherit Test;

	private Node lhs;
	private Node rhs;
	constant one_only = 0;
	constant compare_txt = "should be equal to";
	constant compare_fnc = "";

	protected void create( string _name, Node _lhs, Node _rhs, mapping _options )
	{
		name = _name;
		lhs = _lhs;
		rhs = _rhs;
		options = _options;
	}

	Node define(  )
	{
		string _rhs = String.trim_all_whites(simple_reconstitute(rhs));
		string _lhs = String.trim_all_whites(simple_reconstitute(lhs));
		code=({TOKEN("{"),
			   code_snipplet_t( "test_compare",
							 ([ "LINE_PREPROC":"#line",
								"FILE":TS_CSTRING(group,.NodeUtil.file(lhs)),
								"FILE_STR":sprintf("%O",.NodeUtil.file(lhs)),
								"LINE":(string).NodeUtil.line(lhs),
								"LHS":_lhs, "RHS":_rhs,
								"LHSS":sprintf("%O",_lhs),
								"RHSS":sprintf("%O",_rhs),
								"ONE_ONLY":(string)one_only,
								"COMPARE_TXT":TS_CSTRING(group,compare_txt),
								"INVERT_TEST":(string)((compare_fnc=="!") == !!options->fails)
							 ]) ) ,
			   TOKEN("}")});
		return ::define();
	}
}

class TestEqual
{
	inherit TestCompare;
	constant one_only = 0;
}

class TestNEqual
{
	inherit TestCompare;

	constant compare_txt = "should not be equal to";
	constant one_only = 1;
	constant compare_fnc = "!";
}

class SubTest
{
	inherit Test;
	string pre, post;

	string call()
	{
		return ""; // Do not call automatically.
	}

	string declare()
	{
		return COMPOSE(code[0..2])+";\n";
	}

	Node define()
	{
		if( code ) code = rec_create_verify( code );
		Node failed = TOKEN("return 0;");
		[pre,post] = make_require( options, group, c_name() );
		if( finally )
			failed = ({TOKEN("do {"),finally,failed,TOKEN(" } while(0);")});
		array c = ({})+code;
		c[1] = Token( group->c_name()+"::"+c[1]->text, c[1]->line,c[1]->file);
		return .NodeUtil.replace_identifiers( ({TOKEN(pre),
										.NodeUtil.fix_call(c),
										Token("\n"),
										TOKEN(post),}),
											   (["FAILED":failed, "FINALLY":finally?finally:""]) );
	}
}

class Group( string name, string file, int line )
{
	string module;
	multiset has_includes = (< "core/pch.h","optestsuite.h","testutils.h","testgroups.h" >);
	array(array(string|int)) includes = ({});
	string global_vars_prot="";
	string global_depend="";
	array(string) dependencies = ({});
	array(string) testfiles = ({});

	mapping(string:int) string_already = ([]);
	string global_strings = "";
	mapping(string:int) cstring_already = ([]);
	string global_cstrings = "";

	multiset       require = (<>);
	array(Test)      tests = ({});
	multiset(string) files = (<>);
	Node              init = ({});
	Node              exit = ({});
	Node           globals = ({});
	Node          toplevel = ({});
	Node          globvars = ({});
	Node	   arrays_init = ({});
	int                 id = group_num++;
	int       require_init = 0;
	int     require_uninit = 0;
	array(Table) tables    = ({});
	string        imports  = "";
	string      group_base = "";
	int				nopch  = 0;

	// caches
	string _c_name = 0;
	mapping table_ids = ([]);
	int next_table_id = 0;

	multiset seen_classes = (<>);

	Test find_test( string n )
	{
		foreach( tests, Test t )
			if( glob(n,t->name) )
				return t;
		return 0;
	}

	void add_dependency( string x )
	{
		if( x != name )
			dependencies |= ({ x });
	}

	string _sprintf( )
	{
		return sprintf("Group(%O [%s:%d])", name,(file/"/")[-1],line);
	}

	int `>( Group x ) {	return name > x->name;	}
	int `<( Group x  ){	return name < x->name;	}


	//! Move out all TYPE ClassName::FunctionName( TYPE ) { } blocks to the
	//! toplevel scope, to be compatible with booth vc++ and gcc.
	//!
	//! In the process the ClassName has to be fixed to include the
	//! classname of the testgroup class.
	Node recursively_fix_class_names( Node x )
	{
		string name = c_name();
		for( int index=0; index<sizeof(x); index++ )
		{
			Node q = x[index];
			if( !q ) continue;
			if( arrayp( q ) )
				x[index] = recursively_fix_class_names( q );
			else if( seen_classes[ q->text ] &&	 (sizeof( x ) > index+3) &&
					 x[index+1]=="::" && stringp( x[index+2]->text ) )
			{
				int start = index+1;
				while( is_likely_type( x[start-1] ) )
				{
					if( seen_classes[x[--start]->text] )
						x[start]->text = name+"::"+x[start]->text;
				}
				int end = index+2;
				while( end < sizeof(x)-1 )
				{
					end++;
					if( ( arrayp(x[end]) && x[end][0] == "{" ) || ( !arrayp(x[end]) && x[end]->text == ";" ) )
						break;
				}
				if( arrayp(x[end]) && x[end][0] == "{" )
				{
					toplevel += copy_value(x[start..end]);
					for( int i = start; i<=end; i++ )
						x[i] = 0;
				}
				index = end;
			}
		}
		return x - ({0});
	}

	// Removes all default values in function
	void strip_default_args(Node n)
	{
		for (int index = 0; index < sizeof(n); index++)
		{
			if (n[index]->text == "=")
			{
				// Clear until next , or )
				while((index < sizeof(n)) && n[index] != "," && n[index] != ")")
				{
					n[index]->text = "";
					index++;
				}
			}
		}
	}

    //! Move out function bodies from the 'globals' section to avoid making them inline.
	void move_methods( )
	{
		string name = c_name();
		if( sizeof( globals ) < 3 )
		  return;
		for( int index=3; index<sizeof(globals); index++ )
		{
			if( arrayp( globals[index] ) && globals[index][0] == "{" )
			{
				// We have a function, class, namespace or other item.
				if( arrayp( globals[index-1] ) && globals[index-1][0] == "(" )
				{
					// .. and this can only be a function. We have a winner.
					// TYPE     NAME     (...ARGS...) { BODY }
					//
					// index-3  index-2  index-1        index
					//
					int start = index-3;
					while( is_likely_type( globals[start-1] ) )
						start--;

					array func_body = .NodeUtil.dup_node( globals[start..index] );

					globals[index] = Token( ";" ); // Convert the body to a single ';'.

					func_body[ -3 ]->text = name+"::"+func_body[ -3 ]->text;
					strip_default_args(func_body[-2]);
					//need to strip static and virtual in method
					//declaration because it's not valid in the definition
					map(func_body, strip_decl_kw);
					toplevel += ({ func_body });
				}
			}
		}
	}

	Node strip_decl_kw ( Node n ){
		if( objectp(n) && (n->text == "static" || n->text == "virtual") )
			n->text = "";
		return n;
	}

	// number of top level elements in array
	int array_length(Node n)
	{
		if (arrayp(n))
		{
			int commas = 0;
			for (int i = 0; i < sizeof(n); i++)
			{
				if (!arrayp(n[i]) && n[i] == ",")
					commas++;
			}
			return commas + 1;
		}
		else
			return 0;
	}

	// Recursively merge a node into a string
	string merge_node(Node n)
	{
		string res = "";

		if (!arrayp(n))
			return n;

		for(int i = 0; i < sizeof(n); i++)
		{
			if (arrayp(n[i]))
				res += merge_node(n[i]) + " ";
			else
			{
				res += n[i] + " ";
			}
		}

		return String.trim_whites(res);
	}

	// Get top level elements
	array get_array_elements(Node n)
	{
		array elements = ({});
		if(!arrayp(n))
			return elements;

		for(int i=0; i < sizeof(n);	i++)
		{
			if (!arrayp(n[i]))
			{
				if (n[i] ==	"{") continue;
				if (n[i] == "}" || n[i] == ";") return elements;
			}
			array current_element = ({});
			while(i < sizeof(n) && n[i] != "," && n[i] != "}")
			{
				current_element += ({n[i]});
				i++;
			}
			if (sizeof(current_element) == 1)
				elements += current_element;
			else
				elements += ({merge_node(current_element)});
		}
		return elements;
	}

	// Create initialization code for an array by unfolding a initialize every element
	string array_init_unfold(string ident, string type, Node body, array dimensions)
	{
		string unfold_array(Node body, array dim_counter, int current_dim)
		{
			string res = "";
			array elements = get_array_elements(body);

			for (int i=0; i < sizeof(elements); i++)
			{
				// if dimensions[current_dim] is zero it's something that can't be resolved, probably
				// a define, in that case this test is skipped
				if (dimensions[current_dim] &&
				   dim_counter[current_dim] >= dimensions[current_dim])
					SYNTAX("Illegal array initialization", body);

				if(arrayp(elements[i]) && elements[i][0] == "{" && current_dim < sizeof(dim_counter) - 1)
				{
					dim_counter[current_dim+1] = 0;
					res += unfold_array(elements[i], dim_counter, current_dim + 1);
				}
				else
				{
					if (arrayp(elements[i]))	// Special case for arrays of structs
					{
						res += "\t\t{ " + type + " temp = " + merge_node(elements[i]) + ";";
						res += ident;
						for(int j = 0; j < sizeof(dim_counter); j++)
						{
							res += "[" + dim_counter[j] + "]";
						}
						res += " = temp; }\n";
					}
					else						// The normal case
					{
						res += "\t\t" + ident;
						for(int j = 0; j < sizeof(dim_counter); j++)
						{
							res += "[" + dim_counter[j] + "]";
						}

						res += " = " + merge_node(elements[i]) + ";\n";
					}
				}
				dim_counter[current_dim]++;
			}

			return res;
		};

		array zero_array = ({});
		for(int k = 0; k < sizeof(dimensions); k++)
			zero_array += ({0});
		return unfold_array(body, zero_array, 0);
	}

	int string_length(Node n)
	{
		int res = 0;
		if (arrayp(n))
		{
			string s;
			foreach(n, s)
			{
				res += strlen((string)s)-2;
			}
		}
		else
			res += strlen((string)n)-2;

		return res;
	}

	// Create initialization code for a string
	string make_string_init(string ident, Node n)
	{
		return "{ const char* temp = " + merge_node(n) + ";\n" +
			   "  op_strcpy(" + ident +", temp);}\n";
	}

	void move_initializers( )
	{
		array q = init;
		init = ({});
		for( int i = 0; i<sizeof( globals ); i++ )
		{
			if( globals[i] == "=" )
			{
				int start = i-1;
				int end = i;

				int body = i + 1;

				if( arrayp(globals[start]) && globals[start][0]=="[" )
				{
					while( globals[end] != ";" )
						end++;

					array dimensions = ({});
					bool treat_array_as_string = false;
					while( arrayp(globals[start]) && globals[start][0]=="[" )
					{
						if(globals[start][1]=="]")
						{
							int dim = array_length(globals[body]);

							if (dim == 0) // Not initialized as an array, assume we can treat this as a string
							{
								treat_array_as_string = true;
								globals[start] = ({Token("["+(string_length(globals[body..end-1])+1)+"]")});
							}
							else
							{
								dimensions += ({dim});
								globals[start] = ({Token("["+dim+"]")});
							}
						}
						else
						{
							dimensions += ({(int)(string)globals[start][1]});
						}
						start--;
					}
					dimensions = reverse(dimensions);
					int ident_index = start;
					string identifier = globals[start];
					bool type_is_pointer = globals[ident_index-1] == "*";

					if( arrayp( globals[start] ) )
						SYNTAX("Unknown global variable syntax", globals[start] );
					while( !arrayp(globals[start-1]) && globals[start-1] && globals[start-1] != ";" )
						start--;

					int start_type_index = start;
					while (globals[start_type_index] == "const" || globals[start_type_index] == "static")
						start_type_index++;

					string type = merge_node(globals[start_type_index..ident_index-1]);

					if(!treat_array_as_string)
						arrays_init += ({Token(array_init_unfold(identifier, type, globals[body], dimensions))});
					else
						arrays_init += ({Token(make_string_init(identifier, globals[body .. end-1]))});

					// strip static	and const if needed
					for (int j = ident_index; j >= start; j-- )
						if (globals[j] == "static" || ((globals[j] == "const") && !type_is_pointer))
							globals[j] = 0;

					// Remove array body and close with a ;
					globals[body - 1] = ({Token(";")});
					for( int j = body; j<=end; j++ )
						globals[j] = 0;

					i=end+1;
				}
				else
				{
					while( globals[end] != ";" && globals[end] != "," )
						end++;
					end--;
					if( !is_identifier( globals[start] ) )
						SYNTAX(sprintf("Expected name of global variable, got %O",
									   .NodeUtil.text( globals[start] ) ), globals[start] );
					init += ({ copy_value(globals[start..end]),Token(";") });
					for( int j = start+1; j<=end; j++ )
						globals[j] = 0;
					i=end+1;
				}
			}
			else if(globals[i] == "class"  && i < sizeof(globals)-2 && (arrayp(globals[i+2]) || globals[i+2]==":"))
			{
				seen_classes[globals[i+1]->text]=1;
			}
		}
		init += q;
	}

	int get_table_id( Table x )
	{
		if( table_ids[x] )
			return table_ids[x];
		return table_ids[x]=++next_table_id;
	}

	string c_name( )
	{
		if( _c_name )
			return _c_name;
		return _c_name="ST_"+safe_name(name);
	}

	void add_global_var( Node z )
	{
		bool has_st;
		foreach( z, Node q )
			if( q == "static" )
			{
				has_st=true;
				break;
			}
		if( !has_st )
			globvars += ({ Token( "static " ) });
		globvars += z;
	}

	string globalvars()
	{
		string pre, post;
		[pre,post] = make_require( this, 0, c_name() );
		string conditional = ifdef();
		string scond = conditional ? pre : "";
		string econd = conditional ? post : "";
		string gg = "";
		return scond + COMPOSE( globvars ) + "\n"+econd;
	}

	string ifdef()
	{
		if( sizeof(require) )
			return "#ifdef "+c_name()+"_DEFINED";
	}

	Node make_array( string x, int unic )
	{
		if (!sizeof(x))
			return "0";

		if( unic )
			x = string_to_unicode( x );

		int n = strlen(x);
		x = Gz.deflate(9)->deflate(x);
// 		x = Image.GIF.lzw_encode(x);
//  		werror("[str: %d bytes -> %d bytes, %d%% left, gzip %d, %d%%]\n",
// 			   n, strlen(x), (strlen(x)*100)/n,
// 			   strlen(z), (strlen(z)*100)/n);
		array res = ({});
		string line = "    ";
		foreach( (array)x, int z )
		{
			line += z+",";
			if( strlen( line ) > 80 )
			{
				res += ({Token(line+"\n")});
				line = "    ";
			}
		}
		return ({res, Token(line),Token("\n")});
	}

	string define()
	{
#ifdef DEBUG
		werror("%-40s", name);
#endif
		DEBUG_TIME_START();

		tests->resolv();

		string pre = "", post = "";
		string conditional = ifdef();
		string scond = conditional ? ""+conditional+"\n" : "#if 1\n";
		string econd = conditional ? "#endif // "+conditional+"\n": "#endif // 1\n";
		array(string) da_list = filter(tests->call()-({0}),strlen);
		DEBUG_TIME( "call" );

		mapping base = ([
			"#STARTCOND\n":scond,
			"CLASS":c_name()
		]);

		array tests_node = ({});
		string tests_protos = "";

		// TESTS
		foreach( tests, object test )
		{
			tests_node += test->define(); // Order matters. define needs to be called first.
			tests_protos += test->declare()+"\n";
		}
		DEBUG_TIME( "tdef" );
		Node req_init=({});
		if( require_init )
			req_init = code_snipplet_t( "require_group_init", base );
		else if( require_uninit )
			req_init = code_snipplet_t( "require_group_uninit", base );

		Node tests_init = ({});
		int i;
		foreach( da_list, string z )
		{
			tests_init += code_snipplet_t( "test_array_init", (["N":(string)i,"CLASS":c_name(),"FUNCTION":z]) );
			i++;
		}
		DEBUG_TIME( "init" );

		// The order of these calls is relevant.
		move_initializers();
		globals = recursively_fix_class_names( globals );
		move_methods();
		// End order dependency.

		base->GROUP_BASE = group_base != "" ? ", public " + group_base : "";
		base->GLOBALS = ({TOKEN(scond),globals,Token("\n"),TOKEN(econd)});
		base->EXIT_CODE = ({Token("\n"),exit});
		base->NTESTS = (string)sizeof(da_list);
		base->REQUIRE_INIT = req_init;
		base->INITIALIZERS = ({Token("\n"),init});
		base->TESTS_PROTOS = ({Token(tests_protos), sort(tables)->define()});
		base->FILE = TS_CSTRING(this,(file-prefix)[1..]);
		base->LINE = (string)line;
		base->GROUPNAME = TS_CSTRING(this,name);

		base->ARRAYS_INIT = arrays_init;
		base->TESTS_INIT = tests_init + map(sort(tables)->initialize(),Token);

		// NOTE: The strings are not available before this point.
		base->UNI_STRING_LEN = (string)sizeof( global_strings );
		base->UNI_STRING = make_array(global_strings,1);
		base->STRING_LEN = (string)sizeof( global_cstrings );
		base->STRING = make_array(global_cstrings,0);

		base->TESTS = tests_node;
		base->OUTLINED = ({TOKEN(scond), toplevel, Token("\n"),TOKEN(econd)});

		Node res_tokens = code_snipplet_t( "suite", base );
		string res="// "+name+"\n"+COMPOSE(res_tokens) + "\n";
		DEBUG_TIME("comp");
#ifdef DEBUG
		werror("\n");
#endif
		return res;
	}

	Test low_add_test( Test t )
	{
		tests += ({ t });
		return t;
	}
}

//
// ********************* Main parser
//
string current_testfile;

//! Return the contents of a directory as if it had been written as table data.
Node get_directory_contents( Node base, array(string) directories, array(string) globs, int recurse, int dirs)
{
	if( !sizeof( globs ) )
		globs = ({ "*" });
	array res = ({});
	foreach( sort(directories), string d )
	{
		string current_dir = d;
		d = combine_path( base->file, "../", d );
		Stdio.Stat st;
		if( !(st = cache_file_stat( d )) )
			ERROR(sprintf("The directory %O does not exist", d), base );
		old_mtimes[d] = st->mtime;
		depends[current_testfile][d]=1;

		foreach( sort(get_dir( d )), string q )
		{
			if( (< "CVS", ".cvsignore" >)[q] || q[-1] == '~' ) // Always ignore CVS and backup files
				continue;
			string p = combine_path( d, q );
			Stdio.Stat s = cache_file_stat( p );
			if( !s )
				continue;

			if( s->isdir && recurse )
				res += get_directory_contents( base, ({ combine_path( current_dir, q) }), globs, 1, dirs);
			else if( dirs || !s->isdir )
			{
				int ok = 0;
				foreach( globs, string g)
					if( glob( g, q ) )
					{
						ok=1;
						break;
					}
				if( ok )
					res +=
						({
							({
								Token( "{", base->line, base->file),
								Token( sprintf("%O", combine_path(current_dir, q)), base->line, base->file),
								Token( ":", base->line, base->file),
								Token( sprintf("%O", q), base->line, base->file),
								Token( "}", base->line, base->file),
							}),
							Token( ",", base->line, base->file),
						});
			}
		}
	}
	return res;
}

//! Return the contents of a file, parsed by the C-parser, as if it had
//! been written inline with ,-characters separating the tokens, and
//! the lines enclosed in { } brackets. This simulates how table data
//! is written inline.
//!
//! If types are included, use then to guess how to represent the read
//! data (as-is or as strings, basically)
Node get_table_file_contents( Node base, string file, array types, Group g )
{
	array res = ({});
	file = combine_path( base->file, "../", file );
	Stdio.Stat st;
	if( !(st=cache_file_stat( file ) ) )
		ERROR(sprintf("The file %O does not exist", file), base );

	depends[current_testfile][file]=1;
	old_mtimes[file] = st->mtime;

	string data = Stdio.read_file( file );
	if( !data )
		ERROR(sprintf("The file %O could not be read", file), base );

	int lno=1;
	foreach( (data-"\r") / "\n",string line )
	{
		array w = SPLIT("#line "+lno+"\n"+line,file);
		w = w[1..];
		if( !sizeof(w) || w[0]->text[0] == '#' )
			continue;
		if( types )
		{
			for( int i = 0; i<sizeof(types); i++ )
			{
				if( i >= sizeof(w) )
					w += ({ Token( "", lno, file ) });
				switch( types[i] )
				{
					case "char *":
					case "const char *":
						if( !strlen(w[i]->text) || w[i]->text[0] != '"' )
							w[i]->text = sprintf("%O", w[i]->text );
						w[i]->text = sprintf("%s",parse_string( w[i]->text,w[i] ));
						break;
					case "uni_char *":
					case "const uni_char *":
						if( !strlen(w[i]->text) || w[i]->text[0] != '"' )
							w[i]->text = sprintf("%O", w[i]->text );
						w[i]->text = sprintf("UNI_L(%s)",parse_string( w[i]->text,w[i] ));
						break;
					case "int":
					case "float":
						break;
				}
			}
			array tmp =({});
			for( int i = 0; i<sizeof( w ); i++ )
				tmp += ({ w[i], Token( ",", lno, file ) });
			res += ({ ({ Token( "{", lno+1, file ), })+ tmp+ ({Token( "}", lno, file ),})	});
		}
		lno++;
	}
	return res;
}


enum Changers {
	CHANGED_SIZE = 1,
	CHANGED_STRUCTURE = 2,
	CHANGED_STRUCTURE_AND_SIZE = 3,
};

//! Handle things that can be cached (#string and #include)
array preprocess1( Node t )
{
	int changed;
	for( int i = 0; i<sizeof( t ); i++ )
	{
		if( arrayp( t[i] ) )
			t[i] = preprocess1( t[i] );
		else if( t[i]->text == "TestUtils" && i<sizeof(t)-2 && t[i+1]->text == "::" )
		{
			t[i]->text = "g_selftest.utils";
			t[i+1]->text = "->";
		}
		else if( t[i]->text == "state" && i<sizeof(t)-2 && t[i+1]->text == "." )
		{
			t[i]->text = "g_selftest.utils";
			t[i+1]->text = "->";
		}
		else if( t[i]->text[0] == '#' && (search( t[i]->text, "define" ) == -1) )
		{
			Node q;
			if( catch {
				q = SPLIT( t[i]->text[1..], t[i]->file );
				} )
				break;
			while( !sizeof( String.trim_all_whites(q[0]->text) ) )
				q = q[1..];
			switch( q[0]->text )
			{
				case "string":
				case "include":
					// check if the file exist before sending it of to the preprocessor. The preprocessor
					// doesn't handle that very well.
					string include_file = q[1]->text[1..sizeof(q[1]->text)-2]; // Trim the quotes
					if (!Stdio.exist(combine_path( dirname(t[i]->file), include_file)))
						SYNTAX(sprintf("#%s \"%s\": File not found.", q[0]->text, include_file), t);

					array cc;
					if( mixed err = catch(cc = SPLIT(cpp( sprintf("# %d %O\n%s",
																  t[i]->line, t[i]->file,
																  t[i]->text) ), "-")) )
						ERROR(sprintf("Failed to process preprocessor statement '%s': %s\n",
									  t[i]->text, describe_backtrace(err)), t[i]);
					foreach( Array.uniq(cc->file), string fn )
					{
						Stdio.Stat x = cache_file_stat( fn );
						if( x )
						{
							old_mtimes[ fn ] = x->mtime;
							depends[current_testfile][fn]=1;
						}
					}
					q = cc;
					while( arrayp( q ) )
						q = q[-1];

					q->trailing_whitespaces = t[i]->trailing_whitespaces;
					if( sizeof( cc ) == 1 )
					{
						// Only for "#string <X>\n"
						t[i] = cc[0];
					}
					else
					{
						changed = 1;
						t[i] = preprocess1(cc);
					}
					break;
			}
		}
	}
	if( changed )
	{
		if( Node offending = catch( t = .NodeUtil.group( Array.flatten( t ) ) ) )
			if( objectp( offending ) )
				SYNTAX( sprintf("Syntax error after preprocessing; stray %O", offending->text ),
						offending );
			else
				SYNTAX( sprintf("Syntax error after preprocessing; unmatched %O",
								offending[0]->text ), offending[0] );
	}
	return  t;
}

//! Handle foreach, table and iterate.
//!
//! This function will parse the whole tree, recursively, multiple
//! times (once for each level of foreach() that is actually
//! expanded).
Node preprocess( Node t, bool honor_table )
{
	if (!sizeof(t))
		return ({});

	Changers changed;
	Node tk, token = t[0];
	mapping constants = ([]);

	for( int i=0; i<sizeof(t); i++ )
	{
		if( arrayp( tk = t[i] ) )
			t[i] = preprocess( tk, false );
		else switch( tk->text )
		{
			case "constant":
				if( honor_table )
				{
					changed |= CHANGED_SIZE;
					int start = i++;
					string name = (string)IDENTIFIER("constant");

					if( (tk=GOBBLE()) != "=" )
						SYNTAX(sprintf("Expected '=' after 'constant IDENT'; got %O",tk->text), tk );
					constants[name] = ({ });
					while( (tk=GOBBLE())->text != ";" )
						constants[name] += ({tk});
					for( int j = start; j<i; j++ )
						t[j]=0;
				}
				break;
			case "__DIRECTORY__":
				werror("Warning: __DIRECTORY__ will be discontinued in python rewrite, use OPFILE_SELFTEST_DATA_FOLDER instead\n");
				t[i] = Token( sprintf("%O",combine_path( t[i]->file, ".." )),t[i]->line, t[i]->file );
				break;

			case "table":
				if( honor_table )
				{
					changed |= CHANGED_SIZE;
					int start=i;
					array types;
					i++;
					string name = (string)IDENTIFIER("table");
					if( tables[ tk->file+":"+name ] )
						SYNTAX(sprintf("The table %O is already defined\n", tk->file+":"+name),tk);

					if( arrayp( t[i] ) && t[i][0]=="(")
					{
						types = column( do_group(trim(t[i])), 0 );
						i++;
					}
					// table [( types )] ident { table }

					switch( arrayp(t[i]) ? 0 : t[i]->text )
					{
						case "read":
							i++;
							string file = parse_string( GOBBLE() );
							if( (tk=GOBBLE()) != ";" )
								SYNTAX( sprintf("Expected ; after filename, got %O\n", tk),tk );
							tables[ tk->file+":"+name ] = Table( types,
																 ({ Token( "{", t[i]->line, t[i]->file), })
																 + get_table_file_contents( t[i], file, types, current_group )
																 + ({Token( "}", t[i]->line, t[i]->file), })  );
							break;

						case "read_filelist":
							{
								i++;
								string file = parse_string( GOBBLE() );
								if( (tk=GOBBLE()) != ";" )
									SYNTAX( sprintf("Expected ; after filename, got %O\n", tk),tk );
								Node file_contents = get_table_file_contents( t[i], file, types, current_group );
								foreach(file_contents, array(Token) q)
								{
									if (q[0] == "{")
									{
										string tmp_q = q[1] + "";
										if (tmp_q[0] == '"')
										{
											string tmp_dir = dirname(q[1]->file);
											tmp_q = tmp_q[1..strlen(tmp_q)-2];
											tmp_dir = combine_path(tmp_dir, dirname(tmp_q));
											tmp_dir = tmp_dir[..strlen(tmp_dir)-2];
											string tmp_base = basename(tmp_q);
											if (tmp_base != "." && tmp_base != "..")
											{
												global_test_files += ({ combine_path(tmp_dir, tmp_base) - (prefix + "/")});
											}
										}
									}
								}

								tables[ tk->file+":"+name ] = Table( types,
																	 ({ Token( "{", t[i]->line, t[i]->file), })
																	 + file_contents
																	 + ({Token( "}", t[i]->line, t[i]->file), })  );
								break;
							}

						case "filelist":
							{
								// filelist OR_LIST [name OR_LIST] [recursively] [directories]
								i++;
								array directories = ({});
								array globs = ({});
								int recurse;
								int state = 0;
								int dirs = 0;
								while( t[i] != ";" )
								{
									if( arrayp( t[i] ) )
										SYNTAX("Did not expect list inside 'table filelist'", t[i-1] );
									switch( state )
									{
										case 0: // Allow string.
											directories += ({ parse_string( t[i] ) });
											state = 1;
											break;
										case 1: // Allow 'or', 'name', 'recursively' or 'directories'
											switch( (string)t[i] )
											{
												case "or": state = 0; break;
												case "name": state = 2; break;
												case "recursively": recurse=1; state=3; break;
												case "directories": dirs=1; state=3; break;
												default:
													SYNTAX(sprintf("Syntax error. Expected 'or', 'name', 'recursively' or 'directories',  not %O",
																   (string)t[i]), t[i] );
											}
											break;
										case 2: // Allow string
											globs += ({ parse_string( t[i] ) });
											state = 3;
											break;
										case 3: // Allow 'or', 'recursively' or 'directories'
											switch( (string)t[i] )
											{
												case "or": state = 2; break;
												case "recursively": recurse=1; state=3; break;
												case "directories": dirs=1; state=3; break;
												default:
													SYNTAX(sprintf("Syntax error. Expected 'or', 'recursively' or 'directories',  not %O",
																   (string)t[i]), t[i] );
											}
											break;
									}
									i++;
									if( i >= sizeof( t ) )
										SYNTAX("End of file while reading table filelist options.", t[i-1] );
								}

								// We have now read the filelist options.
								Node directory_contents = get_directory_contents( t[i], directories, globs, recurse, dirs );
								tables[ tk->file+":"+name ] = FileListTable( types,
																	 ({ Token( "{", t[i]->line, t[i]->file), })
																	 + directory_contents
																	 + ({Token( "}", t[i]->line, t[i]->file), })
																   );
								break;
							}
						default:
							if( !arrayp( t[i] ) )
								SYNTAX(sprintf("Expected table data. Got %O", t[i]->text),t[i] );
							tables[ tk->file+":"+name ] = Table( types, t[i] );
							i++;
					}
					for( int q = start; q<i; q++ )
						t[q] = 0;
					i--;
				}
				break;
			case "foreach":
				{
					Node ident, code, values;
					int start = i;
					changed |= CHANGED_STRUCTURE_AND_SIZE;
					i++; // the foreach keyword
					ident = t[i++];
					if( !arrayp(ident) || ident[0] != "(" )
						SYNTAX(sprintf("Expected (variable list) after foreach, got %O", (string)tk),tk);
					if( (tk=IDENTIFIER("foreach")) != "from" )
						SYNTAX(sprintf("Expected 'from' after variable list, got %O",
									   (string)tk),tk);
					if( catch {
							values = t[i++];
							code = t[i];
						} )
						SYNTAX("Unexpected end of file while parsing foreach statement", tk );

					bool need_path_prefix = false;

					if( arrayp(values) )
						values = do_group( trim(values) );
					else if( !tables[values->file+":"+(string)values] )
						SYNTAX("There is no table named %O", values );
					else
					{
						// FIXME: Do this in a less hacky way
						Table t = tables[values->file+":"+(string)values];
						need_path_prefix = t->need_path_prefix();
						values = t->get_table();
					}

					if( !arrayp( code ) || code[0] != "{" )
						SYNTAX("Expected {} block after 'foreach X from {}'", tk );

					code = trim( code ); // strip  { } pair

					ident = column(do_group( trim(ident) ),0);
					if( sizeof( ident ) == 1 )
						values = map( values, lambda( mixed q ) {
												  if( arrayp(q) && !arrayp(q[0]) )
													  return ({ q });
												  return q;
											  } );
					array res = ({});
					foreach( values, array cval )
					{
						array tmp = code;
						if( !arrayp( cval ) )
							ERROR("Internal error: Table is not a list", code);
						if( !arrayp( ident ) )
							SYNTAX("Expected array as type",code);
						if( sizeof( cval ) > sizeof( ident ) )
							SYNTAX(sprintf("Foreach: There are only %d variable%s, but there are %d elements in the value array\n",
										   sizeof(ident),(sizeof(ident)!=1?"s":""),sizeof(cval)),code[0]);
						else if( sizeof( cval ) < sizeof( ident ) )
							SYNTAX(sprintf("Foreach: There are %d variables, but only %d element%s in the value array\n",
										   sizeof(ident),sizeof(cval),(sizeof(cval)!=1?"s":"")),code[0]);

						/* FIXME: Do this in one pass */
						for( int n = 0; n<sizeof( ident ); n++ )
						{
							string newident = cval[n][0];
							if (need_path_prefix)
								newident = "ST_make_path(" + cval[n][0] + ")";

							tmp = .NodeUtil.replace_macro(
										.NodeUtil.replace_identifier(tmp,ident[n],
											newident),
									    "$("+ident[n]+")", cval[n][1] );
						}
						res += tmp;
					}

					for( ; start<i; start++ )  t[start]=0;
					t[i] = res;
				}
				break;

			case "iterate":
				{
					Node ident, code;
					int start = i;
					changed |= CHANGED_SIZE;
					i++;
					ident = t[i++];

					if( (tk=IDENTIFIER("iterate")) != "from" )
						SYNTAX(sprintf("Expected 'from' after identifier list in iterate, got %O", (string)tk),tk);

					if( catch {
							tk = IDENTIFIER("iterate");
							code = t[i];
						} )
						SYNTAX("Unexpected end of file while parsing iterate statement", tk );

					Table table;
					if( !(table = tables[tk->file+":"+(string)tk]) )
						SYNTAX(sprintf("There is no table named %O",(string)tk), tk );

					if( !arrayp( code ) || code[0] != "{" )
						SYNTAX("Expected {} block after 'iterate (X) from table'", tk );
					if( !arrayp(ident) )
						SYNTAX("Expected (ident) as iterate identifier", ident);
					ident = column(do_group( trim(ident) ),0);
					for(  ;start<i; start++ ) t[start]=0;
					t[i] = preprocess(table->iterate(ident,code), false);
				}
				break;
		}
	}

	if( !(changed & CHANGED_SIZE) )
		return t;

	t-=({0});

	if( sizeof(constants) )
		t = .NodeUtil.replace_identifiers(t,constants);

	if( changed & CHANGED_STRUCTURE )
	{
		if( Node offending = catch( t = .NodeUtil.group( Array.flatten( t ) ) ) )
			if( objectp( offending ) )
				SYNTAX( sprintf("Syntax error after foreach expansion; stray %O", offending->text ),
						offending );
			else
				SYNTAX( sprintf("Syntax error after foreach expansion; unmatched %O",
								offending[0]->text ), offending[0] );
		return preprocess( t, honor_table );
	}
	return t;
}


//! Parse and remove all external definitions from the tree. This is
//! only applied to the 'global {}' node-block.
//! It's done to get the right scope for the externs.
Node strip_externs( Node data, Group g )
{
	int i;
	int altered;
	for( int i = 0; i<sizeof( data ); i++ )
	{
		if( arrayp(data[i]) )
			data[i] = strip_externs( data[i], g );
		else if( data[i]->text == "extern" )
		{
			altered=1;
			int j = i;
			while( data[j]->text != ";" )
			{
				if( j == sizeof( data ) )
					ERROR("Illegal extern declaration", data[i] );
				j++;
			}
			g->globvars += data[i..j];
			for( ;i<=j; i++ )
				data[i] = 0;
			i--;
		}
	}
	if( altered )
		return data - ({ 0 });
	return data;
}

// "//! text..."
//
// This syntax can be used anywhere where you can normally place strings.
// It's most convenient in html {} blocks.
Node convert_special_comments( Node t )
{
	string trim_one_white( string x )
	{
		if( strlen(x) && (<' ','\t'>)[x[0]] )
			return x[1..];
		return x;
	};

	int last_tl = -1, pivot, change;
	array finalize = ({});
	for( int i = 0; i<sizeof( t ); i++ )
	{
		mixed q = t[i];
		if( arrayp( q ) )
			t[i] = convert_special_comments( q );
		else if( has_prefix( q->text, "//" ) || has_prefix( q->text, "/*" ) )
		{
			if( has_prefix( q->text[2..], "!" ) )
			{
				string str = trim_one_white(q->text[3..]);
				if( !strlen(str) || str[-1] != '\n' )
					str += "\n";
				change=1;
				if( last_tl != -1 && pivot == i-1)
				{
					t[last_tl]->text += str;
					t[i]=0;
					pivot++;
				}
				else
				{
					finalize += ({ i });
					t[last_tl=pivot=i]->text = str;
				}
			}
			else
			{
				change=1;
				pivot++;
				t[i]=0;
			}
		}
	}
	if( change )
	{
		foreach( finalize, int i )
			t[i]->text = replace(sprintf("%O", t[i]->text ), "\\n\"\n\"", "\\n");
		return t - ({ 0 });
	}
	return t;
}

int parse_testfile(mapping opts, Node t, int i, string dir )
{
	bool is_uni = false;
	Node name, path;

	if (PEEK()->text == "uni")
	{
		is_uni = true;
		GOBBLE();
	}

	Node token = t[i];
	name = IDENTIFIER("file")->text;
	path = parse_string(GOBBLE());

	global_test_files += ({ combine_path(dir, path) - (prefix + "/")});

	if (is_uni)
		opts->testfiles |= ([ name: Token("ST_uni_make_path(\"" + path + "\")") ]);
	else
		opts->testfiles |= ([ name: Token("ST_make_path(\"" + path + "\")") ]);

	return i;
}

int parse_require( mapping opts, Node t, int i )
{
	ADT.Stack stack = ADT.Stack();
	Node tk;

	do
	{
		Node n = GOBBLE();
		string x;
		if( arrayp(n) && n[0] == '(' )
		{
			mapping so = ([]);
			parse_require( so, trim(n), 0 );
			if( so->require )
				stack->push( "("+(so->require*" ")+")" );
			opts->require_init |= so->require_init;
			opts->require_uninit |= so->require_uninit;
			opts->required_ok |= so->required_ok;
			opts->required_ok |= so->required_fail;
		}
		else switch( n->text )
		{
			case "initialization": case "init":
				opts->require_init = 1;
				break;
			case "noinit":
				opts->require_uninit = 1;
				break;
			case "success":
				x = parse_string( GOBBLE() );
				if( opts->required_ok )
					opts->required_ok += ({ x });
				else
					opts->required_ok = ({ x });
				break;

			case "failure":
				x = parse_string( GOBBLE() );
				if( opts->required_fail )
					opts->required_fail += ({ x });
				else
					opts->required_fail = ({ x });
				break;

			case "&&":
			case "||":
			case "!":
				stack->push( (string)n );
				break;
			case "defined":
				tk = GOBBLE();
				if( arrayp( tk ) )
					tk=tk[1];
				if( !is_identifier( tk ) )
					SYNTAX("Wanted ifdef identifier", tk );
				stack->push( "defined("+tk->text+")" );
				break;

			case "undefined":
				tk = GOBBLE();
				if( arrayp( tk ) )
					tk=tk[1];
				if( !is_identifier( tk ) )
					SYNTAX("Wanted ifdef identifier", tk );
				stack->push( "!defined("+tk->text+")" );
				break;

			default:
				if( !tk ) tk = n;
				if( !is_identifier( tk ) )
					SYNTAX(sprintf("Wanted ifdef identifier, got %O", simple_reconstitute(tk)), tk );
				stack->push( "defined("+tk->text+")" );
				break;
		}
		tk = PEEK();
	} while( tk != ";" );

	string res = "";
	while( sizeof(stack) )
	{
		res += stack->pop()+" ";
    }
	if( sizeof( res ) )
		if( !opts->require )
			opts->require = (multiset)map(split_one_level( res, "&&" ), paranthesize_complex);
		else
			opts->require += (multiset)map(split_one_level( res, "&&" ),paranthesize_complex);
	return i;
}

#define DELAY() do {													\
	Node n = GOBBLE();													\
	string when = "post";												\
	if( !arrayp( n ) )													\
		switch( (string)n )												\
		{																\
			case "pre":	when = "pre"; n = GOBBLE(); break;				\
			case "post":when = "post";n = GOBBLE(); break;				\
		}																\
	opts["delay_"+when] = parse_float( n );								\
	opts["require_init"] = 1; 											\
} while(0)


string cache_file_name( string filename )
{
	string z=replace( filename[strlen(prefix)+1..],(["/":".","\\":"."])),d;
	sscanf( z, "modules.%s", z );
	sscanf( z, "%[^.].%s", d, z );
	mkdir( combine_path(cachedir,d) );
	sscanf( z, "%s.ot", z );

	// We might not have all that many characters in the filename.
	// Move common ones to the end of the path.
	foreach( ({ "tests", "selftest", "testsuite" }), string common_prefix )
		if( sscanf( z, common_prefix+".%s", z ) )
			return combine_path(cachedir, d, z+"."+common_prefix);
	return combine_path( cachedir, d, z );
}

// Main parser. Calls the other parsers above, also handles all
// top-level keywords (after macro expansion).
Group current_group;
int parse_file( string file )
{
	Test current_test;
	array(Node) t;
	DEBUG_TIME_START();
	string file_data = Stdio.read_file( file )-"\r";

	if( !file_data )
	{
		werror("Failed to read %O\n", file );
		abort(1);
	}

	// Check for arrays in the same way that the array checking script
	// does. (line based)

	if (mixed result = catch(t = SPLIT(file_data, file)))
	{
		werror("\nFailed to split file: %O\nReason: %O\n", file, arrayp(result)?result[0]:"unknown");
		abort(1);
	}

	file_data = 0;
	if( Node offending = catch( t = .NodeUtil.group( t ) ) )
		if( objectp( offending ) )
			SYNTAX( sprintf("Syntax error; stray %O", offending->text ), offending );
		else
			SYNTAX( sprintf("Syntax error; unmatched %O", offending[0]->text ), offending[0] );

	// First, convert all 'special' comments to strings. and remove
	// normal comments.
	t = preprocess1( convert_special_comments(t) );
	if( !quiet )
		werror(" parse .. ");
	DEBUG_TIME("parse");

	// The preprocessor handles tables, foreach, class-magic and iterate.

	t = preprocess( t, true );
	DEBUG_TIME("preprocess");

	// Skip files without content.
	if (!sizeof(t))
		return false;

	for( int i=0; i<sizeof(t); i++ )
		if( sizeof(t[i]) == 1 )
			t[i] = t[i][0];

	string language = "c++";
	Node tk, token = t[0];
	int found_group_content, found_group;
	int i;
	multiset default_require = (<>);

	while( 1 )
	{
		// Skip free-standing ';' characters
		while( i<sizeof(t) && (!t[i] || (objectp(t[i]) && (!strlen(String.trim_all_whites(t[i]->text)) || t[i]->text==";")) || (arrayp(t[i]) && !sizeof(t[i])) ) )
			i++;
		if( i >= sizeof( t ) )
		{
			DEBUG_TIME("fin");
			return true;
		}
		switch( (string)(tk=IDENTIFIER("toplevel")) )
		{
			case "disabled":
				{
					bool disabled = true;
					if( found_group_content )
						ERROR("Toplevel 'disabled' not allowed after test has been defined or 'include'\n",tk);
					if( PEEK() == "if" )
					{
						GOBBLE();
						disabled = !!parse( "bool", GOBBLE(), -1 );
					}
					if( disabled )
					{
						if( found_group )
							m_delete( groups, current_group->name );
						foreach( glob( file+":*", indices(tables)), string t )
							m_delete( tables, t );
						return false;
					}
				}
				break;

			case "language":
				if( !found_group )
					ERROR("'language' before 'group'", tk );
				language = "";
				while( PEEK() != ";" )
					language += lower_case(GOBBLE()->text);
				SEMICOLON("language");
				break;


			case "include":
				{
					int ifexists=0;
					multiset include_require = (<>);
					// includeif [IFDEF_EXPRESSION] "file"
					void handle_if_token( Node t )
					{
						for( int i=0; i<sizeof( t ); )
						{
							tk = GOBBLE();
							if( arrayp( tk ) )
								handle_if_token( trim(tk) );
							else switch( tk->text )
							{
								case ";":
									SYNTAX("; before file to include", tk );
									break;
								case "&&":
								case "if":     break; // Noop
								case "exists": ifexists=1; break;
								case "defined":
									tk = GOBBLE();
									if( arrayp( tk ) )
										tk=tk[1];
								default:
									if( !is_identifier( tk ) )
										SYNTAX("Wanted ifdef identifier", tk );
									include_require["defined("+tk->text+")"]=1;
									break;
								case "undefined":
									tk = GOBBLE();
									if( arrayp( tk ) )
										tk=tk[1];
									if( !is_identifier( tk ) )
										SYNTAX("Wanted ifdef identifier", tk );
									include_require["!defined("+tk->text+")"]=1;
									break;
								case "||":
									SYNTAX("|| not supported in include if-conditions", tk );

							}
						}
					};

					int end = i;
					while( arrayp(t[end]) || (t[end]->text[0] != '"' && t[end]->text != "<")) {
						end++;
					}
					handle_if_token( t[i..end-1] );
					i = end;
					if( !found_group )
						ERROR("'include' before 'group'", tk );

					found_group_content=1;
					int platform_include = 0;
					string fn = "";
					if (t[i] == "<")
					{
						platform_include = 1;

						int end = i;
						i++; // <
						while (t[i] && !arrayp(t[i]) && t[i]->text != ">")
						{
							fn += t[i];
							i++;
						}
						if (t[i] != ">")
							ERROR("Expected '>'", t[i]);
						i++; // >
					}
					else
					{
						fn = parse_string( GOBBLE() );
					}
					if( !ifexists || cache_file_stat( combine_path( prefix, fn ) ) )
					{
						if( !current_group->has_includes[fn] )
						{
							current_group->has_includes[fn]=1;
							current_group->includes += ({ ({ .NodeUtil.line( tk ), .NodeUtil.file( tk ), fn,
											  ({default_require|include_require,}), platform_include }) });
						}
						else
						{
							foreach( current_group->includes, array q ) {
								if( q[2]==fn )
								{
									q[3] += ({default_require|include_require});
									q[4] = platform_include;
								}
							}
						}
					}
					SEMICOLON("include");
				}
				break;

			case "subtest":
				if( !found_group )
					ERROR("'subtest' before 'group", tk );
				found_group_content=1;
				Node code = ({ Token("int") });
				mapping opts = ([
					"require":copy_value(default_require),
				]);

				code += ({ IDENTIFIER( "subtest" ) });

				if( !arrayp( PEEK() ) || PEEK()[0] != "(" )
					SYNTAX("Expected arguments after subtest name\n",code[-1]);
				code += ({GOBBLE()});

				// Post arguments, pre body.
				while( tk = PEEK() )
				{
					if( arrayp(tk) && tk[0] == "{" )
					{
						// BODY
						tk[-1] = ({Token("FINALLY"),Token(";return 1;}")});
						current_group->low_add_test(current_test=SubTest("",code+({GOBBLE()}),opts));
						break;
					}
					switch( (string)IDENTIFIER( "after test arguments and before ;" ) )
					{
						case "require":i=parse_require(opts,t,i); break;
						default:SYNTAX("Unsupported option: "+tk->text,tk);
					}
					SEMICOLON("option");
				}
				break;

			case "group":
				found_group=1;
				default_require=(<>);
				string nm = parse_string( GOBBLE() );
				if( !groups[nm] )
				{
					Group new_group;
					groups[nm] = new_group = Group( nm,tk->file,tk->line );
					current_group = new_group;
				}
				else
				{
					WARN("Redefining group "+nm,tk);
					WARN("Previously defined here",groups[nm]);
					current_group = groups[nm];
				}
				language = "c++";
				groups[nm]->files[ file ] = 1;
				SEMICOLON("group");
				foreach( glob( file+":*", indices(tables)), string t )
				{
// 					werror("%O belongs to %O in %O\n", t, current_group->name, file );
					if( tables[t]->line > current_group->line )
					{
						tables[t]->group = current_group;
						current_group->tables += ({ tables[t] });
					}
				}
				break;

			case "manual":
				if( !found_group )
					ERROR("'manual' before 'group'", tk );
				tk = GOBBLE();
				if( !arrayp( tk ) || tk[0] != "(" )
					SYNTAX("Expected '(\"name\", \"question\")' after 'manual'",tk);
				Node id_token = Token( "", tk[0]->line, tk[0]->file );
				tk = trim(tk);
				{
					string name;
					for( int comma=0; comma<sizeof(tk); comma++ )
					{
						if( tk[comma] == "," )
						{
							opts = ([
								"require":copy_value(default_require),
								"repeat":1,
								"async":1,
								"manual":parse_string( tk[comma+1..] ),
							]);
							name = parse_string( tk[..comma-1] );
						}
					}
					while( tk = PEEK() )
					{
						if( tk == ";" )	break;
						switch( (string)IDENTIFIER( "after test arguments and before ;" ) )
						{
							case "delay":  DELAY();                    break;
							case "require":i=parse_require(opts,t,i); break;
							default: SYNTAX("Unsupported option: "+tk->text,tk);
						}
						SEMICOLON("option");
					}
					if( opts->manual )
						current_group->low_add_test(current_test = Test(name, ({id_token}), opts));
					else
						SYNTAX("Expected two arguments '(\"name\", \"question\")' for "
							   "'test_manual'",tk);
				}
				break;
			case "test_equal":
			case "test_nequal":
				found_group_content=1;
				string type = (string)tk;
				if( !current_group )
					ERROR("test before 'group'", tk );

				// test_equal( string, string, group, group );
				token = tk;
				tk = GOBBLE();

				if( !arrayp( tk ) || tk[0] != "(" || tk[-1] != ")")
					SYNTAX("Expected '(arguments)' after 'test_*'",tk);

				tk = trim(tk);
				array res = ({});

				int last=0,cc=0;
				for( ;cc<sizeof(tk); cc++ )
				{
					if( tk[cc] == "," )
					{
						res += ({tk[last..cc-1]});
						last = cc+1;
					}
				}
				res += ({tk[last..]});

				if( sizeof( res ) < 3 ) // (name, val1, val2) OR  (name, type, val1, val2)
					SYNTAX("Wrong number of arguments to test_equal, expected 3", token );
				if( sizeof( res ) == 4 )
				{
					WARN("test_equal and test_nequal with 4 arguments is deprecated. Use only 3 (skip format string)", token);
					res = res[0..0]+res[2..];
				}
				opts = (["require":copy_value(default_require),
						 "repeat":1 ]);

				array args = ({ parse_string( res[0] ),	res[1], res[2], opts });

				while( tk = PEEK() )
				{
					if( tk->text == ";" )
					{
						program test_type;
						switch( type )
						{
							case "test_equal":  test_type =  TestEqual; break;
							case "test_nequal": test_type = TestNEqual; break;
						}
						current_group->low_add_test( test_type( @args ) );
						i++;
						break;
					}
					switch( (string)IDENTIFIER( "after test arguments and before ;" ) )
					{
						case "repeat": opts->repeat = parse_int( GOBBLE() );   break;
						case "fails":  opts->fails = 1;          			   break;
						case "disabled": opts->disabled = 1;                   break;
 						case "leakcheck":  opts->leakcheck = 1;    			   break;
						case "delay":  DELAY();                                break;
						case "require":	i=parse_require(opts,t,i);   		   break;
						default:SYNTAX("Unsupported option: "+tk->text,tk);
					}
					SEMICOLON("option");
					token = tk;
				}
				break;

			case "data":
			case "html":
			case "text":
			case "xml":
			case "xhtml":
			case "svg":
				{
					found_group_content=1;
					if( !found_group )
						ERROR("'"+((string)tk)+"' before 'group'", tk );
					/**
					Supported syntaxes:
					 data "type" "data";
					 data "type" {data};
					And
					 type "data";
					 type "url" "data";
					 type {data};
					 type "url" {data};
					Where type is html, text, xml, xhtml or svg.
					**/
					string type, url, data;

					Node first_arg = GOBBLE();
					Node second_arg = PEEK(); // this will hold the data

					int is_first_string = parse_string(first_arg, 1) != "0";
					int is_second_string = parse_string(second_arg, 1) != "0";
					if ( (string)tk == "data")
					{
						type = parse_string(first_arg);
						second_arg = GOBBLE();
						if (arrayp(second_arg))
							second_arg = trim(second_arg);
						else
							second_arg = ({second_arg});
						url = "";
					}
					else
					{
						switch( (string)tk )
						{
							case "html": type = "text/html"; break;
							case "text": type = "text/plain"; break;
							case "xml": type = "text/xml"; break;
							case "xhtml": type = "application/xhtml+xml"; break;
							case "svg": type = "image/svg+xml"; break;
						}
						if (is_first_string && is_second_string)
						{
							url = parse_string( first_arg );
							second_arg = ({ GOBBLE() });
						}
						else if (is_first_string && arrayp(second_arg))
						{
							url = parse_string( first_arg );
							second_arg = trim(GOBBLE());
						}
						else if (is_first_string)
						{
							url = "";
							second_arg = ({ first_arg });
						}
						else if (arrayp(first_arg))
						{
							url = "";
							second_arg = trim(first_arg);
						}
						else
						{
							if( !first_arg || sizeof( first_arg ) == 0 )
								SYNTAX("Expected string or block after " + ((string)tk) + "\n", tk );
							else if (objectp(first_arg) && String.trim_all_whites(first_arg->text) == ";")
								ERROR(((string)tk) + " block lacks url and/or content", tk );
							else
								ERROR("Tokens following '"+((string)tk)+"' not recognized: '" +
									(arrayp(first_arg) ? simple_reconstitute(first_arg) : first_arg->text) + "'", tk );
						}
					}
					mapping defs = ([
						"require"     : copy_value(default_require),
						"type"        : type,
						"url"         : url,
						"require_init": 1,
						"document_block_line_number" : .NodeUtil.line(second_arg[0])
					]);
					data = parse_string(second_arg);
					if (url != "" && data == "")
						ERROR("A " + ((string)tk) + " block with an explicit url must have content.", tk);
					current_group->low_add_test( current_test=HTML( data, defs ) );
				}
				break;

			case "finally":
				if( !current_test )
					ERROR("'finally' before 'test'", tk );
				tk = GOBBLE();
				if( !arrayp( tk ) || tk[0] != "{" || tk[-1] != "}")
					SYNTAX("Expected '{ code-block }'' after 'finally'",tk);
				current_test->finally = trim(tk);
				break;

			case "import":
				if( !found_group )
					ERROR("'import' before 'group'", tk);
				string imp = parse_string( GOBBLE() );

				if (current_group->imports != "")
					current_group->imports += ",";

				current_group->imports += imp;

				SEMICOLON("import");
				break;

			case "test":
				found_group_content=1;
				if( !found_group )
					ERROR("'test' before 'group'", tk );
				tk = GOBBLE();
				if( !arrayp( tk ) || tk[0] != "(" || tk[-1] != ")")
					SYNTAX("Expected '(\"name\")' after 'test'",tk);

				opts = ([
					"require":copy_value(default_require),
					"repeat":1,
					"testfiles":([ ]),
				]);
				string name = parse_string( trim(tk) );
				string t_language = language;

				while( tk = PEEK() )
				{
					if( arrayp(tk) && sizeof(tk) == 1 )
						tk = tk[0];
					if( arrayp(tk) && tk[0] == "{" )
					{
						// End of options, start of test code.
						switch( t_language[..3] )
						{
							case "ecma":
								opts->require_init = 1;
								current_group->low_add_test(current_test = JSTest(name, tk, opts));
								break;

							case "c++":
							case "c": // FIXME: Actually support the 'c' language. This is non-trivial.
								current_group->low_add_test(current_test = Test(name, tk, opts));
								break;

							default:
								SYNTAX(sprintf("Unknown language %O\n",t_language), tk[0] );
								break;
						}
						i++;
						break;
					}
					switch( (string)IDENTIFIER( "after test name and before test body" ) )
					{
						case "language":
							t_language="";
							while( PEEK() != ";" )
								t_language += lower_case(GOBBLE()->text);
							break;
						case "multi":
							{
								string table = (string)IDENTIFIER("multi");
								Table multi;
								if( !(multi = tables[tk->file+":"+table]) )
									ERROR(sprintf("Unknown table %O",table), tk);
								if( !multi->types )
									ERROR("Need typed table for multi",tk);
								Node z = GOBBLE();
								if( !arrayp(z) || z[0] != "(" )
									SYNTAX("Expected list of arguments after table identifier\n",tk);
								array(string) args = column( do_group(trim(z)), 0 );
								opts->multi = ({ multi, args });
							}
							break;
						case "async": opts->async = 1; opts->require_init=1;     break;
						case "manual":
							opts->manual = parse_string( GOBBLE() );
							opts->async=1;
							opts->require_init=1;
							break;
						case "repeat": opts->repeat = parse_int( GOBBLE() );	break;
						case "fails":  opts->fails = 1;          				break;
						case "delay":  DELAY();                                 break;
						case "disabled": opts->disabled = 1;                    break;
						case "require":  i=parse_require(opts,t,i); 			break;
						case "leakcheck":  opts->leakcheck = 1;                 break;
						case "file": i=parse_testfile(opts,t,i,dirname(file));	break;
						case "timer": opts->timer = 1;							break;
						case "allowexceptions": opts->allow_exceptions = 1;		break;
						default:
							SYNTAX("Unsupported option: "+tk->text,tk);
					}
					SEMICOLON("option");
					token = tk;
				}
				break;

			case "require":
				{
					mapping q = ([]);
					if( !current_group )
						ERROR("require before group",tk);
					else if( sizeof(current_group->tests) )
						WARN("Group scope 'require' after tests also affect tests defined before the require",t[i]);
					i = parse_require( q, t, i );

					current_group->require_init |= q->require_init;
					current_group->require_uninit |= q->require_uninit;
					if( q->require && sizeof( q->require ) )
						default_require |= (multiset)q->require;
					current_group->require = copy_value(default_require);
					SEMICOLON("require");
					break;
				}

			case "global":
				if( !current_group )
					ERROR("'globals' before 'group'", tk );
				if( sizeof( current_group->globals ) )
				{
					werror("'globals' for %O previously defined at %s:%d\n", current_group->name,
						   .NodeUtil.file(current_group->globals),.NodeUtil.line(current_group->globals));
					ERROR("The group already has a globals section.", tk );
				}
				Node data = GOBBLE();
				if (!arrayp(data))
				{
					current_group->group_base = parse_string(data);
					data = GOBBLE();
				}
				current_group->globals = trim(strip_externs(data, current_group));
				break;

			case "setup":
				if( !current_group )
					ERROR("'setup' before 'group'", tk );
				if( sizeof( current_group->init ) )
				{
					werror("'setup' for %O previously defined in %s:%d\n", current_group->name,
						   .NodeUtil.file(current_group->init),.NodeUtil.line(current_group->init));
					ERROR("The group already has a setup section.", tk );
				}
				current_group->init = trim(GOBBLE());
				break;

			case "exit":
				if( !current_group )
					ERROR("'exit' before 'group'", tk );
				if( sizeof( current_group->exit ) )
				{
					werror("'exit' for %O previously defined in %s:%d\n", current_group->name,
						   .NodeUtil.file(current_group->exit),.NodeUtil.line(current_group->exit));
					ERROR("The group already has a exit section.", tk );
				}
				current_group->exit = trim(GOBBLE());
				break;

			case "nopch":
				if( !current_group )
					ERROR("'nopch' before 'group'", tk );

				if (current_group->nopch)
					WARN("Redundant 'nopch' is ignored", tk);

				current_group->nopch = 1;

				SEMICOLON("nopch");
				break;

			default:
				SYNTAX("Unknown keyword "+tk->text, tk );
		}
		token = tk;
	}
}

//
// ********************* Directory recursion code.
//

mapping old_mtimes = ([]);

bool recursively_parse( string directory, bool check_only )
{
	TIMER td = TIMER();
	int df = 1;
	array(string) files = get_dir( directory );
	if( !files )
	{
		werror("Warning: Failed to read directory %O\n", directory);
		return false;
	}
	foreach( sort( files ), string file )
	{
		if( file == "CVS" || file == "documentation" ) // optimize
			continue;
        if( !(glob( "*.ot", file ) || (file/".")[0] == file ))
            continue;
		string fn = combine_path( directory, file );
		Stdio.Stat st = cache_file_stat( fn );
		if( !st )
			continue;

		if( st->isdir )
		{
			if( recursively_parse( fn, check_only ) )
				return true;
		}
		else if( glob( "*.ot", file ) && file[0] != '.' )
		{
			// Skip modules not represented in the module filter.
			string module_name = ((fn-prefix)/"/")[2];
			if ( skip_module( module_name ))
				continue;

			if( check_only )
			{
				if( st->mtime != old_mtimes[fn] )
					return true;
			}
			else if( st->mtime != old_mtimes[fn] )
			{
				if( df )
				{
					if( !quiet )
						werror( "\n"+directory[strlen(prefix)+1..]+"\n");
					df = 0;
				}
				TIMER ts = TIMER();
				string tf = (fn/"/")[-1];
				old_mtimes[fn]=st->mtime;
				depends[current_testfile=fn]=(<>);
				tf = tf[strlen(tf)-39..];
				if( !quiet )
					werror("  %s %s",tf,("."*(40-strlen(tf))));
				reset();
				parse_file( fn );
				if( !quiet )
					werror("%3dms .. ", (int)(ts->get()*1000) );
				output(fn);
				if( !quiet )
					werror("%3dms\n", (int)(ts->get()*1000) );
			}
		}
	}
	if( !df )
	{
		if( !quiet )
			werror("%s %dms\n", directory[strlen(prefix)+1..], (int)(td->get()*1000) );
	}
}

string generate_includes( array(Group) g )
{
	string res = "";
	string last_condition = "";
	string old_file = "", old_line=0;

	array foo = ({});
	multiset seen = (<>);
	foreach( g->includes, array xx )
		foreach( xx, array(string|int|array(array(multiset))) x )
		{
			if( !seen[x[2]]++ )
				foo += ({ x });
			else
				foreach( foo, array z ) // WARNING: O(n^2)
					if( z[2] == x[2] )
						z[3] += x[3];
		}

	foreach( foo, array(string|int|array(multiset)) x )
	{
		string defs = make_require_or(x[3]);
		if( defs != last_condition )
		{
			if( strlen( last_condition ) )
			{
				old_line++;
				res += "#endif // "+last_condition+"\n";
			}
			last_condition = defs;
			if( strlen( defs ) )
			{
				old_line++;
				res += "#if "+defs+"\n";
			}
		}
		if( x[1] != old_file || x[0] != ++old_line )
		{
			res += sprintf( "#line %d %O\n", x[0], .NodeUtil.trim_fn(x[1]) );
			old_file = x[1];
			old_line = x[0];
		}
		if (x[4]) // platform include
			res += sprintf( "#include <%s>\n", x[2] );
		else
			res += sprintf( "#include %O\n", x[2] );
	}
	if( strlen( last_condition ) )
		res += "#endif // "+last_condition+"\n";
	return res;
}

string grouptemplate;
void output( string x )
{
	array gg = sort(values( groups ));

	if( !quiet )
		werror("g");
	mapping inf = ([
		"source":x,
		"output":({}),
		"groups":gg->name,
		"imports":gg->imports,
		"nopch":gg->nopch,
		"global_depend":gg->global_depend*"",
		"group_classes":gg->c_name(),
		"disabled":false,
	]);

	if( !quiet )
		werror("e");
	x = x[strlen(prefix)+1..];
	inf->module = (x/"/")[1];
	inf->dependencies = gg->dependencies*({});
	if( !grouptemplate )
	{
		string gtemplatef = combine_path( lower_on_nt(__FILE__), "../../src/grouptemplate.cpp");
		grouptemplate = Stdio.read_file( gtemplatef );
		if( !grouptemplate )
			werror("Failed to read group template (%O)\n", gtemplatef );
	}
	if( !quiet )
		werror("n");

	string res;
	if( sizeof(gg) && grouptemplate )
	{
		string glob = gg->globalvars()*"";
		if( !quiet )
			werror("e");
	    string code = gg->define()*"";
		if( !quiet )
			werror("r");
		string incs = generate_includes(gg);
		if( !quiet )
			werror("a");

		res = replace( grouptemplate, ([
						   "__INCLUDES__":incs,
						   "__GLOBVARS__":glob,
						   "__TESTS__":code,
						   "__YEAR__":(string)(localtime(time())["year"]+1900)
					   ]) );

		inf->tests = Array.sum(gg->numtests);
		if( !quiet )
			werror("t");
	}
	else if( !quiet )
		werror("erat");

	x = x[..strlen(x)-4];
	array z = x/"/";
	z[-1] = "ot_"+replace(z[1..]*"_",".","_")+".cpp";
	string y = z[1..]*"/";

	if( !quiet )
		werror("e .. ");
 	Stdio.mkdirhier( combine_path( outputdir, y, ".." ) );
 	if( !res )
		inf->disabled = true;

	replace_if_change(combine_path( outputdir, y ), res );
	inf->output = ({"generated/"+y});
	Stdio.write_file( combine_path( outputdir, y )+".info", encode_value( inf ) );

	if( write_deps )
	{
		string deptext = y + ".info: " + inf->source;
		foreach( depends[inf->source]; string depend; )
			deptext += " " + depend;
		deptext += " " + common_deps + "\n";
		foreach( depends[inf->source]; string depend; )
			deptext += depend + ":\n";
		replace_if_change( combine_path( outputdir, y )+".d", deptext );
	}
}

int replace_if_change( string file, string data, int|void notag )
{
#ifdef __NT__
	if (data)
		data = replace(replace(data, "\r\n", "\n"), "\n", "\r\n" );
#endif

	// Enable/disable debug versions of the tests
	// debug versions have their '#line' statements removed.
#ifdef DEBUG
	string stripped = "";
	foreach( data / "\n", string z )
		if( !has_prefix( z, "#line" ) )
			stripped += z+"\n";
	data = stripped;
#endif
	string old = Stdio.read_file( file );
	if( old != data )
	{
		if( old )
			Stdio.cp( file, file+".old" );
		if( !quiet )
			werror("%s ", (file/"/")[-1] );
		if( Stdio.write_file( file, data ) != strlen( data ) )
		{
			werror("Error: Failed to write "+file+"\n");
			abort(1);
		}
		old_mtimes[file] = file_stat(file)->mtime;
		changed_testcase_count++;
		return 1;
	}
	return 0;
}

bool check_old_mtimes( string ... files )
{
	bool res = false;
	int ss;
	foreach( files, string f )
	{
		string base = lower_on_nt(__FILE__);
		if( f )
			base = combine_path( base, "../", f );
		if( catch(ss = cache_file_stat( base )->mtime) )
		{
			werror("Error: Failed to read required file %O\n", base );
			abort(1);
		}
		if( ss != old_mtimes[base] )
		{
			old_mtimes[base]=ss;
			res = true;
		}
	}
	return res;
}

int removed_testcase_count;
array sources = ({});
array testgroups = ({});
array module_filter = ({});	// The parsed modules filter
int sources_jumbo = 1;
string last_jumbo_file = "";

/**
 * Read the module filter file.
 *
 * Returns true if the file has been modified, false otherwise
 */
bool read_module_filter (string filter_file)
{
	bool has_changed = false;

	Stdio.Stat x = cache_file_stat( filter_file );

	if (x)
	{
		if (!old_mtimes[filter_file] || x->mtime > old_mtimes[filter_file])
		{
			old_mtimes[filter_file] = x->mtime;
			has_changed = true;
		}
	}
	else
	{
		if (old_mtimes[filter_file])
		{
			m_delete(old_mtimes, filter_file);
			has_changed = true;
		}

		return has_changed;
	}

	Stdio.File f = Stdio.File(filter_file, "r");
	foreach(f->line_iterator();; string line)
	{
		string trimmed_line = String.trim_all_whites( line );
		if (sizeof(trimmed_line) && !glob("#*", trimmed_line))	// Strip comments
			module_filter += ({trimmed_line});
	}

	return has_changed;
}

bool skip_module( string module )
{
	if (sizeof(module_filter) == 0)
		return false;

	foreach (module_filter, string m)
	{
		if (glob( m, module ))
			return false;
	}

	return true;
}

void read_info( string path )
{
	string f = Stdio.read_file( path );
	if( !f )
	{
		werror("Failed to read info from "+f+"\n");
		return;
	}
	mapping inf;
	catch(inf = decode_value( f ));

	if( !inf )
	{
		werror("Failed to decode info read from "+f+"\n");
		return;
	}
	if( !file_stat( inf->source ) )
	{
		werror(inf->source+" is gone\n");
		removed_testcase_count++;
		map( inf->output,rm );
		rm( path );
		return;
	}
	if( inf->disabled )
		return;

	if (!skip_module(inf->module))
	{
		array(string) source_files = inf->output;
#ifdef __NT__
		foreach(inf->nopch; int i; int nopch)
		{
			if (nopch)
				source_files[i] += " #[winnopch]";
		}
#endif
		string this_jumbo_file = "generated/selftest_jumbo_"+ inf->module +".cpp";
		if (this_jumbo_file != last_jumbo_file)
		{
			sources += ({ "# [jumbo="+this_jumbo_file+"]" });
			last_jumbo_file = this_jumbo_file;
		}
		sources += source_files;
		foreach( inf->group_classes; int i; string cc )
			testgroups += ({({ cc, inf->groups[i], inf->module, inf->global_depend, inf->imports[i] })});
	}
}

void recursively_check_generated( string path )
{
	foreach( sort(get_dir( path )||({})), string file )
	{
		file = combine_path(path, file);
		if( has_suffix( file, ".info" ) )
			read_info( file );
		else
		{
			Stdio.Stat st = file_stat( file );
			if( st && st->isdir )
				recursively_check_generated( file );
		}
	}
}

void real_main(int argc, array argv)
{
	add_constant("bool", typeof( bool ) );
	add_constant("true", 1 ); add_constant("false", 0 );
	add_constant("TRUE", 1 ); add_constant("FALSE", 0 );

	TIMER ts2 = TIMER();
	TIMER ts = TIMER();

	array(string) parse_prefix = ({});
	bool finalize_only = false;

	// Parse command line
	foreach(Getopt.find_all_options(argv, ({
			({ "outroot", Getopt.HAS_ARG, ({"--outroot", "-o"}) }),
			({ "modulefile", Getopt.HAS_ARG, ({"--modulefile", "-m"}) }),
			({ "finalize", Getopt.NO_ARG, ({"--finalize", "-F"}) }),
			({ "deps", Getopt.NO_ARG, ({"--deps", "-D"}) }),
			({ "quiet", Getopt.NO_ARG, ({"--quiet", "-q"}) }),
			({ "help", Getopt.NO_ARG, ({"--help", "-h"}) }) }) ),
		mixed option)
	{
		switch(option[0])
		{
			case "outroot":
				outroot = normalize_path_on_nt(lower_on_nt(option[1]));
				outputdir = combine_path( outroot, "modules", "selftest", "generated" );
				cachedir = combine_path( outroot, "modules", "selftest", "parser", "cache" );
				test_files_listing = combine_path( outroot, "modules", "selftest", "selftest_data" );
				break;
			case "modulefile":
				if (Stdio.exist(option[1]) && Stdio.is_file(option[1]) )
					module_filter_file = option[1];
				else
					werror("Warning: File not found: %O\n", option[1]);
				break;
			case "finalize":
				finalize_only = true;
				break;
			case "deps":
				write_deps = true;
				break;
			case "quiet":
				quiet = true;
				break;
			case "help":
				werror(
					"Usage: parse_tests.pike [option(s)] file-or-dir-to-parse\n\n"
					" Options:\n"
					" --outroot     -o      Output root directory\n"
					" --modulefile  -m      Location of the module filter file\n"
					" --finalize    -F      Only produce common files from .cpp.info files\n"
					" --deps        -D      Write .cpp.d makefile snippets\n"
					" --help        -h      Show this message\n"
					" --quiet       -q      Do not report progress\n"
				);
				abort(0);
				break;
		}
	}

	string test_files_str = Stdio.read_file(test_files_listing);
	if (test_files_str)
	{
		global_test_files += test_files_str / "\n";
	}

	// Take care of the non-option argument. The parse root should be the first argument if any.
	argv = Getopt.get_args(argv);
	if( sizeof(argv) > 1 && sizeof(argv[-1]) > 0)
	{
		parse_prefix = ({ normalize_path_on_nt(lower_on_nt(argv[-1])) });
	}
	else
	{
		parse_prefix = ({
			combine_path( prefix, "modules" ),
			combine_path( prefix, "adjunct" ),
			combine_path( prefix, "platforms" ),
			combine_path( prefix, "data" )
		});
		if ( outroot != prefix )
			parse_prefix += ({ outroot });
	}

	code_snipplet_init();

	if (!file_stat( parse_prefix[0] )->isdir)
	{
		TIMER ts = TIMER();
		current_testfile = combine_path( prefix, parse_prefix[0] );
		depends[current_testfile]=(<>);
		if( !quiet )
		{
			string tf = (current_testfile/"/")[-1];
			tf = tf[strlen(tf)-39..];
			werror("  %s %s",tf,("."*(40-strlen(tf))));
		}
		reset();
		parse_file(current_testfile);
		if( !quiet )
			werror("%3dms .. ", (int)(ts->get()*1000) );
		output(current_testfile);
		if( !quiet )
			werror("%3dms\n", (int)(ts->get()*1000) );
		return;
	}

	Stdio.mkdirhier( cachedir );

	string mtime_cache_file = combine_path(cachedir, "old_mtimes" );
	if( !finalize_only && cache_file_stat( mtime_cache_file ) )
	{
		int ok = 1;
		if( catch([old_mtimes,depends] = decode_value(Stdio.read_file(mtime_cache_file))) )
		{
			werror("Warning: Failed to read modified cache-file "+mtime_cache_file+"\n");
			ok=0;
			depends = ([]);
			old_mtimes = ([]);
		}

		foreach( depends; string base; multiset ff )
		{
			foreach( ff; string depend; )
			{
				Stdio.Stat st = cache_file_stat( depend );
				if( !st || st->mtime != old_mtimes[depend] )
				{
					old_mtimes[base]=0;
					break;
				}
			}
		}

		bool filter_has_changed = read_module_filter( module_filter_file );

		if( check_old_mtimes( 0, "utilities.pike", "NodeUtil.pmod", "code_snipplets.pike",
							  "../src/code_snipplets.cpp", "ugly-macros.h", "pike-compat.h",
							  "../src/grouptemplate.cpp") )
		{
			old_mtimes = ([]);
			if ( !quiet )
				werror("Templates or parser changed. Full rebuild\n");
			check_old_mtimes( 0, "utilities.pike", "NodeUtil.pmod", "code_snipplets.pike",
							  "../src/code_snipplets.cpp", "ugly-macros.h", "pike-compat.h",
							  "../src/grouptemplate.cpp");
			global_test_files = ({});
		}
		else
		{
			bool status = false;
			foreach(parse_prefix, string p)
			{
				if ( !quiet )
					werror("Checking for changed *.ot-files in %O ...\n", p);
				status = recursively_parse( p, true );
				if ( status )
					break;
			}
			if ( !status )
			{
				foreach( indices(old_mtimes), string file )
				{
					Stdio.Stat x = cache_file_stat( file );
					if( !x || x->mtime > old_mtimes[file] )
					{
						if( x )
							old_mtimes[file] = x->mtime;
						else
							m_delete(old_mtimes,file);
						ok = 0;
					}
				}
				if( ok )
				{
					if ( !quiet )
						werror("No changes [%3dms]\n", (int)(ts->get()*1000) );
					if (filter_has_changed == false)
						abort(0);
				}
			}
			if ( !quiet )
				werror("Changed files [%3dms]\n", (int)(ts->get()*1000) );
		}
	}
	else if ( !finalize_only )
	{
		check_old_mtimes( 0, "utilities.pike", "NodeUtil.pmod", "code_snipplets.pike",
						  "../src/code_snipplets.cpp", "ugly-macros.h", "pike-compat.h",
						  "../src/grouptemplate.cpp");
		read_module_filter( module_filter_file );
	}

 	mkdir( outputdir );

	if ( !finalize_only )
	{
		foreach(parse_prefix, string p)
		{
			if ( !quiet )
				werror("Parsing *.ot-files in %O ..\n", p);
			recursively_parse( p, false );
		}
	}

	recursively_check_generated( outputdir );

	if ( !quiet )
	{
		if ( finalize_only )
			write("Finalization done.");
		else
		{
			write("Parsing all done in %.1fs.", ts->get());
			if( changed_testcase_count )
				write(" %d test file%s new or modified.",
					changed_testcase_count, changed_testcase_count == 1 ? " was" : "s were");
		}
		if ( removed_testcase_count )
			write( " %d old testfile%s removed.\n", removed_testcase_count,
				removed_testcase_count==1?"":"s");
		else
			write("\n");
	}

	int nt = sizeof(testgroups);


	string cc="#ifndef SELFTEST_TESTGROUPS_H\n#define SELFTEST_TESTGROUPS_H\n\n";
	string init="";
	foreach( testgroups;int i; array(string) tg )
	{
		cc+= "extern void *"+tg[0]+"_Create(int flag);\n";
		init += "    g["+i+"].constructor = &"+tg[0]+"_Create;\n";
		init += sprintf("    g["+i+"].name = %O;\n"
					  "    g["+i+"].module = %O;\n"
					  "    g["+i+"].imports = %O;\n\n",
					  tg[1], tg[2], tg[4]);
	}
	mapping rep = ([
		"NGROUPS":sizeof(testgroups),
		"PROTOS":cc,
		"INIT":init
	]);

	cc = COMPOSE(code_snipplet_t( "test_groups", rep ));
	cc += "#endif // SELFTEST_TESTGROUPS_H\n";

	if( replace_if_change( combine_path( outputdir,"testgroups.h" ), cc ) && !quiet )
		werror("changed.\n");

	if( replace_if_change( combine_path( outputdir,"../module.sources_selftest" ), sources*"\n"+"\n" ) && !quiet )
		werror("changed.\n");

 	if( Stdio.write_file( mtime_cache_file, encode_value( ({old_mtimes,depends}) ) ) < 5 )
 		werror("Warning: Failed to write modified time cache file "+mtime_cache_file+"\n");

	// Write selftest_data
	Stdio.write_file(test_files_listing, Array.uniq(global_test_files) * "\n");

#ifdef DEBUG
	// Debug code to test selftest_data
	werror("Copying %d files\n", sizeof (Array.uniq(global_test_files)));
	foreach(Array.uniq(global_test_files), string fn)
	{
		if (!Stdio.exist(combine_path(prefix, fn)))
			werror("%O does not exist\n", fn);
		else
		{
			string target_file = combine_path(prefix, "opera/selftest_data", fn);
			Stdio.mkdirhier(dirname(target_file));
			Stdio.cp(combine_path(prefix, fn), target_file);
		}
	}
#endif

	if ( !quiet )
		werror("Total time used: %1fs\n", ts2->get() );
}



int main(int argc, array argv)
{
	mixed err = catch(real_main(argc,argv));

	if( intp(err) )
		return err;
	else
		throw(err);
}
