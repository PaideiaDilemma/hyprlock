#pragma once

#include "IWidget.hpp"
#include "Shadowable.hpp"
#include "../../helpers/Math.hpp"
#include "../../helpers/AnimatedVariable.hpp"
#include "../Framebuffer.hpp"
#include "../../config/ConfigManager.hpp"
#include "../../helpers/Color.hpp"
#include <hyprutils/math/Vector2D.hpp>
#include <string>
#include <any>
#include <vector>

struct SPreloadedAsset;
class CSessionLockSurface;

class CSessionPicker : public IWidget {
  public:
    struct SSessionAsset {
        SLoginSessionConfig loginSession;
        std::string         textResourceID;
        SPreloadedAsset*    textAsset = nullptr;
    };

    CSessionPicker(const Vector2D& viewport, const std::unordered_map<std::string, std::any>& props);
    ~CSessionPicker();

    virtual bool draw(const SRenderData& data);

    void         onGotSessionEntryText(const std::string& sessionName);

  private:
    void                       requestSessionEntryTexts();
    std::vector<SSessionAsset> vLoginSessions;

    Vector2D                   viewport;
    Vector2D                   configPos;
    Vector2D                   size;
    std::string                halign       = "";
    std::string                valign       = "";
    int                        rounding     = -1;
    int                        borderSize   = -1;
    int                        entryHeight  = -1;
    int                        entrySpacing = -1;

    size_t                     biggestEntryTextWidth = 0;

    struct {
        CHyprColor          inner;
        CHyprColor          selected;

        CGradientValueData* border         = nullptr;
        CGradientValueData* selectedBorder = nullptr;
    } m_sColorConfig;

    CShadowable shadow;
    bool        updateShadow = true;
};
