Keys, certificates and a test widget for widget signing selftests.

Certificates and keys:
----------------------
- cacert.pem	CA root certificate
- cakey.pem		CA private key, passphrase: "CA key"
- signercert.pem	certificate for signing widgets, issuer cert is the CA cert
- privsignerkey.pem	private key for the signer certificate, passphrase: "Private key"

Certificates generated according to: modules/libcrypto/documentation/xmlsignature.html 

signature.xml
-------------
signature.xml for the widget is generated from signature-template.xml with a
tool like xmlsec1:

cd widget
xmlsec1 --sign --privkey-pem ../privsignerkey.pem,../signercert.pem --output signature.xml ../signature-template.xml

You may also use modules/libcrypto/scripts/signwidget.sh (it should be easier but the tools have some problems with Windows).
