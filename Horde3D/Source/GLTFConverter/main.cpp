// Horde3D glTF 2.0 converter command line
// SPDX-License-Identifier: EPL-1.0

#include "converter.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace {
void help()
{
	std::puts("Horde3D GLTFConv 0.1");
	std::puts("Usage: GLTFConv input.gltf|input.glb [-dest directory] [-asset resource/path]");
	std::puts("");
	std::puts("The destination is a Horde3D content root. The optional asset path is");
	std::puts("used inside that root and in generated resource references.");
}
}

int main(int argc, char **argv)
{
	if(argc < 2) { help(); return 1; }
	std::string input = argv[1], destination = ".", assetPath;
	bool infoOnly = false;
	for(int i = 2; i < argc; ++i)
	{
		if(std::strcmp(argv[i], "-dest") == 0 && i + 1 < argc) destination = argv[++i];
		else if(std::strcmp(argv[i], "-asset") == 0 && i + 1 < argc) assetPath = argv[++i];
		else if(std::strcmp(argv[i], "-info") == 0) infoOnly = true;
		else { std::fprintf(stderr, "Unknown or incomplete option: %s\n", argv[i]); help(); return 1; }
	}

	Horde3D::GLTFConverter::Converter converter;
	if(!converter.load(input)) { std::fprintf(stderr, "GLTFConv: %s\n", converter.error().c_str()); return 2; }
	std::puts(converter.summary().c_str());
	if(infoOnly) return 0;
	if(!converter.write(destination, assetPath)) { std::fprintf(stderr, "GLTFConv: %s\n", converter.error().c_str()); return 3; }
	std::printf("Converted '%s' to '%s'\n", input.c_str(), destination.c_str());
	return 0;
}
