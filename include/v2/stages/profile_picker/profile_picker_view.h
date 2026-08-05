/**
 * @file profile_picker_view.h
 * @brief PDO list view: browse cursor + paint. Content rows come from the model.
 */
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>

#include <tempo/hardware/display.h>

namespace pocketpd {

    /** @brief PDO / non-PD content only; browse cursor lives on ProfilePickerView. */
    struct ProfilePickerViewModel {
        static constexpr uint8_t MAX_ROWS = 8;

        enum class Kind : uint8_t { NonPd, PdList };

        struct PdoRow {
            enum class Type : uint8_t { Fixed, Pps };
            Type type = Type::Fixed;
            int32_t min_mv = 0;
            int32_t max_mv = 0;
            int32_t max_ma = 0;
        };

        Kind kind = Kind::PdList;
        std::array<PdoRow, MAX_ROWS> rows{};
        uint8_t count = 0;
    };

    class ProfilePickerView {
    private:
        int m_committed = 0;
        int m_pending = 0;

    public:
        /** @brief Last committed PDO index (survives re-entry). */
        int cursor() const {
            return m_committed;
        }

        int pending_cursor() const {
            return m_pending;
        }

        void begin_browse() {
            m_pending = m_committed;
        }

        bool cursor_move(int delta, int count) {
            if (count <= 0) {
                return false;
            }
            const int next = std::clamp(m_pending + delta, 0, count - 1);
            if (next == m_pending) {
                return false;
            }
            m_pending = next;
            return true;
        }

        void commit_pending() {
            m_committed = m_pending;
        }

        void render(tempo::Display& d, const ProfilePickerViewModel& vm) const {
            d.clear();

            if (vm.kind == ProfilePickerViewModel::Kind::NonPd) {
                const char* line1 = "Non-PD Source";
                const char* line2 = "Passthrough Mode";
                const auto x1 = static_cast<uint8_t>((128 - d.text_width(line1)) / 2);
                const auto x2 = static_cast<uint8_t>((128 - d.text_width(line2)) / 2);
                d.draw_text(x1, 28, line1);
                d.draw_text(x2, 44, line2);
                return;
            }

            std::array<char, 32> buffer{};
            for (uint8_t row_index = 0; row_index < vm.count; ++row_index) {
                const auto y = static_cast<uint8_t>(9 * (row_index + 1));
                if (row_index == m_pending) {
                    d.draw_text(0, y, ">");
                }

                const auto& row = vm.rows[row_index];
                std::array<char, 12> vbuf{};
                if (row.type == ProfilePickerViewModel::PdoRow::Type::Fixed) {
                    const int v = row.max_mv / 1000;
                    const int a = row.max_ma / 1000;
                    std::snprintf(vbuf.data(), vbuf.size(), "%7d.0V", v);
                    std::snprintf(buffer.data(), buffer.size(), "PDO %-12s%dA", vbuf.data(), a);
                    d.draw_text(10, y, buffer.data());
                } else {
                    const int min_v = row.min_mv / 1000;
                    const int min_dv = (row.min_mv % 1000) / 100;
                    const int max_v = row.max_mv / 1000;
                    const int max_dv = (row.max_mv % 1000) / 100;
                    const int a = row.max_ma / 1000;
                    std::snprintf(
                        vbuf.data(), vbuf.size(), "%2d.%d-%2d.%dV", min_v, min_dv, max_v, max_dv
                    );
                    std::snprintf(buffer.data(), buffer.size(), "PPS %-12s%dA", vbuf.data(), a);
                    d.draw_text(10, y, buffer.data());
                }
            }
        }
    };

} // namespace pocketpd
