# -*- Mode: Makefile; tab-width: 4 -*-
# Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
#
# This file is part of the Opera web browser.  It may not be distributed
# under any circumstances.
#
# Build the external encodings regression tests.
# Tested on Linux amd64. May work for other configurations as well.
#
CFLAGS=-g -fshort-wchar -Iinternal -I../../ -I../../../../ -DENCODINGS_REGTEST -DUNICODE -DUNICODE_UTF16
CXXFLAGS=$(CFLAGS)
CXX=g++

OBJS=big5-decoder.o big5hkscs-decoder.o dbcs-decoder.o euc-jp-decoder.o euc-kr-decoder.o \
	euc-tw-decoder.o gbk-decoder.o hz-decoder.o identityconverter.o inputconverter.o \
	iso-2022-cn-decoder.o iso-2022-jp-decoder.o iso-8859-1-decoder.o jis-decoder.o \
	sbcs-decoder.o sjis-decoder.o utf16-decoder.o utf7-decoder.o \
	utf8-decoder.o \
	big5hkscs-encoder.o dbcs-encoder.o encoder-utility.o euc-tw-encoder.o \
	gb18030-encoder.o hz-encoder.o iso-2022-cn-encoder.o iso-8859-1-encoder.o \
	jis-encoder.o outputconverter.o sbcs-encoder.o utf16-encoder.o \
	utf7-encoder.o utf8-encoder.o \
	filetablemanager.o reverse_table_builder.o romtablemanager.o \
	tablemanager.o \
	charconv.o chartest.o fake_opfile.o utility.o \
	charsetnames.o \
	utf8.o encodingbin.o

HEADERS=internal/core/pch.h ../../charconverter.h
DECODERS_HEADERS=$(wildcard ../../decoders/*.h)
ENCODERS_HEADERS=$(wildcard ../../encoders/*.h)
TABLEMAN_HEADERS=$(wildcard ../../tablemanager/*.h)
UTILITY__HEADERS=$(wildcard ../../utility/*.h)
CHARCONV_HEADERS=$(wildcard *.h)

BIN=charconv

all: $(BIN) encoding.bin

clean:
	-rm $(OBJS) $(BIN) encoding.bin

%.o: ../../decoders/%.cpp $(HEADERS) $(DECODERS_HEADERS)
	$(CXX) -c $(CXXFLAGS) $< -o $@

%.o: ../../encoders/%.cpp $(HEADERS) $(ENCODERS_HEADERS)
	$(CXX) -c $(CXXFLAGS) $< -o $@

%.o: ../../tablemanager/%.cpp $(HEADERS) $(TABLEMAN_HEADERS)
	$(CXX) -c $(CXXFLAGS) $< -o $@

%.o: ../../utility/%.cpp $(HEADERS) $(UTILITY__HEADERS)
	$(CXX) -c $(CXXFLAGS) $< -o $@

%.o: %.cpp $(HEADERS) $(CHARCONV_HEADERS)
	$(CXX) -c $(CXXFLAGS) $< -o $@

utf8.o: ../../../../modules/unicode/utf8.cpp $(HEADERS)
	$(CXX) -c $(CXXFLAGS) $< -o $@

encodingbin.o: ../../../../platforms/core/encodingbin.cpp $(HEADERS)
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(BIN): $(OBJS)
	$(CXX) $(OBJS) -o $(BIN)

encoding.bin: ../../../../opera/encoding.bin
	ln -s ../../../../opera/encoding.bin .

.PHONY: clean all
