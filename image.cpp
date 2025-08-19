#include "image.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#include <cstring>
#include <iostream>

Image::Image() : w(0), h(0), indexedMode(false) {}
Image::Image(int width, int height) : w(width), h(height), indexedMode(false) {
	pixels.resize(w*h);
}

bool Image::loadFromFile(const std::string& path ) {
	int channels;
	uint8_t* buffer = stbi_load(path.c_str(), &w, &h, &channels, STBI_rgb_alpha);
	if (!buffer) {
		std::cerr<<"[ERROR] Failed to load image: "<<path<<"\n";
		return false;
	}
	indexedMode=false;
	pixels.resize(w*h);
	std::memcpy(pixels.data(), buffer, w*h*4);
	stbi_image_free(buffer);
	return true;
}


bool Image::saveToFile(const std::string& path) const {
	std::vector<uint8_t> buffer(w*h*4);
	for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
		RGBA c=pixel(x,y);
		int idx=(y*w+x)*4;
		buffer[idx+0]=c.r;
		buffer[idx+1]=c.g;
		buffer[idx+2]=c.b;
		buffer[idx+3]=c.a;
	}
	return stbi_write_png(path.c_str(),w,h,4,buffer.data(),w*4)!=0;
}

int Image::width() const { return w; }
int Image::height() const { return h; }

RGBA Image::pixel(int x,int y) const {
	return pixels[y*w+x];
}

void Image::setPixel(int x,int y, RGBA pixel) {
	if (!indexedMode) {
		pixels[y*w+x] = pixel;
	}
}

Image Image::scaled(int newW,int newH,bool nearest) const {
	Image out(newW,newH);
	if (nearest) {
		for (int y=0;y<newH;y++) {
			for (int x=0;x<newW;x++) {
				int srcX = x * w / newW;
				int srcY = y * h / newH;
				out.pixels[y*newW+x] = pixel(srcX,srcY);
			}
		}
	} else {
		for (int y=0;y<newH;y++) {
			for (int x=0;x<newW;x++) {
				float gx = (float)x * (w-1) / (float)(newW-1);
				float gy = (float)y * (h-1) / (float)(newH-1);
				int x0 = (int)gx;
				int y0 = (int)gy;
				int x1 = std::min(x0+1,w-1);
				int y1 = std::min(y0+1,h-1);
				float dx = gx-x0;
				float dy = gy-y0;
				auto lerp=[&](uint8_t a,uint8_t b,float t){ return (uint8_t)(a*(1-t)+b*t); };
				RGBA c00=pixel(x0,y0);
				RGBA c10=pixel(x1,y0);
				RGBA c01=pixel(x0,y1);
				RGBA c11=pixel(x1,y1);
				RGBA top{
					lerp(c00.r,c10.r,dx),
					lerp(c00.g,c10.g,dx),
					lerp(c00.b,c10.b,dx),
					lerp(c00.a,c10.a,dx)};
				RGBA bottom{
					lerp(c01.r,c11.r,dx),
					lerp(c01.g,c11.g,dx),
					lerp(c01.b,c11.b,dx),
					lerp(c01.a,c11.a,dx)};
				out.pixels[y*newW+x]=RGBA{
					lerp(top.r,bottom.r,dy),
					lerp(top.g,bottom.g,dy),
					lerp(top.b,bottom.b,dy),
					lerp(top.a,bottom.a,dy)};
			}
		}
	}
	return out;
}

void Image::allocateIndexed(int colors) {
	indexedMode=true;
	indexed.assign(w*h,0);
}

void Image::setIndexedPixel(int x,int y,uint8_t index) {
	if(indexedMode) {
		indexed[y*w+x]=index;
	}
}
uint8_t Image::indexedPixelAt(int x,int y) const {
	if(indexedMode) return indexed[y*w+x];
	return 0;
}