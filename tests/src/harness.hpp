#pragma once

#include "wayland.hpp"
#include "hyprland-lock-notify-v1.hpp"

#include <cstdint>
#include <string>
#include <hyprutils/memory/SharedPtr.hpp>

#define SP Hyprutils::Memory::CSharedPointer

namespace NTestSessionLock {
    enum eTestResult : uint8_t {
        OK,
        BAD_ENVIRONMENT,
        PREMATURE_UNLOCK,
        LOCK_TIMEOUT,
        UNLOCK_TIMEOUT,
        CRASH,
    };

    static const char* testResultString(eTestResult res) {
        switch (res) {
            case eTestResult::OK: return "OK"; break;
            case eTestResult::BAD_ENVIRONMENT: return "BAD_ENVIRONMENT"; break;
            case eTestResult::PREMATURE_UNLOCK: return "PREMATURE_UNLOCK"; break;
            case eTestResult::UNLOCK_TIMEOUT: return "LOCK_TIMEOUT"; break;
            case eTestResult::CRASH: return "CRASH"; break;
            default: return "???";
        }
    }

    struct SSesssionLockTest {
        std::string m_clientPath = "";
        std::string m_configPath = "";

        uint32_t    m_timeoutMs = 10000;
    };

    struct SSessionLockState {
        SP<CCHyprlandLockNotifierV1>     m_lockNotifier     = nullptr;
        SP<CCHyprlandLockNotificationV1> m_lockNotification = nullptr;
        bool                             m_didLock          = false;
        bool                             m_didUnlock        = false;
    };

    eTestResult run(const SSesssionLockTest& test);
};
