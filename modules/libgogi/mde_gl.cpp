#include "core/pch.h"
#include "modules/libgogi/mde.h"

#ifdef MDE_OPENGL_PAINTING

#ifdef WIN32
#pragma comment (lib, "opengl32.lib")
#define WINGDIAPI
#define APIENTRY __stdcall
#endif

#include <GL/gl.h>
#include <stdlib.h>
#include <assert.h>

//MDE_BUFFER *gl_screen_buffer;

struct MDE_BUFFER_GL_INFO {
	unsigned int texture;
	void *lockdata;
	MDE_RECT lockarea;
	int tex_w, tex_h;
};

int GetNearestPowerOfTwo(int val)
{
	int i;
	for(i = 31; i >= 0; i--)
		if ((val - 1) & (1<<i))
			break;
	return (1<<(i + 1));
}

int MDE_GL_FORMAT(MDE_FORMAT format)
{
	switch(format)
	{
	case MDE_FORMAT_BGRA32:
		return GL_BGRA_EXT;
	case MDE_FORMAT_BGR24:
		return GL_BGR_EXT;
	default:
		MDE_ASSERT(FALSE);
		return GL_RGBA;
	}
}

MDE_BUFFER *MDE_CreateBuffer(int w, int h, MDE_FORMAT format, int pal_len)
{
	if (format != MDE_FORMAT_BGRA32 && format != MDE_FORMAT_BGR24)
	{
		MDE_ASSERT(0);
		return NULL;
	}
	
	MDE_BUFFER *buf = OP_NEW(MDE_BUFFER, ());
	if (!buf)
		return NULL;

	MDE_InitializeBuffer(w, h, MDE_GetBytesPerPixel(format) * w, format, NULL, NULL, buf);

	MDE_BUFFER_GL_INFO *info = OP_NEW(MDE_BUFFER_GL_INFO, ());
	if (!info)
	{
		MDE_DeleteBuffer(buf);
		return NULL;
	}
	buf->user_data = info;

	info->texture = 0;
	info->lockdata = NULL;
	info->tex_w = GetNearestPowerOfTwo(buf->w);
	info->tex_h = GetNearestPowerOfTwo(buf->h);
	buf->stride = info->tex_w * MDE_GetBytesPerPixel(buf->format);

	return buf;
}

void MDE_DeleteBuffer(MDE_BUFFER *&buf)
{
	if (buf)
	{
		MDE_BUFFER_GL_INFO *info = (MDE_BUFFER_GL_INFO *) buf->user_data;
		if (info)
		{
			if (info->texture)
				glDeleteTextures(1, &info->texture);
			OP_DELETE(info);
		}
		char *data_ptr = (char*) buf->data;
		OP_DELETEA(data_ptr);
		OP_DELETEA(buf->pal);
		OP_DELETE(buf);
		buf = NULL;
	}
}

void* MDE_LockBuffer(MDE_BUFFER *buf, const MDE_RECT &rect, int &stride, bool readable)
{
	MDE_BUFFER_GL_INFO *info = (MDE_BUFFER_GL_INFO *) buf->user_data;
	MDE_ASSERT(!info->lockdata);
	stride = buf->stride;
	info->lockdata = OP_NEWA(unsigned char, stride*info->tex_h);
	if (!info->lockdata)
		return NULL;
	info->lockarea = rect;
	if (readable)
	{
		if (info->texture)
		{
			glBindTexture(GL_TEXTURE_2D, info->texture);
			glGetTexImage(GL_TEXTURE_2D, 0, MDE_GL_FORMAT(buf->format), GL_UNSIGNED_BYTE, info->lockdata);
			glBindTexture(GL_TEXTURE_2D, 0);
		}
	}
	return ((unsigned char*)info->lockdata) + stride*rect.y + MDE_GetBytesPerPixel(buf->format)*rect.x;
}

void MDE_UnlockBuffer(MDE_BUFFER *buf, bool changed)
{
	MDE_BUFFER_GL_INFO *info = (MDE_BUFFER_GL_INFO *) buf->user_data;
	MDE_ASSERT(info->lockdata);
	if (changed)
	{
		if (info->texture)
		{
			MDE_RECT rect = info->lockarea;
			glBindTexture(GL_TEXTURE_2D, info->texture);
			glPixelStorei(GL_UNPACK_ROW_LENGTH, buf->stride/MDE_GetBytesPerPixel(buf->format));
			glTexSubImage2D(GL_TEXTURE_2D, 0, rect.x, rect.y, rect.w, rect.h,
					MDE_GL_FORMAT(buf->format), GL_UNSIGNED_BYTE, 
					((unsigned char*)info->lockdata) + buf->stride*rect.y + MDE_GetBytesPerPixel(buf->format)*rect.x);
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
			glBindTexture(GL_TEXTURE_2D, 0);
		}
		else
		{
			glGenTextures(1, &info->texture);
			glBindTexture(GL_TEXTURE_2D, info->texture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexImage2D(GL_TEXTURE_2D, 0, MDE_GetBytesPerPixel(buf->format), info->tex_w, info->tex_h, 0, MDE_GL_FORMAT(buf->format), GL_UNSIGNED_BYTE, info->lockdata);
			glBindTexture(GL_TEXTURE_2D, 0);
		}
	}
	unsigned char *ptr = (unsigned char*)info->lockdata;
	OP_DELETEA(ptr);
	info->lockdata = NULL;
}

// == PAINTING ================================================================

void DrawQuad(int x, int y, int w, int h, float ss = 0.f, float st = 0.f, float es = 1.0f, float et = 1.0f)
{
	glBegin(GL_QUADS);
		glTexCoord2f(ss, st); glVertex2i(x, y);
		glTexCoord2f(es, st); glVertex2i(x + w, y);
		glTexCoord2f(es, et); glVertex2i(x + w, y + h);
		glTexCoord2f(ss, et); glVertex2i(x, y + h);
	glEnd();
}

void MDE_SetColor(unsigned int col, MDE_BUFFER *dstbuf)
{
	dstbuf->col = col;
}

void MDE_SetClipRect(const MDE_RECT &rect, MDE_BUFFER *dstbuf)
{
	//MDE_ASSERT(dstbuf == gl_screen_buffer);
	dstbuf->clip = MDE_RectClip(rect, dstbuf->outer_clip);
	MDE_RECT r = rect;
	r.x += dstbuf->ofs_x;
	r.y += dstbuf->ofs_y;
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	r.y = viewport[3] - (r.y + r.h);
	glScissor(r.x, r.y, r.w, r.h);
}

void MDE_DrawBuffer(MDE_BUFFER *srcbuf, const MDE_RECT &dst, int srcx, int srcy, MDE_BUFFER *dstbuf)
{
	MDE_DrawBufferStretch(srcbuf, dst, MDE_MakeRect(srcx, srcy, dst.w, dst.h), dstbuf);
}

void MDE_DrawBufferStretch(MDE_BUFFER *srcbuf, const MDE_RECT &dst, const MDE_RECT &src, MDE_BUFFER *dstbuf)
{
	//MDE_ASSERT(dstbuf == gl_screen_buffer);
	if (srcbuf->method == MDE_METHOD_ALPHA)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	MDE_BUFFER_GL_INFO *info = (MDE_BUFFER_GL_INFO *) srcbuf->user_data;
	if (info)
		glBindTexture(GL_TEXTURE_2D, info->texture);
	if (srcbuf->method == MDE_METHOD_COLOR)
	{
		glColor4ub(MDE_COL_R(dstbuf->col), MDE_COL_G(dstbuf->col), MDE_COL_B(dstbuf->col), MDE_COL_A(dstbuf->col));
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
		glColor4ub(255, 255, 255, 255);
	DrawQuad(dst.x + dstbuf->ofs_x, dst.y + dstbuf->ofs_y, dst.w, dst.h, 
		(float)(src.x)/(float)info->tex_w, (float)(src.y)/(float)info->tex_h,
		(float)(src.x+src.w)/(float)info->tex_w, (float)(src.y+src.h)/(float)info->tex_h);
	glBindTexture(GL_TEXTURE_2D, 0);

	glDisable(GL_BLEND);
}

void MDE_DrawBufferData(MDE_BUFFER_DATA *srcdata, MDE_BUFFER_INFO *srcinf, int src_stride, const MDE_RECT &dst, int srcx, int srcy, MDE_BUFFER *dstbuf)
{
	MDE_DrawBufferDataStretch(srcdata, srcinf, src_stride, dst, MDE_MakeRect(srcx, srcy, srcdata->w - srcx, srcdata->h - srcy), dstbuf);
}

void MDE_DrawBufferDataStretch(MDE_BUFFER_DATA *srcdata, MDE_BUFFER_INFO *srcinf, int src_stride, const MDE_RECT &dst, const MDE_RECT &src, MDE_BUFFER *dstbuf)
{
	//MDE_ASSERT(dstbuf == gl_screen_buffer);
	if (srcinf->method == MDE_METHOD_COLOR)
		glColor4ub(MDE_COL_R(srcinf->col), MDE_COL_G(srcinf->col), MDE_COL_B(srcinf->col), MDE_COL_A(srcinf->col));
	else
		glColor4ub(255, 255, 255, 255);
	DrawQuad(dst.x + dstbuf->ofs_x, dst.y + dstbuf->ofs_y, dst.w, dst.h);
}

void MDE_DrawRect(const MDE_RECT &rect, MDE_BUFFER *dstbuf)
{
	//MDE_ASSERT(dstbuf == gl_screen_buffer);
	glColor4ub(MDE_COL_R(dstbuf->col), MDE_COL_G(dstbuf->col), MDE_COL_B(dstbuf->col), MDE_COL_A(dstbuf->col));
	MDE_RECT r = rect;
	r.x += dstbuf->ofs_x;
	r.y += dstbuf->ofs_y;
	glBegin(GL_LINE_STRIP);
		glVertex2i(r.x, r.y);
		glVertex2i(r.x + r.w - 1, r.y);
		glVertex2i(r.x + r.w - 1, r.y + r.h - 1);
		glVertex2i(r.x, r.y + r.h - 1);
		glVertex2i(r.x, r.y);
	glEnd();
}

void MDE_DrawRectFill(const MDE_RECT &rect, MDE_BUFFER *dstbuf)
{
	//MDE_ASSERT(dstbuf == gl_screen_buffer);
	glColor4ub(MDE_COL_R(dstbuf->col), MDE_COL_G(dstbuf->col), MDE_COL_B(dstbuf->col), MDE_COL_A(dstbuf->col));
	DrawQuad(rect.x + dstbuf->ofs_x, rect.y + dstbuf->ofs_y, rect.w, rect.h);
}

void MDE_DrawRectInvert(const MDE_RECT &rect, MDE_BUFFER *dstbuf)
{
	//MDE_ASSERT(dstbuf == gl_screen_buffer);
	glColor4ub(0, 0, 0, 128);
	DrawQuad(rect.x + dstbuf->ofs_x, rect.y + dstbuf->ofs_y, rect.w, rect.h);
}

void MDE_DrawEllipse(const MDE_RECT &rect, MDE_BUFFER *dstbuf)
{
	//MDE_ASSERT(dstbuf == gl_screen_buffer);
}

void MDE_DrawEllipseFill(const MDE_RECT &rect, MDE_BUFFER *dstbuf)
{
	//MDE_ASSERT(dstbuf == gl_screen_buffer);
}

void MDE_DrawEllipseInvertFill(const MDE_RECT &rect, MDE_BUFFER *dstbuf)
{
	//MDE_ASSERT(dstbuf == gl_screen_buffer);
}

void MDE_DrawLine(int x1, int y1, int x2, int y2, MDE_BUFFER *dstbuf)
{
	//MDE_ASSERT(dstbuf == gl_screen_buffer);
	glColor4ub(MDE_COL_R(dstbuf->col), MDE_COL_G(dstbuf->col), MDE_COL_B(dstbuf->col), MDE_COL_A(dstbuf->col));
	glBegin(GL_LINES);
		glVertex2i(x1 + dstbuf->ofs_x, y1 + dstbuf->ofs_y);
		glVertex2i(x2 + dstbuf->ofs_x, y2 + dstbuf->ofs_y);
	glEnd();
}
void MDE_DrawLineInvert(int x1, int y1, int x2, int y2, MDE_BUFFER *dstbuf)
{
	// FIXME:
	MDE_ASSERT(0);
	//MDE_ASSERT(dstbuf == gl_screen_buffer);
	glColor4ub(MDE_COL_R(dstbuf->col), MDE_COL_G(dstbuf->col), MDE_COL_B(dstbuf->col), MDE_COL_A(dstbuf->col));
	glBegin(GL_LINES);
		glVertex2i(x1 + dstbuf->ofs_x, y1 + dstbuf->ofs_y);
		glVertex2i(x2 + dstbuf->ofs_x, y2 + dstbuf->ofs_y);
	glEnd();
}

bool MDE_UseMoveRect()
{
	return false;
}

void MDE_MoveRect(const MDE_RECT &rect, int dx, int dy, MDE_BUFFER *dstbuf)
{
	MDE_ASSERT(0);
}

#endif // MDE_OPENGL_PAINTING
