#!/usr/bin/perl
#
# Copyright (C) 2007-2008 Opera Software AS.  All rights reserved.
#
# This file is part of the Opera web browser.  It may not be distributed
# under any circumstances.


# This script will create various different test-configurations, and
# a make file.  Running the makefile through 'make' will perform all
# the tests.
#
# This test is not part of the regular selftest system due to the many
# different configurations and long compiletimes that would have been
# involved.
#
# This configuration test is meant to be a sanity check for the many
# macros, inlining, versions and other configuration parameters that
# are involved for memory operations.
#
# Author: Morten Rolland, mortenro@opera.com
#

$MACROS = "modules/memory/src/memory_macros.h";

&setup();

# The arguments to the 'mktest' function are (values 0 or 1):
#
# s/S MEMORY_NAMESPACE_OP_NEW
#
# n MEMORY_REGULAR_OP_NEW
# N MEMORY_INLINED_OP_NEW
#
# g MEMORY_REGULAR_GLOBAL_NEW
# G MEMORY_INLINED_GLOBAL_NEW
#
# e MEMORY_REGULAR_NEW_ELEAVE
# E MEMORY_INLINED_NEW_ELEAVE
#
# a MEMORY_REGULAR_ARRAY_ELEAVE
# A MEMORY_INLINED_ARRAY_ELEAVE
#
# l MEMORY_REGULAR_GLOBAL_DELETE
# L MEMORY_INLINED_GLOBAL_DELETE
#
# d/D ENABLE_MEMORY_DEBUGGING
# p/P USE_POOLING_MALLOC
# r/R REPORT_MEMMAN

# Try many different configurations ...

for ( $dbg = 0; $dbg < 2; $dbg++ ) {
    for ( $namesp = 0; $namesp < 2; $namesp++ ) {
	for ( $inlnew = 0; $inlnew < 2; $inlnew++ ) {
	    for ( $inldel = 0; $inldel < 2; $inldel++ ) {
		for ( $pooled = 0; $pooled < 2; $pooled++ ) {
		    for ( $repmem = 0; $repmem < 2; $repmem++ ) {
			$c = $dbg;
			$c = 2*$c + $namesp;
			$c = 2*$c + $inlnew;
			$c = 2*$c + $inldel;
			$c = 2*$c + $pooled;
			$c = 2*$c + $repmem;

			if ( $c == 63 ) {
			    $more_tests = 0;
			}

			&mktest("c$c", $namesp,
				1-$inlnew, $inlnew,
				1-$inlnew, $inlnew,
				1-$inlnew, $inlnew,
				1-$inlnew, $inlnew,
				1-$inldel, $inldel,
				$dbg,
				$pooled,
				$repmem);
		    }
		}
	    }
	}
    }
}

&finish();

sub setup {
    %configs = ();
    $testnum = 1;
    $more_tests = 1;
    $runmaketests = "";
    $alltarget = "all:\n";
}

sub finish {
    open(MAKEFILE, "> Makefile.auto") || die("Can't create Makefile.auto");
    print MAKEFILE "# Automatically generated Makefile to run all\n";
    print MAKEFILE "# configuration tests (created by mkcfgs.pl)\n\n";
    print MAKEFILE "$alltarget$runmaketests";
    close(MAKEFILE);
}

sub mktest {
    $defines = "";
    $letterconfig = "";

    $current_config = shift();
    if ( defined($configs{$current_config}) ) {
	print "\nERROR: Configuration $current_config duplicated\n\n";
	exit 1;
    }
    $configs{$current_config} = 1;

    $MEMORY_NAMESPACE_OP_NEW = shift();
    $MEMORY_REGULAR_OP_NEW = shift();
    $MEMORY_INLINED_OP_NEW = shift();
    $MEMORY_REGULAR_GLOBAL_NEW = shift();
    $MEMORY_INLINED_GLOBAL_NEW = shift();
    $MEMORY_REGULAR_NEW_ELEAVE = shift();
    $MEMORY_INLINED_NEW_ELEAVE = shift();
    $MEMORY_REGULAR_ARRAY_ELEAVE = shift();
    $MEMORY_INLINED_ARRAY_ELEAVE = shift();
    $MEMORY_REGULAR_GLOBAL_DELETE = shift();
    $MEMORY_INLINED_GLOBAL_DELETE = shift();
    $ENABLE_MEMORY_DEBUGGING = shift();
    $USE_POOLING_MALLOC = shift();
    $REPORT_MEMMAN = shift();

    &ifdef("ENABLE_MEMORY_DEBUGGING", $ENABLE_MEMORY_DEBUGGING, "dD");
    &ifdef("USE_POOLING_MALLOC", $USE_POOLING_MALLOC, "pP");
    &ifdef("REPORT_MEMMAN", $REPORT_MEMMAN, "rR");
    &define("MEMORY_NAMESPACE_OP_NEW", $MEMORY_NAMESPACE_OP_NEW, "sS");
    &define("MEMORY_REGULAR_OP_NEW", $MEMORY_REGULAR_OP_NEW, "n");
    &define("MEMORY_INLINED_OP_NEW", $MEMORY_INLINED_OP_NEW, "N");
    &define("MEMORY_REGULAR_GLOBAL_NEW", $MEMORY_REGULAR_GLOBAL_NEW, "g");
    &define("MEMORY_INLINED_GLOBAL_NEW", $MEMORY_INLINED_GLOBAL_NEW, "G");
    &define("MEMORY_REGULAR_NEW_ELEAVE", $MEMORY_REGULAR_NEW_ELEAVE, "e");
    &define("MEMORY_INLINED_NEW_ELEAVE", $MEMORY_INLINED_NEW_ELEAVE, "E");
    &define("MEMORY_REGULAR_ARRAY_ELEAVE", $MEMORY_REGULAR_ARRAY_ELEAVE, "a");
    &define("MEMORY_INLINED_ARRAY_ELEAVE", $MEMORY_INLINED_ARRAY_ELEAVE, "A");
    &define("MEMORY_REGULAR_GLOBAL_DELETE", $MEMORY_REGULAR_GLOBAL_DELETE,"l");
    &define("MEMORY_INLINED_GLOBAL_DELETE", $MEMORY_INLINED_GLOBAL_DELETE,"L");

    &define("MEMORY_EXTERNAL_DECLARATIONS", "\"$MACROS\"", "");

    &define("HAVE_MALLOC", "", "");
    &define("MEMORY_ALIGNMENT", "8");
    &define("MEMORY_GROPOOL_ALLOCATOR", "", "");

    $thistest = "c$testnum";
    $thisfile = "cfg$testnum.cpp";
    $testnum++;

    &define("MEMORY_CONFIG_TEST", "\"$thisfile/\" CONFIG", "");

    &define("op_lowlevel_malloc", "cfg_malloc", "");
    &define("op_lowlevel_free", "cfg_free", "");

    $exe = $thisfile;
    $exe =~ s/\.cpp$//;
    $alltarget .= "\tg++ -E -dD -I../../.. $thisfile -o ${exe}.i\n";
    $alltarget .= "\tg++ -O -ggdb -I../../.. ";
    $alltarget .= "memory_config.cpp $thisfile -o $exe\n";
    $runmaketests .= "\t@./$exe\n";

    open(FILE, "> $thisfile") || die("Can't create $thisfile");
    $thisline = 1;
    &output_file_header();

    &op_malloc();
    &op_free();
    &op_global_new();
    &op_global_delete();

    &OP_NEW();
    &OP_NEWA();
    &OP_NEW_L();
    &OP_NEWA_L();
    &OP_DELETE();

    &OP_NEWGRO();
    &OP_NEWGRO_L();
    &OP_NEWGROA();
    &OP_NEWGROA_L();

    &OP_NEW_pooled();
    &OP_NEW_pooled_L();
    &OP_NEW_pooled_T();
    &OP_NEW_pooled_TL();
    &OP_NEW_pooled_P();
    &OP_NEW_pooled_PL();
    &OP_NEW_pooled_accounted();
    &OP_NEW_pooled_accounted_L();
    &OP_NEW_pooled_accounted_T();
    &OP_NEW_pooled_accounted_TL();
    &OP_NEW_pooled_accounted_P();
    &OP_NEW_pooled_accounted_PL();
    &output_file_footer();
    close(FILE);
}

sub define {
    my($macro, $value, $cfg) = @_;
    if ( $value eq "" ) {
	$defines .= "#define $macro\n";
    } else {
	$defines .= "#define $macro $value\n";
	$letterconfig .= $cfg if length($cfg) == 1 && $value;
	$letterconfig .= substr($cfg,$value,1) if length($cfg) == 2;
    }
}

sub ifdef {
    my($macro, $defineit, $cfg) = @_;
    if ( $defineit ) {
	$defines .= "#define $macro\n";
	$letterconfig .= $cfg if length($cfg) == 1;
	$letterconfig .= substr($cfg,1,1) if length($cfg) == 2;
    } else {
	$defines .= "// #define $macro\n";
	$letterconfig .= substr($cfg,0,1) if length($cfg) == 2;
    }
}

sub output {
    my($line) = @_;
    foreach $x ( split(/\n/, $line) ) {
	$testfile .= "$x\n";
	$thisline++;
    }
}

sub testline {
    my($line) = @_;
    my($linenum) = $thisline;
    die("testline must not have \\n\n") if $line =~ /\n/;
    &output("    $line");
    $testfile =~ s/%l/$linenum/g;
    $testfile =~ s/%f/\\"$thisfile\\"/g;
}

sub operation {
    my($format, $args) = @_;
    if ( $args eq "" ) {
	&output("    operation(\"$format\");");
    } else {
	&output("    operation(\"$format\", $args);");
    }
}

sub output_file_header {
    &output("#define CONFIG \"$letterconfig\"");
    &output(" ");
    &output($defines);
    &output("\n// --------------------------\n\n");
    &output("#include <stdio.h>");
    &output("#include <stdlib.h>");
    &output("extern \"C\" void* cfg_malloc(size_t size);");
    &output("extern \"C\" void* cfg_free(void* ptr);");
    &output("typedef unsigned short uni_char;");
    &output("#define OPMEMORY_MEMORY_SEGMENT");
    &output("#include \"modules/memory/memory_fundamentals.h\"");
    &output("void operation(const char* format, ...);");
    &output("void expect(const char* format, ...);");
    &output("void complete(void);");
    if ( $more_tests ) {
	&output("void run_cfg$testnum(void);");
    }
    &output("extern const char* current_test;");
    &output("extern int verbose;");

    &output("#define BOOL int");
    &output("#define FALSE 0");
    &output("class MemoryManager { public:");
    &output("static void IncDocMemoryCount(size_t, BOOL);");
    &output("static void DecDocMemoryCount(size_t); };");

    &output("class Foobar {");
    &output("public: Foobar(void) { a = 17; } Foobar(int x) { a = x; }");
    &output("private: int a; };");

    &output("class Pooled {");
    &output("OP_ALLOC_POOLING");
    &output("OP_ALLOC_POOLING_SMO_DOCUMENT");
    &output("public: Pooled(void) { p = 17; }");
    &output("virtual ~Pooled(void) {}");
    &output("private: int p; };");

    &output("class Pooled_L {");
    &output("OP_ALLOC_POOLING");
    &output("OP_ALLOC_POOLING_SMO_DOCUMENT_L");
    &output("public: Pooled_L(void) { p = 17; }");
    &output("virtual ~Pooled_L(void) {}");
    &output("private: int p; };");

    &output("class Pooled_T {");
    &output("OP_ALLOC_POOLING");
    &output("OP_ALLOC_POOLING_SMO_TEMPORARY");
    &output("public: Pooled_T(void) { p = 17; }");
    &output("virtual ~Pooled_T(void) {}");
    &output("private: int p; };");

    &output("class Pooled_TL {");
    &output("OP_ALLOC_POOLING");
    &output("OP_ALLOC_POOLING_SMO_TEMPORARY_L");
    &output("public: Pooled_TL(void) { p = 17; }");
    &output("virtual ~Pooled_TL(void) {}");
    &output("private: int p; };");

    &output("class Pooled_P {");
    &output("OP_ALLOC_POOLING");
    &output("OP_ALLOC_POOLING_SMO_PERSISTENT");
    &output("public: Pooled_P(void) { p = 17; }");
    &output("virtual ~Pooled_P(void) {}");
    &output("private: int p; };");

    &output("class Pooled_PL {");
    &output("OP_ALLOC_POOLING");
    &output("OP_ALLOC_POOLING_SMO_PERSISTENT_L");
    &output("public: Pooled_PL(void) { p = 17; }");
    &output("virtual ~Pooled_PL(void) {}");
    &output("private: int p; };");

    &output("class PooledAccounted {");
    &output("OP_ALLOC_ACCOUNTED_POOLING");
    &output("OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT");
    &output("public: PooledAccounted(void) { p = 17; }");
    &output("virtual ~PooledAccounted(void) {}");
    &output("private: int p; };");

    &output("class PooledAccounted_L {");
    &output("OP_ALLOC_ACCOUNTED_POOLING");
    &output("OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT_L");
    &output("public: PooledAccounted_L(void) { p = 17; }");
    &output("virtual ~PooledAccounted_L(void) {}");
    &output("private: int p; };");

    &output("class PooledAccounted_T {");
    &output("OP_ALLOC_ACCOUNTED_POOLING");
    &output("OP_ALLOC_ACCOUNTED_POOLING_SMO_TEMPORARY");
    &output("public: PooledAccounted_T(void) { p = 17; }");
    &output("virtual ~PooledAccounted_T(void) {}");
    &output("private: int p; };");

    &output("class PooledAccounted_TL {");
    &output("OP_ALLOC_ACCOUNTED_POOLING");
    &output("OP_ALLOC_ACCOUNTED_POOLING_SMO_TEMPORARY_L");
    &output("public: PooledAccounted_TL(void) { p = 17; }");
    &output("virtual ~PooledAccounted_TL(void) {}");
    &output("private: int p; };");

    &output("class PooledAccounted_P {");
    &output("OP_ALLOC_ACCOUNTED_POOLING");
    &output("OP_ALLOC_ACCOUNTED_POOLING_SMO_PERSISTENT");
    &output("public: PooledAccounted_P(void) { p = 17; }");
    &output("virtual ~PooledAccounted_P(void) {}");
    &output("private: int p; };");

    &output("class PooledAccounted_PL {");
    &output("OP_ALLOC_ACCOUNTED_POOLING");
    &output("OP_ALLOC_ACCOUNTED_POOLING_SMO_PERSISTENT_L");
    &output("public: PooledAccounted_PL(void) { p = 17; }");
    &output("virtual ~PooledAccounted_PL(void) {}");
    &output("private: int p; };");

    $runtests = "";
}

sub output_file_footer {
    &output("void run_cfg(void) {");
    &output($runtests);
    &output("}");
    print FILE "$testfile";
    $testfile = "";
}

sub start_test {
    my($testname) = @_;
    &output("static void test_$testname(void) {");
    &output("    current_test = MEMORY_CONFIG_TEST \"/$testname\";");
    $runtests .= "    test_$testname();\n";
    print FILE "$testfile";
    $testfile = "";
}

sub end_test {
    &output("    complete();");
    &output("}");
    print FILE "$testfile";
    $testfile = "";
}

sub is_cfg {
    my(@cfgs) = @_;
    my($matches) = 0;
    my($cfg,$ok,$c);

    my($tmpcfgs) = "@cfgs";

    while ( $#cfgs >= 0 ) {
	$cfg = shift(@cfgs);
	$ok = 1;
	for ( $t = 0; $t < length($cfg); $t++ ) {
	    $c = substr($cfg,$t,1);

	    $ok = 0 if $c eq 's' && $MEMORY_NAMESPACE_OP_NEW;
	    $ok = 0 if $c eq 'S' && !$MEMORY_NAMESPACE_OP_NEW;

	    $ok = 0 if $c eq 'n' && !$MEMORY_REGULAR_OP_NEW;
	    $ok = 0 if $c eq 'N' && !$MEMORY_INLINED_OP_NEW;

	    $ok = 0 if $c eq 'g' && !$MEMORY_REGULAR_GLOBAL_NEW;
	    $ok = 0 if $c eq 'G' && !$MEMORY_INLINED_GLOBAL_NEW;

	    $ok = 0 if $c eq 'e' && !$MEMORY_REGULAR_NEW_ELEAVE;
	    $ok = 0 if $c eq 'E' && !$MEMORY_INLINED_NEW_ELEAVE;

	    $ok = 0 if $c eq 'a' && !$MEMORY_REGULAR_ARRAY_ELEAVE;
	    $ok = 0 if $c eq 'A' && !$MEMORY_INLINED_ARRAY_ELEAVE;

	    $ok = 0 if $c eq 'l' && !$MEMORY_REGULAR_GLOBAL_DELETE;
	    $ok = 0 if $c eq 'L' && !$MEMORY_INLINED_GLOBAL_DELETE;

	    $ok = 0 if $c eq 'd' && $ENABLE_MEMORY_DEBUGGING;
	    $ok = 0 if $c eq 'D' && !$ENABLE_MEMORY_DEBUGGING;

	    $ok = 0 if $c eq 'p' && $USE_POOLING_MALLOC;
	    $ok = 0 if $c eq 'P' && !$USE_POOLING_MALLOC;

	    $ok = 0 if $c eq 'r' && $REPORT_MEMMAN;
	    $ok = 0 if $c eq 'R' && !$REPORT_MEMMAN;
	}
	$matches++ if $ok;
    }

    $rc = 0;
    $rc = 1 if $matches > 0;

    # print "$letterconfig: is_config($tmpcfgs) => $rc\n";

    return $rc;
}

sub expif {
    my($cfg,$format,$args) = @_;
    my($t) = 0;
    my($c);

    if ( &is_cfg(split(/\s/, $cfg)) ) {
	&expect($format, $args);
    }
}

sub expect {
    my($format, $args) = @_;
    if ( $args eq "" ) {
	&output("    expect(\"$format\");");
    } else {
	&output("    expect(\"$format\",$args);");
    }
}

#
# The tests
#

sub op_malloc {
    &start_test("op_malloc");
    &operation("op_malloc(100)", "");

    &expif("D", "OpMemDebug_OpMalloc(100,%f,%l)", "");
    &expif("d", "cfg_malloc(100)", "");

    &testline("char* a = (char*)op_malloc(100);");
    &end_test();
}

sub op_free {
    &start_test("op_free");
    &output("    char* ptr = \"some data\";");
    &operation("op_free(%p)", "ptr");

    &expif("D", "OpMemDebug_OpFree(%p,%f,%l)", "ptr");
    &expif("d", "cfg_free(%p)", "ptr");

    &testline("op_free(ptr);", "");
    &end_test();
}

sub op_global_new {
    &start_test("op_global_new");
    &operation("new Foobar(42)");

    $s = "sizeof(Foobar)";

    &expif("DG", "OpMemDebug_GlobalNew(%d)", "$s");
    &expif("dG", "cfg_malloc(%d)", "$s");
    &expif("g", "operator new(%d)", "$s");

    &testline("Foobar* f = new Foobar(42);");
    &end_test();
}

sub op_global_delete {
    &start_test("op_global_delete");
    &operation("delete f");
    &output("    Foobar foo(17);");
    &output("    Foobar* f = &foo;");

    &expif("DL", "OpMemDebug_GlobalDelete(%p)", "f");
    &expif("dL", "cfg_free(%p)", "f");
    &expif("l", "operator delete(%p)", "f");

    &testline("delete f;");
    &end_test();
}

sub OP_NEW {
    &start_test("OP_NEW");
    &operation("OP_NEW(Foobar,(42))", "");

    $s = "sizeof(Foobar)";
    $o = "\\\"Foobar\\\"";

    &expif("DN", "OpMemDebug_New(%d,%f,%l,$o)", "$s");
    &expif("Dn", "operator new(%d,TOpAllocNewDefault,%f,%l,$o)", "$s");
    &expif("dSn", "operator new(%d,TOpAllocNewDefault)", "$s");
    &expif("dsn", "operator new(%d)", "$s");
    &expif("dN", "cfg_malloc(%d)", "$s");

    &testline("Foobar* f = OP_NEW(Foobar,(42));");
    &end_test();
}

sub OP_NEWGRO {
    &start_test("OP_NEWGRO");
    &operation("OP_NEWGRO(Foobar,(),&arena)", "");

    $s = "sizeof(Foobar)";
    $o = "\\\"Foobar\\\"";

    &testline("OpMemGroup arena;");
    &expif("D", "OpMemGroup::NewGRO(%d,%p,%f,%l,$o)", "$s,&arena");
    &expif("d", "OpMemGroup::NewGRO(%d)", "$s");

    &testline("Foobar* f = OP_NEWGRO(Foobar,(),&arena);");
    &end_test();
}

sub OP_NEWGRO_L {
    &start_test("OP_NEWGRO_L");
    &operation("OP_NEWGRO_L(Foobar,(),&arena)", "");

    $s = "sizeof(Foobar)";
    $o = "\\\"Foobar\\\"";

    &testline("OpMemGroup arena;");
    &expif("D", "OpMemGroup::NewGRO_L(%d,%p,%f,%l,$o)", "$s,&arena");
    &expif("d", "OpMemGroup::NewGRO_L(%d)", "$s");

    &testline("Foobar* f = OP_NEWGRO_L(Foobar,(),&arena);");
    &end_test();
}

sub OP_NEWGROA {
    &start_test("OP_NEWGROA");
    &operation("OP_NEWGROA(Foobar,10,&arena)", "");

    $s = "10*sizeof(Foobar)";
    $o = "\\\"Foobar\\\"";

    &testline("OpMemGroup arena;");
    &expif("D", "OpMemGroup::NewGRO(%d,%p,%f,%l,$o)", "$s,&arena");
    &expif("d", "OpMemGroup::NewGRO(%d)", "$s");

    &testline("Foobar* f = OP_NEWGROA(Foobar,10,&arena);");
    &end_test();
}

sub OP_NEWGROA_L {
    &start_test("OP_NEWGROA_L");
    &operation("OP_NEWGROA_L(Foobar,10,&arena)", "");

    $s = "10*sizeof(Foobar)";
    $o = "\\\"Foobar\\\"";

    &testline("OpMemGroup arena;");
    &expif("D", "OpMemGroup::NewGRO_L(%d,%p,%f,%l,$o)", "$s,&arena");
    &expif("d", "OpMemGroup::NewGRO_L(%d)", "$s");

    &testline("Foobar* f = OP_NEWGROA_L(Foobar,10,&arena);");
    &end_test();
}

sub OP_NEW_pooled {
    &start_test("OP_NEW_pooled");
    &operation("OP_NEW(Pooled,())");

    $s = "sizeof(Pooled)";
    $o = "\\\"Pooled\\\"";

    #&expif("DP", "OpMemDebug_NewSMOD(%d,%f,%l,$o)", "$s");
    &expif("D", "OpMemDebug_NewSMOD(%d,%f,%l,$o)", "$s");
    #&expif("DpN", "OpMemDebug_New(%d,%f,%l,$o)", "$s");
    #&expif("Dpn", "operator new(%d,TOpAllocNewDefault,%f,%l,$o)", "$s");

    &expif("dP", "OpPooledAlloc(%d)", "$s");
    &expif("dpG", "cfg_malloc(%d)", "$s");
    &expif("dpgS", "operator new(%d,TOpAllocNewDefault)", "$s");
    &expif("dpgs", "operator new(%d)", "$s");

    &testline("Pooled* ptr = OP_NEW(Pooled,());");

    # Continue the test by verifying that OP_DELETE works too

    &operation("OP_DELETE(%p)", "ptr");

    &expif("D", "OpMemDebug_TrackDelete(%p,%f,%l)", "ptr");

    #&expif("DP", "OpMemDebug_PooledDelete(%p)", "ptr");
    &expif("D", "OpMemDebug_PooledDelete(%p)", "ptr");
    #&expif("DpL", "OpMemDebug_GlobalDelete(%p)", "ptr");
    &expif("dP", "OpPooledFree(%p)", "ptr");
    &expif("dpL", "cfg_free(%p)", "ptr");
    &expif("dpl", "operator delete(%p)", "ptr");

    &testline("OP_DELETE(ptr);");
    &end_test();
}

sub OP_NEW_pooled_L {
    &start_test("OP_NEW_pooled_L");
    &operation("OP_NEW_L(Pooled_L,())");

    $s = "sizeof(Pooled_L)";
    $o = "\\\"Pooled_L\\\"";

    &expif("D", "OpMemDebug_NewSMOD_L(%d,%f,%l,$o)", "$s");

    &expif("dP", "OpPooledAlloc_L(%d)", "$s");
    &expif("dpG", "op_meminternal_malloc_leave(%d)", "$s");
    &expif("dpgS", "operator new(%d,TOpAllocNewDefault_L)", "$s");
    &expif("dpgs", "operator new(%d,TLeave)", "$s");

    &testline("Pooled_L* ptr = OP_NEW_L(Pooled_L,());");
    &end_test();
}

sub OP_NEW_pooled_T {
    &start_test("OP_NEW_pooled_T");
    &operation("OP_NEW(Pooled_T,())");

    $s = "sizeof(Pooled_T)";
    $o = "\\\"Pooled_T\\\"";

    &expif("D", "OpMemDebug_NewSMOT(%d,%f,%l,$o)", "$s");

    &expif("dP", "OpPooledAllocT(%d)", "$s");
    &expif("dpG", "cfg_malloc(%d)", "$s");
    &expif("dpgS", "operator new(%d,TOpAllocNewDefault)", "$s");
    &expif("dpgs", "operator new(%d)", "$s");

    &testline("Pooled_T* ptr = OP_NEW(Pooled_T,());");
    &end_test();
}

sub OP_NEW_pooled_TL {
    &start_test("OP_NEW_pooled_TL");
    &operation("OP_NEW_L(Pooled_TL,())");

    $s = "sizeof(Pooled_TL)";
    $o = "\\\"Pooled_TL\\\"";

    &expif("D", "OpMemDebug_NewSMOT_L(%d,%f,%l,$o)", "$s");

    &expif("dP", "OpPooledAllocT_L(%d)", "$s");
    &expif("dpG", "op_meminternal_malloc_leave(%d)", "$s");
    &expif("dpgS", "operator new(%d,TOpAllocNewDefault_L)", "$s");
    &expif("dpgs", "operator new(%d,TLeave)", "$s");

    &testline("Pooled_TL* ptr = OP_NEW_L(Pooled_TL,());");
    &end_test();
}

sub OP_NEW_pooled_P {
    &start_test("OP_NEW_pooled_P");
    &operation("OP_NEW(Pooled_P,())");

    $s = "sizeof(Pooled_P)";
    $o = "\\\"Pooled_P\\\"";

    &expif("D", "OpMemDebug_NewSMOP(%d,%f,%l,$o)", "$s");

    &expif("dP", "OpPooledAllocP(%d)", "$s");
    &expif("dpG", "cfg_malloc(%d)", "$s");
    &expif("dpgS", "operator new(%d,TOpAllocNewDefault)", "$s");
    &expif("dpgs", "operator new(%d)", "$s");

    &testline("Pooled_P* ptr = OP_NEW(Pooled_P,());");
    &end_test();
}

sub OP_NEW_pooled_PL {
    &start_test("OP_NEW_pooled_PL");
    &operation("OP_NEW_L(Pooled_PL,())");

    $s = "sizeof(Pooled_PL)";
    $o = "\\\"Pooled_PL\\\"";

    &expif("D", "OpMemDebug_NewSMOP_L(%d,%f,%l,$o)", "$s");

    &expif("dP", "OpPooledAllocP_L(%d)", "$s");
    &expif("dpG", "op_meminternal_malloc_leave(%d)", "$s");
    &expif("dpgS", "operator new(%d,TOpAllocNewDefault_L)", "$s");
    &expif("dpgs", "operator new(%d,TLeave)", "$s");

    &testline("Pooled_PL* ptr = OP_NEW_L(Pooled_PL,());");
    &end_test();
}

sub OP_NEW_pooled_accounted {
    &start_test("OP_NEW_pooled_accounted");
    &operation("OP_NEW(PooledAccounted,())", "");

    $s = "sizeof(PooledAccounted)";
    $o = "\\\"PooledAccounted\\\"";

    #&expif("DP", "OpMemDebug_NewSMOD(%d,%f,%l,$o)", "$s");
    &expif("D", "OpMemDebug_NewSMOD(%d,%f,%l,$o)", "$s");
    # &expif("DpR", "OpMemDebug_OverloadedNew(%d,%f,%l,$o)", "$s");
    # FIXME: These next two ones should be pooling; probably NewSMOD()
    #&expif("DprN", "OpMemDebug_New(%d,%f,%l,$o)", "$s");
    #&expif("Dprn", "operator new(%d,TOpAllocNewDefault,%f,%l,$o)", "$s");

    &expif("dP", "OpPooledAlloc(%d)", "$s");
    &expif("dpN", "cfg_malloc(%d)", "$s");
    &expif("dpnSR dpns", "operator new(%d)", "$s");
    &expif("dpnSr", "operator new(%d,TOpAllocNewDefault)", "$s");

    &expif("R", "IncDocMemoryCount(%d,0)", "$s");

    &testline("PooledAccounted* ptr = OP_NEW(PooledAccounted,());", "");

    # Continue the test by verifying that OP_DELETE works too

    &operation("OP_DELETE(%p)", "ptr");

    &expif("D", "OpMemDebug_TrackDelete(%p,%f,%l)", "ptr");
    &expif("R", "DecDocMemoryCount(%d)", "$s");

    #&expif("DP", "OpMemDebug_PooledDelete(%p)", "ptr");
    &expif("D", "OpMemDebug_PooledDelete(%p)", "ptr");
    # &expif("DpL", "OpMemDebug_GlobalDelete(%p)", "ptr");
    # &expif("Dpl
    &expif("dP", "OpPooledFree(%p)", "ptr");
    &expif("dpL", "cfg_free(%p)", "ptr");
    &expif("dpl", "operator delete(%p)","ptr");

    &testline("OP_DELETE(ptr);", "");
    &end_test();
}

sub OP_NEW_pooled_accounted_L {
    &start_test("OP_NEW_pooled_accounted_L");
    &operation("OP_NEW(PooledAccounted_L,())", "");

    $s = "sizeof(PooledAccounted_L)";
    $o = "\\\"PooledAccounted_L\\\"";

    &expif("D", "OpMemDebug_NewSMOD_L(%d,%f,%l,$o)", "$s");

    &expif("dP", "OpPooledAlloc_L(%d)", "$s");
    &expif("dpN", "op_meminternal_malloc_leave(%d)", "$s");
    &expif("dpnSR dpns", "operator new(%d,TLeave)", "$s");
    &expif("dpnSr", "operator new(%d,TOpAllocNewDefault_L)", "$s");

    &expif("R", "IncDocMemoryCount(%d,0)", "$s");

    &testline("PooledAccounted_L* ptr = OP_NEW_L(PooledAccounted_L,());", "");
    &end_test();
}

sub OP_NEW_pooled_accounted_T {
    &start_test("OP_NEW_pooled_accounted_T");
    &operation("OP_NEW(PooledAccounted_T,())", "");

    $s = "sizeof(PooledAccounted_T)";
    $o = "\\\"PooledAccounted_T\\\"";

    &expif("D", "OpMemDebug_NewSMOT(%d,%f,%l,$o)", "$s");

    &expif("dP", "OpPooledAllocT(%d)", "$s");
    &expif("dpN", "cfg_malloc(%d)", "$s");
    &expif("dpnSR dpns", "operator new(%d)", "$s");
    &expif("dpnSr", "operator new(%d,TOpAllocNewDefault)", "$s");

    &expif("R", "IncDocMemoryCount(%d,0)", "$s");

    &testline("PooledAccounted_T* ptr = OP_NEW(PooledAccounted_T,());", "");
    &end_test();
}

sub OP_NEW_pooled_accounted_TL {
    &start_test("OP_NEW_pooled_accounted_TL");
    &operation("OP_NEW(PooledAccounted_TL,())", "");

    $s = "sizeof(PooledAccounted_TL)";
    $o = "\\\"PooledAccounted_TL\\\"";

    &expif("D", "OpMemDebug_NewSMOT_L(%d,%f,%l,$o)", "$s");

    &expif("dP", "OpPooledAllocT_L(%d)", "$s");
    &expif("dpN", "op_meminternal_malloc_leave(%d)", "$s");
    &expif("dpnSR dpns", "operator new(%d,TLeave)", "$s");
    &expif("dpnSr", "operator new(%d,TOpAllocNewDefault_L)", "$s");

    &expif("R", "IncDocMemoryCount(%d,0)", "$s");

    &testline("PooledAccounted_TL* ptr = OP_NEW_L(PooledAccounted_TL,());", "");
    &end_test();
}

sub OP_NEW_pooled_accounted_P {
    &start_test("OP_NEW_pooled_accounted_P");
    &operation("OP_NEW(PooledAccounted_P,())", "");

    $s = "sizeof(PooledAccounted_P)";
    $o = "\\\"PooledAccounted_P\\\"";

    &expif("D", "OpMemDebug_NewSMOP(%d,%f,%l,$o)", "$s");

    &expif("dP", "OpPooledAllocP(%d)", "$s");
    &expif("dpN", "cfg_malloc(%d)", "$s");
    &expif("dpnSR dpns", "operator new(%d)", "$s");
    &expif("dpnSr", "operator new(%d,TOpAllocNewDefault)", "$s");

    &expif("R", "IncDocMemoryCount(%d,0)", "$s");

    &testline("PooledAccounted_P* ptr = OP_NEW(PooledAccounted_P,());", "");
    &end_test();
}

sub OP_NEW_pooled_accounted_PL {
    &start_test("OP_NEW_pooled_accounted_PL");
    &operation("OP_NEW(PooledAccounted_PL())", "");

    $s = "sizeof(PooledAccounted_PL)";
    $o = "\\\"PooledAccounted_PL\\\"";

    &expif("D", "OpMemDebug_NewSMOP_L(%d,%f,%l,$o)", "$s");

    &expif("dP", "OpPooledAllocP_L(%d)", "$s");
    &expif("dpN", "op_meminternal_malloc_leave(%d)", "$s");
    &expif("dpnSR dpns", "operator new(%d,TLeave)", "$s");
    &expif("dpnSr", "operator new(%d,TOpAllocNewDefault_L)", "$s");

    &expif("R", "IncDocMemoryCount(%d,0)", "$s");

    &testline("PooledAccounted_PL* ptr = OP_NEW_L(PooledAccounted_PL,());", "");
    &end_test();
}

sub OP_NEWA {
    &start_test("OP_NEWA");
    &operation("OP_NEWA(Foobar,10)", "");

    $s = "sizeof(Foobar)";
    $o = "\\\"Foobar\\\"";

    &expif("Dn", "operator new[](%d,TOpAllocNewDefaultA,%f,%l,$o,10,10,%d)",
	   "10*$s,$s");
    &expif("DN", "OpMemDebug_NewA(%d,%f,%l,$o,10,10,%d)", "10*$s,$s");

    &expif("dSn", "operator new[](%d,TOpAllocNewDefaultA)", "10*$s");
    &expif("dsn", "operator new[](%d)", "10*$s");
    &expif("dN", "cfg_malloc(%d)", "10*$s");

    &testline("Foobar* f = OP_NEWA(Foobar,10);", "");
    &end_test();
}

sub OP_NEW_L {
    &start_test("OP_NEW_L");
    &operation("OP_NEW_L(Foobar,(42))", "");

    $s = "sizeof(Foobar)";
    $o = "\\\"Foobar\\\"";

    &expif("Dn", "operator new(%d,TOpAllocNewDefault_L,%f,%l,$o)", "$s");
    &expif("DN", "OpMemDebug_New_L(%d,%f,%l,$o)", "$s");
    &expif("dSn", "operator new(%d,TOpAllocNewDefault_L)", "$s");
    &expif("dsn", "operator new(%d,TLeave)", "$s");
    &expif("dN", "op_meminternal_malloc_leave(%d)", "$s");

    &testline("Foobar* f = OP_NEW_L(Foobar,(42));");
    &end_test();
}

sub OP_NEWA_L {
    &start_test("OP_NEWA_L");
    &operation("OP_NEWA_L(Foobar,9)", "");

    $s = "sizeof(Foobar)";
    $o = "\\\"Foobar\\\"";

    &expif("Dn", "operator new[](%d,TOpAllocNewDefaultA_L,%f,%l,$o,9,9,%d)",
	   "9*$s,$s");
    &expif("DN", "OpMemDebug_NewA_L(%d,%f,%l,$o,9,9,%d)", "9*$s,$s");
    &expif("dSn", "operator new[](%d,TOpAllocNewDefaultA_L)", "9*$s");
    &expif("dsn", "operator new[](%d,TLeave)", "9*$s");
    &expif("dN", "op_meminternal_malloc_leave(%d)", "9*$s");

    &testline("Foobar* f = OP_NEWA_L(Foobar,9);");
    &end_test();
}

sub OP_DELETE {
    &start_test("OP_DELETE");
    &output("    char* ptr = \"some data\";");
    &operation("OP_DELETE(%p)", "ptr");

    &expif("D", "OpMemDebug_TrackDelete(%p,%f,%l)", "ptr");

    &expif("l", "operator delete(%p)", "ptr");
    &expif("DL", "OpMemDebug_GlobalDelete(%p)", "ptr");
    &expif("dL", "cfg_free(%p)", "ptr");

    &testline("OP_DELETE(ptr);");
    &end_test();
}
