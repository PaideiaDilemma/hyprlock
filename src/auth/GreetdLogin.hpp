#pragma once

#include "Auth.hpp"
#include "../helpers/NotJson.hpp"
#include <string>
#include <expected>

// GREETD PROTOCOL
enum eGreetdResponse : uint8_t {
    GREETD_RESPONSE_UNKNOWN = 0xff,
    GREETD_RESPONSE_SUCCESS = 0,
    GREETD_RESPONSE_ERROR   = 1,
    GREETD_RESPONSE_AUTH    = 2,
};

enum eGreetdErrorMessageType : uint8_t {
    GREETD_ERROR_AUTH = 0,
    GREETD_ERROR      = 1,
};

enum eGreetdAuthMessageType : uint8_t {
    GREETD_AUTH_VISIBLE = 0,
    GREETD_AUTH_SECRET  = 1,
    GREETD_AUTH_INFO    = 2,
    GREETD_AUTH_ERROR   = 3,
};

// INTERNAL
enum eRequestError : uint8_t {
    GREETD_REQUEST_ERROR_SEND   = 0,
    GREETD_REQUEST_ERROR_READ   = 1,
    GREETD_REQUEST_ERROR_PARSE  = 2,
    GREETD_REQUEST_ERROR_FORMAT = 3,
};

class CGreetdLogin : public IAuthImplementation {
  public:
    virtual ~CGreetdLogin() = default;

    virtual eAuthImplementations getImplType() {
        return AUTH_IMPL_GREETD;
    }
    virtual void                       init();
    virtual void                       handleInput(const std::string& input);
    virtual bool                       checkWaiting();
    virtual std::optional<std::string> getLastFailText();
    virtual std::optional<std::string> getLastPrompt();
    virtual void                       terminate();

    struct {
        std::string             error = "";
        eGreetdErrorMessageType errorType;
        std::string             message = "";
        eGreetdAuthMessageType  authMessageType;

        bool                    startingSession = false;
    } m_sAuthState;

    friend class CAuth;

  private:
    std::expected<NNotJson::SObject, eRequestError> request(const NNotJson::SObject& req);

    //
    void        createSession();
    void        cancelSession();
    void        recreateSession();
    void        startSession();
    int         m_iSocketFD      = -1;
    std::string m_sLoginUserName = "";

    bool        m_bOk = true;
};
