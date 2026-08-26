#include "app.h"

int main(int argc, char **argv)
{
	GLTFViewer app(argc, argv);
	if(!app.init()) return 1;
	return app.run();
}
