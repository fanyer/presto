The turbosettings.xml file provided here is a template including settings for
Opera Turbo in both the Web Turbo and OBML version.

The file should be signed before deployed. At least in the case where the
expiry information is present. If signing is not necessary the product shipped
with the file needs to have TWEAK_OBML_COMM_VERIFY_CONFIG_SIGNATURE set to NO. 

To sign a file a new RSA key needs to be created. This is done by invoking the
creatersakey.(bat|sh) script in the scripts directory. The keys currently in
cvs are strictly for testing purposes. The password for the private key is:

VwMqR0GgUIuA

* scripts/creatersakey.(bat|sh)

This command creates a new 2048 bit RSA private key and encrypts it using a
password you have to enter, it will then output the public key as DER and PEM
files (You must enter the password for this part) and create a C headerfile
containing the public key. This headerfile is included in the sourcecode
that will load the signed files.

   keys/turbo_config_key.priv	: Private key encrypted with password
   keys/turbo_config_key.der	: Public key (DER encoded)
   keys/turbo_config_key.pem	: PEM version of turbo_config_key.pub
   turbo_config_key.h			: H-file with char array of turbo_config_key.pub, MUST be copied into source


The actual signing of the settings file is handled by the signfile.(bat|sh)
script.

* signfile.(bat|sh)

  Command line: signfile.(bat|sh) inputfile outputfile

This command will create a signature for inputfile using the key in
turbo_config_key.priv (you must enter the password), and creates an outputfile
starting with a "// " followed by the base64 encoded signture on the first
line The content from the inputfile is then appended.

A selftest signature check is done by the script.

When the file is signed it may be packaged with the product and pointed out
by a setting to take effect:

[Proxy]
Web Turbo Config File=


Note: These scripts are basically copies of the browser.js signing scripts in the install module.
