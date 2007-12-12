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

#include "bitcodec.h"

namespace rududu {

CBitCodec::CBitCodec(CMuxCodec * RangeCodec)
{
	InitModel();
	setRange(RangeCodec);
}

CBitCodec::~CBitCodec()
{

}

const int CBitCodec::mod[][2] = {
	{101356, 0},
	{106680, -2560},
	{110641, -4498},
	{114166, -6605},
	{117591, -8893},
	{121006, -11377},
	{124494, -14073},
	{128079, -17002},
	{131886, -20199},
	{135892, -23688},
	{140179, -27512},
	{144782, -31714},
	{149803, -36360},
	{155298, -41515},
	{161378, -47272},
	{168113, -53727},
	{175651, -61016},
	{184155, -69306},
	{193910, -78843},
	{205141, -89893},
	{218293, -102874},
	{233893, -118322},
	{252718, -137016},
	{275918, -160101},
	{305231, -189320},
	{343531, -227533},
	{395640, -279586},
	{470821, -354728},
	{588789, -472690},
	{800532, -684513},
	{1032508, -940044},
	{1001686, -970865},
	{970865, -1001686},
	{940044, -1032508},
	{684513, -800532},
	{472690, -588789},
	{354728, -470821},
	{279586, -395640},
	{227533, -343531},
	{189320, -305231},
	{160101, -275918},
	{137016, -252718},
	{118322, -233893},
	{102874, -218293},
	{89893, -205141},
	{78843, -193910},
	{69306, -184155},
	{61016, -175651},
	{53727, -168113},
	{47272, -161378},
	{41515, -155298},
	{36360, -149803},
	{31714, -144782},
	{27512, -140179},
	{23688, -135892},
	{20199, -131886},
	{17002, -128079},
	{14073, -124494},
	{11377, -121006},
	{8893, -117591},
	{6605, -114166},
	{4498, -110641},
	{2560, -106680},
	{0, -101356}
};

void CBitCodec::InitModel(void){
	for (int i = 0; i < BIT_CONTEXT_NB; i++){
		Freq[i] = HALF_FREQ_COUNT << FREQ_POWER;
	}
}

}
