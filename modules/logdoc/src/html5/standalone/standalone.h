#ifndef HTML5STANDALONE_H
#define HTML5STANDALONE_H

#ifdef HTML5_STANDALONE

#ifndef CONST_ARRAY
# if 1//def HAS_COMPLEX_GLOBALS
#  define CONST_ARRAY(name, type) const type const name[] = {
#  define CONST_ENTRY(x)            x
#  define CONST_END(name)         };
#  define CONST_ARRAY_INIT(name) ((void)0)
# else
#  define CONST_ARRAY(name, type) void init_##name () { const type *local = name; int i=0;
#  define CONST_ENTRY(x)         local[i++] = x
#  define CONST_END(name)        ;}
#  define CONST_ARRAY_INIT(name) init_##name()
# endif // HAS_COMPLEX_GLOBALS
#endif // CONST_ARRAY

class Ragnarok_UTF16toUTF8Converter
{
public:
	Ragnarok_UTF16toUTF8Converter() : m_surrogate(0), m_num_converted(0) {}

	virtual int Convert(const void *src, unsigned len, void *dest, int maxlen,
	                    int *read);

	virtual void Reset() { m_surrogate = 0; m_num_converted = 0; }
private:
	UINT16 m_surrogate; ///< Stored surrogate value
	unsigned m_num_converted;
};

#endif // HTML5_STANDALONE
#endif // HTML5STANDALONE_H
