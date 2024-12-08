#include "Auth.hpp"
#include "Pam.hpp"
#include "Fingerprint.hpp"
#include "../config/ConfigManager.hpp"
#include "../core/hyprlock.hpp"
#include "src/helpers/Log.hpp"

#include <hyprlang.hpp>
#include <memory>

CAuth::CAuth() {
    m_vImpls.push_back(std::make_shared<CPam>());
    static auto* const PENABLEFINGERPRINT = (Hyprlang::INT* const*)g_pConfigManager->getValuePtr("general:enable_fingerprint");
    if (**PENABLEFINGERPRINT)
        m_vImpls.push_back(std::make_shared<CFingerprint>());
}

void CAuth::start() {
    for (const auto& i : m_vImpls) {
        i->init();
    }
}

void CAuth::submitInput(const std::string& input) {
    for (const auto& i : m_vImpls) {
        i->handleInput(input);
    }
}

bool CAuth::isAuthenticated() {
    for (const auto& i : m_vImpls) {
        if (i->isAuthenticated())
            return true;
    }

    return false;
}

bool CAuth::checkWaiting() {
    for (const auto& i : m_vImpls) {
        if (i->checkWaiting())
            return true;
    }

    return false;
}

std::string CAuth::getInlineFeedback() {
    for (const auto& i : m_vImpls) {
        const auto FEEDBACK = (m_bDisplayFailText) ? i->getLastFailText() : i->getLastPrompt();
        if (FEEDBACK.has_value())
            return FEEDBACK.value();
    }

    return "Ups, empty authentication feedack";
}

std::optional<std::string> CAuth::getFailText(eAuthImplementations implType) {
    for (const auto& i : m_vImpls) {
        if (i->getImplType() == implType) {
            return i->getLastFailText();
        }
    }
    return std::nullopt;
}

std::optional<std::string> CAuth::getPrompt(eAuthImplementations implType) {
    for (const auto& i : m_vImpls) {
        if (i->getImplType() == implType) {
            return i->getLastPrompt();
        }
    }
    return std::nullopt;
}

std::shared_ptr<IAuthImplementation> CAuth::getImpl(eAuthImplementations implType) {
    for (const auto& i : m_vImpls) {
        if (i->getImplType() == implType)
            return i;
    }

    return nullptr;
}

void CAuth::terminate() {
    for (const auto& i : m_vImpls) {
        i->terminate();
    }
}

static void passwordCheckTimerCallback(std::shared_ptr<CTimer> self, void* data) {
    // check result
    if (g_pAuth->isAuthenticated()) {
        g_pHyprlock->unlock();
    } else {
        g_pHyprlock->clearPasswordBuffer();
        g_pAuth->m_iFailedAttempts += 1;
        Debug::log(LOG, "Failed attempts: {}", g_pAuth->m_iFailedAttempts);

        g_pAuth->m_bDisplayFailText = true;
        g_pHyprlock->enqueueForceUpdateTimers();

        g_pHyprlock->renderAllOutputs();
    }
}

void CAuth::enqueueCheckAuthenticated() {
    g_pHyprlock->addTimer(std::chrono::milliseconds(1), passwordCheckTimerCallback, nullptr);
}
