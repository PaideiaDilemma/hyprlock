
#include "config/ConfigManager.hpp"
#include "core/hyprlock.hpp"
#include "helpers/Log.hpp"
#include "helpers/MiscFunctions.hpp"
#include "core/AnimationManager.hpp"
#include <cstddef>
#include <string_view>

void help() {
    std::println("Usage: hyprlock [options]\n\n"
                 "Options:\n"
                 "  -g, --greetd             - Start hyprlock for session login via greetd\n"
                 "  -v, --verbose            - Enable verbose logging\n"
                 "  -q, --quiet              - Disable logging\n"
                 "  -c FILE, --config FILE   - Specify config file to use\n"
                 "  --display NAME           - Specify the Wayland display to connect to\n"
                 "  --immediate              - Lock immediately, ignoring any configured grace period\n"
                 "  --immediate-render       - Do not wait for resources before drawing the background\n"
                 "  --no-fade-in             - Disable the fade-in animation when the lock screen appears\n"
                 "  --session-dirs DIR1,DIR2 - Specify additional directories to search for session files\n"
                 "  -V, --version            - Show version information\n"
                 "  -h, --help               - Show this help message");
}

std::optional<std::string> parseArg(const std::vector<std::string>& args, const std::string& flag, std::size_t& i) {
    if (i + 1 < args.size()) {
        return args[++i];
    } else {
        std::println(stderr, "Error: Missing value for {} option.", flag);
        return std::nullopt;
    }
}

static void printVersion() {
    constexpr bool ISTAGGEDRELEASE = std::string_view(HYPRLOCK_COMMIT) == HYPRLOCK_VERSION_COMMIT;
    if (ISTAGGEDRELEASE)
        std::println("Hyprlock version v{}", HYPRLOCK_VERSION);
    else
        std::println("Hyprlock version v{} (commit {})", HYPRLOCK_VERSION, HYPRLOCK_COMMIT);
}

int main(int argc, char** argv, char** envp) {
    std::string              configPath;
    std::string              wlDisplay;
    bool                     immediate       = false;
    bool                     immediateRender = false;
    bool                     noFadeIn        = false;
    bool                     greetdLogin     = false;

    std::vector<std::string> args(argv, argv + argc);

    // Used for greetd login
    std::string                      additionalSessionDirs;
    std::vector<SLoginSessionConfig> loginSessions;

    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            help();
            return 0;
        }

        if (arg == "--version" || arg == "-V") {
            printVersion();
            return 0;
        }

        if (arg == "--greetd" || arg == "-g") {
            greetdLogin     = true;
            immediate       = true;
            immediateRender = true;
        } else if (arg == "--session-dirs" && i + 1 < (std::size_t)argc) {
            if (auto value = parseArg(args, arg, i); value)
                additionalSessionDirs = *value;
            else
                return 1;
        } else if (arg == "--verbose" || arg == "-v")
            Debug::verbose = true;

        else if (arg == "--quiet" || arg == "-q")
            Debug::quiet = true;

        else if ((arg == "--config" || arg == "-c") && i + 1 < (std::size_t)argc) {
            if (auto value = parseArg(args, arg, i); value)
                configPath = *value;
            else
                return 1;

        } else if (arg == "--display" && i + 1 < (std::size_t)argc) {
            if (auto value = parseArg(args, arg, i); value)
                wlDisplay = *value;
            else
                return 1;

        } else if (arg == "--immediate")
            immediate = true;

        else if (arg == "--immediate-render")
            immediateRender = true;

        else if (arg == "--no-fade-in")
            noFadeIn = true;

        else {
            std::println(stderr, "Unknown option: {}", arg);
            help();
            return 1;
        }
    }

    printVersion();
    g_pAnimationManager = makeUnique<CHyprlockAnimationManager>();

    try {
        g_pConfigManager = makeUnique<CConfigManager>(configPath);
        g_pConfigManager->init();
    } catch (const std::exception& ex) {
        Debug::log(CRIT, "ConfigManager threw: {}", ex.what());
        if (std::string(ex.what()).contains("File does not exist"))
            Debug::log(NONE, "           Make sure you have a config.");

        return 1;
    }

    if (noFadeIn)
        g_pConfigManager->m_AnimationTree.setConfigForNode("fadeIn", false, 0.f, "default");

    if (greetdLogin) {
        CVarList sessionDirs{additionalSessionDirs, 0, ',', true};
        loginSessions                 = gatherSessions(std::vector<std::string>{sessionDirs.begin(), sessionDirs.end()});
        const auto CONFIGUREDSESSIONS = g_pConfigManager->getLoginSessionConfigs();
        loginSessions.insert(loginSessions.end(), CONFIGUREDSESSIONS.begin(), CONFIGUREDSESSIONS.end());
    }

    try {
        g_pHyprlock = makeUnique<CHyprlock>(wlDisplay, immediate, immediateRender, greetdLogin, loginSessions);
        g_pHyprlock->run();
    } catch (const std::exception& ex) {
        Debug::log(CRIT, "Hyprlock threw: {}", ex.what());
        return 1;
    }

    return 0;
}
