This is the README for the key system in Opera. Its mission in life is to
convey they wish from the product people into the Opera core about what keys
that should be defined. It replaces pi/OpKeys.h .

FOR THE IMPATIENT

- Grab opkeys_template.h.

- Put it in one of your platform/product directories.

- Put a YES after each key you want, and a NO for those you don't want.

- #define PRODUCT_OPKEY_FILE somewhere in your build system or ask the hardcore
  module owner to include a proper line in hardcore/keys/product_opkeyfiles.h .
  The define should point out the filename and path to where you put
  opkeys_template.h above.

- Compile and be happy, or, in the unlikely event that it doesn't work: take a
  deep breath, put a smile on your face and contact rikard@opera.com for help.

HOW IT WORKS

The key definitions are stored in files names module.keys. Theoreticall these
files can be placed in all modules adjuncts or platforms but normally only
the platform will want to add such a file.

keys.pike takes these files and outputs a number of other files:

opkeys.h contains the enum with the different keys. Each key is surrounded by
an ifdef that is turned on only if the key is enabled in the product key file.

opkeys_mappings.cpp contains the mapping functions that convert between OP_KEYs
and their string representation.

At compile time, opkeys.h will include the platform/product key definition file
and set ifdeffs accordingly. These ifdeffs in turn enable or disable the
corresponding keys.

EOT
