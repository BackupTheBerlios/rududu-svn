/***************************************************************************
 *   Copyright (C) 2007 by Nicolas Botti                                   *
 *   rududu@laposte.net                                                    *
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

namespace rududu {

typedef enum cspace {rvb, yuv, yog, yv12, i420};

#define BORDER	15
#define MAX_COMPONENT	3

class CImage{
	friend class COBMC;
public:
	CImage(unsigned int x, unsigned int y, int cmpnt, int Align);

    ~CImage();

	void Init( unsigned int x, unsigned int y, int cmpnt, int Align);

	template <class input_t> void inputRGB(input_t * pIn, int stride, short offset);
	template <class input_t> void inputSGI(input_t * pIn, int stride, short offset);
	template <class output_t> void outputRGB(output_t * pOut, int stride, short offset);
	template <class output_t, bool i420> void outputYV12(output_t * pOut, int stride, short offset);

	CImage & operator-= (const CImage & In);

	template <int pos> void interH(const CImage & In);
	template <int pos> void interV(const CImage & In);

	void extend(void);

	int get_dimXAlign(){ return dimXAlign; }
	short * get_pImage(int cmp){ return pImage[cmp]; }

private:
	unsigned int dimX;
	unsigned int dimY;
	unsigned int dimXAlign;
	int component;
	cspace colorspace;

	char * pData;
	short * pImage[MAX_COMPONENT];
};

}

