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

#include <image.h>

#include <vector>
#include <stack>

#define SUB_IMAGE_CNT	16

namespace rududu {

typedef struct  {
	CImage * sub[SUB_IMAGE_CNT];
} sSubImage;

class CImageBuffer{
public:
	CImageBuffer(unsigned int x, unsigned int y, int cmpnt, unsigned int size);

    ~CImageBuffer();

	CImage ** insert(int index);
	void remove(unsigned int index);
	CImage ** operator[](int index);
	void calc_sub(int index);

private:
	std::vector<sSubImage> image_list;
	std::stack<CImage*> free_stack;
	unsigned int images_left;

	CImage * getFree();
};

}
