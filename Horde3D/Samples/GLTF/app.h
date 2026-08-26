#pragma once

#include "../Framework/sampleapp.h"
#include "Horde3DHDRI.h"
#include <vector>

class GLTFViewer : public SampleApplication
{
public:
	GLTFViewer(int argc, char **argv);

protected:
	bool initResources() override;
	void releaseResources() override;
	void update() override;
	void render() override;
	void keyEventHandler(int key, int keyState, int mods) override;

private:
	H3DRes _hdriSource = 0;
	H3DHdriIbl _ibl;
	H3DNode _model = 0;
	std::vector<H3DRes> _materials;
	int _pbrDebugView = 0;
};
