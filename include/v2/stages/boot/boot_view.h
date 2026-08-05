/**
 * @file boot_view.h
 * @brief Splash screen view for BootStage.
 */
#pragma once

#include <tempo/hardware/display.h>

#include "v2/images.h"

namespace pocketpd {

    struct BootViewModel {
        const char* fw_version = "";
    };

    class BootView {
    public:
        void render(tempo::Display& d, const BootViewModel& vm) {
            d.clear();
            d.draw_bitmap(0, 0, 128 / 8, 64, bitmap::LOGO.data());
            d.draw_text(67, 64, "FW: ");
            d.draw_text(87, 64, vm.fw_version);
        }
    };

} // namespace pocketpd
