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

CImage ** CImageBuffer::operator[](int index)
{
	return image_list[index].sub;
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

void CImageBuffer::calc_sub(int index)
{
	if (image_list[index].sub[4] == 0)
		image_list[index].sub[4] = getFree();
	image_list[index].sub[4]->interH<1>(*image_list[index].sub[0]);

	if (image_list[index].sub[8] == 0)
		image_list[index].sub[8] = getFree();
	image_list[index].sub[8]->interH<2>(*image_list[index].sub[0]);

	if (image_list[index].sub[12] == 0)
		image_list[index].sub[12] = getFree();
	image_list[index].sub[12]->interH<3>(*image_list[index].sub[0]);

	for( int i = 0; i < 16; i += 4){
		if (image_list[index].sub[i+1] == 0)
			image_list[index].sub[i+1] = getFree();
		image_list[index].sub[i+1]->interV<1>(*image_list[index].sub[i]);

		if (image_list[index].sub[i+2] == 0)
			image_list[index].sub[i+2] = getFree();
		image_list[index].sub[i+2]->interV<2>(*image_list[index].sub[i]);

		if (image_list[index].sub[i+3] == 0)
			image_list[index].sub[i+3] = getFree();
		image_list[index].sub[i+3]->interV<3>(*image_list[index].sub[i]);
	}

	for( int i = 0; i < 16; i++){
		image_list[index].sub[i]->extend();
	}
}

}
