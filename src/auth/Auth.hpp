#pragma once

#include "../helpers/SensitiveString.hpp"
#include <memory>
#include <optional>
#include <vector>

enum eAuthImplementations {
    AUTH_IMPL_PAM         = 0,
    AUTH_IMPL_FINGERPRINT = 1,
    AUTH_IMPL_CUSTOM      = 2,
};

class IAuthImplementation {
  public:
    virtual ~IAuthImplementation() = default;

    virtual eAuthImplementations       getImplType()                             = 0;
    virtual void                       init()                                    = 0;
    virtual void                       handleInput(const SensitiveString& input) = 0;
    virtual bool                       checkWaiting()                            = 0;
    virtual std::optional<std::string> getLastFailText()                         = 0;
    virtual std::optional<std::string> getLastPrompt()                           = 0;
    virtual void                       terminate()                               = 0;

    friend class CAuth;
};

class CAuth {
  public:
    CAuth();

    void start();

    void submitInput(const SensitiveString& input);
    bool checkWaiting();

    // Used by the PasswordInput field. We are constraint to a single line for the authentication feedback there.
    // Based on m_bDisplayFailText, this will return either the fail text or the prompt.
    // Based on m_eLastActiveImpl, it will select the implementation.
    std::string                          getInlineFeedback();

    std::optional<std::string>           getFailText(eAuthImplementations implType);
    std::optional<std::string>           getPrompt(eAuthImplementations implType);

    std::shared_ptr<IAuthImplementation> getImpl(eAuthImplementations implType);

    void                                 terminate();

    // Should only be set via the main thread
    bool   m_bDisplayFailText = false;
    size_t m_iFailedAttempts  = 0;

    void   enqueueUnlock();
    void   enqueueFail();
    void   postActivity(eAuthImplementations implType);

  private:
    std::vector<std::shared_ptr<IAuthImplementation>> m_vImpls;
    std::optional<eAuthImplementations>               m_eLastActiveImpl = std::nullopt;
};

inline std::unique_ptr<CAuth> g_pAuth;
