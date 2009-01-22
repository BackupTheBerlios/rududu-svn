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

#include "utils.h"
#include "imagebuffer.h"
#include "obme.h"
#include "wavelet2d.h"
#include <iostream>

using namespace std;

namespace rududu {

class CRududuCodec{
public:
	int quant;

    CRududuCodec(cmode mode, int width, int height, int component);
	static CRududuCodec decoder(istream & s);

    ~CRududuCodec();

	int encode(unsigned char * pImage, int stride, unsigned char * pBuffer, CImage ** outImage);
	int decode(unsigned char * pBuffer, CImage ** outImage);
	void saveParams(ostream & s);

	static short quants(int idx);

	inline int width(void){ return predImage->dimX; }
	inline int height(void){ return predImage->dimY; }

private :
	CImageBuffer images;
	CImage * predImage;
	COBMC * obmc;
	CWavelet2D * wavelet;
	CMuxCodec codec;

	int key_count;

	void encodeImage(CImage * pImage);
	void decodeImage(CImage * pImage);

};

}

