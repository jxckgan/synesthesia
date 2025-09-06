#ifdef __APPLE__
#include "cli/cli.h"
#include "cli/headless.h"
#endif

#include <iostream>

int app_main(int argc, char** argv);

int main(int argc, char* argv[]) {
#ifdef __APPLE__
    CLI::Arguments args = CLI::Arguments::parseCommandLine(argc, argv);
    
    if (args.showHelp) {
        CLI::Arguments::printHelp();
        return 0;
    }
    
    if (args.showVersion) {
        CLI::Arguments::printVersion();
        return 0;
    }
    
    if (args.headless) {
        try {
            CLI::HeadlessInterface interface;
            interface.run(args.enableAPI, args.audioDevice);
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "Error in headless mode: " << e.what() << std::endl;
            return 1;
        }
    }
#endif
    
    return app_main(argc, argv);
}