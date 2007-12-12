
#include <iostream>

#include "wavelet2d.h"

using namespace std;

namespace rududu {

#define XI		1.149604398f
#define ALPHA	(-3.f/2.f)
#define BETA	(-1.f/16.f)
#define GAMMA	(4.f/5.f)
#define DELTA	(15.f/32.f)

#define SQRT2	1.414213562f

CWavelet2D::CWavelet2D(int x, int y, int level, int Align):
	pLow(0),
	pHigh(0),
	DimX(x),
	DimY(y)
{
	Init(level, Align);
}

CWavelet2D::CWavelet2D(int x, int y, int level, CWavelet2D * pHigh, int Align):
	pLow(0),
	pHigh(0),
	DimX(x),
	DimY(y)
{
	this->pHigh = pHigh;
	pHigh->DBand.pParent = &DBand;
	DBand.pChild = &pHigh->DBand;
	pHigh->HBand.pParent = &HBand;
	HBand.pChild = &pHigh->HBand;
	pHigh->VBand.pParent = &VBand;
	VBand.pChild = &pHigh->VBand;

	Init(level, Align);
}

CWavelet2D::~CWavelet2D()
{
	delete pLow;
}

void CWavelet2D::Init(int level, int Align){
	DBand.Init(DimX >> 1, DimY >> 1, Align);
	VBand.Init(DimX >> 1, (DimY + 1) >> 1, Align);
	HBand.Init((DimX + 1) >> 1, DimY >> 1, Align);
	if (level > 1){
		pLow = new CWavelet2D((DimX + 1) >> 1, (DimY + 1) >> 1, level - 1, this, Align);
	}else{
		LBand.Init((DimX + 1) >> 1, (DimY + 1) >> 1, Align);
	}
}

void CWavelet2D::CodeBand(CMuxCodec * pCodec, int method)
{
	CWavelet2D * pCurWav = this;

	switch( method ){
		case 0 :
// 			while( pCurWav != 0 ){
// 				pCurWav->VBand.enu<encode>(pCodec);
// 				pCurWav->HBand.enu<encode>(pCodec);
// 				pCurWav->DBand.enu<encode>(pCodec);
// 				if (pCurWav->pLow == 0){
// 					pCurWav->LBand.pred<encode>(pCodec);
// 				}
// 				pCurWav = pCurWav->pLow;
// 			}
			break;
		case 1 :
			pCurWav->VBand.buildTree<true>();
			pCurWav->HBand.buildTree<true>();
			pCurWav->DBand.buildTree<true>();
			while( pCurWav->pLow ) pCurWav = pCurWav->pLow;
			pCurWav->LBand.pred<encode>(pCodec);
			while( pCurWav ) {
				pCurWav->VBand.tree<encode>(pCodec);
				pCurWav->HBand.tree<encode>(pCodec);
				pCurWav->DBand.tree<encode>(pCodec);
				pCurWav = pCurWav->pHigh;
			}
	}
}

void CWavelet2D::DecodeBand(CMuxCodec * pCodec, int method)
{
	CWavelet2D * pCurWav = this;

	switch( method ){
		case 0 :
// 			while( pCurWav != 0 ){
// 				pCurWav->VBand.enu<decode>(pCodec);
// 				pCurWav->HBand.enu<decode>(pCodec);
// 				pCurWav->DBand.enu<decode>(pCodec);
// 				if (pCurWav->pLow == 0)
// 					pCurWav->LBand.pred<decode>(pCodec);
// 				pCurWav = pCurWav->pLow;
// 			}
			break;
		case 1 :
			while( pCurWav->pLow ) pCurWav = pCurWav->pLow;
			pCurWav->LBand.pred<decode>(pCodec);
			while( pCurWav ) {
				pCurWav->VBand.tree<decode>(pCodec);
				pCurWav->HBand.tree<decode>(pCodec);
				pCurWav->DBand.tree<decode>(pCodec);
				pCurWav = pCurWav->pHigh;
			}
	}
}

unsigned int CWavelet2D::TSUQ(short Quant, float Thres)
{
	unsigned int Count = 0;
	Count += DBand.TSUQ(Quant, Thres);
	Count += HBand.TSUQ(Quant, Thres);
	Count += VBand.TSUQ(Quant, Thres);

	if (pLow != 0) {
		Count += pLow->TSUQ(Quant, Thres);
	} else {
		Count += LBand.TSUQ(Quant, 0.5f);
	}
	return Count;
}

void CWavelet2D::TSUQi(short Quant)
{
	DBand.TSUQi(Quant);
	HBand.TSUQi(Quant);
	VBand.TSUQi(Quant);

	if (pLow != 0){
		pLow->TSUQi(Quant);
	} else {
		LBand.TSUQi(Quant);
	}
}

#define PRINT_STAT(band) \
	cout << band << " :\t"; \
	/*cout << "Moyenne : " << Mean << endl;*/ \
	cout << Var << endl;

void CWavelet2D::Stats(void)
{
	float Mean = 0, Var = 0;
	DBand.Mean(Mean, Var);
	PRINT_STAT("D");
	HBand.Mean(Mean, Var);
	PRINT_STAT("H");
	VBand.Mean(Mean, Var);
	PRINT_STAT("V");

	if (pLow != 0)
		pLow->Stats();
	else{
		LBand.Mean(Mean, Var);
		PRINT_STAT("L");
	}
}

inline short CWavelet2D::mult08(short a)
{
	a -= a >> 2;
	a += a >> 4;
	return a + (a >> 8);
}

void CWavelet2D::TransLine97H(short * i, int len)
{
	short * iend = i + len - 5;

	short tmp = i[0] + i[2];
	i[1] -= tmp + (tmp >> 1);
	i[0] -= i[1] >> 3;

	tmp = i[2] + i[4];
	i[3] -= tmp + (tmp >> 1);
	i[2] -= (i[1] + i[3]) >> 4;
	i[1] += mult08(i[0] + i[2]);
	i[0] += i[1] - (i[1] >> 4);

	i++;

	for( ; i < iend; i += 2) {
		tmp = i[3] + i[5];
		i[4] -= tmp + (tmp >> 1);
		i[3] -= (i[2] + i[4]) >> 4;
		i[2] += mult08(i[1] + i[3]);
		tmp = i[0] + i[2];
		i[1] += (tmp >> 1) - (tmp >> 5);
	}

	i[4] -= i[3] * 2 + i[3];
	i[3] -= (i[2] + i[4]) >> 4;
	i[2] += mult08(i[1] + i[3]);
	tmp = i[0] + i[2];
	i[1] += (tmp >> 1) - (tmp >> 5);

	i[4] += 2 * mult08(i[3]);
	tmp = i[2] + i[4];
	i[3] += (tmp >> 1) - (tmp >> 5);
}

void CWavelet2D::TransLine97HI(short * i, int len)
{
	short * iend = i + len - 5;

	i[0] -= i[1] - (i[1] >> 4);

	short tmp = i[1] + i[3];
	i[2] -= (tmp >> 1) - (tmp >> 5);
	i[1] -= mult08(i[0] + i[2]);
	i[0] += i[1] >> 3;

	for( ; i < iend; i += 2) {
		tmp = i[3] + i[5];
		i[4] -= (tmp >> 1) - (tmp >> 5);
		i[3] -= mult08(i[2] + i[4]);
		i[2] += (i[1] + i[3]) >> 4;
		tmp = i[0] + i[2];
		i[1] += tmp + (tmp >> 1);
	}

	i[3] -= 2 * mult08(i[2]);
	i[2] += (i[1] + i[3]) >> 4;
	tmp = i[0] + i[2];
	i[1] += tmp + (tmp >> 1);

	i[3] += i[2] * 2 + i[2];
}

void CWavelet2D::Transform97(short * pImage, int Stride)
{
	short * i[6];
	i[0] = pImage;
	for( int j = 1; j < 6; j++)
		i[j] = i[j-1] + Stride;

	for( int j = 0; j < 5; j++)
		TransLine97H(i[j], DimX);

	for(int k = 0 ; k < DimX; k++) {
		short tmp = i[0][k] + i[2][k];
		i[1][k] -= tmp + (tmp >> 1);
		i[0][k] -= i[1][k] >> 3;

		tmp = i[2][k] + i[4][k];
		i[3][k] -= tmp + (tmp >> 1);
		i[2][k] -= (i[1][k] + i[3][k]) >> 4;
		i[1][k] += mult08(i[0][k] + i[2][k]);
		i[0][k] += i[1][k] - (i[1][k] >> 4);
	}

	for( int j = 0; j < 6; j++)
		i[j] += Stride;

	for( int j = 6; j < DimY; j += 2 ) {

		TransLine97H(i[4], DimX);
		TransLine97H(i[5], DimX);

		for(int k = 0 ; k < DimX; k++) {
			short tmp = i[3][k] + i[5][k];
			i[4][k] -= tmp + (tmp >> 1);
			i[3][k] -= (i[2][k] + i[4][k]) >> 4;
			i[2][k] += mult08(i[1][k] + i[3][k]);
			tmp = i[0][k] + i[2][k];
			i[1][k] += (tmp >> 1) - (tmp >> 5);
		}

		for( int k = 0; k < 6; k++)
			i[k] += 2 * Stride;
	}

	TransLine97H(i[4], DimX);

	for(int k = 0 ; k < DimX; k++) {
		i[4][k] -= i[3][k] * 2 + i[3][k];
		i[3][k] -= (i[2][k] + i[4][k]) >> 4;
		i[2][k] += mult08(i[1][k] + i[3][k]);
		short tmp = i[0][k] + i[2][k];
		i[1][k] += (tmp >> 1) - (tmp >> 5);

		i[4][k] += 2 * mult08(i[3][k]);
		tmp = i[2][k] + i[4][k];
		i[3][k] += (tmp >> 1) - (tmp >> 5);
	}
}

void CWavelet2D::Transform97I(short * pImage, int Stride)
{
	short * i[6];
	i[0] = pImage;
	for( int j = 1; j < 6; j++)
		i[j] = i[j-1] + Stride;

	for(int k = 0 ; k < DimX; k++) {
		i[0][k] -= i[1][k] - (i[1][k] >> 4);

		short tmp = i[1][k] + i[3][k];
		i[2][k] -= (tmp >> 1) - (tmp >> 5);
		i[1][k] -= mult08(i[0][k] + i[2][k]);
		i[0][k] += i[1][k] >> 3;
	}

	for( int j = 5; j < DimY; j += 2 ){
		for(int k = 0 ; k < DimX; k++) {
			short tmp = i[3][k] + i[5][k];
			i[4][k] -= (tmp >> 1) - (tmp >> 5);
			i[3][k] -= mult08(i[2][k] + i[4][k]);
			i[2][k] += (i[1][k] + i[3][k]) >> 4;
			tmp = i[0][k] + i[2][k];
			i[1][k] += tmp + (tmp >> 1);
		}

		TransLine97HI(i[0], DimX);
		TransLine97HI(i[1], DimX);

		for( int k = 0; k < 6; k++)
			i[k] += 2 * Stride;
	}

	for(int k = 0 ; k < DimX; k++) {
		i[3][k] -= 2 * mult08(i[2][k]);
		i[2][k] += (i[1][k] + i[3][k]) >> 4;
		short tmp = i[0][k] + i[2][k];
		i[1][k] += tmp + (tmp >> 1);

		i[3][k] += i[2][k] * 2 + i[2][k];
	}

	TransLine97HI(i[0], DimX);
	TransLine97HI(i[1], DimX);
	TransLine97HI(i[2], DimX);
	TransLine97HI(i[3], DimX);
}

void CWavelet2D::Transform53H(short * pImage, int Stride)
{
	for( int j = 0; j < DimY; j++){
		short * i = pImage;
		short * iend = pImage + DimX - 3;

		i[1] -= (i[0] + i[2]) >> 1;
		i[0] += i[1] >> 1;

		i++;

		for( ; i < iend; i += 2) {
			i[2] -= (i[1] + i[3]) >> 1;
			i[1] += (i[0] + i[2]) >> 2;
		}

		i[2] -= i[1];
		i[1] += (i[0] + i[2]) >> 2;

		pImage += Stride;
	}
}

void CWavelet2D::Transform53HI(short * pImage, int Stride)
{
	for( int j = 0; j < DimY; j++){
		short * i = pImage;
		short * iend = pImage + DimX - 3;

		i[0] -= i[1] >> 1;

		for( ; i < iend; i += 2) {
			i[2] -= (i[1] + i[3]) >> 2;
			i[1] += (i[0] + i[2]) >> 1;
		}

		i[1] += i[0];

		pImage += Stride;
	}
}

void CWavelet2D::Transform53V(short * pImage, int Stride)
{
	short * i[4];
	i[0] = pImage;
	for( int j = 1; j < 4; j++)
		i[j] = i[j-1] + Stride;

	for(int k = 0 ; k < DimX; k++) {
		i[1][k] -= (i[0][k] + i[2][k]) >> 1;
		i[0][k] += i[1][k] >> 1;
	}

	for( int j = 0; j < 4; j++)
		i[j] += Stride;

	for( int j = 4; j < DimY; j += 2 ) {
		for(int k = 0 ; k < DimX; k++) {
			i[2][k] -= (i[1][k] + i[3][k]) >> 1;
			i[1][k] += (i[0][k] + i[2][k]) >> 2;
		}

		for( int k = 0; k < 4; k++)
			i[k] += 2 * Stride;
	}

	for(int k = 0 ; k < DimX; k++) {
		i[2][k] -= i[1][k];
		i[1][k] += (i[0][k] + i[2][k]) >> 2;
	}
}

void CWavelet2D::Transform53VI(short * pImage, int Stride)
{
	short * i[4];
	i[0] = pImage;
	for( int j = 1; j < 4; j++)
		i[j] = i[j-1] + Stride;

	for(int k = 0 ; k < DimX; k++) {
		i[0][k] -= i[1][k] >> 1;
	}

	for( int j = 3; j < DimY; j += 2 ){
		for(int k = 0 ; k < DimX; k++) {
			i[2][k] -= (i[1][k] + i[3][k]) >> 2;
			i[1][k] += (i[0][k] + i[2][k]) >> 1;
		}

		for( int k = 0; k < 4; k++)
			i[k] += 2 * Stride;
	}

	for(int k = 0 ; k < DimX; k++) {
		i[1][k] += i[0][k];
	}
}

void CWavelet2D::TransformHaarH(short * pImage, int Stride)
{
	for( int j = 0; j < DimY; j++){
		short * i = pImage;
		short * iend = pImage + DimX - 1;

		for( ; i < iend; i += 2) {
			i[1] -= i[0];
			i[0] += i[1] >> 1;
		}

		pImage += Stride;
	}
}

void CWavelet2D::TransformHaarHI(short * pImage, int Stride)
{
	for( int j = 0; j < DimY; j++){
		short * i = pImage;
		short * iend = pImage + DimX - 1;

		for( ; i < iend; i += 2) {
			i[0] -= i[1] >> 1;
			i[1] += i[0];
		}

		pImage += Stride;
	}
}

void CWavelet2D::TransformHaarV(short * pImage, int Stride)
{
	short * i[2];
	i[0] = pImage;
	i[1] = i[0] + Stride;

	for( int j = 0; j < DimY; j += 2 ) {
		for(int k = 0 ; k < DimX; k++) {
			i[1][k] -= i[0][k];
			i[0][k] += i[1][k] >> 1;
		}

		i[0] += 2 * Stride;
		i[1] += 2 * Stride;
	}
}

void CWavelet2D::TransformHaarVI(short * pImage, int Stride)
{
	short * i[2];
	i[0] = pImage;
	i[1] = i[0] + Stride;

	for( int j = 0; j < DimY; j += 2 ) {
		for(int k = 0 ; k < DimX; k++) {
			i[0][k] -= i[1][k] >> 1;
			i[1][k] += i[0][k];
		}

		i[0] += 2 * Stride;
		i[1] += 2 * Stride;
	}
}

void CWavelet2D::LazyImage(short * pImage, int Stride)
{
	int DHVpos, Ipos1, Ipos2, LStride;
	short * pLOut;
	if (pLow == 0){
		pLOut = LBand.pBand;
		LStride = LBand.DimXAlign;
	}else{
		pLOut = pImage;
		LStride = Stride;
	}

	for(int j = 0; j < DimY; j += 2){
		DHVpos = (j >> 1) * HBand.DimX;
		Ipos1 = j * Stride;
		Ipos2 = (j >> 1) * LStride;
		for(int pEnd = Ipos1 + DimX; Ipos1 < pEnd; Ipos1 += 2){
			pLOut[Ipos2] = pImage[Ipos1];
			VBand.pBand[DHVpos] = pImage[Ipos1 + 1];
			HBand.pBand[DHVpos] = pImage[Ipos1 + Stride];
			DBand.pBand[DHVpos] = pImage[Ipos1 + Stride + 1];
			DHVpos++;
			Ipos2++;
		}
	}
}

void CWavelet2D::LazyImageI(short * pImage, int Stride)
{
	int DHVpos, Ipos1, Ipos2, LStride;
	short * pLIn;
	if (pLow == 0){
		pLIn = LBand.pBand;
		LStride = LBand.DimXAlign;
	}else{
		pLIn = pImage;
		LStride = Stride;
	}

	for(int j = DimY - 2; j >= 0; j -= 2){
		DHVpos = ((j >> 1) + 1) * HBand.DimX - 1;
		Ipos1 = j * Stride + DimX - 2;
		Ipos2 = (j >> 1) * LStride + (DimX >> 1) - 1;
		for(int pEnd = Ipos1 - DimX; Ipos1 > pEnd; Ipos1 -= 2){
			pImage[Ipos1] = pLIn[Ipos2] ;
			pImage[Ipos1 + 1] = VBand.pBand[DHVpos];
			pImage[Ipos1 + Stride] = HBand.pBand[DHVpos];
			pImage[Ipos1 + Stride + 1] = DBand.pBand[DHVpos];
			DHVpos--;
			Ipos2--;
		}
	}
}

template <trans t>
void CWavelet2D::Transform(short * pImage, int Stride)
{
	if (t == cdf97) {
		Transform97(pImage, Stride);
	} else if (t == cdf53) {
		Transform53H(pImage, Stride);
		Transform53V(pImage, Stride);
	} else if (t == haar) {
		TransformHaarH(pImage, Stride);
		TransformHaarV(pImage, Stride);
	}

	LazyImage(pImage, Stride);

	if (pLow != 0)
		pLow->Transform<t>(pImage, Stride);
}

template <trans t>
void CWavelet2D::TransformI(short * pImage, int Stride)
{
	if (pLow != 0)
		pLow->TransformI<t>(pImage, Stride);

	LazyImageI(pImage, Stride);

	if (t == cdf97) {
		Transform97I(pImage, Stride);
	} else if (t == cdf53) {
		Transform53VI(pImage, Stride);
		Transform53HI(pImage, Stride);
	} else if (t == haar) {
		TransformHaarVI(pImage, Stride);
		TransformHaarHI(pImage, Stride);
	}
}

template void CWavelet2D::Transform<cdf97>(short *, int);
template void CWavelet2D::Transform<cdf53>(short *, int);
template void CWavelet2D::Transform<haar>(short *, int);
template void CWavelet2D::TransformI<cdf97>(short *, int);
template void CWavelet2D::TransformI<cdf53>(short *, int);
template void CWavelet2D::TransformI<haar>(short *, int);

void CWavelet2D::SetWeight(trans t, float baseWeight)
{
	float scale, scale2;
	if (t == cdf97) {
		scale = XI;
		scale2 = XI * XI;
	} else {
		scale = SQRT2;
		scale2 = 2;
	}

	if (pHigh != 0){
		DBand.Weight = pHigh->VBand.Weight;
		VBand.Weight = pHigh->LBand.Weight;
		HBand.Weight = VBand.Weight;
		LBand.Weight = VBand.Weight * scale2;
	}else{
		DBand.Weight = baseWeight / scale2;
		VBand.Weight = baseWeight;
		HBand.Weight = baseWeight;
		LBand.Weight = baseWeight * scale2;
	}

	if (pLow != 0)
		pLow->SetWeight(t);
}

}
