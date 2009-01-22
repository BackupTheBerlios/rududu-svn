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

#include "imagebuffer.h"

#define ALIGN	32

namespace rududu {

CImageBuffer::CImageBuffer(unsigned int x, unsigned int y, int cmpnt, unsigned int size):
	images_left(size - 1)
{
	CImage * Img = new CImage(x, y, cmpnt, ALIGN);
	free_stack.push(Img);
}

CImageBuffer::~CImageBuffer()
{
	while (image_list.size() != 0)
		remove(image_list.size() - 1);
	while( free_stack.size() != 0 ){
		delete free_stack.top();
		free_stack.pop();
	}
}

CImage * CImageBuffer::getFree(void)
{
	if (free_stack.size() != 0) {
		CImage * ret = free_stack.top();
		free_stack.pop();
		return ret;
	}
	if (images_left > 0) {
		CImage * ex = 0;
		if (free_stack.size() != 0)
			ex = free_stack.top();
		else
			ex = image_list[0].sub[0];
		images_left--;
		return new CImage(ex, ALIGN);
	}
	return 0;
}

CImage ** CImageBuffer::insert(int index)
{
	sSubImage tmp;
	tmp.sub[0] = getFree();
	if (tmp.sub[0] != 0){
		for( int i = 1; i < SUB_IMAGE_CNT; i++) tmp.sub[i] = 0;
		image_list.insert(image_list.begin() + index, tmp);
		return (*this)[index];
	}
	return 0;
}

void CImageBuffer::remove(unsigned int index)
{
	if (index >= image_list.size()) return;
	for( int i = 0; i < SUB_IMAGE_CNT; i++) {
		if (image_list[index].sub[i] != 0)
			free_stack.push(image_list[index].sub[i]);
	}
	image_list.erase(image_list.begin() + index);
}

void CImageBuffer::interpolate(int idx)
{
	for( int i = 1; i < SUB_IMAGE_CNT; i++) {
		if (image_list[idx].sub[i] == 0)
			image_list[idx].sub[i] = getFree();
	}
	image_list[idx].sub[0]->inter_half_pxl(*image_list[idx].sub[2],
	                                       *image_list[idx].sub[1],
	                                       *image_list[idx].sub[3]);
}

}
