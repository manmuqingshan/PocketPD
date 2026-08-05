/**
 * @file menu_view.h
 * @brief Stateful list view for MenuStage (cursor/scroll + paint).
 */
#pragma once

#include <cstdint>

#include <tempo/hardware/display.h>

#include "v2/ui/table_view.h"

namespace pocketpd {

    /** @brief Row content only; selection lives on MenuView. */
    struct MenuViewModel {
        const TableRow* rows = nullptr;
        uint8_t count = 0;
    };

    class MenuView {
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

        void render(tempo::Display& d, const MenuViewModel& vm) {
            TableModel model;
            model.rows = vm.rows;
            model.count = vm.count;
            m_table.render(d, model);
        }
    };

} // namespace pocketpd
