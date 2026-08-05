/**
 * @file menu_stage.h
 * @brief Top-level menu stage.
 */
#pragma once

#include <array>
#include <cstdint>
#include <variant>

#include <tempo/bus/visit.h>

#include "v2/app.h"
#include "v2/events.h"
#include "v2/stages/menu/menu_view.h"
#include "v2/ui/table_view.h"

namespace pocketpd {

    class MenuStage : public App::Stage,
                      public App::UseLog<MenuStage>,
                      public App::UseRender<MenuStage, MenuView> {
    private:
        enum class Item : uint8_t {
            PROFILE_PICKER,
            SETTINGS,
        };

        struct MenuItem {
            Item item;
            const char* label;
        };

        static constexpr std::array<MenuItem, 2> ITEMS = {{
            {Item::PROFILE_PICKER, "Profile Picker"},
            {Item::SETTINGS, "Settings"},
        }};

        std::array<TableRow, ITEMS.size()> m_rows{};

        uint8_t count() const {
            return static_cast<uint8_t>(ITEMS.size());
        }

    public:
        MenuStage() = default;

        void on_enter(Conductor&, uint32_t) override {
            view().reset();
            request_render();
        }

        void on_event(Conductor& conductor, const Event& event, uint32_t) override {
            auto handler = tempo::overloaded{
                [&](const EncoderEvent& evt) {
                    if (evt.delta == 0) {
                        return;
                    }
                    if (view().cursor_move(evt.delta, count())) {
                        request_render();
                    }
                },
                [&](const ButtonEvent& evt) {
                    if (evt.id == ButtonId::L && evt.gesture == Gesture::LONG) {
                        conductor.pop();
                        return;
                    }
                    if (evt.id == ButtonId::ENCODER && evt.gesture == Gesture::LONG) {
                        switch (ITEMS[view().cursor()].item) {
                        case Item::PROFILE_PICKER:
                            conductor.push<ProfilePickerStage>();
                            break;
                        case Item::SETTINGS:
                            conductor.push<SettingsStage>();
                            break;
                        }
                    }
                },
                [](const auto&) {},
            };
            std::visit(handler, event);
        }

        MenuViewModel view_model() {
            for (size_t i = 0; i < ITEMS.size(); ++i) {
                m_rows[i].text = ITEMS[i].label;
            }
            return MenuViewModel{
                .rows = m_rows.data(),
                .count = count(),
            };
        }
    };

} // namespace pocketpd
