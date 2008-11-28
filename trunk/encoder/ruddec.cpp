/***************************************************************************
 *   Copyright (C) 2008 by Nicolas Botti                                   *
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

#include <fstream>
#include <SDL/SDL.h>
#include <unistd.h>

#include "rududucodec.h"

using namespace std;
using namespace rududu;

int main( int argc, char *argv[] )
{
	static const char magic[] = {'R', 'D', 'D', 'x'};
	ifstream infile(argv[1], ios_base::in | ios_base::binary);

	for( int i = 0; i < 4; i++){
		if (magic[i] != infile.get())
			return 1;
	}

	CRududuCodec decoder = CRududuCodec::decoder(infile);
	int w = decoder.width();
	int h = decoder.height();

	if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) {
		cerr << "Unable to init SDL: " << SDL_GetError() << endl;
		exit(1);
	}

	SDL_Surface * screen = SDL_SetVideoMode(w, h, 0, SDL_SWSURFACE | SDL_ASYNCBLIT | SDL_HWACCEL);
	if ( screen == NULL ) {
		cerr << "Unable to set video" << endl;
		exit(1);
	}

	SDL_WM_SetCaption("Rududu player", "media-player");

	SDL_Overlay * yuv_overlay = SDL_CreateYUVOverlay(w, h, SDL_IYUV_OVERLAY, screen);
	if ( yuv_overlay == NULL ) {
		cerr << "SDL: Couldn't create SDL_yuv_overlay: " << SDL_GetError() << endl;
		exit(1);
	}

	memset(yuv_overlay->pixels[1], 128, yuv_overlay->pitches[1] * h);
	memset(yuv_overlay->pixels[2], 128, yuv_overlay->pitches[2] * h);

	SDL_Rect rect;
	rect.x = 0;
	rect.y = 0;
	rect.w = w;
	rect.h = h;

	char * buff = new char[w * h];
	infile.read(buff, w * h);
	int buff_size = infile.gcount();
	do {
		CImage * outImage = 0;
		int size_dec = decoder.decode((unsigned char*)buff, &outImage);
		cerr << size_dec << endl;
		buff_size -= size_dec;
		memmove(buff, buff + size_dec, buff_size);
		infile.read(buff + buff_size, w * h - buff_size);
		buff_size += infile.gcount();

		if (SDL_LockYUVOverlay(yuv_overlay) < 0) continue;

		outImage->output(yuv_overlay->pixels[0], yuv_overlay->pitches[0]);

		SDL_UnlockYUVOverlay(yuv_overlay);
		SDL_DisplayYUVOverlay(yuv_overlay, &rect);
// 		usleep(200*1000);
	} while(buff_size > 0);

	SDL_Quit();
	infile.close();
	delete[] buff;

	return 0;
}
