#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <exception>
#include <filesystem>
#include <string>
#include <system_error>
#include "UI/includes/UI_main.h"

namespace {

// The UI loads its assets with relative paths ("assets/font.ttf", "assets/grass.png").
// Resolve them against the directory of the executable first, so the game also works when it
// is started from another working directory (shortcut, IDE, build folder); fall back to the
// current working directory when the executable directory has no assets/ folder.
void selectAssetDirectory(const char* argv0) {
    namespace fs = std::filesystem;
    try {
        std::error_code ec;

        fs::path exePath = fs::read_symlink("/proc/self/exe", ec); // Linux: exact executable path
        if (ec || exePath.empty()) {
            ec.clear();
            exePath.clear();
            if (argv0 != nullptr && argv0[0] != '\0') {
                exePath = fs::absolute(fs::path(argv0), ec);
                if (ec) exePath.clear();
            }
        }

        const fs::path fontRel = fs::path("assets") / "font.ttf";
        fs::path exeDir = exePath.parent_path();
        if (!exeDir.empty()) {
            ec.clear();
            if (fs::exists(exeDir / fontRel, ec)) {
                ec.clear();
                fs::current_path(exeDir, ec);
                if (!ec) {
                    std::cout << "[Main] Assets folder: " << (exeDir / "assets").string() << "\n";
                    return;
                }
            }
        }

        ec.clear();
        if (fs::exists(fontRel, ec)) {
            std::cout << "[Main] Assets folder: " << (fs::current_path(ec) / "assets").string() << "\n";
        } else {
            ec.clear();
            std::cerr << "[Main] ERROR: assets/font.ttf was not found next to the executable ("
                      << (exeDir.empty() ? std::string("?") : exeDir.string()) << ") or in the working directory ("
                      << fs::current_path(ec).string() << "). Text will be missing.\n";
        }
    } catch (const std::exception& e) {
        // Path conversion problems must never stop the game; keep the current working directory
        std::cerr << "[Main] Warning: could not resolve the assets folder (" << e.what() << ").\n";
    }
}

void printUsage() {
    std::cout << "Usage: energy_crisis [--shot <out.png>] [--scene NAME] [--frames N] [--seed S] [--lint]\n"
              << "  --shot <out.png>  render N frames of a scene, save the window to a PNG and exit\n"
              << "  --scene NAME      " << ui::shot::sceneNames() << " (default: game)\n"
              << "  --frames N        frames rendered before the capture (default: 90)\n"
              << "  --seed S          match seed for reproducible captures (default: 1)\n"
              << "  --lint            print text layout problems; exit code = number of problems\n";
}

// Parses the screenshot / lint options. Returns false (after printing why) on a bad command line.
bool parseShotOptions(int argc, char* argv[], ShotOptions& opts) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto needValue = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "[Main] " << name << " needs a value.\n";
                return nullptr;
            }
            return argv[++i];
        };
        if (arg == "--shot") {
            const char* v = needValue("--shot");
            if (!v) return false;
            opts.outPath = v;
            opts.enabled = true;
        } else if (arg == "--scene") {
            const char* v = needValue("--scene");
            if (!v) return false;
            opts.scene = v;
            if (!ui::shot::isKnownScene(opts.scene)) {
                std::cerr << "[Main] Unknown scene \"" << opts.scene << "\". Scenes: " << ui::shot::sceneNames() << "\n";
                return false;
            }
        } else if (arg == "--frames") {
            const char* v = needValue("--frames");
            if (!v) return false;
            opts.frames = std::max(1, std::atoi(v));
        } else if (arg == "--seed") {
            const char* v = needValue("--seed");
            if (!v) return false;
            opts.seed = static_cast<unsigned int>(std::strtoul(v, nullptr, 10));
        } else if (arg == "--lint") {
            opts.lint = true;
            opts.enabled = true;
        } else if (arg == "--help" || arg == "-h") {
            printUsage();
            return false;
        } else {
            std::cerr << "[Main] Unknown option \"" << arg << "\".\n";
            printUsage();
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    std::cout << "========================================\n";
    std::cout << "    Energy Crisis - Game UI Test        \n";
    std::cout << "========================================\n";

    ShotOptions shotOptions;
    if (!parseShotOptions(argc, argv, shotOptions)) {
        return 2;
    }

    selectAssetDirectory(argc > 0 ? argv[0] : nullptr);

    std::cout << "[Main] Initializing UI_main...\n";
    UI_main ui(shotOptions);

    std::cout << "[Main] Calling main UI function render()...\n";
    int exitCode = ui.render();

    std::cout << "[Main] UI test completed successfully.\n";
    return exitCode;
}
