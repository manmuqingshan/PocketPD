/**
 * @file profile_picker_stage.h
 * @brief Profile-selection stage for NormalStage entry.
 */
#pragma once

#include <algorithm>
#include <cstdint>
#include <variant>

#include <tempo/bus/visit.h>
#include <tempo/core/time.h>

#include "v2/app.h"
#include "v2/events.h"
#include "v2/hal/pd_sink_controller.h"
#include "v2/pocketpd.h"
#include "v2/stages/profile_picker/profile_picker_view.h"

namespace pocketpd {

    class ProfilePickerStage : public App::Stage,
                               public App::UseLog<ProfilePickerStage>,
                               public App::UseRender<ProfilePickerStage, ProfilePickerView> {
    private:
        enum class PowerSourceType : uint8_t { NON_PD, PD };

        PdSinkController& m_pd_sink;
        tempo::TimeoutTimer m_passthrough_timeout;

        void commit(Conductor& conductor) {
            if (m_pd_sink.pdo_count() <= 0) {
                return;
            }
            view().commit_pending();
            conductor.reset_root<NormalStage>(static_cast<int8_t>(view().cursor()));
        }

    public:
        static constexpr const char* LOG_TAG = "ProfilePicker";

        explicit ProfilePickerStage(PdSinkController& pd_sink) : m_pd_sink(pd_sink) {}

        const char* name() const override {
            return "PROFILE_PICKER";
        }

        int cursor_index() const {
            return view().cursor();
        }

        PowerSourceType power_source_type() const {
            return (m_pd_sink.pdo_count() > 0) ? PowerSourceType::PD : PowerSourceType::NON_PD;
        }

        void on_enter(Conductor&, uint32_t now_ms) override {
            view().begin_browse();
            request_render();

            if (power_source_type() == PowerSourceType::NON_PD) {
                m_passthrough_timeout.set(now_ms, PICKER_PASSTHROUGH_AUTO_MS);
            } else {
                m_passthrough_timeout.disarm();
            }
        }

        void on_tick(Conductor& conductor, uint32_t now_ms) override {
            if (m_passthrough_timeout.reached(now_ms)) {
                conductor.reset_root<NormalStage>(static_cast<int8_t>(-1));
            }
        }

        void on_event(Conductor& conductor, const Event& event, uint32_t) override {
            if (power_source_type() == PowerSourceType::NON_PD) {
                auto pass_handler = tempo::overloaded{
                    [&](const ButtonEvent&) { conductor.reset_root<NormalStage>(-1); },
                    [&](const EncoderEvent&) { conductor.reset_root<NormalStage>(-1); },
                    [](const auto&) {},
                };
                std::visit(pass_handler, event);
                return;
            }

            auto handler = tempo::overloaded{
                [&](const EncoderEvent& evt) {
                    if (view().cursor_move(evt.delta, m_pd_sink.pdo_count())) {
                        request_render();
                    }
                },
                [&](const ButtonEvent& evt) {
                    if (evt.id == ButtonId::L && evt.gesture == Gesture::LONG) {
                        conductor.pop();
                        return;
                    }
                    if (evt.id == ButtonId::ENCODER && evt.gesture == Gesture::LONG) {
                        commit(conductor);
                    }
                },
                [](const auto&) {},
            };
            std::visit(handler, event);
        }

        ProfilePickerViewModel view_model() const {
            ProfilePickerViewModel vm;
            if (power_source_type() == PowerSourceType::NON_PD) {
                vm.kind = ProfilePickerViewModel::Kind::NonPd;
                return vm;
            }

            vm.kind = ProfilePickerViewModel::Kind::PdList;
            const int count = m_pd_sink.pdo_count();
            vm.count = static_cast<uint8_t>(
                std::min(count, static_cast<int>(ProfilePickerViewModel::MAX_ROWS))
            );

            for (uint8_t i = 0; i < vm.count; ++i) {
                auto& row = vm.rows[i];
                if (m_pd_sink.is_index_fixed(i)) {
                    row.type = ProfilePickerViewModel::PdoRow::Type::Fixed;
                    row.max_mv = m_pd_sink.pdo_max_voltage_mv(i);
                    row.max_ma = m_pd_sink.pdo_max_current_ma(i);
                } else {
                    row.type = ProfilePickerViewModel::PdoRow::Type::Pps;
                    row.min_mv = m_pd_sink.pdo_min_voltage_mv(i);
                    row.max_mv = m_pd_sink.pdo_max_voltage_mv(i);
                    row.max_ma = m_pd_sink.pdo_max_current_ma(i);
                }
            }
            return vm;
        }
    };

} // namespace pocketpd
