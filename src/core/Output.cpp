#include "Output.hpp"
#include "../helpers/Log.hpp"
#include "hyprlock.hpp"

void COutput::create(WP<COutput> pSelf, SP<CCWlOutput> pWlOutput, uint32_t _name) {
    name       = _name;
    m_wlOutput = pWlOutput;
    m_self     = pSelf;

    m_wlOutput->setDescription([this](CCWlOutput* r, const char* description) {
        stringDesc = description ? std::string{description} : "";
        Debug::log(LOG, "output {} description {}", name, stringDesc);
    });

    m_wlOutput->setName([this](CCWlOutput* r, const char* name) {
        stringName = std::string{name} + stringName;
        stringPort = std::string{name};
        Debug::log(LOG, "output {} name {}", name, name);
    });

    m_wlOutput->setScale([this](CCWlOutput* r, int32_t sc) { scale = sc; });

    m_wlOutput->setDone([this](CCWlOutput* r) {
        Debug::log(LOG, "output {} done", name);
        if (g_pHyprlock->m_bLocked && !m_sessionLockSurface) {
            // if we are already locked, create a surface dynamically
            Debug::log(LOG, "Creating a surface dynamically for output as we are already locked");
            createSessionLockSurface();
        }
    });

    m_wlOutput->setMode([this](CCWlOutput* r, uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
        // handle portrait mode and flipped cases
        if (transform % 2 == 1)
            size = {height, width};
        else
            size = {width, height};
    });

    m_wlOutput->setGeometry(
        [this](CCWlOutput* r, int32_t x, int32_t y, int32_t physical_width, int32_t physical_height, int32_t subpixel, const char* make, const char* model, int32_t transform_) {
            transform = (wl_output_transform)transform_;

            Debug::log(LOG, "output {} make {} model {}", name, make ? make : "", model ? model : "");
        });
}

void COutput::createSessionLockSurface() {
    m_sessionLockSurface = makeUnique<CSessionLockSurface>(m_self.lock());
}

Vector2D COutput::getViewport() const {
    return (m_sessionLockSurface) ? m_sessionLockSurface->size : size;
}
