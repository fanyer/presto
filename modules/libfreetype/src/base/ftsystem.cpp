/***************************************************************************/
/*                                                                         */
/*  ftsystem.c                                                             */
/*                                                                         */
/*    ANSI-specific FreeType low-level system interface (body).            */
/*                                                                         */
/*  Copyright 1996-2002, 2006, 2008-2011 by                                */
/*  David Turner, Robert Wilhelm, and Werner Lemberg.                      */
/*                                                                         */
/*  This file is part of the FreeType project, and may only be used,       */
/*  modified, and distributed under the terms of the FreeType project      */
/*  license, LICENSE.TXT.  By continuing to use, modify, or distribute     */
/*  this file you indicate that you have read the license and              */
/*  understand and accept it fully.                                        */
/*                                                                         */
/***************************************************************************/

  /*************************************************************************/
  /*                                                                       */
  /* This file contains the default interface used by FreeType to access   */
  /* low-level, i.e. memory management, i/o access as well as thread       */
  /* synchronisation.  It can be replaced by user-specific routines if     */
  /* necessary.                                                            */
  /*                                                                       */
  /*************************************************************************/

#include "core/pch.h"
#ifdef USE_FREETYPE


#include "modules/libfreetype/include/ft2build.h"
#include FT_CONFIG_CONFIG_H
#include FT_INTERNAL_DEBUG_H
#include FT_INTERNAL_STREAM_H
#include FT_SYSTEM_H
#include FT_ERRORS_H
#include FT_TYPES_H
#include "modules/util/opfile/opfile.h"


  /*************************************************************************/
  /*                                                                       */
  /*                       MEMORY MANAGEMENT INTERFACE                     */
  /*                                                                       */
  /*************************************************************************/

  /*************************************************************************/
  /*                                                                       */
  /* It is not necessary to do any error checking for the                  */
  /* allocation-related functions.  This will be done by the higher level  */
  /* routines like ft_mem_alloc() or ft_mem_realloc().                     */
  /*                                                                       */
  /*************************************************************************/


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    ft_alloc                                                           */
  /*                                                                       */
  /* <Description>                                                         */
  /*    The memory allocation function.                                    */
  /*                                                                       */
  /* <Input>                                                               */
  /*    memory :: A pointer to the memory object.                          */
  /*                                                                       */
  /*    size   :: The requested size in bytes.                             */
  /*                                                                       */
  /* <Return>                                                              */
  /*    The address of newly allocated block.                              */
  /*                                                                       */
  FT_CALLBACK_DEF( void* )
  ft_alloc( FT_Memory  memory,
            long       size )
  {
    FT_UNUSED( memory );

    return op_malloc( size );
  }


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    ft_realloc                                                         */
  /*                                                                       */
  /* <Description>                                                         */
  /*    The memory reallocation function.                                  */
  /*                                                                       */
  /* <Input>                                                               */
  /*    memory   :: A pointer to the memory object.                        */
  /*                                                                       */
  /*    cur_size :: The current size of the allocated memory block.        */
  /*                                                                       */
  /*    new_size :: The newly requested size in bytes.                     */
  /*                                                                       */
  /*    block    :: The current address of the block in memory.            */
  /*                                                                       */
  /* <Return>                                                              */
  /*    The address of the reallocated memory block.                       */
  /*                                                                       */
  FT_CALLBACK_DEF( void* )
  ft_realloc( FT_Memory  memory,
              long       cur_size,
              long       new_size,
              void*      block )
  {
    FT_UNUSED( memory );
    FT_UNUSED( cur_size );

    return op_realloc( block, new_size );
  }


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    ft_free                                                            */
  /*                                                                       */
  /* <Description>                                                         */
  /*    The memory release function.                                       */
  /*                                                                       */
  /* <Input>                                                               */
  /*    memory  :: A pointer to the memory object.                         */
  /*                                                                       */
  /*    block   :: The address of block in memory to be freed.             */
  /*                                                                       */
  FT_CALLBACK_DEF( void )
  ft_free( FT_Memory  memory,
           void*      block )
  {
    FT_UNUSED( memory );

    op_free( block );
  }


  /*************************************************************************/
  /*                                                                       */
  /*                     RESOURCE MANAGEMENT INTERFACE                     */
  /*                                                                       */
  /*************************************************************************/

#ifndef FT_CONFIG_OPTION_DISABLE_STREAM_SUPPORT

  /*************************************************************************/
  /*                                                                       */
  /* The macro FT_COMPONENT is used in trace mode.  It is an implicit      */
  /* parameter of the FT_TRACE() and FT_ERROR() macros, used to print/log  */
  /* messages during execution.                                            */
  /*                                                                       */
#undef  FT_COMPONENT
#define FT_COMPONENT  trace_io

  /* We use the macro STREAM_FILE for convenience to extract the       */
  /* system-specific stream handle from a given FreeType stream object */
#define STREAM_FILE( stream )  ( (OpFile*)stream->descriptor.pointer )


#ifdef READ_FT_FONT_FROM_RAM
class FT_RAM_File
{
public:
    FT_RAM_File() : data(NULL) {}
    ~FT_RAM_File() { OP_DELETEA(data); }
    
    char *data;
    int pos;
    int len;
};
#endif // READ_FT_FONT_FROM_RAM


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    ft_ansi_stream_close                                               */
  /*                                                                       */
  /* <Description>                                                         */
  /*    The function to close a stream.                                    */
  /*                                                                       */
  /* <Input>                                                               */
  /*    stream :: A pointer to the stream object.                          */
  /*                                                                       */
  FT_CALLBACK_DEF( void )
  ft_ansi_stream_close( FT_Stream  stream )
  {
#ifdef READ_FT_FONT_FROM_RAM
      FT_RAM_File *of = ((FT_RAM_File*)stream->descriptor.pointer);
      if (of)
    {
	OP_DELETE(of);
    }
#else
    OpFile* file = ( STREAM_FILE( stream ) );
    if (file)
    {
	file->Close();
	OP_DELETE(file);
    }
#endif // READ_FT_FONT_FROM_RAM
    stream->descriptor.pointer = NULL;
    stream->size               = 0;
    stream->base               = 0;
  }


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    ft_ansi_stream_io                                                  */
  /*                                                                       */
  /* <Description>                                                         */
  /*    The function to open a stream.                                     */
  /*                                                                       */
  /* <Input>                                                               */
  /*    stream :: A pointer to the stream object.                          */
  /*                                                                       */
  /*    offset :: The position in the data stream to start reading.        */
  /*                                                                       */
  /*    buffer :: The address of buffer to store the read data.            */
  /*                                                                       */
  /*    count  :: The number of bytes to read from the stream.             */
  /*                                                                       */
  /* <Return>                                                              */
  /*    The number of bytes actually read.  If `count' is zero (this is,   */
  /*    the function is used for seeking), a non-zero return value         */
  /*    indicates an error.                                                */
  /*                                                                       */
  FT_CALLBACK_DEF( unsigned long )
  ft_ansi_stream_io( FT_Stream       stream,
                     unsigned long   offset,
                     unsigned char*  buffer,
                     unsigned long   count )
  {
	if ( !count && offset > stream->size )
      return 1;

#ifdef READ_FT_FONT_FROM_RAM
      FT_RAM_File *of = ((FT_RAM_File*)stream->descriptor.pointer);

      OP_ASSERT(of->len >= 0 && offset <= (unsigned long)of->len);
      of->pos = offset;

      int bytes_to_copy = MIN(count, (unsigned long)(of->len - of->pos));
      op_memcpy(buffer, of->data + of->pos, bytes_to_copy);
      of->pos += bytes_to_copy;

      return bytes_to_copy;
#else
    OpFile*  file = STREAM_FILE( stream );
    if (OpStatus::IsError(file->SetFilePos(offset)))
	return 0;

    OpFileLength bytes_read;
    if (OpStatus::IsError(file->Read(buffer, count, &bytes_read)))
	return 0;

    return (unsigned long)bytes_read;
#endif // READ_FT_FONT_FROM_RAM
  }


  /* documentation is in ftstream.h */

  FT_BASE_DEF( FT_Error )
  FT_Stream_Open( FT_Stream    stream,
                  const char*  filepathname )
  {
    if ( !stream )
      return FT_Err_Invalid_Stream_Handle;

	stream->descriptor.pointer = NULL;
    stream->pathname.pointer   = (char*)filepathname;
    stream->base               = 0;
    stream->pos                = 0;
    stream->read               = NULL;
    stream->close              = NULL;

    BOOL ok = FALSE;
    OpStackAutoPtr<OpFile> file(OP_NEW(OpFile, ()));

#ifdef READ_FT_FONT_FROM_RAM
    OpStackAutoPtr<FT_RAM_File> of(OP_NEW(FT_RAM_File, ()));
#endif // READ_FT_FONT_FROM_RAM

    OpString temp;
    ANCHOR(OpString, temp);
    if (OpStatus::IsSuccess(temp.SetFromUTF8(filepathname)))
    {
	if (file.get() && OpStatus::IsSuccess(file->Construct(temp.CStr())))
	{
	    OpFileLength length;
	    if (OpStatus::IsSuccess(file->Open(OPFILE_READ)) &&
		OpStatus::IsSuccess(file->GetFileLength(length)))
	    {
#ifdef READ_FT_FONT_FROM_RAM
		of.get();
		of->pos = 0;
		of->len = (unsigned long)length;
		of->data = OP_NEWA(char, of->len);
                if (!of->data)
                    return FT_Err_Out_Of_Memory;
		OpFileLength bytes_read = 0;
                OP_STATUS status = file->Read(of->data, (unsigned long)length, &bytes_read);
		if (OpStatus::IsError(status))
                    return status == OpStatus::ERR_NO_MEMORY ?
                        FT_Err_Out_Of_Memory : FT_Err_Invalid_Stream_Read;
		file->Close();

		stream->size                = of->len;
		stream->descriptor.pointer  = of.release();
#else
		stream->size                = (unsigned long)length;
		stream->descriptor.pointer  = file.release();
#endif // READ_FT_FONT_FROM_RAM
		stream->pathname.pointer    = (char*)filepathname;
		stream->pos                 = 0;
		stream->read                = ft_ansi_stream_io;
		stream->close               = ft_ansi_stream_close;
		ok = TRUE;
	    }
	}
    }

    if (!ok)
    {
	FT_ERROR(( "FT_Stream_Open:" ));
	FT_ERROR(( " could not open `%s'\n", filepathname ));

	return FT_Err_Cannot_Open_Resource;
    }

    FT_TRACE1(( "FT_Stream_Open:" ));
    FT_TRACE1(( " opened `%s' (%d bytes) successfully\n",
                filepathname, stream->size ));

    return FT_Err_Ok;
  }

#endif /* !FT_CONFIG_OPTION_DISABLE_STREAM_SUPPORT */

#ifdef FT_DEBUG_MEMORY

  extern FT_Int
  ft_mem_debug_init( FT_Memory  memory );

  extern void
  ft_mem_debug_done( FT_Memory  memory );

#endif


  /* documentation is in ftobjs.h */

  FT_BASE_DEF( FT_Memory )
  FT_New_Memory( void )
  {
    FT_Memory  memory;


    memory = (FT_Memory)op_malloc( sizeof ( *memory ) );
    if ( memory )
    {
      memory->user    = 0;
      memory->alloc   = ft_alloc;
      memory->realloc = ft_realloc;
      memory->free    = ft_free;
#ifdef FT_DEBUG_MEMORY
      ft_mem_debug_init( memory );
#endif
    }

    return memory;
  }


  /* documentation is in ftobjs.h */

  FT_BASE_DEF( void )
  FT_Done_Memory( FT_Memory  memory )
  {
#ifdef FT_DEBUG_MEMORY
    ft_mem_debug_done( memory );
#endif
    memory->free( memory, memory );
  }

#endif // USE_FREETYPE

/* END */
