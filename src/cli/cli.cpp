#include "cli.h"

#include <iostream>
#include <cstring>
#include "version.h"

namespace CLI {

Arguments Arguments::parseCommandLine(int argc, char* argv[]) {
    Arguments args;
    
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--headless") == 0 || strcmp(argv[i], "-h") == 0) {
            args.headless = true;
        }
        else if (strcmp(argv[i], "--enable-api") == 0) {
            args.enableAPI = true;
        }
        else if (strcmp(argv[i], "--help") == 0) {
            args.showHelp = true;
        }
        else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            args.showVersion = true;
        }
        else if (strcmp(argv[i], "--device") == 0 || strcmp(argv[i], "-d") == 0) {
            if (i + 1 < argc) {
                args.audioDevice = argv[++i];
            }
        }
        else {
            std::cerr << "Unknown argument: " << argv[i] << std::endl;
            std::cerr << "Use --help for usage information." << std::endl;
        }
    }
    
    return args;
}

void Arguments::printHelp() {
    std::cout << "Synesthesia - Real-time Audio Visualisation\n\n";
    std::cout << "Usage: Synesthesia [OPTIONS]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --headless, -h        Run in headless mode (no GUI)\n";
    std::cout << "  --enable-api          Start API server automatically\n";
    std::cout << "  --device, -d <name>   Use specific audio device\n";
    std::cout << "  --version, -v         Show version information\n";
    std::cout << "  --help                Show this help message\n\n";
    std::cout << "In headless mode:\n";
    std::cout << "  - Use arrow keys to navigate audio devices\n";
    std::cout << "  - Press Enter to select a device\n";
    std::cout << "  - Press 'q' or Ctrl+C to quit\n";
    std::cout << "  - Press 'a' to toggle API server\n\n";
}

void Arguments::printVersion() {
    std::cout << "Synesthesia " << SYNESTHESIA_VERSION_STRING << std::endl;
    std::cout << "Built with C++20" << std::endl;
#ifdef USE_NEON_OPTIMIZATIONS
    std::cout << "ARM NEON optimizations: Enabled" << std::endl;
#endif
#ifdef ENABLE_API_SERVER
    std::cout << "API Server: Enabled" << std::endl;
#else
    std::cout << "API Server: Disabled" << std::endl;
#endif
}

}