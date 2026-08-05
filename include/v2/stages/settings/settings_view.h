/**
 * @file settings_view.h
 * @brief Stateful list view for SettingsStage (cursor/scroll + paint).
 */
#pragma once

#include <array>
#include <cstdint>
#include <cstdio>

#include <tempo/hardware/display.h>

#include "v2/ui/table_view.h"

namespace pocketpd {
    struct SettingsViewModel {
        static constexpr uint8_t MAX_ROWS = 3;

        struct Row {
            const char* label = nullptr;
            bool enabled = false;
        };

        std::array<Row, MAX_ROWS> rows{};
        uint8_t count = 0;
    };

    class SettingsView {
    private:
        TableView m_table{};

    public:
        void reset() {
            m_table.reset();
        }

        bool cursor_move(int delta, uint8_t count) {
            return m_table.move_cursor(delta, count);
        }

        uint8_t cursor() const {
            return m_table.cursor();
        }

        void render(tempo::Display& d, const SettingsViewModel& vm) {
            std::array<std::array<char, 32>, SettingsViewModel::MAX_ROWS> buffers{};
            std::array<TableRow, SettingsViewModel::MAX_ROWS> rows{};

            for (uint8_t i = 0; i < vm.count; ++i) {
                const char mark = vm.rows[i].enabled ? 'X' : ' ';
                std::snprintf(
                    buffers[i].data(), buffers[i].size(), "[%c] %s", mark, vm.rows[i].label
                );
                rows[i].text = buffers[i].data();
            }

            TableModel model;
            model.rows = rows.data();
            model.count = vm.count;
            m_table.render(d, model);
        }
    };

} // namespace pocketpd
