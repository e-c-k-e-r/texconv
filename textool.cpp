#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <string>
#include <algorithm>

#include "common.h"
#include "imagecontainer.h"

static bool g_verbose = false;

#define REDCOLOR	   "\033[31m"
#define YELLOWCOLOR     "\033[33m"
#define NOCOLOR	        "\033[0m"

inline void logDebug(const std::string& msg) {
	if (g_verbose) std::cout << msg << std::endl;
}
inline void logWarning(const std::string& msg) {
	std::cerr << YELLOWCOLOR << "[WARNING] " << msg << NOCOLOR << std::endl;
}
inline void logError(const std::string& msg) {
	std::cerr << REDCOLOR << "[ERROR] " << msg << NOCOLOR << std::endl;
}
inline void logFatal(const std::string& msg) {
	std::cerr << REDCOLOR << "[FATAL] " << msg << NOCOLOR << std::endl;
	exit(1);
}

struct CommandLineOptions {
	std::vector<std::string> inputs;
	std::string output;
	std::string format;
	std::string preview;
	std::string codeUsage;

	bool mipmap	 = false;
	bool compress   = false;
	bool stride	 = false;
	bool verbose	= false;
	bool nearest	= false;
	bool bilinear   = false;
};

bool parseArgs(int argc, char** argv, CommandLineOptions& opts) {
	for (int i=1; i<argc; i++) {
		std::string arg = argv[i];
		if ((arg=="-i"||arg=="--in") && i+1<argc) {
			opts.inputs.push_back(argv[++i]);
		} else if ((arg=="-o"||arg=="--out") && i+1<argc) {
			opts.output = argv[++i];
		} else if ((arg=="-f"||arg=="--format") && i+1<argc) {
			opts.format = argv[++i];
		} else if ((arg=="-p"||arg=="--preview") && i+1<argc) {
			opts.preview = argv[++i];
		} else if (arg=="--vqcodeusage" && i+1<argc) {
			opts.codeUsage = argv[++i];
		} else if (arg=="-m"||arg=="--mipmap") {
			opts.mipmap = true;
		} else if (arg=="-c"||arg=="--compress") {
			opts.compress = true;
		} else if (arg=="-s"||arg=="--stride") {
			opts.stride = true;
		} else if (arg=="-v"||arg=="--verbose") {
			opts.verbose = true;
		} else if (arg=="-n"||arg=="--nearest") {
			opts.nearest = true;
		} else if (arg=="-b"||arg=="--bilinear") {
			opts.bilinear = true;
		} else {
			logError("Unknown option: " + arg);
			return false;
		}
	}
	return true;
}

int main(int argc, char** argv) {
	CommandLineOptions opts;
	if (!parseArgs(argc, argv, opts)) {
		return -1;
	}
	g_verbose = opts.verbose;

	std::unordered_map<std::string,int> supportedFormats = {
		{"ARGB1555", PIXELFORMAT_ARGB1555},
		{"RGB565"  , PIXELFORMAT_RGB565},
		{"ARGB4444", PIXELFORMAT_ARGB4444},
		{"YUV422"  , PIXELFORMAT_YUV422},
		{"BUMPMAP" , PIXELFORMAT_BUMPMAP},
		{"PAL4BPP" , PIXELFORMAT_PAL4BPP},
		{"PAL8BPP" , PIXELFORMAT_PAL8BPP}
	};

	if (opts.inputs.empty()) {
		logError("No input file(s) specified");
		return -1;
	}
	if (opts.output.empty()) {
		logError("No output file specified");
		return -1;
	}

	std::string palFilename = opts.output + ".pal";

	int pixelFormat = -1;
	if (!opts.format.empty()) {
		auto it = supportedFormats.find(opts.format);
		if (it != supportedFormats.end()) {
			pixelFormat = it->second;
		}
	}
	if (pixelFormat == -1) {
		logError("Unsupported format: " + opts.format);
		return -1;
	}

	int textureType = (pixelFormat << PIXELFORMAT_SHIFT);
	if (opts.mipmap)   textureType |= FLAG_MIPMAPPED;
	if (opts.compress) textureType |= FLAG_COMPRESSED;
	if (opts.stride)   textureType |= (FLAG_STRIDED | FLAG_NONTWIDDLED);

	FilterMode mipmapFilter = (isPaletted(textureType)) ? NEAREST : BILINEAR;
	if (opts.nearest)  mipmapFilter = NEAREST;
	if (opts.bilinear) mipmapFilter = BILINEAR;

	ImageContainer images;
	if (!images.load(opts.inputs, textureType, mipmapFilter)) {
		return -1;
	}

	if (textureType & FLAG_STRIDED) {
		int strideSetting = images.width() / 32;
		textureType |= strideSetting;
	}

	std::ofstream out(opts.output, std::ios::binary);
	if (!out.is_open()) {
		logError("Failed to open file for writing: " + opts.output);
		return -1;
	}

	int expectedSize = writeTextureHeader(out, images.width(), images.height(), textureType);
	std::streampos positionBeforeData = out.tellp();

	if (isPaletted(textureType)) {
		convertPaletted(out, images, textureType, palFilename);
	} else {
		convert16BPP(out, images, textureType);
	}

	std::streampos positionAfterData = out.tellp();
	int padding = expectedSize - (positionAfterData - positionBeforeData);
	if (padding > 0) {
		if (padding >= 32) logWarning("Padding is " + std::to_string(padding));
		writeZeroes(out, padding);
		logDebug("Added " + std::to_string(padding) + " bytes of padding");
	}

	out.close();
	logDebug("Saved texture " + opts.output);

    std::string previewFilename = opts.preview;
    std::string codeUsageFilename = (textureType & FLAG_COMPRESSED) ? opts.codeUsage : "";

    if (!previewFilename.empty() || !codeUsageFilename.empty()) {
        if (generatePreview(opts.output, palFilename, previewFilename, codeUsageFilename)) {
            if (!previewFilename.empty())  std::cout << "[INFO] Saved preview image " << previewFilename << "\n";
            if (!codeUsageFilename.empty()) std::cout << "[INFO] Saved code usage image " << codeUsageFilename << "\n";
        } else {
            if (!previewFilename.empty())  std::cerr << "[ERROR] Failed to save preview image " << previewFilename << "\n";
            if (!codeUsageFilename.empty()) std::cerr << "[ERROR] Failed to save code usage image " << codeUsageFilename << "\n";
        }
    }

	return 0;
}