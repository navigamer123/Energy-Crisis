#include <algorithm>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#include "UI/includes/UI_lang.h"
#include "UI/includes/UI_main.h"
#include "UI/includes/UI_settings.h"
#include "UI/includes/UI_arcadeMode.h"

namespace {

// The UI loads its assets with relative paths ("assets/font.ttf",
// "assets/background.png"). Resolve them against the directory of the
// executable first, so the game also works when it is started from another
// working directory (shortcut, IDE, build folder); fall back to the current
// working directory when the executable directory has no assets/ folder.
#if !defined(__ANDROID__)
void selectAssetDirectory(const char *argv0) {
  namespace fs = std::filesystem;
  try {
    std::error_code ec;

    fs::path exePath;
#if defined(__APPLE__)
    char applePath[1024];
    uint32_t appleSize = sizeof(applePath);
    if (_NSGetExecutablePath(applePath, &appleSize) == 0) {
      exePath = fs::canonical(applePath, ec);
      if (ec)
        exePath = applePath;
    }
#else
    exePath =
        fs::read_symlink("/proc/self/exe", ec); // Linux: exact executable path
#endif

    if (ec || exePath.empty()) {
      ec.clear();
      exePath.clear();
      if (argv0 != nullptr && argv0[0] != '\0') {
        exePath = fs::absolute(fs::path(argv0), ec);
        if (ec)
          exePath.clear();
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
          std::cout << "[Main] Assets folder: " << (exeDir / "assets").string()
                    << "\n";
          return;
        }
      }
    }

    ec.clear();
    if (fs::exists(fontRel, ec)) {
      std::cout << "[Main] Assets folder: "
                << (fs::current_path(ec) / "assets").string() << "\n";
    } else {
      ec.clear();
      std::cerr << "[Main] ERROR: assets/font.ttf was not found next to the "
                   "executable ("
                << (exeDir.empty() ? std::string("?") : exeDir.string())
                << ") or in the working directory ("
                << fs::current_path(ec).string()
                << "). Text will be missing.\n";
    }
  } catch (const std::exception &e) {
    // Path conversion problems must never stop the game; keep the current
    // working directory
    std::cerr << "[Main] Warning: could not resolve the assets folder ("
              << e.what() << ").\n";
  }
}
#endif

void printUsage() {
  std::cout << "Usage: energy_crisis [--shot <out.png> | --record <dir>] "
               "[--scene NAME] [--frames N] [--seed S] [--lint]\n"
            << "  --shot <out.png>  render N frames of a scene, save the "
               "window to a PNG and exit\n"
            << "  --record <dir>    save every K-th of the N frames as "
               "<dir>/frame_00001.png ... and exit\n"
            << "  --every K         with --record: keep every K-th frame "
               "(default: 2, i.e. 30 fps)\n"
            << "  --scene NAME      " << ui::shot::sceneNames()
            << " (default: game)\n"
            << "  --frames N        frames rendered before the capture "
               "(default: 90)\n"
            << "  --seed S          match seed for reproducible captures "
               "(default: 1)\n"
            << "  --lint            print text layout problems; exit code = "
               "number of problems\n";
}

// Parses the screenshot / lint options. Returns false (after printing why) on a
// bad command line.
bool parseShotOptions(int argc, char *argv[], ShotOptions &opts) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto needValue = [&](const char *name) -> const char * {
      if (i + 1 >= argc) {
        std::cerr << "[Main] " << name << " needs a value.\n";
        return nullptr;
      }
      return argv[++i];
    };
    if (arg == "--shot") {
      const char *v = needValue("--shot");
      if (!v)
        return false;
      opts.outPath = v;
      opts.enabled = true;
    } else if (arg == "--record") {
      const char *v = needValue("--record");
      if (!v)
        return false;
      std::error_code ec; // absolute now: the working directory moves to the
                          // assets folder later
      std::filesystem::path dir = std::filesystem::absolute(v, ec);
      opts.recordDir = ec ? std::string(v) : dir.string();
      opts.enabled = true;
    } else if (arg == "--every") {
      const char *v = needValue("--every");
      if (!v)
        return false;
      opts.recordEvery = std::max(1, std::atoi(v));
    } else if (arg == "--scene") {
      const char *v = needValue("--scene");
      if (!v)
        return false;
      opts.scene = v;
      if (!ui::shot::isKnownScene(opts.scene)) {
        std::cerr << "[Main] Unknown scene \"" << opts.scene
                  << "\". Scenes: " << ui::shot::sceneNames() << "\n";
        return false;
      }
    } else if (arg == "--frames") {
      const char *v = needValue("--frames");
      if (!v)
        return false;
      opts.frames = std::max(1, std::atoi(v));
    } else if (arg == "--seed") {
      const char *v = needValue("--seed");
      if (!v)
        return false;
      opts.seed = static_cast<unsigned int>(std::strtoul(v, nullptr, 10));
    } else if (arg == "--lint") {
      opts.lint = true;
      opts.enabled = true;
    } else if (arg.rfind("-language=", 0) == 0 ||
               arg.rfind("--language=", 0) == 0) {
      // Handled by detectStartupLanguage
    } else if (arg == "--arcade" || arg == "-arcade" || arg == "--pc" || arg == "-pc") {
      // Platform selector, processed in main()
    } else if (arg == "--emulation" || arg == "-emulation" || arg == "--bot-vs-bot" || arg == "-bot-vs-bot" || arg == "--demo") {
      // Bot vs bot emulation test mode, processed in main()
    } else if (arg.rfind("--days=", 0) == 0 || arg.rfind("-days=", 0) == 0) {
      // Arcade days override, processed in main()
    } else if (arg.rfind("--seconds=", 0) == 0 || arg.rfind("-seconds=", 0) == 0) {
      // Arcade day duration override, processed in main()
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

std::string detectStartupLanguage(int argc, char *argv[]) {
  // 1. Check for command-line language argument (-language=en / -language=bg)
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg.rfind("-language=", 0) == 0 || arg.rfind("--language=", 0) == 0) {
      size_t eq = arg.find('=');
      std::string lang = arg.substr(eq + 1);
      if (lang == "en" || lang == "bg")
        return lang;
    } else if (arg == "-language" || arg == "--language") {
      if (i + 1 < argc) {
        std::string lang = argv[i + 1];
        if (lang == "en" || lang == "bg")
          return lang;
      }
    }
  }

  // 2. Bulgarian by default (only English if launched with specific flag)
  return "bg";
}

} // namespace

int main(int argc, char *argv[]) {
  std::cout << "========================================\n";
  std::cout << "    Energy Crisis - Game UI Test        \n";
  std::cout << "========================================\n";

  ShotOptions shotOptions;
  if (!parseShotOptions(argc, argv, shotOptions)) {
    return 2;
  }

  bool arcadeMode = false;
  bool emulationMode = false;
#if defined(ARCADE_MODE) && (ARCADE_MODE != 0)
  arcadeMode = true;
#endif
  if (argc > 0 && argv[0] && std::string(argv[0]).find("Arcade") != std::string::npos) {
    arcadeMode = true;
  }
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--arcade" || arg == "-arcade") {
      arcadeMode = true;
    } else if (arg == "--pc" || arg == "-pc") {
      arcadeMode = false;
    } else if (arg == "--emulation" || arg == "-emulation" || arg == "--bot-vs-bot" || arg == "-bot-vs-bot" || arg == "--demo") {
      emulationMode = true;
    } else if (arg.rfind("--days=", 0) == 0 || arg.rfind("-days=", 0) == 0) {
      size_t eq = arg.find('=');
      std::string d = arg.substr(eq + 1);
      setenv("ARCADE_DAYS", d.c_str(), 1);
    } else if (arg.rfind("--seconds=", 0) == 0 || arg.rfind("-seconds=", 0) == 0) {
      size_t eq = arg.find('=');
      std::string s = arg.substr(eq + 1);
      setenv("ARCADE_DAY_SECONDS", s.c_str(), 1);
    }
  }
  ArcadeMode::setEnabled(arcadeMode);
  std::cout << "[Main] Platform mode: " << (ArcadeMode::isEnabled() ? "ARCADE CABINET" : "PC DESKTOP") << "\n";
  if (emulationMode) {
    std::cout << "[Main] Mode: BOT VS BOT EMULATION / ARCADE TEST\n";
  }

#if !defined(__ANDROID__)
  selectAssetDirectory(argc > 0 ? argv[0] : nullptr);
#endif

  std::string startupLang = detectStartupLanguage(argc, argv);
  std::cout << "[Main] Selected startup language: " << startupLang << "\n";

  std::cout << "[Main] Loading game settings...\n";
  UI_settings::get().load();
  UI_settings::get().setLanguage(startupLang);
  Lang::load(startupLang);

  std::cout << "[Main] Initializing UI_main...\n";
  UI_main ui(shotOptions);
  if (emulationMode) {
    ui.setEmulationMode(true);
  }

  std::cout << "[Main] Calling main UI function render()...\n";
  int exitCode = ui.render();

  std::cout << "[Main] UI test completed successfully.\n";
  return exitCode;
}
