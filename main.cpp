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

} // namespace

int main(int argc, char* argv[]) {
    std::cout << "========================================\n";
    std::cout << "    Energy Crisis - Game UI Test        \n";
    std::cout << "========================================\n";

    selectAssetDirectory(argc > 0 ? argv[0] : nullptr);

    // [Team Demo / HX-02] --demo starts the judge demo right away (any key then returns to the menu)
    bool startDemo = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--demo") startDemo = true;
    }

    std::cout << "[Main] Initializing UI_main...\n";
    UI_main ui;
    if (startDemo) ui.startDemo();

    std::cout << "[Main] Calling main UI function render()...\n";
    ui.render();

    std::cout << "[Main] UI test completed successfully.\n";
    return 0;
}
