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

#pragma once

#include "bandcodec.h"

namespace rududu {

class CWavelet2D{
public:
	CWavelet2D(int x, int y, int level, int Align = ALIGN);

    ~CWavelet2D();

	template <trans t> void Transform(short * pImage, int Stride);
	template <trans t> void TransformI(short * pImage, int Stride);
	template <bool forward> void DCT4(void);
	void SetWeight(trans t, float baseWeight = 1.);

	void DecodeBand(CMuxCodec * pCodec, int method);
	void CodeBand(CMuxCodec * pCodec, int method, short Quant, int lambda);

	template <bool use_dct> unsigned int TSUQ(short Quant, float Thres);
	template <bool use_dct> void TSUQi(short Quant);

	void Stats(void);

	CBandCodec DBand;
	CBandCodec HBand;
	CBandCodec VBand;
	CBandCodec LBand;
	CWavelet2D * pLow;
	CWavelet2D * pHigh;

private:

	int DimX;
	int DimY;

	CWavelet2D(int x, int y, int level, CWavelet2D * pHigh, int Align);
	void Init(int level, int Align);

	// TODO : tester si inline c'est mieux
	static void TransLine97(short * i, int len);
	static void TransLine97I(short * i, int len);
	void Transform97(short * pImage, int Stride);
	void Transform97I(short * pImage, int Stride);

	void Transform53H(short * pImage, int Stride);
	void Transform53HI(short * pImage, int Stride);
	void Transform53V(short * pImage, int Stride);
	void Transform53VI(short * pImage, int Stride);

	static void TransLineHaar(short * i, int len);
	static void TransLineHaarI(short * i, int len);
	void TransformHaar(short * pImage, int Stride);
	void TransformHaarI(short * pImage, int Stride);

	template <bool forward> void DCT4H(void);
	template <bool forward> void DCT4V(void);

	static inline short mult08(short a);
};

}
