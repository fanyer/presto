# -*- Mode: Makefile; tab-width: 4 -*-
# vim: syntax=make ts=4

# Sources definitions for building test plugin.

npo_SRC			= $(npo_SRC_BASE)/instance.cpp \
				  $(npo_SRC_BASE)/script.cpp \
				  $(npo_SRC_BASE)/plugin.cpp \
				  $(npo_SRC_BASE)/library.cpp \
				  $(npo_SRC_BASE)/windowed.cpp \
				  $(npo_SRC_BASE)/windowless.cpp
ifeq ($(shell uname), Darwin)
npo_SRC			+= $(npo_SRC_BASE)/instance-mac.mm
endif
