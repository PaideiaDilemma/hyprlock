#include "Sodium.hpp"

#include "../helpers/Log.hpp"
#include "src/core/hyprlock.hpp"

#include <filesystem>
#include <hyprutils/path/Path.hpp>
#include <sodium.h>

static std::string getSecretsConfigPath() {
    static const auto paths = Hyprutils::Path::findConfig("secrets");
    RASSERT(paths.first.has_value(), "[Sodium] Failed to find secrets.conf");
    // check permissions
    using std::filesystem::perms;
    const auto PERMS = std::filesystem::status(paths.first.value()).permissions();
    if ((PERMS & perms::group_read) != perms::none || (PERMS & perms::group_write) != perms::none || (PERMS & perms::others_read) != perms::none ||
        (PERMS & perms::others_write) != perms::none) {
        RASSERT(false, "[Sodium] secrets.conf has insecure permissions");
    }
    return paths.first.value();
}

void* const* CSodiumPWHash::getConfigValuePtr(const std::string& name) {
    return m_config.getConfigValuePtr(name.c_str())->getDataStaticPtr();
}

CSodiumPWHash::CSodiumPWHash() : m_config(getSecretsConfigPath().c_str(), {}) {
    m_config.addConfigValue("hyprlock:pw_hash", Hyprlang::STRING{""});
    m_config.commence();
    auto result = m_config.parse();

    if (result.error)
        Debug::log(ERR, "Config has errors:\n{}\nProceeding ignoring faulty entries", result.getError());

    m_checkerThread = std::thread([this]() { checkerLoop(); });
}

CSodiumPWHash::~CSodiumPWHash() {
    ;
}

void CSodiumPWHash::init() {
    RASSERT(sodium_init() >= 0, "Failed to initialize libsodium");
    //RASSERT(crypto_pwhash_str(out, testPW.c_str(), testPW.size(), crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) == 0, "asdf");
    //Debug::log(LOG, "Hash: {}", out);
}

void CSodiumPWHash::handleInput(const SensitiveString& input) {
    std::lock_guard<std::mutex> lk(m_sCheckerState.requestMutex);

    m_sCheckerState.input.set(input);
    m_sCheckerState.requested = true;

    m_sCheckerState.requestCV.notify_all();
}

bool CSodiumPWHash::checkWaiting() {
    return m_sCheckerState.requested;
}

std::optional<std::string> CSodiumPWHash::getLastFailText() {
    return m_sCheckerState.failText.empty() ? std::nullopt : std::optional<std::string>(m_sCheckerState.failText);
}

std::optional<std::string> CSodiumPWHash::getLastPrompt() {
    return "Password: ";
}

void CSodiumPWHash::terminate() {
    m_sCheckerState.requestCV.notify_all();
    if (m_checkerThread.joinable())
        m_checkerThread.join();
}

void CSodiumPWHash::checkerLoop() {
    static auto* const PPWHASH     = (Hyprlang::STRING*)getConfigValuePtr("hyprlock:pw_hash");
    const auto         PWHASH      = std::string(*PPWHASH);
    const bool         NEEDSREHASH = crypto_pwhash_str_needs_rehash(PWHASH.c_str(), crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0;

    while (!g_pHyprlock->isUnlocked()) {
        std::unique_lock<std::mutex> lk(m_sCheckerState.requestMutex);
        m_sCheckerState.requestCV.wait(lk, [this]() { return m_sCheckerState.requested || g_pHyprlock->m_bTerminate; });

        if (PWHASH.empty() || PWHASH.size() > crypto_pwhash_STRBYTES) {
            m_sCheckerState.failText = "Invalid password hash";
            Debug::log(ERR, "[Sodium] Invalid password hash set in secrets.conf");
        } else if (NEEDSREHASH) {
            m_sCheckerState.failText = "Password hash failed to meet requirements";
            Debug::log(ERR, "[Sodium] Password hash failed to meet requirements, please rehash.");
        } else if (crypto_pwhash_str_verify(PWHASH.c_str(), m_sCheckerState.input.c_str(), m_sCheckerState.input.length()) == 0) {
            g_pAuth->enqueueUnlock();
        } else {
            g_pAuth->enqueueFail();
            m_sCheckerState.failText = "Failed to authenticate";
            Debug::log(LOG, "[Sodium] Failed to authenticate");
        }

        m_sCheckerState.input.clear();
        m_sCheckerState.requested = false;
    }
}
