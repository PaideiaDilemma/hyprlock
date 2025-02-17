#include "GreetdLogin.hpp"
#include "../config/ConfigManager.hpp"
#include "../core/hyprlock.hpp"
#include "../helpers/NotJson.hpp"
#include "../helpers/Log.hpp"

#include <hyprutils/string/VarList.hpp>
#include <hyprutils/os/Process.hpp>
#include <sys/socket.h>
#include <sys/un.h>

// TODO: move to it's own thread

static constexpr eGreetdAuthMessageType messageTypeFromString(const std::string_view& type) {
    if (type == "visible")
        return GREETD_AUTH_VISIBLE;
    if (type == "secret")
        return GREETD_AUTH_SECRET;
    if (type == "info")
        return GREETD_AUTH_INFO;
    if (type == "error")
        return GREETD_AUTH_ERROR;
    return GREETD_AUTH_ERROR;
}

static constexpr eGreetdErrorMessageType errorTypeFromString(const std::string_view& type) {
    if (type == "auth_error")
        return GREETD_ERROR_AUTH;
    if (type == "error")
        return GREETD_ERROR;
    return GREETD_ERROR;
}

static constexpr std::string getErrorString(eRequestError error) {
    switch (error) {
        case GREETD_REQUEST_ERROR_SEND: return "Failed to send payload to greetd";
        case GREETD_REQUEST_ERROR_READ: return "Failed to read response from greetd";
        case GREETD_REQUEST_ERROR_PARSE: return "Failed to parse response from greetd";
        case GREETD_REQUEST_ERROR_FORMAT: return "Invalid greetd response";
        default: return "Unknown error";
    }
};

static int socketConnect() {
    const int FD = socket(AF_UNIX, SOCK_STREAM, 0);
    if (FD < 0) {
        Debug::log(ERR, "Failed to create socket");
        return -1;
    }

    sockaddr_un serverAddress = {.sun_family = 0};
    serverAddress.sun_family  = AF_UNIX;

    const auto PGREETDSOCK = std::getenv("GREETD_SOCK");

    if (PGREETDSOCK == nullptr) {
        Debug::log(ERR, "GREETD_SOCK not set!");
        return -1;
    }

    strncpy(serverAddress.sun_path, PGREETDSOCK, sizeof(serverAddress.sun_path) - 1);

    if (connect(FD, (sockaddr*)&serverAddress, SUN_LEN(&serverAddress)) < 0) {
        Debug::log(ERR, "Failed to connect to greetd socket");
        return -1;
    }

    return FD;
}

static int sendToSock(int fd, const std::string& PAYLOAD) {
    const uint32_t LEN   = PAYLOAD.size();
    uint32_t       wrote = 0;

    while (wrote < sizeof(LEN)) {
        auto n = write(fd, (char*)&LEN + wrote, sizeof(LEN) - wrote);
        if (n < 1) {
            Debug::log(ERR, "Failed to write to greetd socket");
            return -1;
        }

        wrote += n;
    }

    wrote = 0;

    while (wrote < LEN) {
        auto n = write(fd, PAYLOAD.c_str() + wrote, LEN - wrote);
        if (n < 1) {
            Debug::log(ERR, "Failed to write to greetd socket");
            return -1;
        }

        wrote += n;
    }

    return 0;
}

static std::string readFromSock(int fd) {
    uint32_t len     = 0;
    uint32_t numRead = 0;

    while (numRead < sizeof(len)) {
        auto n = read(fd, (char*)&len + numRead, sizeof(len) - numRead);
        if (n < 1) {
            Debug::log(ERR, "Failed to read from greetd socket");
            return "";
        }

        numRead += n;
    }

    numRead = 0;
    std::string msg(len, '\0');

    while (numRead < len) {
        auto n = read(fd, msg.data() + numRead, len - numRead);
        if (n < 1) {
            Debug::log(ERR, "Failed to read from greetd socket");
            return "";
        }

        numRead += n;
    }

    return msg;
}

static bool sendGreetdRequest(int fd, const NNotJson::SObject& request) {
    if (fd < 0) {
        Debug::log(ERR, "[GreetdLogin] Invalid socket fd");
        return false;
    }

    const auto PAYLOAD = NNotJson::serialize(request);

    Debug::log(TRACE, "[GreetdLogin] Request: {}", PAYLOAD);

    if (sendToSock(fd, PAYLOAD) < 0) {
        Debug::log(ERR, "[GreetdLogin] Failed to send payload to greetd");
        return false;
    }

    return true;
}

std::expected<NNotJson::SObject, eRequestError> CGreetdLogin::request(const NNotJson::SObject& req) {
    if (!sendGreetdRequest(m_iSocketFD, req)) {
        m_bOk = false;
        return std::unexpected(GREETD_REQUEST_ERROR_SEND);
    }

    const auto RESPONSESTR = readFromSock(m_iSocketFD);
    if (RESPONSESTR.empty()) {
        Debug::log(ERR, "[GreetdLogin] Failed to read response from greetd");
        m_bOk = false;
        return std::unexpected(GREETD_REQUEST_ERROR_READ);
    }

    Debug::log(TRACE, "[GreetdLogin] Response: {}", RESPONSESTR);

    const auto [RESULTOBJ, ERROR] = NNotJson::parse(RESPONSESTR);
    if (ERROR.status != NNotJson::SError::NOT_JSON_OK) {
        Debug::log(ERR, "[GreetdLogin] Failed to parse response from greetd: {}", ERROR.message);
        m_bOk = false;
        return std::unexpected(GREETD_REQUEST_ERROR_PARSE);
    }

    if (!RESULTOBJ.values.contains("type")) {
        Debug::log(ERR, "[GreetdLogin] Invalid greetd response");
        m_bOk = false;
        return std::unexpected(GREETD_REQUEST_ERROR_PARSE);
    }

    return RESULTOBJ;
}

void CGreetdLogin::startSession() {
    m_sAuthState.startingSession = true;

    // TODO: not like this
    Hyprutils::String::CVarList args(g_pHyprlock->m_sGreetdLoginSessionState.vLoginSessions[g_pHyprlock->m_sGreetdLoginSessionState.iSelectedLoginSession].exec, 0, ' ');

    NNotJson::SObject           startSession{.values = {
                                       {"type", "start_session"},
                                   }};
    startSession.values["cmd"] = std::vector<std::string>{args.begin(), args.end()};

    if (!sendGreetdRequest(m_iSocketFD, startSession))
        m_bOk = false;
    else {
        if (g_pHyprlock->m_sCurrentDesktop == "Hyprland")
            g_pHyprlock->spawnSync("hyprctl dispatch exit");
        else
            g_pAuth->enqueueUnlock();
    }
}

void CGreetdLogin::cancelSession() {
    NNotJson::SObject cancelSession{
        .values =
            {
                {"type", "cancel_session"},
            },
    };

    const auto RESPONSE = request(cancelSession);
    if (!RESPONSE.has_value())
        m_bOk = false;

    // ?? For some reason cancelSession is sometimes answered by an OS Error 111
}

inline static const std::string& getType(NNotJson::SObject& obj) {
    return std::get<std::string>(obj.values["type"]);
}

void CGreetdLogin::createSession() {
    NNotJson::SObject createSession = {
        .values =
            {
                {"type", "create_session"},
                {"username", m_sLoginUserName},
            },
    };

    Debug::log(INFO, "Creating session for user {}", m_sLoginUserName);

    auto RESPONSEOPT = request(createSession);
    if (!RESPONSEOPT.has_value()) {
        Debug::log(ERR, "Failed to create session: {}", getErrorString(RESPONSEOPT.error()));
        m_bOk = false;
        return;
    }

    if (getType(RESPONSEOPT.value()) == "error") {
        // try to restart
        cancelSession();
        auto RESPONSEOPT = request(createSession);
        if (!RESPONSEOPT.has_value()) {
            Debug::log(ERR, "Failed to create session: {}", getErrorString(RESPONSEOPT.error()));
            m_bOk = false;
            return;
        }
        if (getType(RESPONSEOPT.value()) == "error")
            m_bOk = false;
    } else if (getType(RESPONSEOPT.value()) == "auth_message") {
        m_sAuthState.authMessageType = messageTypeFromString(std::get<std::string>(RESPONSEOPT.value().values["auth_message_type"]));
        m_sAuthState.message         = std::get<std::string>(RESPONSEOPT.value().values["auth_message"]);
    } else if (getType(RESPONSEOPT.value()) == "success")
        startSession();
    else
        Debug::log(ERR, "Unexpected response from greetd");
}

void CGreetdLogin::recreateSession() {
    cancelSession();
    createSession();
}

void CGreetdLogin::init() {
    m_iSocketFD = socketConnect();
    if (m_iSocketFD < 0) {
        m_bOk = false;
        return;
    }

    const auto LOGINUSER = g_pConfigManager->getValue<Hyprlang::STRING>("login:user");
    m_sLoginUserName     = *LOGINUSER;

    Debug::log(LOG, "Login user: {}", m_sLoginUserName);

    createSession();
}

void CGreetdLogin::handleInput(const std::string& input) {
    if (!m_bOk)
        return;

    // We have some auth problem
    if (m_sAuthState.authMessageType == GREETD_AUTH_ERROR) {
        g_pAuth->enqueueFail(m_sAuthState.message, AUTH_IMPL_GREETD);
        recreateSession();
        return;
    }

    // Greetd sends some info messages regarding auth (usually directly from pam itself)
    while (m_sAuthState.authMessageType == GREETD_AUTH_INFO) {
        // Empty reply
        NNotJson::SObject postAuthMessageResponse{
            .values =
                {
                    {"type", "post_auth_message_response"},
                    {"response", ""},
                },
        };
        auto RESPONSEOPT = request(postAuthMessageResponse);
        if (!RESPONSEOPT.has_value()) {
            Debug::log(ERR, "Failed to create session: {}", getErrorString(RESPONSEOPT.error()));
            m_bOk = false;
            return;
        } else if (getType(RESPONSEOPT.value()) == "error") {
            m_sAuthState.error     = std::get<std::string>(RESPONSEOPT.value().values["description"]);
            m_sAuthState.errorType = errorTypeFromString(std::get<std::string>(RESPONSEOPT.value().values["error_type"]));
            if (!m_sAuthState.error.empty())
                g_pAuth->enqueueFail(m_sAuthState.error, AUTH_IMPL_GREETD);
            recreateSession();
            return;
        } else if (getType(RESPONSEOPT.value()) == "success") {
            startSession();
            return;
        } else if (getType(RESPONSEOPT.value()) == "auth_message") {
            m_sAuthState.authMessageType = messageTypeFromString(std::get<std::string>(RESPONSEOPT.value().values["auth_message_type"]));
            m_sAuthState.message         = std::get<std::string>(RESPONSEOPT.value().values["auth_message"]);
            if (!m_sAuthState.message.empty())
                g_pAuth->enqueueFail(m_sAuthState.message, AUTH_IMPL_GREETD);
            recreateSession();
            return;
        }
    }

    // We have a auth_message_type of GREETD_AUTH_VISIBLE or GREETD_AUTH_SECRET and respond to it
    NNotJson::SObject postAuthMessageResponse{
        .values =
            {
                {"type", "post_auth_message_response"},
                {"response", input},
            },
    };

    auto RESPONSEOPT = request(postAuthMessageResponse);
    if (!RESPONSEOPT.has_value()) {
        Debug::log(ERR, "Failed to create session: {}", getErrorString(RESPONSEOPT.error()));
        m_bOk = false;
    } else if (getType(RESPONSEOPT.value()) == "error") {
        m_sAuthState.error     = std::get<std::string>(RESPONSEOPT.value().values["description"]);
        m_sAuthState.errorType = errorTypeFromString(std::get<std::string>(RESPONSEOPT.value().values["error_type"]));
        if (m_sAuthState.errorType == eGreetdErrorMessageType::GREETD_ERROR_AUTH && !m_sAuthState.error.empty())
            g_pAuth->enqueueFail(m_sAuthState.error, AUTH_IMPL_GREETD);
        recreateSession();
    } else if (getType(RESPONSEOPT.value()) == "success") {
        startSession();
    }

    // More auth messages? fallthrough
};

bool CGreetdLogin::checkWaiting() {
    return m_sAuthState.startingSession;
}

std::optional<std::string> CGreetdLogin::getLastFailText() {
    if (!m_sAuthState.error.empty()) {
        return m_sAuthState.error;
    } else if (m_sAuthState.authMessageType == GREETD_AUTH_ERROR)
        return m_sAuthState.message;

    return std::nullopt;
}

std::optional<std::string> CGreetdLogin::getLastPrompt() {
    if (!m_sAuthState.message.empty())
        return m_sAuthState.message;
    return std::nullopt;
}

void CGreetdLogin::terminate() {
    if (m_iSocketFD > 0)
        close(m_iSocketFD);

    m_iSocketFD = -1;
}
