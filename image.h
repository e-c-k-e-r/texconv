#pragma once

#include <string>
#include <map>
#include <vector>

#include "common.h"

class Image {
public:
	Image();
	Image(int width, int height);

	bool loadFromFile(const std::string& path);
	bool saveToFile(const std::string& path) const;

	int width() const;
	int height() const;

	RGBA pixel(int x,int y) const;
	void setPixel(int x,int y, RGBA pixel);

	Image scaled(int newW,int newH,bool nearest) const;

	void allocateIndexed(int colors);
	void setIndexedPixel(int x,int y, uint8_t index);
	uint8_t indexedPixelAt(int x,int y) const;

	bool isIndexed() const { return indexedMode; }

private:
	int w,h;
	bool indexedMode;
	std::vector<RGBA> pixels;
	std::vector<uint8_t> indexed;
};