# -*- Mode: Makefile; tab-width: 4 -*-
# Make file to drive pod2html to generate HTML under ../../documentation/
#
# First rule is the one that'll be built by default:
.PHONY defaultrule: html
.PHONY: defaultrule
.SUFFIXES: .pod
.INTERMEDIATE: .pod.munged

PODS	= $(wildcard *.pod)
# ../../crypto/des/*.pod ?
OUTDIR	?= ../../documentation
HUSH	?= @
POD2HTML = pod2html
# Add any flags you want to pass to pod2html, e.g. --noindex --verbose
PODFLAGS =
# Used by http://marc.info/?l=openssl-dev&m=104638650123431&w=2 but
# not helpful (even when this Makefile is moved to where it might be):
# PODFLAGS += --podroot=doc --podpath=apps:crypto:ssl --header
PODFLAGS += --podroot=. --podpath=. --header --htmlroot=../documentation

HTML := $(PODS:%.pod=$(OUTDIR)/%.html)
html: $(HTML)
	$(HUSH) rm pod2htm[di].tmp

# we must convert e.g. "SSL_CTX_new(3)" -> "SSL_CTX_new" in the pod file
%.pod.munged: %.pod
	$(HUSH) sed -e 's/L<\([^>()]*\)\(([0-9])\)\?[|,]\1\2\?>/L<\1\2|\1>/g' \
				-e 's/L<\([^>]*\)\(([0-9])\)>/L<\1\2|\1>/g' $< >$@
$(HTML): $(OUTDIR)/%.html: %.pod.munged
	$(HUSH) $(POD2HTML) $(PODFLAGS) --title=$(@F:%.html=%) --infile=$< --outfile=$@ && rm $<

ifeq (,$(filter index.pod,$(PODS)))
$(OUTDIR)/index.html: $(OUTDIR)/crypto.html
	$(HUSH) $(RM) $@; ln $< $@ || cp $< $@
endif # use crypto.html as index.html unless upstream adds an index.pod

clean:
	$(HUSH) $(RM) $(HTML) *.munged pod2htm[di].tmp
