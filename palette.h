#pragma once

#include <unordered_map>
#include <vector>
#include <string>
#include <cstdint>
#include "common.h"

class ImageContainer;

class Palette {
public:
	Palette() = default;
	Palette(const ImageContainer& images);

	int colorCount() const { return (int)colorsVec.size(); }
	void clear() { colorsMap.clear(); colorsVec.clear(); }

	void insert(uint32_t argb);

	int indexOf(uint32_t argb) const;
	uint32_t colorAt(int index) const;

	bool load(const std::string& filename);
	bool save(const std::string& filename) const;

private:
	std::unordered_map<uint32_t,int> colorsMap;
	std::vector<uint32_t> colorsVec;
};