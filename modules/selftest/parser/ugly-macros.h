#ifndef _UGLY_MACROS_H
#define _UGLY_MACROS_H
#ifdef __NT__
# define FILE_LINE "%s(%d):"
#else
# define FILE_LINE "%s:%d:"
#endif

#define TOKEN(X) Token(X,__LINE__,__FILE__) 
#define COMPOSE(X) reconstitute_with_line_numbers( /*.NodeUtil.recursively_trim_fn*/(X) )
#define SPLIT(X,FN) hide_whitespaces(tokenize(split(X),FN))
#define GOBBLE() ((sizeof(t)>i)?t[i++]:0)
#define PEEK()    ((sizeof(t)>i)?t[i]:0)
#define SYNTAX(X,T) do{werror("\n" FILE_LINE "\tSyntax error:\t"+replace((X),"%","%%")+"\n",.NodeUtil.file(T),.NodeUtil.line(T));abort(1);}while(0)
#define ERROR(X,T) do{werror("\n" FILE_LINE "\Error:\t"+replace((X),"%","%%")+"\n",.NodeUtil.file(T),.NodeUtil.line(T));abort(1);}while(0)
#define WARN(X,T)  do{werror("\n" FILE_LINE "\Warning:\t"+replace((X),"%","%%")+"\n",.NodeUtil.file(T),.NodeUtil.line(T));}while(0)
#define SEMICOLON(X)  do{if(PEEK()!=";") SYNTAX("Missing ; after "+(X),(PEEK()||t[i]));GOBBLE();}while(0)
#define IDENTIFIER(X) get_identifier(X,GOBBLE(),token)
#define SKIPP( WHEN, REASON )	code_snipplet_t("test_skip_if",(["WHEN":(WHEN),  "NAME":TS_CSTRING(group,name), "REASON":TS_CSTRING(group,"("+(REASON)+")")]))

#define STR_VERIFY "verify"
#define STR_VERIFY_SUCC "verify_success"
#define STR_VERIFY_STAT "verify_status"
#define STR_VERIFY_STR "verify_string"
#define STR_VERIFY_NOOM "verify_not_oom"
#define STR_VERIFY_FILES_EQUAL "verify_files_equal"
#define STR_VERIFY_TRAP "verify_trap"

#endif
