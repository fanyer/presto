#include "core/pch.h"
#include "modules/libgogi/mde.h"

#ifdef MDE_SUPPORT_ROTATE

bool MDE_RotateBuffer(MDE_BUFFER *buf, double rad, double scale, unsigned int bg_col, bool antialias)
{
	// FIX: Need to store pal_len in buffer if we want indexed images to work.
	if (MDE_GetBytesPerPixel(buf->format) != 4)
		return false; // Only support 32 bit for now.

	scale = 2 - scale;
	MDE_BUFFER *dstbuf = buf;
	MDE_BUFFER *srcbuf = MDE_CreateBuffer(buf->w, buf->h, buf->format, 0);
	if (!dstbuf)
		return false;

	MDE_DrawBuffer(buf, MDE_MakeRect(0, 0, buf->w, buf->h), 0, 0, srcbuf);

	int center_x = buf->w / 2;
	int center_y = buf->h / 2;

	MDE_F1616 cosrs = (MDE_F1616) (cos(-rad) * scale * 65536);
	MDE_F1616 sinrs = (MDE_F1616) (sin(-rad) * scale * 65536);

	MDE_F1616 start_x = (center_x << 16) - (center_x * cosrs + center_y * sinrs);
	MDE_F1616 start_y = (center_y << 16) - (center_y * cosrs - center_x * sinrs);

	int x, y;
	MDE_F1616 tx, ty;
	int tempx, tempy;
	unsigned long *dst32 = (unsigned long *) dstbuf->data;
	unsigned long *src32 = (unsigned long *) srcbuf->data;

	if (!antialias)
	{
		// FIX: For both odd and even images?
		start_x += 32768;
		start_y += 32768;

		for (y = 0; y < dstbuf->h; y++)
		{
			tx = start_x;
			ty = start_y;
			for (x = 0; x < dstbuf->w; x++)
			{
				tempx = (tx >> 16);
				tempy = (ty >> 16);
				if (tempx >= 0 && tempx < buf->w && tempy >=0 && tempy < buf->h)
					dst32[x] = src32[tempx + tempy * buf->w];
				else
					dst32[x] = bg_col;

				tx += cosrs;
				ty -= sinrs;
			}
			dst32 += buf->w;
			start_x += sinrs;
			start_y += cosrs;
		}
	}
	else
	{
		int i, r, g, b, a;

		// Calculate lookup table for texel power.
		MDE_F1616 power_lut[16][16][4];
		for(y = 0; y < 16; y++)
			for(x = 0; x < 16; x++)
			{
				float fracU = (float)x / 16;
				float fracV = (float)y / 16;
				power_lut[x][y][0] = (MDE_F1616) (((1.0f - fracU) * (1.0f - fracV)) * 65536);
				power_lut[x][y][1] = (MDE_F1616) ((fracU * (1.0f - fracV)) * 65536);
				power_lut[x][y][2] = (MDE_F1616) (((1.0f - fracU) * fracV) * 65536);
				power_lut[x][y][3] = (MDE_F1616) ((fracU * fracV) * 65536);
			}

		for (y = 0; y < dstbuf->h; y++)
		{
			tx = start_x;
			ty = start_y;
			for (x = 0; x < dstbuf->w; x++)
			{
				tempx = (tx >> 16);
				tempy = (ty >> 16);
				int cx[4] = {	tempx, tempx + 1, tempx, tempx + 1 };
				int cy[4] = {	tempy, tempy, tempy + 1, tempy + 1 };

				// Calculate power depending of the current position (Slow, Best quality)
				//float fracU = (tx / 65536.0f) - tempx;
				//float fracV = (ty / 65536.0f) - tempy;
				//MDE_F1616 power[4] = {	(MDE_F1616) (((1.0f - fracU) * (1.0f - fracV)) * 65536),
				//						(MDE_F1616) ((fracU * (1.0f - fracV)) * 65536),
				//						(MDE_F1616) (((1.0f - fracU) * fracV) * 65536),
				//						(MDE_F1616) ((fracU * fracV) * 65536) };

				// Get power from lookup table (Faster, lower quality)
				MDE_F1616 *power = power_lut[(tx - (tempx << 16)) >> 12]
											[(ty - (tempy << 16)) >> 12];

				r = g = b = a = 0;
				for(i = 0; i < 4; i++)
				{
					if (cx[i] >= 0 && cx[i] < buf->w && cy[i] >=0 && cy[i] < buf->h)
					{
						unsigned long col = src32[cx[i] + cy[i] * buf->w];
						r += (MDE_COL_R(col) * power[i]) >> 16;
						g += (MDE_COL_G(col) * power[i]) >> 16;
						b += (MDE_COL_B(col) * power[i]) >> 16;
						a += (MDE_COL_A(col) * power[i]) >> 16;
					}
					/*else // eventual remove this..
					{
						r += (MDE_COL_R(bg_col) * power[i]) >> 16;
						g += (MDE_COL_G(bg_col) * power[i]) >> 16;
						b += (MDE_COL_B(bg_col) * power[i]) >> 16;
					}*/
				}
				dst32[x] = MDE_RGBA(r, g, b, a);

				tx += cosrs;
				ty -= sinrs;
			}
			dst32 += buf->w;
			start_x += sinrs;
			start_y += cosrs;
		}
	}

	MDE_DeleteBuffer(srcbuf);
	return true;
}

#endif
