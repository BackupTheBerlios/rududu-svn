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
#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>
#include <ffmpeg/swscale.h>

#include "rududucodec.h"

using namespace rududu;


int main ( int argc, char *argv[] )
{
	AVFormatContext *pFormatCtx;

	av_register_all();

	// Open video file
	if ( av_open_input_file ( &pFormatCtx, argv[1], 0, 0, 0 ) != 0 )
		return -1; // Couldn't open file

	// Retrieve stream information
	if ( av_find_stream_info ( pFormatCtx ) < 0 )
		return -1; // Couldn't find stream information

	// Dump information about file onto standard error
	// dump_format ( pFormatCtx, 0, argv[1], 0 );

	// Find the first video stream
	int videoStream = -1;
	for ( int i = 0; i < (int)pFormatCtx->nb_streams; i++ )
		if ( pFormatCtx->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO ) {
			videoStream = i;
			break;
		}

	if ( videoStream == -1 )
		return -1; // Didn't find a video stream

	AVCodecContext * pCodecCtx = pFormatCtx->streams[videoStream]->codec;
	AVCodec * pCodec = avcodec_find_decoder ( pCodecCtx->codec_id );
	if ( pCodec == 0 )	{
		cerr << "Unsupported codec!" << endl;
		return -1; // Codec not found
	}

	if ( avcodec_open ( pCodecCtx, pCodec ) < 0 )
		return -1; // Could not open codec

	AVFrame * pFrame = avcodec_alloc_frame();
	int w = pCodecCtx->width, h = pCodecCtx->height;
	AVPacket packet;

	CRududuCodec encoder(rududu::encode, w, h, 1);
	encoder.quant = 15;
	unsigned char * pStream = new unsigned char[w * h];
	ofstream outfile(argv[2], ios_base::out | ios_base::binary | ios_base::trunc);
	outfile << "RDDx";

	encoder.saveParams(outfile);

	while ( av_read_frame ( pFormatCtx, &packet ) >= 0 ) {
		int frameFinished = 0;
		if ( packet.stream_index == videoStream ) {
			avcodec_decode_video ( pCodecCtx, pFrame, &frameFinished,
			                       packet.data, packet.size );

			if ( frameFinished ) {
				// encode it !
				int size_enc = encoder.encode(pFrame->data[0], pFrame->linesize[0], pStream, 0);
				cerr << size_enc << endl;
				outfile.write((char *)pStream, size_enc);
			}
		}
		av_free_packet ( &packet );
	}

	outfile.close();

	delete[] pStream;

	av_free ( pFrame );
	avcodec_close ( pCodecCtx );
	av_close_input_file ( pFormatCtx );

	return 0;
}
