#pragma once

#include "Auth.hpp"

#include <condition_variable>
#include <optional>
#include <string>
#include <hyprlang.hpp>
#include <thread>

class CSodiumPWHash : public IAuthImplementation {
  public:
    CSodiumPWHash();
    ~CSodiumPWHash() override;

    eAuthImplementations getImplType() override {
        return AUTH_IMPL_CUSTOM;
    }
    void                       init() override;
    void                       handleInput(const SensitiveString& input) override;
    bool                       checkWaiting() override;
    std::optional<std::string> getLastFailText() override;
    std::optional<std::string> getLastPrompt() override;
    void                       terminate() override;

  private:
    void* const* getConfigValuePtr(const std::string& name);

    struct {
        std::condition_variable requestCV;
        SensitiveString         input;
        bool                    requested = false;
        std::mutex              requestMutex;
        std::string             failText;
    } m_sCheckerState;

    std::thread       m_checkerThread;
    void              checkerLoop();

    Hyprlang::CConfig m_config;
};
