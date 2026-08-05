/**
 * @file boot_stage.h
 * @brief First stage at power-on. Renders the splash screen with the logo and firmware version,
 * then requests `ObtainStage` after `BOOT_TO_OBTAIN_MS`.
 */
#pragma once

#include <cstdint>

#include <tempo/core/time.h>

#include "v2/app.h"
#include "v2/pocketpd.h"
#include "v2/stages/boot/boot_view.h"

namespace pocketpd {

    class BootStage : public App::Stage,
                      public App::UseLog<BootStage>,
                      public App::UseRender<BootStage, BootView> {
    private:
        tempo::TimeoutTimer m_timeout;

    public:
        static constexpr const char* LOG_TAG = "Boot";

        BootStage() = default;

        const char* name() const override {
            return "BOOT";
        }

        void on_enter(Conductor&, uint32_t now_ms) override {
            m_timeout.set(now_ms, BOOT_TO_OBTAIN_MS);
            request_render();
        }

        void on_tick(Conductor& conductor, uint32_t now_ms) override {
            if (m_timeout.reached(now_ms)) {
                conductor.replace<ObtainStage>();
            }
        }

        BootViewModel view_model() const {
            return BootViewModel{.fw_version = FW_VERSION};
        }
    };

} // namespace pocketpd
