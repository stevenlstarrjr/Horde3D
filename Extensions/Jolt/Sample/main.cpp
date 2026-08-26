// Horde3D Jolt Physics sample

#if defined(__APPLE__) || defined(__APPLE_CC__)
#include "AvailabilityMacros.h"
#include "TargetConditionals.h"
#if TARGET_OS_IPHONE
#include "SDL_main.h"
#ifdef __cplusplus
extern "C"
#endif
#endif
#endif

#include "app.h"

int main(int argc, char **argv)
{
	JoltPhysicsSample app(argc, argv);
	if(!app.init()) return 1;
	return app.run();
}
