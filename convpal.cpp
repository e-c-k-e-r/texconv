#include "imagecontainer.h"
#include "twiddler.h"
#include "palette.h"
#include "vqtools.h"
#include "common.h"

#include <iostream>
#include <vector>
#include <cstring>

static void vectorizeARGB(const ImageContainer& images, std::vector<Vec<4>>& vectors) {
	for ( int i = 0; i < images.imageCount(); i++ ) {
		const Image& img = images.getByIndex(i);
		for ( int y = 0; y < img.height(); y++ ) {
			for ( int x=0; x < img.width(); x++ ) {
				RGBA px = img.pixel(x,y);
				Vec<4> vec;
				vec[0] = px.a/255.f;
				vec[1] = px.r/255.f;
				vec[2] = px.g/255.f;
				vec[3] = px.b/255.f;
				vectors.push_back(vec);
			}
		}
	}
}

static void devectorizeARGB(const ImageContainer& srcImages, const std::vector<Vec<4>>& vectors, const VectorQuantizer<4>& vq, std::vector<Image>& indexedImages, Palette& palette) {
	int vindex = 0;
	for ( int i = 0; i < srcImages.imageCount(); i++ ) {
		const Image& src = srcImages.getByIndex(i);
		Image dst( src.width(), src.height() );
		dst.allocateIndexed(256);

		for ( int y = 0; y < src.height(); y++ ) {
			for ( int x = 0; x < src.width(); x++ ) {
				const Vec<4>& vec = vectors[vindex++];
				int codeIndex = vq.findClosest(vec);
				dst.setIndexedPixel( x, y, (uint8_t) codeIndex );
			}
		}
		indexedImages.push_back(dst);
	}
	
	for ( int i = 0; i < vq.codeCount(); i++ ) {
		const Vec<4>& v = vq.codeVector(i);
		uint32_t color = (uint8_t)( v[0]*255)<<24 | (uint8_t)(v[1]*255)<<16 |
					    (uint8_t)( v[2]*255)<<8 | (uint8_t)(v[3]*255);
		palette.insert(color);
	}
}

void convertToIndexedImages(const ImageContainer& src, const Palette& pal, std::vector<Image>& dst);
void writeUncompressed4BPPData(std::ostream& stream, const std::vector<Image>& indexedImages);
void writeUncompressed8BPPData(std::ostream& stream, const std::vector<Image>& indexedImages);
void writeUncompressedPreview(const std::string& filename, const std::vector<Image>& indexedImages, const Palette& palette);
void writeCompressed4BPPData(std::ostream& stream, const std::vector<Image>& indexedImages, const Palette& palette);
void writeCompressed8BPPData(std::ostream& stream, const std::vector<Image>& indexedImages, const Palette& palette);

/*
 * This conversion basically has three modes:
 *
 * 1. The source images contain <= unique colors than the requested mode
 *    needs, so conversion will be quick and lossless.
 *
 * 2. The source images contain > unique colors than the requested mode
 *    needs. In this case we utilize vector quantization to reduce the
 *    color count.
 *
 * 3. The user has requested for the image to be compressed. This is a two
 *    stage process. First, reduce the input images to the color count needed.
 *    Then, using the reduced images as input, perform vector quantization
 *    with a vector dimension of 32 or 64 (2x4 or 4x4 pixel blocks).
 */
void convertPaletted(std::ostream& stream, const ImageContainer& images, int textureType, const std::string& paletteFilename) {
	const int maxColors = isFormat(textureType, PIXELFORMAT_PAL4BPP) ? 16 : 256;
	Palette palette(images);
	std::vector<Image> indexedImages;

	//qDebug("Palette contains %d colors", palette.colorCount());

	if (palette.colorCount() > maxColors) {
		// The palette has too many colors, so perform a vector quantization to reduce
		// the color count down to what we need.
		//qDebug("Reducing palette to %d colors", maxColors);
		palette.clear();
		VectorQuantizer<4> vq;
		std::vector<Vec<4>> vectors;
		vectorizeARGB(images, vectors);
		vq.compress(vectors, maxColors);
		devectorizeARGB(images, vectors, vq, indexedImages, palette);
	} else {
		// Convert the input images to indexed images so we can use the same output code
		// as the reduced color images.
		convertToIndexedImages(images, palette, indexedImages);
	}

	// The palette is finished now, so save it.
	palette.save(paletteFilename);

	// Write data
	if (textureType & FLAG_COMPRESSED) {
		if (isFormat(textureType, PIXELFORMAT_PAL4BPP))
			writeCompressed4BPPData(stream, indexedImages, palette);
		if (isFormat(textureType, PIXELFORMAT_PAL8BPP))
			writeCompressed8BPPData(stream, indexedImages, palette);
	} else {
		if (isFormat(textureType, PIXELFORMAT_PAL4BPP))
			writeUncompressed4BPPData(stream, indexedImages);
		if (isFormat(textureType, PIXELFORMAT_PAL8BPP))
			writeUncompressed8BPPData(stream, indexedImages);
	}
}


// Converts the src images to indexed images.
// The indexed images are sorted from smallest to largest.
void convertToIndexedImages(const ImageContainer& src, const Palette& pal, std::vector<Image>& dst) {
	for (int i=0; i<src.imageCount(); i++) {
		const Image& img = src.getByIndex(i);
		Image dstImg(img.width(), img.height()/*, Image::Format_ARGB32*/);
		for (int y=0; y<img.height(); y++)
			for (int x=0; x<img.width(); x++) {
				RGBA px = img.pixel(x,y);
				uint32_t argb = packColor(px);
				uint8_t index = (uint8_t) pal.indexOf(argb);
				dstImg.setIndexedPixel( x, y, index );
			}
		dst.push_back(dstImg);
	}
}

void writeUncompressed4BPPData(std::ostream& stream, const std::vector<Image>& indexedImages) {
	// Write mipmap offset if necessary
	if (indexedImages.size() > 1)
		writeZeroes(stream, MIPMAP_OFFSET_4BPP);

	// Write all mipmaps from smallest to largest
	for (int i=0; i<indexedImages.size(); i++) {
		const Image& img = indexedImages[i];

		// Special case. There's only one pixel in the 1x1 mipmap level,
		// but it's stored by itself in one byte.
		if (img.width() == 1) {
			uint8_t val = img.indexedPixelAt(0,0);
			stream.write( (char*) &val, 1 );
			continue;
		}

		Twiddler twiddler(img.width(), img.height());
		const int pixels = img.width() * img.height();

		// Write all pixels in pairs
		// First pixel in the least significant nibble.
		// Second pixel in the most significant nibble.
		for (int j=0; j<pixels; j+=2) {
			uint8_t palindex[2];

			for (int k=0; k<2; k++) {
				const int index = twiddler.index(j + k);
				const int x = index % img.width();
				const int y = index / img.width();
				palindex[k] = (uint8_t) img.indexedPixelAt(x, y);
			}

			uint8_t packed = (((palindex[1] & 0xF) << 4) | (palindex[0] & 0xF));
			stream.write( (char*) &packed, 1 );
		}
	}
}

void writeUncompressed8BPPData(std::ostream& stream, const std::vector<Image>& indexedImages) {
	// Write mipmap offset if necessary
	if (indexedImages.size() > 1)
		writeZeroes(stream, MIPMAP_OFFSET_8BPP);

	// Write all mipmaps from smallest to largest
	for (int i=0; i<indexedImages.size(); i++) {
		const Image& img = indexedImages[i];

		Twiddler twiddler(img.width(), img.height());
		const int pixels = img.width() * img.height();

		for (int j=0; j<pixels; j++) {
			const int index = twiddler.index(j);
			const int x = index % img.width();
			const int y = index / img.width();
			uint8_t value = img.indexedPixelAt(x, y);
			stream.write( (char*) &value, 1 );
		}
	}
}



#define STORE_FULL	0	// Store the block in a full 32D vector
#define STORE_LEFT	1	// Store the block in the left half of a 64D vector
#define STORE_RIGHT	2	// Store the block in the right half of a 64D vector

template<uint N>
static void grab2x4Block(const Image& img, const Palette& pal, const int x, const int y, Vec<N>& vec, const uint storeMethod) {
	static const int indexLUT[3][8] = {
		{ 0,  4,  8, 12, 16, 20, 24, 28 }, // Full 32D vector
		{ 0,  4, 16, 20, 32, 36, 48, 52 }, // Left half of 64D vector
		{ 8, 12, 24, 28, 40, 44, 56, 60 }  // Right half of 64D vector
	};

	int index = 0;
	uint hash = vec.hash();

	for (int yy=y; yy<(y+4); yy++) {
		for (int xx=x; xx<(x+2); xx++) {
			uint32_t pixel = pal.colorAt(img.indexedPixelAt(xx, yy));
			argb2vec(pixel, vec, indexLUT[storeMethod][index]);
			RGBA color = unpackColor( pixel );
			hash = combineHash(color, hash);
			index++;
		}
	}

	vec.setHash(hash);
}

static void vectorizePalette(const Palette& pal, std::vector<Vec<4>>& vectors) {
	for (int i=0; i<pal.colorCount(); i++) {
		Vec<4> vec;
		argb2vec(pal.colorAt(i), vec);
		vectors.push_back(vec);
	}
}

static uint8_t findClosest(const std::vector<Vec<4>>& vectors, const Vec<4>& vec) {
	uint8_t closestIndex = 0;
	float closestDistance = Vec<4>::distanceSquared(vectors[0], vec);
	for (int i=1; i<vectors.size(); i++) {
		float distance = Vec<4>::distanceSquared(vectors[i], vec);
		if (distance < closestDistance)	{
			closestIndex = (uint8_t)i;
			closestDistance = distance;
		}
	}
	return closestIndex;
}

void writeCompressed4BPPData(std::ostream& stream, const std::vector<Image>& indexedImages, const Palette& palette) {
	VectorQuantizer<64> vq;
	std::vector<Vec<64>> vectors;

	// Vectorize the input images.
	// Each vector represents a pair of 2x4 pixel blocks. For single images, it's
	// easy since we can just grab a number of 4x4 blocks straight from the source
	// image. It's a bit more complicated for mipmapped images though. They're
	// essentially aligned on a nibble boundary so a single vector represents the
	// second half of the 4x4 pixel block at twiddledIndex[n] as well as the first
	// half of the 4x4 pixel block at twiddledIndex[n+1]. This makes the mipmapped
	// vectorization code a lot more complex.
	if (indexedImages.size() > 1) {
		Vec<64> vec(0);

		for (int i=0; i<indexedImages.size(); i++) {
			const Image& img = indexedImages[i];

			// Ignore images smaller than this
			if (img.width() < MIN_MIPMAP_PALVQ || img.height() < MIN_MIPMAP_PALVQ)
				continue;

			const int imgw = img.width();
			const int imgh = img.height();
			const int blocks = (imgw * imgh) / 16;
			const Twiddler twiddler(imgw / 4, imgh / 4);

			for (int j=0; j<blocks; j++) {
				const int twidx = twiddler.index(j);
				const int x = (twidx % (imgw / 4)) * 4;
				const int y = (twidx / (imgw / 4)) * 4;

				// If this is the first vector we're processing, the first
				// half of it will be empty. So instead of leaving it empty
				// and potentially mess up the encoding by introducing colors that
				// don't exist in the image, we copy the second half of the vector
				// to the first half.
				if (vectors.empty()) {
					grab2x4Block(img, palette, x, y, vec, STORE_LEFT);
				}

				// First half of this block is the second half of the
				// vector we're currently creating.
				grab2x4Block(img, palette, x, y, vec, STORE_RIGHT);

				// This vector is done now, so flush it and remember to
				// clear the hash for the next vector.
				vectors.push_back(vec);
				vec.setHash(0);

				// Second half of this block is the first half of the next
				// vector we're creating.
				grab2x4Block(img, palette, x + 2, y, vec, STORE_LEFT);

				// If this is the last block of the last image, remember to
				// fill the current vector with something good and flush it.
				if ((i == (indexedImages.size() - 1)) && (j == (blocks - 1))) {
					grab2x4Block(img, palette, x + 2, y, vec, STORE_RIGHT);
					vectors.push_back(vec);
				}
			}
		}
	} else {
		// There's only one image, and it's on a byte boundary, so this
		// is simple. Twiddle the data here though, since the mipmapped
		// vectors need to be twiddled, so the same code can be used to
		// devectorize this as well as mipmapped stuff.
		const Image& img = indexedImages[0];
		const int imgw = img.width();
		const int imgh = img.height();
		const int blocks = (imgw * imgh) / 16;
		const Twiddler twiddler(imgw / 4, imgh / 4);

		for (int j=0; j<blocks; j++) {
			const int twidx = twiddler.index(j);
			const int x = (twidx % (imgw / 4)) * 4;
			const int y = (twidx / (imgw / 4)) * 4;

			Vec<64> vec(0);
			grab2x4Block(img, palette, x + 0, y, vec, STORE_LEFT);
			grab2x4Block(img, palette, x + 2, y, vec, STORE_RIGHT);
			vectors.push_back(vec);
		}
	}

	vq.compress(vectors, 256);

	// The palette needs to be in a vector format for the next part,
	// since we need to be able to perform searches in it.
	std::vector<Vec<4>> vectorizedPalette;
	vectorizePalette(palette, vectorizedPalette);

	// Build the codebook
	uint8_t codebook[2048];
	memset(codebook, 0, 2048);
	const Twiddler nibbleLUT(4, 4);
	for (int i=0; i<vq.codeCount(); i++) {
		const Vec<64>& vec = vq.codeVector(i);

		for (int j=0; j<16; j++) {
			Vec<4> color;
			color.set(0, vec[nibbleLUT.index(j) * 4 + 0]);
			color.set(1, vec[nibbleLUT.index(j) * 4 + 1]);
			color.set(2, vec[nibbleLUT.index(j) * 4 + 2]);
			color.set(3, vec[nibbleLUT.index(j) * 4 + 3]);

			// Search the vectorized palette for the closest index
			uint8_t closestIndex = findClosest(vectorizedPalette, color);

			const int byte = j / 2;
			const int nibble = j % 2;

			if (nibble == 1)
				codebook[i*8+byte] |= ((closestIndex & 0xF) << 4);
			else
				codebook[i*8+byte] |= (closestIndex & 0xF);
		}
	}

	// Write the codebook
	stream.write( (char*) codebook, 2048 );

	// Don't write out a zero for the 1x1 mipmap like we would usually
	// do for mipmapped VQ textures. The reason for this is that it's
	// represented by a single nibble in PAL4BPPVQMM textures. And that
	// nibble is part of the first index byte, which will be written next.
	//if (indexedImages.size() > 1)
	//	writeZeroes(stream, 1);

	// Write the index data
	for (int i=0; i<vectors.size(); i++) {
		const Vec<64>& srcvec = vectors.at(i);
		const int c = vq.findClosest(srcvec);
		stream << (uint8_t)c;
	}
}



void writeCompressed8BPPData(std::ostream& stream, const std::vector<Image>& indexedImages, const Palette& palette) {
	VectorQuantizer<32> vq;
	std::vector<Vec<32>> vectors;

	// Vectorize the input images.
	// Each vector represents a 2x4 pixel block.
	// Grab the data as twiddled, it's simpler than twiddling it
	// when we write it to file.
	for (int i=0; i<indexedImages.size(); i++) {
		const Image& img = indexedImages[i];

		// Ignore images smaller than this
		if (img.width() < MIN_MIPMAP_PALVQ || img.height() < MIN_MIPMAP_PALVQ)
			continue;

		const int imgw = img.width();
		const int imgh = img.height();
		const int blocks = (imgw * imgh) / 16;
		const Twiddler twiddler(imgw / 4, imgh / 4);

		for (int j=0; j<blocks; j++) {
			const int twidx = twiddler.index(j);
			const int x = (twidx % (imgw / 4)) * 4;
			const int y = (twidx / (imgw / 4)) * 4;
			Vec<32> vec;

			grab2x4Block(img, palette, x + 0, y, vec, STORE_FULL);
			vectors.push_back(vec);

			grab2x4Block(img, palette, x + 2, y, vec, STORE_FULL);
			vectors.push_back(vec);
		}
	}

	vq.compress(vectors, 256);

	// The palette needs to be in a vector format for the next part,
	// since we need to be able to perform searches in it.
	std::vector<Vec<4>> vectorizedPalette;
	vectorizePalette(palette, vectorizedPalette);

	// Build the codebook
	uint8_t codebook[2048];
	memset(codebook, 0, 2048);
	const Twiddler nibbleLUT(2, 4);
	for (int i=0; i<vq.codeCount(); i++) {
		const Vec<32>& vec = vq.codeVector(i);

		for (int j=0; j<8; j++) {
			Vec<4> color;
			color.set(0, vec[nibbleLUT.index(j) * 4 + 0]);
			color.set(1, vec[nibbleLUT.index(j) * 4 + 1]);
			color.set(2, vec[nibbleLUT.index(j) * 4 + 2]);
			color.set(3, vec[nibbleLUT.index(j) * 4 + 3]);

			// Search the palette for the closest index
			codebook[i * 8 + j] = findClosest(vectorizedPalette, color);
		}
	}

	// Write the codebook
	stream.write( (char*) codebook, 2048 );

	// Write the 1x1 mipmap level
	if (indexedImages.size() > 1)
		writeZeroes(stream, 1);

	// Write the index data
	for (int i=0; i<vectors.size(); i++) {
		const Vec<32>& srcvec = vectors.at(i);
		const int c = vq.findClosest(srcvec);
		stream << (uint8_t)c;
	}
}