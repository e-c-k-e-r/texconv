#ifndef COMMON_H
#define COMMON_H

#include <cstdint>
#include <ostream>

#include "vqtools.h" // contains some cruft

#define PIXELFORMAT_ARGB1555	0
#define PIXELFORMAT_RGB565	  1
#define PIXELFORMAT_ARGB4444	2
#define PIXELFORMAT_YUV422	  3
#define PIXELFORMAT_BUMPMAP	 4
#define PIXELFORMAT_PAL4BPP	 5
#define PIXELFORMAT_PAL8BPP	 6
#define PIXELFORMAT_MASK		7
#define PIXELFORMAT_SHIFT	   27

#define FLAG_NONTWIDDLED		(1 << 26)
#define FLAG_STRIDED			(1 << 25)
#define FLAG_COMPRESSED		 (1 << 30)
#define FLAG_MIPMAPPED		  (1 << 31)

#define TEXTURE_SIZE_MIN	8
#define TEXTURE_SIZE_MAX	1024
#define TEXTURE_STRIDE_MIN  32
#define TEXTURE_STRIDE_MAX  992

#define MIN_MIPMAP_VQ   2
#define MIN_MIPMAP_PALVQ 4

#define TEXTURE_MAGIC   "DTEX"
#define PALETTE_MAGIC   "DPAL"

#define MIPMAP_OFFSET_4BPP  1
#define MIPMAP_OFFSET_8BPP  3
#define MIPMAP_OFFSET_16BPP 6


int nextPowerOfTwo(int x);
bool isValidSize(int width, int height, int textureType);
void writeZeroes(std::ostream& stream, int n);

bool isFormat(int textureType, int pixelFormat);
bool isPaletted(int textureType);
bool is16BPP(int textureType);


uint16_t to16BPP(const RGBA& px, int pixelFormat);
RGBA to32BPP(uint16_t px, int pixelFormat);


void RGBtoYUV422(const RGBA& rgb1, const RGBA& rgb2, uint16_t& yuv1, uint16_t& yuv2);
void YUV422toRGB(const uint16_t yuv1, const uint16_t yuv2, RGBA& rgb1, RGBA& rgb2);

int writeTextureHeader(std::ostream& stream, int width, int height, int textureType);

uint32_t combineHash(const RGBA& rgba, uint32_t seed);

class ImageContainer;

void convert16BPP(std::ostream& stream, const ImageContainer& images, int textureType);
void convertPaletted(std::ostream& stream, const ImageContainer& images, int textureType, const std::string& palFilename);
bool generatePreview(const std::string& textureFilename, const std::string& paletteFilename, const std::string& previewFilename, const std::string& codeUsageFilename);


#endif 