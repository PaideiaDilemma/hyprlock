#include "GreetdLogin.hpp"
#include "../config/ConfigManager.hpp"
#include "../core/hyprlock.hpp"
#include "src/auth/Auth.hpp"
#include "src/core/AnimationManager.hpp"

#include <cstddef>
#include <glaze/glaze.hpp>
#include <hyprutils/string/VarList.hpp>
#include <hyprutils/os/Process.hpp>
#include <sys/socket.h>
#include <sys/un.h>
#include <variant>

int socketConnect() {
    const int FD = socket(AF_UNIX, SOCK_STREAM, 0);
    if (FD < 0) {
        Debug::log(ERR, "Failed to create socket");
        return -1;
    }

    sockaddr_un serverAddress = {0};
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

int sendToSock(int fd, const std::string& PAYLOAD) {
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

std::string readFromSock(int fd) {
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
    std::string msg(len + 1, '\0');

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

bool sendGreetdRequest(int fd, const VGreetdRequest& request) {
    if (fd < 0) {
        Debug::log(ERR, "[GreetdLogin] Invalid socket fd");
        return false;
    }

    std::string payload;
    const auto  GLZERROR = glz::write_json(request, payload);
    if (GLZERROR != glz::error_code::none) {
        const auto GLZERRORSTR = glz::format_error(GLZERROR, payload);
        Debug::log(ERR, "Failed to serialize greetd request: {}", GLZERRORSTR);
        return false;
    }

    Debug::log(INFO, "[GreetdLogin] Request: {}", payload);

    if (sendToSock(fd, payload) < 0) {
        Debug::log(ERR, "[GreetdLogin] Failed to send payload to greetd");
        return false;
    }

    return true;
}

std::optional<VGreetdResponse> CGreetdLogin::request(const VGreetdRequest& req) {
    if (!sendGreetdRequest(m_iSocketFD, req)) {
        m_bOk = false;
        return std::nullopt;
    }

    const auto RESPONSESTR = readFromSock(m_iSocketFD);
    if (RESPONSESTR.empty()) {
        Debug::log(ERR, "[GreetdLogin] Failed to read response from greetd");
        m_bOk = false;
        return std::nullopt;
    }

    Debug::log(INFO, "[GreetdLogin] Response: {}", RESPONSESTR);

    VGreetdResponse res;
    const auto      GLZERROR = glz::read_json(res, RESPONSESTR);
    if (GLZERROR != glz::error_code::none) {
        const auto GLZERRORSTR = glz::format_error(GLZERROR, RESPONSESTR);
        Debug::log(ERR, "Failed to parse response from greetd: {}", GLZERRORSTR);
        m_bOk = false;
        return std::nullopt;
    }

    return res;
}

void CGreetdLogin::startSession() {
    m_sAuthState.startingSession = true;

    SGreetdStartSession startSession;
    // TODO: not like this
    Hyprutils::String::CVarList args(g_pHyprlock->m_sGreetdLoginSessionState.vLoginSessions[g_pHyprlock->m_sGreetdLoginSessionState.iSelectedLoginSession].exec, 0, ' ');
    startSession.cmd = std::vector<std::string>(args.begin(), args.end());

    if (!sendGreetdRequest(m_iSocketFD, VGreetdRequest{startSession}))
        m_bOk = false;
    else {
        if (g_pHyprlock->m_sCurrentDesktop == "Hyprland")
            g_pHyprlock->spawnSync("hyprctl dispatch exit");
        else
            g_pAuth->enqueueUnlock();
    }
}

void CGreetdLogin::cancelSession() {
    SGreetdCancelSession cancelSession;

    const auto           RESPONSE = request(VGreetdRequest{cancelSession});
    if (!RESPONSE.has_value())
        m_bOk = false;

    // ?? For some reason cancelSession is sometimes answered by an OS Error 111
}

void CGreetdLogin::createSession() {
    SGreetdCreateSession createSession;
    createSession.username = m_sLoginUserName;

    const auto RESPONSE = request(VGreetdRequest{createSession});

    if (!RESPONSE.has_value()) {
        m_bOk = false;
        return;
    }

    if (std::holds_alternative<SGreetdErrorResponse>(RESPONSE.value())) {
        // try to restart
        cancelSession();
        const auto RESPONSE = request(VGreetdRequest{createSession});
        if (!RESPONSE.has_value() || std::holds_alternative<SGreetdErrorResponse>(RESPONSE.value()))
            m_bOk = false;
    } else if (std::holds_alternative<SGreetdSuccessResponse>(RESPONSE.value()))
        startSession();
    else if (std::holds_alternative<SGreetdAuthMessageResponse>(RESPONSE.value()))
        m_sAuthState.message = std::get<SGreetdAuthMessageResponse>(RESPONSE.value());
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

    static const auto LOGINUSER = g_pConfigManager->getValue<Hyprlang::STRING>("login:user");
    m_sLoginUserName            = *LOGINUSER;
    createSession();
}

void CGreetdLogin::handleInput(const std::string& input) {
    if (!m_bOk)
        return;

    // We have some auth problem
    if (m_sAuthState.message.auth_message_type == GREETD_AUTH_ERROR) {
        g_pAuth->enqueueFail(m_sAuthState.message.auth_message, AUTH_IMPL_GREETD);
        recreateSession();
        return;
    }

    // Greetd sends some info messages regarding auth (usually directly from pam itself)
    while (m_sAuthState.message.auth_message_type == GREETD_AUTH_INFO) {
        const auto RESPONSE = request(VGreetdRequest{SGreetdPostAuthMessageResponse{}});
        if (!RESPONSE.has_value()) {
            m_bOk = false;
            return;
        } else if (std::holds_alternative<SGreetdErrorResponse>(RESPONSE.value())) {
            m_sAuthState.error = std::get<SGreetdErrorResponse>(RESPONSE.value());
            g_pAuth->enqueueFail(m_sAuthState.error.description, AUTH_IMPL_GREETD);
            recreateSession();
            return;
        } else if (std::holds_alternative<SGreetdSuccessResponse>(RESPONSE.value())) {
            startSession();
            return;
        }

        m_sAuthState.message = std::get<SGreetdAuthMessageResponse>(RESPONSE.value());
    }

    // We have a auth_message_type of GREETD_AUTH_VISIBLE or GREETD_AUTH_SECRET and respond to it
    SGreetdPostAuthMessageResponse postAuthMessageResponse;
    postAuthMessageResponse.response = input;

    const auto RESPONSE = request(VGreetdRequest{postAuthMessageResponse});
    if (!RESPONSE.has_value())
        m_bOk = false;
    else if (std::holds_alternative<SGreetdErrorResponse>(RESPONSE.value())) {
        m_sAuthState.error = std::get<SGreetdErrorResponse>(RESPONSE.value());
        if (m_sAuthState.error.error_type == GREETD_ERROR_AUTH)
            g_pAuth->enqueueFail(m_sAuthState.error.description, AUTH_IMPL_GREETD);
        recreateSession();
    } else if (std::holds_alternative<SGreetdSuccessResponse>(RESPONSE.value())) {
        startSession();
    }
};

bool CGreetdLogin::checkWaiting() {
    return m_sAuthState.startingSession;
}

std::optional<std::string> CGreetdLogin::getLastFailText() {
    if (!m_sAuthState.error.description.empty()) {
        return m_sAuthState.error.description;
    } else if (m_sAuthState.message.auth_message_type == GREETD_AUTH_ERROR)
        return m_sAuthState.message.auth_message;

    return std::nullopt;
}

std::optional<std::string> CGreetdLogin::getLastPrompt() {
    if (!m_sAuthState.message.auth_message.empty())
        return m_sAuthState.message.auth_message;
    return std::nullopt;
}

void CGreetdLogin::terminate() {
    if (m_iSocketFD > 0)
        close(m_iSocketFD);

    m_iSocketFD = -1;
}
