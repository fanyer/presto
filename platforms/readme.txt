# Emacs: please use -*- tab-width: 4 -*-.  Thank you.
# vim:tabstop=4:noexpandtab:
# Platform modules used for the Desktop product.
# Branch manager: current PPP integrators in Desktop (desktop-integrator@opera.com)

# Format of this file:
#
# Every non-blank non-comment line must name a module, optionally
# followed by a '#' and a comment to the end-of-line.
#
# If the comment on the same line as a module name contains a
# semicolon-separated list of platforms in square brackets, the module
# should only be used on the platform(s) listed. Valid platforms are
# mac, unix, and windows.

###############################################################################
#             CORE MILESTONE: core-integration-388
###############################################################################

### Desktop modules ###
crashlog
mac					# [mac]
unix				# [unix]
windows				# [windows]
posix_ipc			# [unix;mac]
flower

### Unix-specific desktop modules ###
paxage				# [unix]
quix				# [unix]
viewix				# [unix]
x11api				# [unix]

### Platform modules supplied by Core ###
media_backends
posix				# [unix;mac]
utilix				# [unix]
vega_backends
windows_common		# [windows]
