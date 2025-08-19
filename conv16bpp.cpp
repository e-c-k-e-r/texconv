#include <iostream>
#include <vector>
#include <unordered_map>
#include <cstring>
#include "imagecontainer.h"
#include "twiddler.h"
#include "vqtools.h"
#include "common.h"


void convertAndWriteTexel(std::ostream& stream, const RGBA& texel, int pixelFormat, bool twiddled);
void writeStrideData(std::ostream& stream, const Image& img, int pixelFormat);
void writeUncompressedData(std::ostream& stream, const ImageContainer& images, int pixelFormat);
void writeCompressedData(std::ostream& stream, const ImageContainer& images, int pixelFormat);

void convert16BPP(std::ostream& stream, const ImageContainer& images, int textureType) {
	const int pixelFormat = (textureType >> PIXELFORMAT_SHIFT) & PIXELFORMAT_MASK;

	if (textureType & FLAG_STRIDED) {
		writeStrideData(stream, images.getByIndex(0), pixelFormat);
	} else if (textureType & FLAG_COMPRESSED) {
		writeCompressedData(stream, images, pixelFormat);
	} else {
		writeUncompressedData(stream, images, pixelFormat);
	}
}

void convertAndWriteTexel(std::ostream& stream, const RGBA& texel, int pixelFormat, bool twiddled) {
	if (pixelFormat == PIXELFORMAT_YUV422) {
		static int index = 0;
		static RGBA savedTexel[3];

		if (!twiddled && index == 1) {
			uint16_t yuv[2];
			RGBtoYUV422(savedTexel[0], texel, yuv[0], yuv[1]);
			stream.write(reinterpret_cast<char*>(&yuv[0]), 2);
			stream.write(reinterpret_cast<char*>(&yuv[1]), 2);
			index = 0;
		} else if (twiddled && index == 3) {
			uint16_t yuv[4];
			RGBtoYUV422(savedTexel[0], savedTexel[2], yuv[0], yuv[2]);
			RGBtoYUV422(savedTexel[1], texel, yuv[1], yuv[3]);
			stream.write(reinterpret_cast<char*>(&yuv[0]), 2);
			stream.write(reinterpret_cast<char*>(&yuv[1]), 2);
			stream.write(reinterpret_cast<char*>(&yuv[2]), 2);
			stream.write(reinterpret_cast<char*>(&yuv[3]), 2);
			index = 0;
		} else {
			savedTexel[index] = texel;
			index++;
		}
	} else {
		uint16_t val = to16BPP(texel, pixelFormat);
		stream.write(reinterpret_cast<char*>(&val), 2);
	}
}

void writeStrideData(std::ostream& stream, const Image& img, int pixelFormat) {
	for (int y=0; y<img.height(); y++)
		for (int x=0; x<img.width(); x++)
			convertAndWriteTexel(stream, img.pixel(x, y), pixelFormat, false);
}

void writeUncompressedData(std::ostream& stream, const ImageContainer& images, int pixelFormat) {
	// Mipmap offset
	if (images.hasMipmaps()) {
		writeZeroes(stream, MIPMAP_OFFSET_16BPP);
	}

	// Texture data, from smallest to largest mipmap
	for (int i=0; i<images.imageCount(); i++) {
		const Image& img = images.getByIndex(i);

		// The 1x1 mipmap level is a bit special for YUV textures. Since there's only
		// one pixel, it can't be saved as YUV422, so save it as RGB565 instead.
		if (img.width() == 1 && img.height() == 1 && pixelFormat == PIXELFORMAT_YUV422) {
			convertAndWriteTexel(stream, img.pixel(0, 0), PIXELFORMAT_RGB565, true);
			continue;
		}

		const Twiddler twiddler(img.width(), img.height());
		const int pixels = img.width() * img.height();

		// Write all texels for this mipmap level in twiddled order
		for (int j=0; j<pixels; j++) {
			const int index = twiddler.index(j);
			const int x = index % img.width();
			const int y = index / img.width();
			convertAndWriteTexel(stream, img.pixel(x, y), pixelFormat, true);
		}
	}
}

// Packs a quad (2x2 16BPP texels) into a single uint64_t
uint64_t packQuad(RGBA topLeft, RGBA topRight, RGBA bottomLeft, RGBA bottomRight, int pixelFormat) {
	uint64_t a, b, c, d;
	if (pixelFormat == PIXELFORMAT_YUV422) {
		uint16_t yuv[4];
		RGBtoYUV422(topLeft,    topRight,    yuv[0], yuv[1]);
		RGBtoYUV422(bottomLeft, bottomRight, yuv[2], yuv[3]);
		a = yuv[0];
		b = yuv[1];
		c = yuv[2];
		d = yuv[3];
	} else {
		a = to16BPP(topLeft,     pixelFormat);
		b = to16BPP(topRight,    pixelFormat);
		c = to16BPP(bottomLeft,  pixelFormat);
		d = to16BPP(bottomRight, pixelFormat);
	}
	return (a << 48) | (b << 32) | (c << 16) | d;
}


// This function counts how many unique 2x2 16BPP pixel blocks there are in the image.
// If there are <= maxCodes, it puts the unique blocks in 'codebook' and 'indexedImages'
// will contain images that index the 'codebook' vector, resulting in quick "lossless"
// compression, if possible.
// It will keep counting blocks even if the block count exceeds maxCodes for the sole
// purpose of reporting it back to the user.
// Returns number of unique 2x2 16BPP pixel blocks in all images.
int encodeLossless(const ImageContainer& images, int pixelFormat, std::vector<Image>& indexedImages, std::vector<uint64_t>& codebook, int maxCodes) {
	std::unordered_map<uint64_t, int> uniqueQuads; // Quad <=> index

	for (int i=0; i<images.imageCount(); i++) {
		const Image& img = images.getByIndex(i);

		// Ignore images smaller than this
		if (img.width() < MIN_MIPMAP_VQ || img.height() < MIN_MIPMAP_VQ)
			continue;

		Image indexedImage(img.width() / 2, img.height() / 2/*, Image::Format_Indexed8*/);
		indexedImage.allocateIndexed(256);

		for (int y=0;y<img.height();y+=2) {
			for (int x=0;x<img.width();x+=2) {
				RGBA tl = img.pixel(x + 0, y + 0);
				RGBA tr = img.pixel(x + 1, y + 0);
				RGBA bl = img.pixel(x + 0, y + 1);
				RGBA br = img.pixel(x + 1, y + 1);
				uint64_t quad = packQuad(tl, tr, bl, br, pixelFormat);

				if ( uniqueQuads.find(quad) == uniqueQuads.end() )
					uniqueQuads[quad] = (int) uniqueQuads.size();

				if ( (int) uniqueQuads.size() <= maxCodes )
					indexedImage.setIndexedPixel( x/2, y/2, uniqueQuads[quad] );
			}
		}

		// Only add the image if we haven't hit the code limit
		if (uniqueQuads.size() <= maxCodes) {
			indexedImages.push_back(indexedImage);
		}
	}

	if (uniqueQuads.size() <= maxCodes) {
		// This texture can be losslessly compressed.
		// Copy the unique quads over to the codebook.
		// indexedImages is already done.
		codebook.resize(uniqueQuads.size());
		for ( auto& kv : uniqueQuads ) codebook[kv.second] = kv.first;
	} else {
		// This texture needs lossy compression
		indexedImages.clear();
	}

	return uniqueQuads.size();
}

// Divides the image into 2x2 pixel blocks and stores them as 12-dimensional
// vectors, (R, G, B) * 4.
void vectorizeRGB(const ImageContainer& images, std::vector<Vec<12>>& vectors) {
	for (int i=0; i<images.imageCount(); i++) {
		const Image& img = images.getByIndex(i);

		// Ignore images smaller than this
		if (img.width() < MIN_MIPMAP_VQ || img.height() < MIN_MIPMAP_VQ)
			continue;

		for (int y=0; y<img.height(); y+=2) {
			for (int x=0; x<img.width(); x+=2) {
				Vec<12> vec;
				uint hash = 0;
				int offset = 0;
				for (int yy=y; yy<(y+2); yy++) {
					for (int xx=x; xx<(x+2); xx++) {
						RGBA pixel = img.pixel(xx, yy);
						rgb2vec(packColor(pixel), vec, offset);
						hash = combineHash(pixel, hash);
						offset += 3;
					}
				}
				vec.setHash(hash);
				vectors.push_back(vec);
			}
		}
	}
}

// Divides the image into 2x2 pixel blocks and stores them as 16-dimensional
// vectors, (A, R, G, B) * 4.
static void vectorizeARGB(const ImageContainer& images, std::vector<Vec<16>>& vectors) {
	for (int i=0; i<images.imageCount(); i++) {
		const Image& img = images.getByIndex(i);

		// Ignore images smaller than this
		if (img.width() < MIN_MIPMAP_VQ || img.height() < MIN_MIPMAP_VQ)
			continue;

		for (int y=0; y<img.height(); y+=2) {
			for (int x=0; x<img.width(); x+=2) {
				Vec<16> vec;
				uint hash = 0;
				int offset = 0;
				for (int yy=y; yy<(y+2); yy++) {
					for (int xx=x; xx<(x+2); xx++) {
						RGBA pixel = img.pixel(xx, yy);
						argb2vec(packColor(pixel), vec, offset);
						hash = combineHash(pixel, hash);
						offset += 4;
					}
				}
				vec.setHash(hash);
				vectors.push_back(vec);
			}
		}
	}
}

static void devectorizeRGB(const ImageContainer& srcImages, const std::vector<Vec<12>>& vectors, const VectorQuantizer<12>& vq, int pixelFormat, std::vector<Image>& indexedImages, std::vector<uint64_t>& codebook) {
	int vindex = 0;

	for (int i=0; i<srcImages.imageCount(); i++) {
		const auto& srcImage = srcImages.getByIndex(i);
		if (srcImage.width() == 1 || srcImage.height() == 1)
			continue;
		Image img(srcImage.width()/2, srcImage.height()/2/*, Image::Format_Indexed8*/);
		img.allocateIndexed(256);
		for (int y=0; y<img.height(); y++) {
			for (int x=0; x<img.width(); x++) {
				const Vec<12>& vec = vectors[vindex];
				int codeIndex = vq.findClosest(vec);
				img.setIndexedPixel(x, y, codeIndex);
				vindex++;
			}
		}
		indexedImages.push_back(img);
	}

	for (int i=0; i<vq.codeCount(); i++) {
		const Vec<12>& vec = vq.codeVector(i);
		RGBA tl = {vec[0], vec[1], vec[2]};
		RGBA tr = {vec[3], vec[4], vec[5]};
		RGBA bl = {vec[6], vec[7], vec[8]};
		RGBA br = {vec[9], vec[10], vec[11]};
		uint64_t quad = packQuad(tl, tr, bl, br, pixelFormat);
		codebook.push_back(quad);
	}
}

static void devectorizeARGB(const ImageContainer& srcImages, const std::vector<Vec<16>>& vectors, const VectorQuantizer<16>& vq, int format, std::vector<Image>& indexedImages, std::vector<uint64_t>& codebook) {
	int vindex = 0;

	for (int i=0; i<srcImages.imageCount(); i++) {
		const auto& srcImage = srcImages.getByIndex(i);
		if (srcImage.width() == 1 || srcImage.height() == 1)
			continue;
		Image img(srcImage.width()/2, srcImage.height()/2/*, Image::Format_Indexed8*/);
		img.allocateIndexed(256);
		for (int y=0; y<img.height(); y++) {
			for (int x=0; x<img.width(); x++) {
				const Vec<16>& vec = vectors[vindex];
				int codeIndex = vq.findClosest(vec);
				img.setIndexedPixel(x, y, codeIndex);
				vindex++;
			}
		}
		indexedImages.push_back(img);
	}

	for (int i=0; i<vq.codeCount(); i++) {
		const Vec<16>& vec = vq.codeVector(i);
		RGBA tl = {vec[1], vec[2], vec[3], vec[0]};
		RGBA tr = {vec[5], vec[6], vec[7], vec[4]};
		RGBA bl = {vec[9], vec[10], vec[11], vec[8]};
		RGBA br = {vec[13], vec[14], vec[15], vec[12]};
		uint64_t quad = packQuad(tl, tr, bl, br, format);
		codebook.push_back(quad);
	}
}

void writeCompressedData(std::ostream& stream, const ImageContainer& images, int pixelFormat) {
	std::vector<Image> indexedImages;
	std::vector<uint64_t> codebook;

	const int numQuads = encodeLossless(images, pixelFormat, indexedImages, codebook, 256);

	std::cout << "Source images contain" << numQuads << "unique quads";

	if (numQuads > 256) {
		if ((pixelFormat != PIXELFORMAT_ARGB1555) && (pixelFormat != PIXELFORMAT_ARGB4444)) {
			std::vector<Vec<12>> vectors;
			VectorQuantizer<12> vq;
			vectorizeRGB(images, vectors);
			vq.compress(vectors, 256);
			devectorizeRGB(images, vectors, vq, pixelFormat, indexedImages, codebook);
		} else {
			std::vector<Vec<16>> vectors;
			VectorQuantizer<16> vq;
			vectorizeARGB(images, vectors);
			vq.compress(vectors, 256);
			devectorizeARGB(images, vectors, vq, pixelFormat, indexedImages, codebook);
		}
	}

	// Build the codebook
	uint16_t codes[256 * 4];
	memset(codes, 0, 2048);
	for (int i=0; i<codebook.size(); i++) {
		const uint64_t& quad = codebook[i];
		codes[i * 4 + 0] = (uint16_t)((quad >> 48) & 0xFFFF);
		codes[i * 4 + 1] = (uint16_t)((quad >> 16) & 0xFFFF);
		codes[i * 4 + 2] = (uint16_t)((quad >> 32) & 0xFFFF);
		codes[i * 4 + 3] = (uint16_t)((quad >>  0) & 0xFFFF);
	}

	// Write the codebook
	for (int i=0; i<1024; i++)
		stream.write( (char*) &codes[i], 2 );

	// Write the 1x1 mipmap level
	if (images.imageCount() > 1)
		writeZeroes(stream, 1);

	// Write all mipmap levels
	for (int i=0; i<indexedImages.size(); i++) {
		const Image& img = indexedImages[i];
		const Twiddler twiddler(img.width(), img.height());
		const int pixels = img.width() * img.height();

		for (int j=0; j<pixels; j++) {
			const int index = twiddler.index(j);
			const int x = index % img.width();
			const int y = index / img.width();
			uint8_t val = img.indexedPixelAt(x, y);
			stream.write( (char*) &val, 1 );
		}
	}
}