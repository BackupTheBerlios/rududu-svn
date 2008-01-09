

#pragma once

#include "bandcodec.h"

namespace rududu {

class CWavelet2D{
public:
	CWavelet2D(int x, int y, int level, int Align = ALIGN);

    ~CWavelet2D();

	template <trans t> void Transform(short * pImage, int Stride);
	template <trans t> void TransformI(short * pImage, int Stride);
	template <bool forward> void DCT4(void);
	void SetWeight(trans t, float baseWeight = 1.);

	void DecodeBand(CMuxCodec * pCodec, int method);
	void CodeBand(CMuxCodec * pCodec, int method);

	template <bool use_dct> unsigned int TSUQ(short Quant, float Thres);
	template <bool use_dct> void TSUQi(short Quant);

	void Stats(void);

	CBandCodec DBand;
	CBandCodec HBand;
	CBandCodec VBand;
	CBandCodec LBand;
	CWavelet2D * pLow;
	CWavelet2D * pHigh;

private:

	int DimX;
	int DimY;

	CWavelet2D(int x, int y, int level, CWavelet2D * pHigh, int Align);
	void Init(int level, int Align);

	// TODO : tester si inline c'est mieux
	static void TransLine97(short * i, int len);
	static void TransLine97I(short * i, int len);
	void Transform97(short * pImage, int Stride);
	void Transform97I(short * pImage, int Stride);

	void Transform53H(short * pImage, int Stride);
	void Transform53HI(short * pImage, int Stride);
	void Transform53V(short * pImage, int Stride);
	void Transform53VI(short * pImage, int Stride);

	void TransformHaarH(short * pImage, int Stride);
	void TransformHaarHI(short * pImage, int Stride);
	void TransformHaarV(short * pImage, int Stride);
	void TransformHaarVI(short * pImage, int Stride);

	template <bool forward> void DCT4H(void);
	template <bool forward> void DCT4V(void);

	static inline short mult08(short a);
};

}
