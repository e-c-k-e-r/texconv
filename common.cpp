#include "common.h"
#include "imagecontainer.h"

#include <cmath>
#include <cassert>
#include <iostream>

#define M_PI 3.1415926535897932384f
#define HALF_PI M_PI/2.0f
#define DOUBLE_PI M_PI*2.0f

static inline bool powerOfTwo(int x) {
	return (x > 0 && (x & (x - 1)) == 0);
}
inline uint8_t clamp255(int v) { return (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v)); }

int nextPowerOfTwo(int x) {
	if (x <= 0) return 1;
	int pw2 = 1;
	while (pw2 < x) pw2 *= 2;
	return pw2;
}

bool isValidSize(int width, int height, int textureType) {
	if (textureType & FLAG_STRIDED) {
		if (width < TEXTURE_STRIDE_MIN || width > TEXTURE_STRIDE_MAX || (width % 32) != 0)
			return false;
		if (height < TEXTURE_SIZE_MIN || height > TEXTURE_SIZE_MAX || !powerOfTwo(height))
			return false;
	} else {
		int minSize = (textureType & FLAG_MIPMAPPED) ? 1 : TEXTURE_SIZE_MIN;
		if (width < minSize || width > TEXTURE_SIZE_MAX || !powerOfTwo(width))
			return false;
		if (height < minSize || height > TEXTURE_SIZE_MAX || !powerOfTwo(height))
			return false;
	}
	return true;
}

void writeZeroes(std::ostream& stream, int n) {
	char zero = 0;
	for (int i=0; i<n; i++) stream.write(&zero, 1);
}

bool isFormat(int textureType, int pixelFormat) {
	return ((textureType >> PIXELFORMAT_SHIFT) & PIXELFORMAT_MASK) == pixelFormat;
}
bool isPaletted(int textureType) {
	return isFormat(textureType, PIXELFORMAT_PAL4BPP) || isFormat(textureType, PIXELFORMAT_PAL8BPP);
}
bool is16BPP(int textureType) { return !isPaletted(textureType); }

static uint16_t toSpherical(const RGBA& c) {
	
	float vx = (c.r/255.0f) * 2.0f - 1.0f;
	float vy = (c.g/255.0f) * 2.0f - 1.0f;
	float vz = (c.b/255.0f); 

	float radius = std::sqrt(vx*vx + vy*vy + vz*vz);
	if (radius < 1e-6f) radius = 1e-6f;

	float polar   = std::acos(vz / radius);
	float azimuth = std::atan2(vy, vx);

	polar = (HALF_PI - polar) / (HALF_PI) * 255.0f;  
	int S = std::max(0, std::min(255, (int)polar));

	if (azimuth < 0) azimuth += 2*M_PI;
	azimuth = azimuth / (2*M_PI) * 255.0f;
	int R = std::max(0, std::min(255, (int)azimuth));

	return (uint16_t)((S << 8) | R);
}

static RGBA toCartesian(uint16_t SR) {
	float S = (1.0 - ((SR >> 8) / 255.0)) * HALF_PI;
	float R = ((SR & 0xFF) / 255.0) * DOUBLE_PI;
	if (R > M_PI) R -= DOUBLE_PI;
	RGBA color;
	color.r = (sin(S) * cos(R) + 1.0f) * 0.5f;
	color.g = (sin(S) * sin(R) + 1.0f) * 0.5f;
	color.b = (cos(S) + 1.0f) * 0.5f;
	color.a = 1;
	return color;
}

uint16_t to16BPP(const RGBA& argb, int pixelFormat) {
	uint16_t a,r,g,b;
	switch (pixelFormat) {
	case PIXELFORMAT_ARGB1555:
		a = (argb.a < 128) ? 0 : 1;
		r = (argb.r >> 3) & 0x1F;
		g = (argb.g >> 3) & 0x1F;
		b = (argb.b >> 3) & 0x1F;
		return (a<<15)|(r<<10)|(g<<5)|b;
	case PIXELFORMAT_RGB565:
		r = (argb.r >> 3) & 0x1F;
		g = (argb.g >> 2) & 0x3F;
		b = (argb.b >> 3) & 0x1F;
		return (r<<11)|(g<<5)|b;
	case PIXELFORMAT_ARGB4444:
		a = (argb.a >> 4) & 0xF;
		r = (argb.r >> 4) & 0xF;
		g = (argb.g >> 4) & 0xF;
		b = (argb.b >> 4) & 0xF;
		return (a<<12)|(r<<8)|(g<<4)|b;
	case PIXELFORMAT_BUMPMAP:
		return toSpherical(argb);
	default:
		std::cerr << "Unsupported format " << pixelFormat << " in to16BPP\n";
		return 0xFFFF;
	}
}

RGBA to32BPP(uint16_t px, int pixelFormat) {
	RGBA out{255,255,255,255};
	switch(pixelFormat) {
	case PIXELFORMAT_ARGB1555:
		out.a = (px>>15)&1 ? 255:0;
		out.r = ((px>>10)&0x1F)<<3;
		out.g = ((px>>5)&0x1F)<<3;
		out.b = ((px>>0)&0x1F)<<3;
		break;
	case PIXELFORMAT_RGB565:
		out.r = ((px>>11)&0x1F)<<3;
		out.g = ((px>>5)&0x3F)<<2;
		out.b = ((px>>0)&0x1F)<<3;
		out.a = 255;
		break;
	case PIXELFORMAT_ARGB4444:
		out.a = ((px>>12)&0xF)<<4;
		out.r = ((px>>8)&0xF)<<4;
		out.g = ((px>>4)&0xF)<<4;
		out.b = ((px>>0)&0xF)<<4;
		break;
	case PIXELFORMAT_BUMPMAP:
		return toCartesian(px);
	default: break;
	}
	return out;
}


void RGBtoYUV422(const RGBA& c1, const RGBA& c2, uint16_t& yuv1, uint16_t& yuv2) {
	int avgR = (c1.r + c2.r)/2;
	int avgG = (c1.g + c2.g)/2;
	int avgB = (c1.b + c2.b)/2;

	int Y0 = clamp255((int)(0.299*c1.r + 0.587*c1.g + 0.114*c1.b));
	int Y1 = clamp255((int)(0.299*c2.r + 0.587*c2.g + 0.114*c2.b));

	int U = clamp255((int)(-0.169*avgR -0.331*avgG +0.499*avgB +128));
	int V = clamp255((int)(0.499*avgR -0.418*avgG -0.0813*avgB +128));

	yuv1 = ((uint16_t)Y0<<8) | (uint16_t)U;
	yuv2 = ((uint16_t)Y1<<8) | (uint16_t)V;
}

void YUV422toRGB(const uint16_t yuv1, const uint16_t yuv2, RGBA& rgb1, RGBA& rgb2) {
	int Y0 = (yuv1>>8)&0xFF;
	int Y1 = (yuv2>>8)&0xFF;
	int U = (yuv1&0xFF) -128;
	int V = (yuv2&0xFF) -128;

	rgb1.r = clamp255((int)(Y0 + 1.375*V));
	rgb1.g = clamp255((int)(Y0 - 0.34375*U - 0.6875*V));
	rgb1.b = clamp255((int)(Y0 + 1.71875*U));
	rgb1.a = 255;

	rgb2.r = clamp255((int)(Y1 + 1.375*V));
	rgb2.g = clamp255((int)(Y1 - 0.34375*U - 0.6875*V));
	rgb2.b = clamp255((int)(Y1 + 1.71875*U));
	rgb2.a = 255;
}


static int getPixelCount(int w,int h,int minw,int minh) {
	if (w<minw || h<minh) return 0;
	return w*h + getPixelCount(w/2,h/2,minw,minh);
}

int calculateSize(int w, int h, int textureType) {
	const bool mipmapped = (textureType & FLAG_MIPMAPPED);
	const bool compressed = (textureType & FLAG_COMPRESSED);
	int bytes = 0;

	if (mipmapped) {
		if (compressed) {
			bytes += 2048; 
			bytes += 1;	
			if (is16BPP(textureType)) {
				
				bytes += getPixelCount(w,h,2,2) / 4;
			} else if (isFormat(textureType, PIXELFORMAT_PAL4BPP)) {
				
				bytes += getPixelCount(w,h,4,4) / 16;
			} else if (isFormat(textureType, PIXELFORMAT_PAL8BPP)) {
				
				bytes += getPixelCount(w,h,4,4) / 8;
			}
		} else {
			const int pixels = getPixelCount(w,h,1,1);
			if (is16BPP(textureType)) {
				bytes += MIPMAP_OFFSET_16BPP;
				bytes += pixels * 2;
			} else if (isFormat(textureType, PIXELFORMAT_PAL4BPP)) {
				bytes += MIPMAP_OFFSET_4BPP;
				bytes += 1; 
				bytes += (pixels - 1) / 2;
			} else if (isFormat(textureType, PIXELFORMAT_PAL8BPP)) {
				bytes += MIPMAP_OFFSET_8BPP;
				bytes += pixels;
			}
		}
	} else {
		const int pixels = getPixelCount(w,h,w,h);
		if (compressed) {
			bytes += 2048; 
			if (is16BPP(textureType)) {
				bytes += pixels / 4;
			} else if (isFormat(textureType, PIXELFORMAT_PAL4BPP)) {
				bytes += pixels / 16;
			} else if (isFormat(textureType, PIXELFORMAT_PAL8BPP)) {
				bytes += pixels / 8;
			}
		} else {
			if (is16BPP(textureType)) {
				bytes += pixels * 2;
			} else if (isFormat(textureType, PIXELFORMAT_PAL4BPP)) {
				bytes += pixels / 2;
			} else if (isFormat(textureType, PIXELFORMAT_PAL8BPP)) {
				bytes += pixels;
			}
		}
	}

	
	if (bytes % 32 == 0) {
		return bytes;
	} else {
		return ((bytes / 32) + 1) * 32;
	}
}


int writeTextureHeader(std::ostream& stream,int width,int height,int textureType){
	int size = calculateSize(width,height,textureType);
	if (textureType & FLAG_STRIDED) {
		width = nextPowerOfTwo(width);
	}

	stream.write(TEXTURE_MAGIC,4);
	int16_t w16=(int16_t)width;
	int16_t h16=(int16_t)height;
	int32_t typ = textureType;
	int32_t sz  = size;
	stream.write((char*)&w16,2);
	stream.write((char*)&h16,2);
	stream.write((char*)&typ,4);
	stream.write((char*)&sz,4);

	return size;
}

uint32_t combineHash(const RGBA& rgba, uint32_t seed) {
	uint32_t val = ((rgba.a<<24)|(rgba.r<<16)|(rgba.g<<8)|rgba.b);
	seed ^= val + 0x9e3779b9 + (seed<<6) + (seed>>2);
	return seed;
}