/***************************************************************************
 *   Copyright (C) 2007-2008 by Nicolas Botti                              *
 *   <rududu@laposte.net>                                                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <stdint.h>
#include <string.h>
#include <math.h>

#include "image.h"
#include "utils.h"

using namespace std;

namespace rududu {

CImage::CImage(unsigned int x, unsigned int y, int cmpnt, int Align):
		pData(0)
{
	dimX = x;
	dimY = y;
	component = cmpnt;
	Init(Align);
}

CImage::CImage(CImage * pImg, int Align):
	pData(0)
{
	dimX = pImg->dimX;
	dimY = pImg->dimY;
	component = pImg->component;
	Init(Align);
}


CImage::~CImage()
{
	delete[] pData;
}

void CImage::Init(int Align)
{
	dimXAlign = ( dimX + 2 * BORDER + Align - 1 ) & ( -Align );
	if ( (dimXAlign * dimY) != 0){
		pData = new char[(dimXAlign * (dimY + 2 * BORDER)) * sizeof(short) * component + Align];
		pImage[0] = (short*)(((intptr_t)pData + Align - 1) & (-Align));
		pImage[0] += BORDER * dimXAlign + BORDER;
		for( int i = 1; i < component; i++){
			// FIXME = pImage should be aligned too
			pImage[i] = pImage[i - 1] + (dimXAlign * (dimY + 2 * BORDER));
		}
	}
}

void CImage::input(unsigned char * pIn, int stride)
{
	for( int c = 0; c < component; c++){
		short * pCur = pImage[c];
		for( unsigned int j = 0; j < dimY; j++){
			for (unsigned int i = 0; i < dimX; i++){
				pCur[i] = ((pIn[i] << 4) | (pIn[i] >> 4)) - 2048;
			}
			pCur += dimXAlign;
			pIn += stride;
		}
	}
}

void CImage::output(unsigned char * pOut, int stride)
{
	for( int c = 0; c < component; c++){
		short * pCur = pImage[c];
		for( unsigned int j = 0; j < dimY; j++){
			for (unsigned int i = 0; i < dimX; i++){
				pOut[i] = clip((pCur[i] + 2056) >> 4, 0, 255);
			}
			pCur += dimXAlign;
			pOut += stride;
		}
	}
}

template <class input_t>
void CImage::inputRGB(input_t * pIn, int stride, short offset)
{
	short * Y = pImage[0], * Co = pImage[1], * Cg = pImage[2];

	for( unsigned int j = 0; j < dimY; j++){
		for (unsigned int i = 0, k = 0; i < dimX; i++ , k += 3){
			Co[i] = pIn[k] - pIn[k + 2];
			Y[i] = pIn[k + 2] + (Co[i] >> 1);
			Cg[i] = pIn[k + 1] - Y[i];
			Y[i] += (Cg[i] >> 1) + offset;
			if (sizeof(input_t) == 1) {
				Y[i] <<= 4;
				Co[i] <<= 3;
				Cg[i] <<= 3;
			}
		}
		Y += dimXAlign;
		Co += dimXAlign;
		Cg += dimXAlign;
		pIn += stride * 3;
	}
}

template void CImage::inputRGB<char>(char*, int, short);

template <class input_t>
void CImage::inputSGI(input_t * pIn, int stride, short offset)
{
	short * Y = pImage[0], * Co = pImage[1], * Cg = pImage[2];
	input_t * R = pIn + stride * dimY;
	input_t * G = R + stride * dimY;
	input_t * B = G + stride * dimY;

	for( unsigned int j = 0; j < dimY; j++){
		R -= stride;
		G -= stride;
		B -= stride;
		for (unsigned int i = 0; i < dimX; i++){
			Co[i] = R[i] - B[i];
			Y[i] = B[i] + (Co[i] >> 1);
			Cg[i] = G[i] - Y[i];
			Y[i] += (Cg[i] >> 1) + offset;
			if (sizeof(input_t) == 1) {
				Y[i] <<= 4;
				Co[i] <<= 3;
				Cg[i] <<= 3;
			}
		}
		Y += dimXAlign;
		Co += dimXAlign;
		Cg += dimXAlign;
	}
}

template void CImage::inputSGI<unsigned char>(unsigned char*, int, short);

template <class output_t>
void CImage::outputRGB(output_t * pOut, int stride, short offset)
{
	short * Y = pImage[0], * Co = pImage[1], * Cg = pImage[2];

	for( unsigned int j = 0; j < dimY; j++){
		for (unsigned int i = 0, k = 0; i < dimX; i++ , k += 3){
			pOut[k + 2] = Y[i] - (Cg[i] >> 1) - offset;
			pOut[k + 1] = Cg[i] + pOut[k + 2];
			pOut[k + 2] -= Co[i] >> 1;
			pOut[k] = Co[i] + pOut[k + 2];
		}
		Y += dimXAlign;
		Co += dimXAlign;
		Cg += dimXAlign;
		pOut += stride;
	}
}

template void CImage::outputRGB<char>(char*, int, short);

template <class output_t, bool i420>
void CImage::outputYV12(output_t * pOut, int stride, short offset)
{
	short * Y = pImage[0], * Co = pImage[1], * Cg = pImage[2];
	output_t * Yo = pOut;
	output_t * Vo = Yo + stride * dimY;
	output_t * Uo = Vo + ((stride * dimY) >> 2);

	const int shift = 12 - sizeof(output_t) * 8;
	if (sizeof(output_t) == 1) offset <<= 4;
	else if (sizeof(output_t) == 2) offset >>= 4;

	if (i420) {
		output_t * tmp = Uo;
		Uo = Vo;
		Vo = tmp;
	}

	for( unsigned int j = 0; j < dimY; j += 2){
		for (unsigned int i = 0; i < dimX; i += 2){
			Yo[i] = ((440 * (Y[i] - offset) + 82 * Co[i] + 76 * Cg[i] + (1 << (8 + shift))) >> (9 + shift)) + 16;
			Yo[i+1] = ((440 * (Y[i+1] - offset) + 82 * Co[i+1] + 76 * Cg[i+1] + (1 << (8 + shift))) >> (9 + shift)) + 16;
			Yo[i+stride] = ((440 * (Y[i+dimXAlign] - offset) + 82 * Co[i+dimXAlign] + 76 * Cg[i+dimXAlign] + (1 << (8 + shift))) >> (9 + shift)) + 16;
			Yo[i+stride+1] = ((440 * (Y[i+dimXAlign+1] - offset) + 82 * Co[i+dimXAlign+1] + 76 * Cg[i+dimXAlign+1] + (1 << (8 + shift))) >> (9 + shift)) + 16;

			Uo[i>>1] = ((- 150 * (Co[i] + Co[i+1] + Co[i+dimXAlign] + Co[i+dimXAlign+1])
			             - 148 * (Cg[i] + Cg[i+1] + Cg[i+dimXAlign] + Cg[i+dimXAlign+1]) + (1 << (9 + shift))) >> (10 + shift)) + 128;
			Vo[i>>1] = ((130 * (Co[i] + Co[i+1] + Co[i+dimXAlign] + Co[i+dimXAlign+1])
			             - 188 * (Cg[i] + Cg[i+1] + Cg[i+dimXAlign] + Cg[i+dimXAlign+1]) + (1 << (9 + shift))) >> (10 + shift)) + 128;
		}
		Y += dimXAlign * 2;
		Co += dimXAlign * 2;
		Cg += dimXAlign * 2;
		Yo += stride * 2;
		Vo += stride >> 1;
		Uo += stride >> 1;
	}
}

template void CImage::outputYV12<char, false>(char*, int, short);
template void CImage::outputYV12<char, true>(char*, int, short);

static inline void extend_line(short * line, int len)
{
	short * s1 = line - BORDER, * s2 = line + len;
	for( int i = 0; i < BORDER; i++) {
		s1[i] = s1[BORDER];
		s2[i] = s2[-1];
	}
}

static void inline extend_top(short * line, int stride)
{
	for( short *dest = line - BORDER, *end = line - BORDER * stride;
	     dest > end; dest -= stride)
		memcpy(dest - stride, dest, stride * sizeof(short));
}

static void inline extend_bottom(short * line, int stride)
{
	for( short *dest = line - BORDER, *end = line + (BORDER - 1) * stride;
	     dest < end; dest += stride)
		memcpy(dest + stride, dest, stride * sizeof(short));
}

void CImage::extend(void)
{
	for( int c = 0; c < component; c++) {
		for( short *line = pImage[c], *end = pImage[c] + dimXAlign * dimY; line < end; line += dimXAlign)
			extend_line(line, dimX);

		extend_top(pImage[c], dimXAlign);
		extend_bottom(pImage[c] + dimXAlign * (dimY - 1), dimXAlign);
	}
}

CImage & CImage::operator-= (const CImage & In)
{
	for (int c = 0; c < component; c++) {
		short * out = pImage[c];
		short * in = In.pImage[c];
		for (unsigned int j = 0; j < dimY; j++) {
			for (unsigned int i = 0; i < dimX; i++) {
				out[i] -= in[i];
			}
			out += dimXAlign;
			in += In.dimXAlign;
		}
	}
	return *this;
}

CImage & CImage::operator+= (const CImage & In)
{
	for (int c = 0; c < component; c++) {
		short * out = pImage[c];
		short * in = In.pImage[c];
		for (unsigned int j = 0; j < dimY; j++) {
			for (unsigned int i = 0; i < dimX; i++) {
				out[i] += in[i];
			}
			out += dimXAlign;
			in += In.dimXAlign;
		}
	}
	return *this;
}

#ifdef __MMX__

float CImage::psnr(const CImage & In, int c)
{
	short * out = pImage[c];
	short * in = In.pImage[c];
	long long sum = 0;
	mmx_t zero;
	zero.v = __builtin_ia32_pxor(zero.v, zero.v);
	for (unsigned int j = 0; j < dimY; j++) {
		mmx_t *v1 = (mmx_t*) in, *v2 = (mmx_t*) out;
		for (unsigned int i = 0; i < (dimX >> 2); i++) {
			mmx_t tmp, r;
			tmp.v = __builtin_ia32_psubsw(v1[i].v, v2[i].v);
			tmp.v = __builtin_ia32_pmaddwd(tmp.v, tmp.v);
			r.q = __builtin_ia32_psrlq(tmp.q, 32);
			tmp.v = __builtin_ia32_paddd(tmp.v, r.v);
			sum += tmp.ud[1];
		}
		out += dimXAlign;
		in += In.dimXAlign;
	}
	return (float)(10. * (log(1 << (12 * 2)) - log((double)sum / (dimX * dimY))) / log(10.));
}

#else

float CImage::psnr(const CImage & In, int c)
{

	short * out = pImage[c];
	short * in = In.pImage[c];
	long long sum = 0;
	for (unsigned int j = 0; j < dimY; j++) {
		for (unsigned int i = 0; i < dimX; i++) {
			int tmp = in[i] - out[i];
			tmp *= tmp;
			sum += tmp;
		}
		out += dimXAlign;
		in += In.dimXAlign;
	}
	return (float)(10. * (log(1 << (12 * 2)) - log((double)sum / (dimX * dimY))) / log(10.));
}

#endif

float CImage::psnr(const unsigned char * in, int stride, int c)
{

	short * out = pImage[c];
	long long sum = 0;
	for (unsigned int j = 0; j < dimY; j++) {
		for (unsigned int i = 0; i < dimX; i++) {
			int tmp = ((in[i] << 4) | (in[i] >> 4)) - 2048;
			tmp -= out[i];
			tmp *= tmp;
			sum += tmp;
		}
		out += dimXAlign;
		in += stride;
	}
	return (float)(10. * (log(1 << (12 * 2)) - log((double)sum / (dimX * dimY))) / log(10.));
}

void CImage::copy(const CImage & In)
{
	for (int c = 0; c < component; c++) {
		short * out = pImage[c];
		short * in = In.pImage[c];
		for (unsigned int j = 0; j < dimY; j++) {
			memcpy(out, in, sizeof(short) * dimX);
			out += dimXAlign;
			in += In.dimXAlign;
		}
	}
}

void CImage::clear(void)
{
	for (int c = 0; c < component; c++) {
		short * out = pImage[c] - BORDER * (1 + dimXAlign);
		memset(out, 0, sizeof(short) * dimXAlign * (dimY + BORDER * 2));
	}
}

#define HALF_5_1

static inline void h_inter_line(const short * in, short * out, int len)
{
	for (int i = 0; i < len; i++)
#ifdef HALF_9_1
		out[i] = (((int)in[i] + in[i + 1]) * 9 - in[i - 1] - in[i + 2] + 8) >> 4;
#endif
#ifdef HALF_5_1
		out[i] = ((in[i] + in[i + 1]) * 5 - in[i - 1] - in[i + 2] + 4) >> 3;
#endif
	extend_line(out, len);

}

static inline void v_inter_line(const short * in, short * out, int len, int stride)
{
	const short * in_v[4] = {in - stride, in, in + stride, in + 2 * stride};

	for (int i = 0; i < len; i++)
#ifdef HALF_9_1
		out[i] = (((int)in_v[1][i] + in_v[2][i]) * 9 - in_v[0][i] - in_v[3][i] + 8) >> 4;
#endif
#ifdef HALF_5_1
		out[i] = ((in_v[1][i] + in_v[2][i]) * 5 - in_v[0][i] - in_v[3][i] + 4) >> 3;
#endif
	extend_line(out, len);
}

void const CImage::inter_half_pxl(CImage & out_h, CImage & out_v, CImage & out_hv)
{
	int c = 0;
	const short * in_line = pImage[c];
	short * h_line = out_h.pImage[c], * v_line = out_v.pImage[c],
		* hv_line = out_hv.pImage[c];

	// interpolation première ligne out_h
	h_inter_line(in_line, h_line, dimX);
	extend_top(h_line, dimXAlign);

	// processing de n - 1 lignes horizontales (n = 1/2 longueur du filtre)
	in_line += dimXAlign;
	h_line += dimXAlign;
	h_inter_line(in_line, h_line, dimX);

	for( unsigned int i = 2; i < dimY; i++){
		in_line += dimXAlign;
		h_line += dimXAlign;
		h_inter_line(in_line, h_line, dimX);
		v_inter_line(in_line - 2 * dimXAlign, v_line, dimX, dimXAlign);
		v_inter_line(h_line - 2 * dimXAlign, hv_line, dimX, dimXAlign);
		v_line += dimXAlign;
		hv_line += dimXAlign;
	}

	extend_bottom(h_line, dimXAlign);

	v_inter_line(in_line - dimXAlign, v_line, dimX, dimXAlign);
	v_inter_line(h_line - dimXAlign, hv_line, dimX, dimXAlign);
	v_line += dimXAlign;
	hv_line += dimXAlign;
	v_inter_line(in_line, v_line, dimX, dimXAlign);
	v_inter_line(h_line, hv_line, dimX, dimXAlign);

	extend_bottom(v_line, dimXAlign);
	extend_bottom(hv_line, dimXAlign);
	extend_top(out_v.pImage[c], dimXAlign);
	extend_top(out_hv.pImage[c], dimXAlign);
}

void CImage::write_pgm(ostream & os)
{
	os << "P5 " << dimX << " " << dimY << " 255" << endl;

	short * pCur = pImage[0];

	for( unsigned int j = 0; j < dimY; j++){
		for( unsigned int i = 0; i < dimX; i++){
			os.put(clip((pCur[i] + 2056) >> 4, 0, 255));
		}
		pCur += dimXAlign;
	}
}

}
