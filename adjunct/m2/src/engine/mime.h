#ifndef MIME_H
#define MIME_H

/**
 * Interface to mimedec in url2
 */

struct mime_content_type_t
{
	OpStringC* media;
	OpStringC* sub;
};



// both able to decode whole rfc822 structures as well as single parts
// to Mime_Decoder - add LoadData(FILE, file start pos (and length?))


/*
class MimeDecoder 
{

	Decode(Buffer data);

	Decode(Header headers,
		   Buffder content);

	Decode(Header header,
		   Header header,
		   ...
		   Buffer content);
		
};

class MimePart 
{
	const char* MediaType();

	const char* MediaSubType();

	// MIMEVersion
	
	// content-type

	// content-id

	// content-description

	// GetContentDisposition

	// content-transfer-encoding

	// size

	// data

	// parse(tex from pop)
	
	// totext() (ala rfc822 & http)

	// MD5

	// language
};
*/

#endif // MIME_H
