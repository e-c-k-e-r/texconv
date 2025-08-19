#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>
#include "common.h"
#include "twiddler.h"
#include "palette.h"
#include "image.h"
#include "vqtools.h"

static const uint32_t colorCodes[256] = {
	0xffffffff, 0xe3aaaa, 0xffc7c7, 0xaac7c7, 0xaac7aa, 0xaaaae3, 0xaaaaff, 0xaae3ff,
	0xffffaae3, 0xe3ffaa, 0xffffaa, 0xffaaff, 0xaaffc7, 0xe3c7ff, 0xc7aaaa, 0xe3e3e3,
	0xffaa7171, 0xc78e8e, 0x718e8e, 0x718e71, 0x7171aa, 0x7171c7, 0x71aac7, 0xc771aa,
	0xffaac771, 0xc7c771, 0xc771c7, 0x71c78e, 0xaa8ec7, 0x8e7171, 0xaaaaaa, 0xc7c7c7,
	0xff710000, 0x8e1c1c, 0x381c1c, 0x381c00, 0x380038, 0x380055, 0x383855, 0x8e0038,
	0xff715500, 0x8e5500, 0x8e0055, 0x38551c, 0x711c55, 0x550000, 0x713838, 0x8e5555,
	0xffaa38aa, 0xc755c7, 0x7155c7, 0x7155aa, 0x7138e3, 0x7138ff, 0x7171ff, 0xc738e3,
	0xffaa8eaa, 0xc78eaa, 0xc738ff, 0x718ec7, 0xaa55ff, 0x8e38aa, 0xaa71e3, 0xc78eff,
	0xff38aa38, 0x55c755, 0x00c755, 0x00c738, 0x00aa71, 0x00aa8e, 0x00e38e, 0x55aa71,
	0xff38ff38, 0x55ff38, 0x55aa8e, 0x00ff55, 0x38c78e, 0x1caa38, 0x38e371, 0x55ff8e,
	0xffe300aa, 0xff1cc7, 0xaa1cc7, 0xaa1caa, 0xaa00e3, 0xaa00ff, 0xaa38ff, 0xff00e3,
	0xffe355aa, 0xff55aa, 0xff00ff, 0xaa55c7, 0xe31cff, 0xc700aa, 0xe338e3, 0xff55ff,
	0xffe3aa00, 0xffc71c, 0xaac71c, 0xaac700, 0xaaaa38, 0xaaaa55, 0xaae355, 0xffaa38,
	0xffe3ff00, 0xffff00, 0xffaa55, 0xaaff1c, 0xe3c755, 0xc7aa00, 0xe3e338, 0xffff55,
	0xffaaaa00, 0xc7c71c, 0x71c71c, 0x71c700, 0x71aa38, 0x71aa55, 0x71e355, 0xc7aa38,
	0xffaaff00, 0xc7ff00, 0xc7aa55, 0x71ff1c, 0xaac755, 0x8eaa00, 0xaae338, 0xc7ff55,
	0xffe30071, 0xff1c8e, 0xaa1c8e, 0xaa1c71, 0xaa00aa, 0xaa00c7, 0xaa38c7, 0xff00aa,
	0xffe35571, 0xff5571, 0xff00c7, 0xaa558e, 0xe31cc7, 0xc70071, 0xe338aa, 0xff55c7,
	0xff3871aa, 0x558ec7, 0x008ec7, 0x008eaa, 0x0071e3, 0x0071ff, 0x00aaff, 0x5571e3,
	0xff38c7aa, 0x55c7aa, 0x5571ff, 0x00c7c7, 0x388eff, 0x1c71aa, 0x38aae3, 0x55c7ff,
	0xff3800aa, 0x551cc7, 0x001cc7, 0x001caa, 0x0000e3, 0x0000ff, 0x0038ff, 0x5500e3,
	0xff3855aa, 0x5555aa, 0x5500ff, 0x0055c7, 0x381cff, 0x1c00aa, 0x3838e3, 0x5555ff,
	0xff380071, 0x551c8e, 0x001c8e, 0x001c71, 0x0000aa, 0x0000c7, 0x0038c7, 0x5500aa,
	0xff385571, 0x555571, 0x5500c7, 0x00558e, 0x381cc7, 0x1c0071, 0x3838aa, 0x5555c7,
	0xff383800, 0x55551c, 0x00551c, 0x005500, 0x003838, 0x003855, 0x007155, 0x553838,
	0xff388e00, 0x558e00, 0x553855, 0x008e1c, 0x385555, 0x1c3800, 0x387138, 0x558e55,
	0xff383838, 0x555555, 0x005555, 0x005538, 0x003871, 0x00388e, 0x00718e, 0x553871,
	0xff388e38, 0x558e38, 0x55388e, 0x008e55, 0x38558e, 0x1c3838, 0x387171, 0x558e8e,
	0xffe33838, 0xff5555, 0xaa5555, 0xaa5538, 0xaa3871, 0xaa388e, 0xaa718e, 0xff3871,
	0xffe38e38, 0xff8e38, 0xff388e, 0xaa8e55, 0xe3558e, 0xc73838, 0xe37171, 0xff8e8e,
	0xffaa0000, 0xc71c1c, 0x711c1c, 0x711c00, 0x710038, 0x710055, 0x713855, 0xc70038,
	0xffaa5500, 0xc75500, 0xc70055, 0x71551c, 0xaa1c55, 0x8e0000, 0xaa3838, 0xc75555
};

static void drawBlock(Image& img,int x,int y,int w,int h,int codeIndex) {
	RGBA c;
	uint32_t packed = colorCodes[codeIndex%256];
	c.a=(packed>>24)&0xFF;
	c.r=(packed>>16)&0xFF;
	c.g=(packed>>8)&0xFF;
	c.b=(packed>>0)&0xFF;
	for(int yy=y;yy<y+h;yy++)
		for(int xx=x;xx<x+w;xx++)
			img.setPixel(xx,yy,c);
}

static Image allocatePreview(int w,int h,bool mipmaps) {
	int ww=mipmaps? (w+w/2):w;
	Image img(ww,h);
	for(int y=0;y<h;y++) for(int x=0;x<ww;x++)
		img.setPixel(x,y,RGBA{0,0,0,0});
	return img;
}

static void blitImage(Image& dst,const Image& src,int ox,int oy){
	for(int y=0;y<src.height();y++){
		for(int x=0;x<src.width();x++){
			dst.setPixel(ox+x,oy+y,src.pixel(x,y));
		}
	}
}

bool generatePreview(const std::string& texFile,
					 const std::string& palFile,
					 const std::string& previewFile,
					 const std::string& codeUsageFile) {
	char magic[4];
	int16_t width,height;
	int32_t textureType,textureSize;
	bool genPreview=!previewFile.empty();
	bool genUsage=!codeUsageFile.empty();

	std::ifstream in(texFile,std::ios::binary);
	if(!in.is_open()) {std::cerr<<"[ERROR] Cannot open "<<texFile<<"\n";return false;}
	in.read(magic,4);
	in.read((char*)&width,2);
	in.read((char*)&height,2);
	in.read((char*)&textureType,4);
	in.read((char*)&textureSize,4);
	if(std::memcmp(magic,TEXTURE_MAGIC,4)!=0) {
		std::cerr<<"Bad texture magic\n"; return false;
	}
	std::vector<uint8_t> data(textureSize);
	in.read((char*)data.data(),textureSize);
	in.close();

	if(textureType&FLAG_STRIDED) width=(textureType&31)*32;
	int pixelFormat=(textureType>>PIXELFORMAT_SHIFT)&PIXELFORMAT_MASK;

	std::vector<Image> decoded;
	std::vector<Image> usage;

	if(textureType&FLAG_STRIDED) {
		Image img(width,height);
		if(pixelFormat==PIXELFORMAT_YUV422) {
			for(int y=0;y<height;y++){
			  for(int x=0;x<width;x+=2){
				uint16_t p0,p1; std::memcpy(&p0,&data[(y*width+x+0)*2],2);
				std::memcpy(&p1,&data[(y*width+x+1)*2],2);
				RGBA c0,c1; YUV422toRGB(p0,p1,c0,c1);
				img.setPixel(x,y,c0);
				img.setPixel(x+1,y,c1);
			  }
			}
		} else {
			for(int y=0;y<height;y++){
			  for(int x=0;x<width;x++){
				uint16_t px; std::memcpy(&px,&data[(y*width+x)*2],2);
				img.setPixel(x,y,to32BPP(px,pixelFormat));
			  }
			}
		}
		decoded.push_back(img);
	}
	else if(is16BPP(textureType) && !(textureType&FLAG_COMPRESSED)) {
		int curW=width,curH=height;
		int offset=0;
		if(textureType&FLAG_MIPMAPPED){curW=1;curH=1;offset=MIPMAP_OFFSET_16BPP;}
		while(curW<=width && curH<=height) {
			Image img(curW,curH);
			Twiddler tw(curW,curH); int pixels=curW*curH;
			for(int i=0;i<pixels;i++){
				uint16_t px; std::memcpy(&px,&data[offset+i*2],2);
				RGBA c=to32BPP(px,pixelFormat);
				int idx=tw.index(i);
				img.setPixel(idx%curW,idx/curW,c);
			}
			decoded.insert(decoded.begin(),img);
			offset+=curW*curH*2; curW*=2;curH*=2;
		}
	}
	else if(isPaletted(textureType) && !(textureType&FLAG_COMPRESSED)) {
		Palette pal; if(!pal.load(palFile)) return false;
		if(isFormat(textureType,PIXELFORMAT_PAL4BPP)){
			int curW=width,curH=height,offset=0;
			if(textureType&FLAG_MIPMAPPED){curW=1;curH=1;offset=MIPMAP_OFFSET_4BPP;}
			while(curW<=width&&curH<=height){
				Image img(curW,curH);
				Twiddler tw(curW,curH);
				if(curW==1&&curH==1){
					int idx=data[offset]&0xF;
					RGBA c=unpackColor(pal.colorAt(idx));
					img.setPixel(0,0,c); offset++;
				} else {
					int pixels=(curW*curH)/2;
					for(int i=0;i<pixels;i++){
						uint8_t byte=data[offset+i];
						int idx0=byte&0xF, idx1=(byte>>4)&0xF;
						int tw0=tw.index(i*2+0), tw1=tw.index(i*2+1);
						img.setPixel(tw0%curW,tw0/curW,unpackColor(pal.colorAt(idx0)));
						img.setPixel(tw1%curW,tw1/curW,unpackColor(pal.colorAt(idx1)));
					}
					offset+=(curW*curH)/2;
				}
				decoded.insert(decoded.begin(),img);
				curW*=2;curH*=2;
			}
		} else if(isFormat(textureType,PIXELFORMAT_PAL8BPP)){
			int curW=width,curH=height,offset=0;
			if(textureType&FLAG_MIPMAPPED){curW=1;curH=1;offset=MIPMAP_OFFSET_8BPP;}
			while(curW<=width&&curH<=height){
				Image img(curW,curH);
				Twiddler tw(curW,curH); int pixels=curW*curH;
				for(int i=0;i<pixels;i++){
					uint8_t idx=data[offset+i];
					int twi=tw.index(i);
					img.setPixel(twi%curW,twi/curW,unpackColor(pal.colorAt(idx)));
				}
				decoded.insert(decoded.begin(),img);
				offset+=curW*curH; curW*=2;curH*=2;
			}
		}
	}
	else if(is16BPP(textureType) && (textureType&FLAG_COMPRESSED)) {
		int curW=width,curH=height,offset=2048;
		if(textureType&FLAG_MIPMAPPED){curW=2;curH=2;offset+=1;}
		while(curW<=width&&curH<=height){
			Image img(curW,curH), cui(curW,curH);
			if(genPreview) for(int y=0;y<curH;y++)for(int x=0;x<curW;x++) img.setPixel(x,y,{0,0,0,0});
			if(genUsage) for(int y=0;y<curH;y++)for(int x=0;x<curW;x++) cui.setPixel(x,y,{0,0,0,0});
			Twiddler tw(curW/2,curH/2); int pixels=(curW/2)*(curH/2);
			for(int i=0;i<pixels;i++){
				int cbidx=data[offset+i];
				uint16_t texel0,texel1,texel2,texel3;
				std::memcpy(&texel0,&data[cbidx*8+0],2);
				std::memcpy(&texel1,&data[cbidx*8+2],2);
				std::memcpy(&texel2,&data[cbidx*8+4],2);
				std::memcpy(&texel3,&data[cbidx*8+6],2);
				int twi=tw.index(i);
				int x=(twi%(curW/2))*2,y=(twi/(curW/2))*2;
				if(genPreview){
					RGBA p0=to32BPP(texel0,pixelFormat);
					RGBA p1=to32BPP(texel1,pixelFormat);
					RGBA p2=to32BPP(texel2,pixelFormat);
					RGBA p3=to32BPP(texel3,pixelFormat);
					img.setPixel(x+0,y+0,p0);
					img.setPixel(x+0,y+1,p1);
					img.setPixel(x+1,y+0,p2);
					img.setPixel(x+1,y+1,p3);
				}
				if(genUsage) drawBlock(cui,x,y,2,2,cbidx);
			}
			if(genPreview) decoded.insert(decoded.begin(),img);
			if(genUsage) usage.insert(usage.begin(),cui);
			offset+=(curW*curH)/4; curW*=2; curH*=2;
		}
	}
	else if(isPaletted(textureType) && (textureType&FLAG_COMPRESSED)) {
		Palette pal; if(!pal.load(palFile)) return false;
		int curW=width,curH=height,offset=2048;
		if(textureType&FLAG_MIPMAPPED){
			curW=(isFormat(textureType,PIXELFORMAT_PAL8BPP)?4:4);
			curH=4; offset+=1;
		}
		while(curW<=width&&curH<=height){
			Image img(curW,curH), cui(curW,curH);
			if(genPreview) for(int y=0;y<curH;y++)for(int x=0;x<curW;x++) img.setPixel(x,y,{0,0,0,0});
			if(genUsage) for(int y=0;y<curH;y++)for(int x=0;x<curW;x++) cui.setPixel(x,y,{0,0,0,0});
			Twiddler tw(curW/4,curH/4); int pixels=(curW/4)*(curH/4);
			for(int i=0;i<pixels;i++){
				int cb0=data[offset+i*2+0], cb1=data[offset+i*2+1];
				int twi=tw.index(i); int x=(twi%(curW/4))*4,y=(twi/(curW/4))*4;
				if(genPreview){
					for(int j=0;j<8;j++){
						int idx=data[cb0*8+j]; RGBA c=unpackColor(pal.colorAt(idx));
						img.setPixel(x+(j%2),y+(j/2),c);
					}
					for(int j=0;j<8;j++){
						int idx=data[cb1*8+j]; RGBA c=unpackColor(pal.colorAt(idx));
						img.setPixel(x+2+(j%2),y+(j/2),c);
					}
				}
				if(genUsage){
					drawBlock(cui,x,y,2,4,cb0);
					drawBlock(cui,x+2,y,2,4,cb1);
				}
			}
			if(genPreview) decoded.insert(decoded.begin(),img);
			if(genUsage) usage.insert(usage.begin(),cui);
			offset+=(curW*curH)/8; curW*=2;curH*=2;
		}
	}

	if(genPreview && !decoded.empty()) {
		if(decoded.size()==1) decoded[0].saveToFile(previewFile);
		else {
			Image canvas=allocatePreview(width,height,true);
			int ox=0,oy=0;
			for(auto& im:decoded){ blitImage(canvas,im,ox,oy);
				if(ox==0){ox=im.width(); oy=0;} else {oy+=im.height();} }
			canvas.saveToFile(previewFile);
		}
	}
	if(genUsage && !usage.empty()) {
		if(usage.size()==1) usage[0].saveToFile(codeUsageFile);
		else {
			Image canvas=allocatePreview(width,height,true);
			int ox=0,oy=0;
			for(auto& im:usage){ blitImage(canvas,im,ox,oy);
				if(ox==0){ox=im.width();oy=0;} else {oy+=im.height();} }
			canvas.saveToFile(codeUsageFile);
		}
	}
	return true;
}