/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  1999-2006
 *
 * Helper class ES_JString_Appender, used to buffer Append(JString, ...) operations
 *
 * @author Stanislav N Jordanov
 */

#ifndef ES_JSTRING_APPENDER_H
#define ES_JSTRING_APPENDER_H

class ES_JString_Appender
{
protected:
    enum { BUFF_SIZE = 64 };

    ES_Context* const context;
    JString*    const target;
    uni_char*         put;
    uni_char          buff[BUFF_SIZE]; // ARRAY OK 2009-07-06 stanislavj

public:
    ES_JString_Appender(ES_Context *ctx, JString *target)
        : context(ctx),
          target(target),
          put(buff)
    {
    }

    ~ES_JString_Appender()
    {
        if (put != buff)
            ::Append(context, target, buff, put - buff);
    }

    void  FlushBuffer()
    {
        if (put != buff)
        {
            ::Append(context, target, buff, put - buff);
            put = buff;
        }
    }

    unsigned  BufferSpaceLeft() const
    {
        return (unsigned)(&buff[BUFF_SIZE] - put);
    }

    uni_char* Reserve(unsigned int nchars)
    {
        OP_ASSERT(nchars < BUFF_SIZE);
        if (BufferSpaceLeft() < nchars)
            FlushBuffer();
        uni_char *res = put;
        put += nchars;
        return res;
    }

    JString*  GetString()
    {
        FlushBuffer();
        return Finalize(context, target);
    }

    void Append(uni_char c)
    {
        if (put == &buff[BUFF_SIZE])
            ::Append(context, target, put = buff, BUFF_SIZE);
        *put++ = c;
    }

    void Append(const uni_char* s, unsigned len)
    {
        if (len == 1)
            Append(*s);
        else if (len > BUFF_SIZE / 2)
        {
            FlushBuffer();
            ::Append(context, target, s, len);
        }
        else
            while (0 < len--)
            {
                if (put == &buff[BUFF_SIZE])
                    ::Append(context, target, put = buff, BUFF_SIZE);
                *put++ = *s++;
            }
    }

    void Append(const uni_char* s)
    {
        while (*s)
        {
            if (put == &buff[BUFF_SIZE])
                ::Append(context, target, put = buff, BUFF_SIZE);
            *put++ = *s++;
        }
    }

    void Append(const char *s)
    {
        while (*s)
        {
            if (put == &buff[BUFF_SIZE])
                ::Append(context, target, put = buff, BUFF_SIZE);
            *put++ = *s++;
        }
    }


    void Append(JString* jstr)
    {
        this->Append(Storage(context, jstr), Length(jstr));
    }
};

#endif//ES_JSTRING_APPENDER_H
