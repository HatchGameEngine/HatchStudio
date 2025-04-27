#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

#include <chrono>
#include <thread>

#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/Diagnostics.h>

#include <Studio/Impl.hpp>

extern "C" {
	#include "Common.h"
}

#include <UI/Graphics/Renderer.hpp>

#include <fstream>

// Platform-specific menubar
namespace UI::Graphics::Renderer {
    void Sleep(double seconds) {
		// https://blat-blatnik.github.io/computerBear/making-accurate-sleep-function/
		using namespace std;
	    using namespace std::chrono;

	    static double estimate = 5e-3;
	    static double mean = 5e-3;
	    static double m2 = 0;
	    static int64_t count = 1;

	    while (seconds > estimate) {
	        auto start = high_resolution_clock::now();
	        this_thread::sleep_for(milliseconds(1));
	        auto end = high_resolution_clock::now();

	        double observed = (end - start).count() / 1e9;
	        seconds -= observed;

	        ++count;
	        double delta = observed - mean;
	        mean += delta / count;
	        m2   += delta * (observed - mean);
	        double stddev = sqrt(m2 / (count - 1));
	        estimate = mean + stddev;
	    }

	    // spin lock
	    auto start = high_resolution_clock::now();
	    while ((high_resolution_clock::now() - start).count() / 1e9 < seconds);
	}
}

namespace UI::SystemDialog {
    bool StartProcess(const char* appPath, const char* cmd, const char* startDir) {
        
        return true;
    }
}

namespace GameLinker {
    void LinkExternalGameLogic(LinkData* linkData, const char* projectFolder) {
        #define DL_EXT ".dylib"

		char binaryPath[1024];
        const char* fileNameDLL = "Binaries/Game" DL_EXT;

        // Update the editor DLL
		sprintf(binaryPath, "%s/%s", projectFolder, "Binaries/Game" DL_EXT);
        std::ifstream src(binaryPath, std::ios::binary);

		sprintf(binaryPath, "%s/%s", projectFolder, "Binaries/GameEditor" DL_EXT);
		std::ofstream dst(binaryPath, std::ios::binary);
		dst << src.rdbuf();

        src.close();
        dst.close();

        // Load that DLL instead
        fileNameDLL = "Binaries/GameEditor" DL_EXT;

        GameLogicSharedObject = SDL_LoadObject(binaryPath);
        if (GameLogicSharedObject) {
            void (*linkGameLogic)(LinkData*) = (void (*)(LinkData*))SDL_LoadFunction(GameLogicSharedObject, "LinkGameLogic");
            if (linkGameLogic) {
                linkGameLogic(linkData);
            }
            else
                Diagnostics::SetError("Could not find \"%s\" in %s! (%s)", "LinkGameLogic", fileNameDLL, SDL_GetError());
        }
        else {
            Diagnostics::SetError("Could not find %s! (%s)", fileNameDLL, SDL_GetError());
        }
    }
}
