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
	CWavelet2D(int x, int y, int level, int level_chg = 0, int Align = ALIGN);

    ~CWavelet2D();

	template <class C> void Transform(C * pImage, int Stride, trans t);
	template <class C> void TransformI(C * pImage, int Stride, trans t);
// 	template <bool forward> void DCT4(void);
	void SetWeight(trans t, float baseWeight = 1.);

	void DecodeBand(CMuxCodec * pCodec);
	void CodeBand(CMuxCodec * pCodec, int Quant, int lambda);

	unsigned int TSUQ(int Quant, float Thres);
	void TSUQi(int Quant);

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

	CWavelet2D(int x, int y, int level, int level_chg, CWavelet2D * pHigh, int Align);
	void Init(int level, int level_chg, int Align);

	// TODO : tester si inline c'est mieux
	template <class C> static void TransLine97(C * i, int len);
	template <class C> static void TransLine97I(C * i, int len);
	template <class C> void Transform97(C * pImage, int Stride);
	template <class C> void Transform97I(C * pImage, int Stride);

	template <class C> static void TransLine75(C * i, int len);
	template <class C> static void TransLine75I(C * i, int len);
	template <class C> void Transform75(C * pImage, int Stride);
	template <class C> void Transform75I(C * pImage, int Stride);

	template <class C> static void TransLine53(C * i, int len);
	template <class C> static void TransLine53I(C * i, int len);
	template <class C> void Transform53(C * pImage, int Stride);
	template <class C> void Transform53I(C * pImage, int Stride);

	template <class C> static void TransLineHaar(C * i, int len);
	template <class C> static void TransLineHaarI(C * i, int len);
	template <class C> void TransformHaar(C * pImage, int Stride);
	template <class C> void TransformHaarI(C * pImage, int Stride);

	/*
	template <bool forward> void DCT4H(void);
	template <bool forward> void DCT4V(void);
	*/

	template <class C> static inline C mult08(C a);
};

}
