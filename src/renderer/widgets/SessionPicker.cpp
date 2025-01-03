#include "SessionPicker.hpp"
#include "../Renderer.hpp"
#include "../../helpers/Log.hpp"
#include "../../core/hyprlock.hpp"
#include "../../helpers/Color.hpp"
#include "../../config/ConfigDataValues.hpp"
#include "src/config/ConfigManager.hpp"
#include "src/core/AnimationManager.hpp"
#include <filesystem>
#include <hyprlang.hpp>
#include <hyprutils/math/Vector2D.hpp>
#include <stdexcept>
#include <fstream>

CSessionPicker::CSessionPicker(const Vector2D& viewport_, const std::unordered_map<std::string, std::any>& props) : shadow(this, props, viewport_) {
    try {
        configPos                     = CLayoutValueData::fromAnyPv(props.at("position"))->getAbsolute(viewport_);
        size                          = CLayoutValueData::fromAnyPv(props.at("size"))->getAbsolute(viewport_);
        rounding                      = std::any_cast<Hyprlang::INT>(props.at("rounding"));
        borderSize                    = std::any_cast<Hyprlang::INT>(props.at("border_size"));
        entrySpacing                  = std::any_cast<Hyprlang::FLOAT>(props.at("entry_spacing"));
        m_sColorConfig.inner          = std::any_cast<Hyprlang::INT>(props.at("inner_color"));
        m_sColorConfig.selected       = std::any_cast<Hyprlang::INT>(props.at("selected_color"));
        m_sColorConfig.border         = CGradientValueData::fromAnyPv(props.at("border_color"));
        m_sColorConfig.selectedBorder = CGradientValueData::fromAnyPv(props.at("selected_border_color"));
        halign                        = std::any_cast<Hyprlang::STRING>(props.at("halign"));
        valign                        = std::any_cast<Hyprlang::STRING>(props.at("valign"));
    } catch (const std::bad_any_cast& e) {
        RASSERT(false, "Failed to construct CSessionPicker: {}", e.what()); //
    } catch (const std::out_of_range& e) {
        RASSERT(false, "Missing property for CSessionPicker: {}", e.what()); //
    }

    viewport = viewport_;

    requestSessionEntryTexts();
}

CSessionPicker::~CSessionPicker() {}

bool CSessionPicker::draw(const SRenderData& data) {
    const Vector2D SIZE{std::max(size.x, static_cast<double>(biggestEntryTextWidth)), size.y};
    const CBox     RECTBOX{
        posFromHVAlign(viewport, SIZE, configPos, halign, valign),
        SIZE,
    };

    const auto ENTRYHEIGHT = RECTBOX.h / (vLoginSessions.size() + 1);
    const auto TOPLEFT     = RECTBOX.pos() + Vector2D{0.0, RECTBOX.h};

    for (size_t i = 0; i < vLoginSessions.size(); ++i) {
        auto&      sessionAsset = vLoginSessions[i];
        const CBox ENTRYBOX{
            TOPLEFT.x,
            TOPLEFT.y - ENTRYHEIGHT - i * (ENTRYHEIGHT + entrySpacing),
            RECTBOX.w,
            ENTRYHEIGHT,
        };

        const auto ENTRYROUND = roundingForBox(ENTRYBOX, rounding);
        const bool SELECTED   = i == g_pHyprlock->m_sGreetdLoginSessionState.iSelectedLoginSession;

        CHyprColor entryCol;
        if (SELECTED)
            entryCol = CHyprColor{m_sColorConfig.selected.asRGB(), m_sColorConfig.selected.a * data.opacity};
        else
            entryCol = CHyprColor{m_sColorConfig.inner.asRGB(), m_sColorConfig.inner.a * data.opacity};

        g_pRenderer->renderRect(ENTRYBOX, entryCol, ENTRYROUND);
        if (borderSize > 0) {
            const CBox ENTRYBORDERBOX{
                ENTRYBOX.pos() - Vector2D{borderSize, borderSize},
                ENTRYBOX.size() + Vector2D{2 * borderSize, 2 * borderSize},
            };

            const auto ENTRYBORDERROUND = roundingForBorderBox(ENTRYBORDERBOX, rounding, borderSize);
            g_pRenderer->renderBorder(ENTRYBORDERBOX, (SELECTED) ? *m_sColorConfig.selectedBorder : *m_sColorConfig.border, borderSize, ENTRYBORDERROUND, data.opacity);
        }

        if (sessionAsset.textAsset) {
            const CBox ASSETBOXCENTERED{
                ENTRYBOX.pos() +
                    Vector2D{
                        ENTRYBOX.size().x / 2 - sessionAsset.textAsset->texture.m_vSize.x / 2,
                        ENTRYBOX.size().y / 2 - sessionAsset.textAsset->texture.m_vSize.y / 2,
                    },
                sessionAsset.textAsset->texture.m_vSize,
            };
            g_pRenderer->renderTexture(ASSETBOXCENTERED, sessionAsset.textAsset->texture, data.opacity);
        }
    }

    return false;
}

// TODO: Move this out of here, so it does not get called for each monitor

struct SSessionEntryAssetCallbackData {
    CSessionPicker*    thisWidget;
    const std::string& sessionName;
};

static void onAssetCallback(void* data) {
    const auto PDATA = (SSessionEntryAssetCallbackData*)data;
    PDATA->thisWidget->onGotSessionEntryText(PDATA->sessionName);
    delete PDATA;
}

void CSessionPicker::onGotSessionEntryText(const std::string& sessionName) {
    auto session = std::find_if(vLoginSessions.begin(), vLoginSessions.end(), [&sessionName](const SSessionAsset& s) { return s.loginSession.name == sessionName; });
    if (session == vLoginSessions.end()) {
        Debug::log(ERR, "Failed to find session entry for {}", sessionName);
        return;
    }

    session->textAsset = g_pRenderer->asyncResourceGatherer->getAssetByID(session->textResourceID);
    if (!session->textAsset)
        Debug::log(ERR, "Failed to get asset for session entry {}", sessionName);
    else
        biggestEntryTextWidth = std::max(biggestEntryTextWidth, static_cast<size_t>(session->textAsset->texture.m_vSize.x));
}

void CSessionPicker::requestSessionEntryTexts() {
    vLoginSessions.resize(g_pHyprlock->m_sGreetdLoginSessionState.vLoginSessions.size());
    for (size_t i = 0; i < vLoginSessions.size(); ++i) {
        const auto& SESSIONDATA          = g_pHyprlock->m_sGreetdLoginSessionState.vLoginSessions[i];
        vLoginSessions[i].textResourceID = std::format("session:{}-{}", (uintptr_t)this, SESSIONDATA.name);

        // request asset preload
        CAsyncResourceGatherer::SPreloadRequest request;
        request.id    = vLoginSessions[i].textResourceID;
        request.asset = SESSIONDATA.name;
        request.type  = CAsyncResourceGatherer::eTargetType::TARGET_TEXT;
        //request.props["font_family"] = fontFamily;
        //request.props["color"]     = colorConfig.font;
        //request.props["font_size"] = rowHeight;
        request.callback     = onAssetCallback;
        request.callbackData = new SSessionEntryAssetCallbackData{this, SESSIONDATA.name};

        vLoginSessions[i].loginSession = SESSIONDATA;

        g_pRenderer->asyncResourceGatherer->requestAsyncAssetPreload(request);
    }
}
