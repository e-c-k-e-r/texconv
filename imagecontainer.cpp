#include <iostream>
#include <algorithm>
#include "imagecontainer.h"
#include "common.h"

bool ImageContainer::load(const std::vector<std::string>& filenames, int textureType, int mipmapFilter) {
	bool mipmapped = (textureType & FLAG_MIPMAPPED);

	if ((filenames.size() > 1) && !mipmapped) {
		std::cerr << "[ERROR] Only one input file may be specified if no mipmap flag is set.\n";
		return false;
	}

	
	for (const auto& filename : filenames) {
		Image img;
		if (!img.loadFromFile(filename)) { 
			std::cerr << "[ERROR] Failed to load image: " << filename << "\n";
			return false;
		}

		if (!isValidSize(img.width(), img.height(), textureType)) {
			std::cerr << "[ERROR] Image " << filename
					  << " has invalid texture size "
					  << img.width() << "x" << img.height() << "\n";
			return false;
		}

		if (mipmapped && img.width() != img.height()) {
			std::cerr << "[ERROR] Image " << filename
					  << " is not square. Mipmapped textures require square images.\n";
			return false;
		}

		textureWidth  = std::max(textureWidth, img.width());
		textureHeight = std::max(textureHeight, img.height());

		images[img.width()] = img;  
		std::cout << "[INFO] Loaded image " << filename << "\n";
	}

	if (mipmapped) {
		if (mipmapFilter == 0) { 
			std::cout << "[INFO] Using nearest-neighbor filtering for mipmaps\n";
		} else {
			std::cout << "[INFO] Using bilinear filtering for mipmaps\n";
		}

		
		for (int size = TEXTURE_SIZE_MAX/2; size >= 1; size /= 2) {
			if (images.count(size*2) && !images.count(size)) {
				Image mipmap = images[size*2].scaled(size, size,
														 mipmapFilter == 0); 
				images[size] = mipmap;
				std::cout << "[INFO] Generated " << size << "x" << size << " mipmap\n";
			}
		}
	}

	if (textureWidth < TEXTURE_SIZE_MIN || textureHeight < TEXTURE_SIZE_MIN) {
		std::cerr << "[ERROR] At least one input image must be 8x8 or larger.\n";
		return false;
	}

	
	keys.clear();
	for (auto& kv : images) keys.push_back(kv.first);
	std::sort(keys.begin(), keys.end());

	return true;
}

void ImageContainer::unloadAll() {
	textureWidth = 0;
	textureHeight = 0;
	images.clear();
	keys.clear();
}

const Image& ImageContainer::getByIndex(int index, bool ascending) const {
	if (index >= (int)keys.size()) {
		static Image dummy; 
		return dummy;
	} else {
		int realIdx = ascending ? index : ((int)keys.size() - index - 1);
		int size = keys[realIdx];
		return images.at(size);
	}
}