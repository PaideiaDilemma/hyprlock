#pragma once

#include "Auth.hpp"
#include "src/helpers/Log.hpp"
#include <cstdint>
#include <glaze/glaze.hpp>
#include <glaze/util/string_literal.hpp>
#include <string>
#include <string_view>
#include <optional>
#include <variant>

struct SGreetdCreateSession {
    const std::string type     = "create_session";
    std::string       username = "";
};

struct SGreetdPostAuthMessageResponse {
    const std::string type     = "post_auth_message_response";
    std::string       response = "";
};

struct SGreetdStartSession {
    const std::string        type = "start_session";
    std::vector<std::string> cmd;
    std::vector<std::string> env;
};

struct SGreetdCancelSession {
    const std::string type = "cancel_session";
};

enum eGreetdResponse {
    GREETD_RESPONSE_UNKNOWN = -1,
    GREETD_RESPONSE_SUCCESS = 0,
    GREETD_RESPONSE_ERROR   = 1,
    GREETD_RESPONSE_AUTH    = 2,
};

template <>
struct glz::meta<eGreetdResponse> {
    static constexpr auto value = enumerate("success", GREETD_RESPONSE_SUCCESS, "error", GREETD_RESPONSE_ERROR, "auth_message", GREETD_RESPONSE_AUTH);
};

enum eGreetdAuthMessageType {
    GREETD_AUTH_VISIBLE = 0,
    GREETD_AUTH_SECRET  = 1,
    GREETD_AUTH_INFO    = 2,
    GREETD_AUTH_ERROR   = 3,
};

template <>
struct glz::meta<eGreetdAuthMessageType> {
    static constexpr auto value = enumerate("visible", GREETD_AUTH_VISIBLE, "secret", GREETD_AUTH_SECRET, "info", GREETD_AUTH_INFO, "error", GREETD_AUTH_ERROR);
};

enum eGreetdErrorMessageTypes {
    GREETD_ERROR_AUTH = 0,
    GREETD_ERROR      = 1,
};

template <>
struct glz::meta<eGreetdErrorMessageTypes> {
    static constexpr auto value = enumerate("auth_error", GREETD_ERROR_AUTH, "error", GREETD_ERROR);
};

struct SGreetdSuccessResponse {
    char DUMMY; // Without any field in SGreetdSuccessResponse, I get unknown_key for "type".
};

struct SGreetdErrorResponse {
    eGreetdErrorMessageTypes error_type;
    std::string              description;
};

struct SGreetdAuthMessageResponse {
    eGreetdAuthMessageType auth_message_type;
    std::string            auth_message;
};

using VGreetdRequest  = std::variant<SGreetdCreateSession, SGreetdPostAuthMessageResponse, SGreetdStartSession, SGreetdCancelSession>;
using VGreetdResponse = std::variant<SGreetdSuccessResponse, SGreetdErrorResponse, SGreetdAuthMessageResponse>;

template <>
struct glz::meta<VGreetdResponse> {
    static constexpr std::string_view tag = "type";
    static constexpr std::array       ids{"success", "error", "auth_message"};
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
        SGreetdErrorResponse       error{};
        SGreetdAuthMessageResponse message{};

        bool                       startingSession = false;
    } m_sAuthState;

    friend class CAuth;

  private:
    std::optional<VGreetdResponse> request(const VGreetdRequest& req);
    void                           createSession();
    void                           cancelSession();
    void                           recreateSession();
    void                           startSession();
    int                            m_iSocketFD      = -1;
    std::string                    m_sLoginUserName = "";

    bool                           m_bOk = true;
};
