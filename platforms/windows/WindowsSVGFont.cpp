#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "WindowsSVGFont.h"
#include "WindowsOpFont.h"

#ifndef USHRT_MAX
#include <limits.h>
#endif

const int WindowsSVGFont::buffer_size = 16384;

double fixed_to_double(FIXED in)
{
	return (double)in.value + (double)in.fract/USHRT_MAX;
}

WindowsSVGFont::WindowsSVGFont(OpFont* font) : SVGSystemFont(font)
{
	m_text_transform.LoadScale(1,-1);
}

OpBpath* WindowsSVGFont::GetGlyphOutline(uni_char uc)
{
	DWORD err = 0;
	OpBpath* path = NULL;
	WindowsOpFont* wopfont = (WindowsOpFont*)m_font;

	HDC fonthdc = WindowsOpFont::fonthdc;
    void * oldGdiObj = NULL;

	if (WindowsOpFont::font_on_dc != wopfont)
	{
		oldGdiObj = SelectObject(fonthdc, wopfont->fnt);
		WindowsOpFont::font_on_dc = wopfont;
	}

	GLYPHMETRICS metrics;
	MAT2 matrix;
	UINT8 buffer[buffer_size];
	
	memset(&matrix, 0, sizeof(matrix));
    matrix.eM11.value = 1;
    matrix.eM22.value = 1;

	err = ::GetGlyphOutline( fonthdc, uc, GGO_NATIVE, &metrics, buffer_size, buffer, &matrix);
	if(err != GDI_ERROR)
	{
		TTPOLYGONHEADER* ph = (TTPOLYGONHEADER*)buffer;
		INT32 phpos = 0;

		path = new OpBpath();
		if(!path)
			return NULL;

		while(ph < (TTPOLYGONHEADER*)&buffer[err])
		{
			path->MoveTo(fixed_to_double(ph->pfxStart.x), fixed_to_double(ph->pfxStart.y), FALSE);
			
			INT32 pcpos = phpos + sizeof(TTPOLYGONHEADER);
			INT32 endpos = phpos + ph->cb;
			TTPOLYCURVE* pc = (TTPOLYCURVE*) &buffer[pcpos];
			while(pc < (TTPOLYCURVE*) &buffer[endpos])
			{
				if(pc->wType == TT_PRIM_LINE)
				{
					// straight line
					for(int i = 0; i < pc->cpfx; i++)
					{
						path->LineTo(fixed_to_double(pc->apfx[i].x), fixed_to_double(pc->apfx[i].y), FALSE);
					}
				}
				else if(pc->wType == TT_PRIM_QSPLINE)
				{
					// quadratic spline
					for(int i = 0; i < pc->cpfx-1; i++)
					{
						POINTFX* b = &pc->apfx[i];
						POINTFX* c = &pc->apfx[i+1];
						SVGCoordinatePair ep;

						if(i < pc->cpfx - 2)
						{
							ep.x = (fixed_to_double(b->x) + fixed_to_double(c->x)) / 2;
							ep.y = (fixed_to_double(b->y) + fixed_to_double(c->y)) / 2;
						}
						else
						{
							ep.x = fixed_to_double(c->x);
							ep.y = fixed_to_double(c->y);
						}

						path->QuadraticCurveTo(fixed_to_double(b->x), fixed_to_double(b->y),
												ep.x, ep.y, 
												FALSE, FALSE);
					}
				}
				else if(pc->wType == TT_PRIM_CSPLINE)
				{
					// cubic spline
					for(int i = 0; i < pc->cpfx-2; i++)
					{
						POINTFX* cp1 = &pc->apfx[i];
						POINTFX* cp2 = &pc->apfx[i+1];
						POINTFX* endp = &pc->apfx[i+2];
						path->CubicCurveTo(fixed_to_double(cp1->x), fixed_to_double(cp1->y), 
												fixed_to_double(cp2->x), fixed_to_double(cp2->y),
												fixed_to_double(endp->x), fixed_to_double(endp->y),
												FALSE, FALSE);
					}
				}
				
				pcpos += 2 * sizeof(WORD) + sizeof(POINTFX) * pc->cpfx;
				pc = (TTPOLYCURVE*) &buffer[pcpos];
			}

			path->Close();

			phpos += ph->cb;
			ph = (TTPOLYGONHEADER*)&buffer[phpos];
		}
	}
	
	if(oldGdiObj != NULL)
		SelectObject(fonthdc, oldGdiObj);

	return path;
}

SVGnumber WindowsSVGFont::GetGlyphAdvance(uni_char uc)
{
	DWORD err = 0;
	GLYPHMETRICS metrics;
	MAT2 matrix;
	
	memset(&matrix, 0, sizeof(matrix));
    matrix.eM11.value = 1;
    matrix.eM22.value = 1;

	WindowsOpFont* wopfont = (WindowsOpFont*)m_font;

	HDC fonthdc = WindowsOpFont::fonthdc;
    void * oldGdiObj = NULL;

	if (WindowsOpFont::font_on_dc != wopfont)
	{
		oldGdiObj = SelectObject(fonthdc, wopfont->fnt);
		WindowsOpFont::font_on_dc = wopfont;
	}

	err = ::GetGlyphOutline( fonthdc, uc, GGO_METRICS, &metrics, 0, NULL, &matrix);
	
	if(oldGdiObj != NULL)
		SelectObject(fonthdc, oldGdiObj);

	if(err != GDI_ERROR)
	{
		return (double) metrics.gmCellIncX;
	}

	return 0;
}

SVGnumber WindowsSVGFont::GetStringWidth(const uni_char* text, INT32 len)
{
	UINT32 realLen = 0;
	SVGnumber stringVisualLen = 0;
	
	if(!text)
	{
		return 0;
	}
	
	if(len < 0)
	{
		realLen = uni_strlen(text);
	}
	else
	{
		realLen = len;
	}
	
	if(realLen == 0)
		return 0;
	
	for(UINT32 i = 0; i < realLen; i++)
	{
		stringVisualLen += GetGlyphAdvance(text[i]);
	}
	
	return stringVisualLen;
}

BOOL WindowsSVGFont::HasGlyph(uni_char uc)
{
	return TRUE;
	//return FindGlyph(uc) != NULL;
}

void WindowsSVGFont::SetSize(SVGnumber size)
{
	m_size = size;
	SVGnumber upem = m_font->Height();
	SVGnumber scaledSize = upem ? size / upem : 1;
	// glyphs are upside down and should be scaled
	m_text_transform.LoadScale( scaledSize, -scaledSize );
}

void WindowsSVGFont::SetWeight(SVGFontWeight weight)
{
}

SVGTransform* WindowsSVGFont::GetTextTransform()
{
	return &m_text_transform;
}
#endif // SVG_SUPPORT
