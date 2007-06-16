/***************************************************************************
 *   Copyright (C) 2007 by Nicolas Botti   *
 *   rududu@laposte.net   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
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

#include "utils.h"
#include "band.h"

namespace rududu {

class CDCT2D{
public:
	CDCT2D(int x, int y, int Align = ALIGN);

	~CDCT2D();

	template <bool forward> void Transform(short * pImage, int stride);
	template <bool pre> void Proc(short * pImage, int stride);
	void SetWeight(trans t, float baseWeight = 1.);

	unsigned int TSUQ(short Quant, float Thres);
	void TSUQi(short Quant);

private:

	int DimX;
	int DimY;
	CBand DCTBand;

	static const float norm[8];

	static void DCT8_H(short * pBlock, int stride);
	static void iDCT8_H(short * pBlock, int stride);
	static void DCT8_V(short * pBlock, int stride);
	static void iDCT8_V(short * pBlock, int stride);

	template <bool pre> static void Proc_H(short * pBlock, int stride);
	template <bool pre> static void Proc_V(short * pBlock, int stride);
};

}

