UNICODEDATA=../data/UnicodeData.txt
CASEFOLD=../data/CaseFolding.txt
BLOCKS=../data/Blocks.txt
JOINING=../data/ArabicShaping.txt
PROPLIST=../data/PropList.txt
LINEBREAK=../data/LineBreak.txt
MIRROR=../data/BidiMirroring.txt
WORDBREAK=../data/WordBreakProperty.txt
SENTENCEBREAK=../data/SentenceBreakProperty.txt
COMPEXCLUSIONS=../data/CompositionExclusions.txt
SCRIPTS=../data/Scripts.txt
DERIVEDJOINING=../data/DerivedJoiningType.txt
NAMESLIST=../data/NamesList.txt
THAIWRAP=../data/thai-wrap.js
HEADER=../unicode.h
PIKE ?= pike -DUNICODE_BUILD_TABLES
PYTHON ?= python
PERL ?= perl

SELFTEST=cls.dat bidi.dat lbt2.dat mirror.dat lower.dat upper.dat wbt.dat sbt.dat comb.dat \
	blocks.dat script.dat scasefolding.dat fcasefolding.dat joiningtype.dat \
	proplist.dat
RUNTIME=../tables/bidiclass.inl ../tables/canonicals.inl ../tables/caseinfo.inl \
	../tables/charclass.inl ../tables/decompositions.inl ../tables/linebreak.inl \
	../tables/mirror.inl ../tables/wordbreak.inl ../tables/sentencebreak.inl \
	../tables/scripts.inl ../tables/casefold.inl ../tables/blocks.inl \
	../tables/joiningtype.inl ../tables/proplist.inl \
	../tables/unicode_linebreak_thai_fallback.inl ../../display/text_shaper.inl

all: $(SELFTEST) $(RUNTIME)

make_x=$(PIKE) ../scripts/$(1) < $(2) > "$(3)" || { rm "$(3)"; exit 1; }
make_py=$(PYTHON) ../scripts/$(1) < $(2) > "$(3)" || { rm "$(3)"; exit 1; }

../tables/decompositions.inl: ../scripts/make_decompose.pike $(UNICODEDATA) $(HEADER) $(COMPEXCLUSIONS)
	 $(call make_x,../scripts/make_decompose.pike,$(UNICODEDATA),../tables/decompositions.inl)

bidi.dat ../tables/bidiclass.inl: ../scripts/make_bidi.pike $(UNICODEDATA) $(HEADER)
	 $(call make_x,../scripts/make_bidi.pike,$(UNICODEDATA),../tables/bidiclass.inl)

mirror.dat ../tables/mirror.inl: ../scripts/make_mirror.pike $(MIRROR) $(HEADER)
	 $(call make_x,../scripts/make_mirror.pike,$(MIRROR),../tables/mirror.inl)

lbt2.dat ../tables/linebreak.inl: ../scripts/make_linebreak.pike $(LINEBREAK) $(HEADER)
	 $(call make_x,../scripts/make_linebreak.pike,$(LINEBREAK),../tables/linebreak.inl)

cls.dat ../tables/charclass.inl: ../scripts/make_charclass.pike $(UNICODEDATA) $(HEADER)
	 $(call make_x,../scripts/make_charclass.pike,$(UNICODEDATA),../tables/charclass.inl)

comb.dat ../tables/canonicals.inl: ../scripts/make_canonicals.pike $(UNICODEDATA) $(HEADER)
	 $(call make_x,../scripts/make_canonicals.pike,$(UNICODEDATA),../tables/canonicals.inl)

lower.dat upper.dat ../tables/caseinfo.inl: ../scripts/make_caseinfo.pike $(UNICODEDATA) $(HEADER)
	 $(call make_x,../scripts/make_caseinfo.pike,$(UNICODEDATA),../tables/caseinfo.inl)

wbt.dat ../tables/wordbreak.inl: ../scripts/make_wordbreak.pike $(WORDBREAK) $(HEADER)
	 $(call make_x,../scripts/make_wordbreak.pike,$(WORDBREAK),../tables/wordbreak.inl)

sbt.dat ../tables/sentencebreak.inl: ../scripts/make_sentencebreak.pike $(SENTENCEBREAK) $(HEADER)
	 $(call make_x,../scripts/make_sentencebreak.pike,$(SENTENCEBREAK),../tables/sentencebreak.inl)

script.dat ../tables/scripts.inl: ../scripts/make_scripts.py $(SCRIPTS) $(HEADER)
	 $(call make_py,../scripts/make_scripts.py,$(SCRIPTS),../tables/scripts.inl)

scasefolding.dat fcasefolding.dat ../tables/casefold.inl: ../scripts/make_casefold.py $(CASEFOLD) $(HEADER)
	 $(call make_py,../scripts/make_casefold.py,$(CASEFOLD),../tables/casefold.inl)

blocks.dat ../tables/blocks.inl: ../scripts/make_blocks.py $(BLOCKS) $(HEADER)
	 $(call make_py,../scripts/make_blocks.py,$(BLOCKS),../tables/blocks.inl)

joiningtype.dat ../tables/joiningtype.inl: ../scripts/make_joiningtype.py $(JOINING) $(HEADER)
	 $(call make_py,../scripts/make_joiningtype.py,$(JOINING),../tables/joiningtype.inl)

proplist.dat ../tables/proplist.inl: ../scripts/make_proplist.py $(PROPLIST) $(HEADER)
	 $(call make_py,../scripts/make_proplist.py,$(PROPLIST),../tables/proplist.inl)

../tables/unicode_linebreak_thai_fallback.inl: ../scripts/make_unicode_linebreak_thai_fallback_data.py $(THAIWRAP)
	$(PYTHON) ../scripts/make_unicode_linebreak_thai_fallback_data.py --input $(THAIWRAP) --output ../tables/unicode_linebreak_thai_fallback.inl

../../display/text_shaper.inl: ../../display/scripts/shaping.pl $(DERIVEDJOINING) $(NAMESLIST)
	cd ../../display/scripts && $(RUNPERL) ./shaping.pl

clean:
	-rm $(SELFTEST) $(RUNTIME)
