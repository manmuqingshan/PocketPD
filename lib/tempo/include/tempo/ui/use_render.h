/**
 * @file use_render.h
 * @brief Optional CRTP mixin: owns a View instance, dirty/period paint, flush.
 *
 * Stages opt in with `UseRender<Derived, View>`. Derived provides `view_model()`
 * (content snapshot). View is a retained instance: interaction state (cursor,
 * scroll) and `render(Display&, model)`. Stage reaches it via `view()`.
 *
 * Detection: `std::is_base_of_v<UseRenderBase, T>` (see `tempo/core/mixin.h`).
 * Framework attach/render entry points are private (Application only).
 */
#pragma once

#include <cstdint>
#include <type_traits>
#include <utility>

#include "tempo/core/mixin.h"
#include "tempo/core/time.h"
#include "tempo/hardware/display.h"

namespace tempo {

    template <typename TEvent, typename... Stages>
    class Application;

    namespace detail {

        template <typename T, typename = void>
        struct has_on_after_render : std::false_type {};

        template <typename T>
        struct has_on_after_render<T, std::void_t<decltype(std::declval<T>().on_after_render())>>
            : std::true_type {};

    } // namespace detail

    /**
     * @brief CRTP mixin that injects a retained View + display rendering into a Stage.
     *
     * @tparam Derived Host stage. Must define `view_model()`.
     * @tparam View    Default-constructible; `void render(Display&, const Model&)`.
     */
    template <typename Derived, typename View>
    class UseRender : public UseRenderBase {
        template <typename TEvent, typename... Stages>
        friend class Application;
        friend Derived;

        View m_view{};
        Display* m_display = nullptr;
        bool m_dirty = true;

        uint32_t m_period_ms = 0;
        uint32_t m_next_ms = 0;
        bool m_period_armed = false;

        void attach(Display& display) {
            m_display = &display;
            m_dirty = true;
            m_period_armed = false;
        }

        void render_if_needed(uint32_t now_ms) {
            if (m_display == nullptr) {
                return;
            }

            bool due = m_dirty;
            if (m_period_ms > 0) {
                if (!m_period_armed) {
                    m_next_ms = now_ms + m_period_ms;
                    m_period_armed = true;
                } else if (time_reached(now_ms, m_next_ms)) {
                    m_next_ms = now_ms + m_period_ms;
                    due = true;
                }
            }

            if (!due) {
                return;
            }

            m_dirty = false;
            m_view.render(*m_display, static_cast<Derived*>(this)->view_model());
            m_display->flush();

            if constexpr (detail::has_on_after_render<Derived>::value) {
                static_cast<Derived*>(this)->on_after_render();
            }
        }

    public:
        /**
         * @brief Test-only.
         */
        void INTERNAL_DO_NOT_USE_attach_display(Display& display) {
            attach(display);
        }

        /**
         * @brief Test-only.
         */
        void INTERNAL_DO_NOT_USE_render(uint32_t now_ms) {
            render_if_needed(now_ms);
        }

    protected:
        UseRender() = default;

        View& view() {
            return m_view;
        }

        const View& view() const {
            return m_view;
        }

        void request_render() {
            m_dirty = true;
        }

        /**
         * @brief 0 = dirty-only; >0 = also repaint on this period (live meters).
         */
        void set_render_period(uint32_t period_ms) {
            m_period_ms = period_ms;
            m_period_armed = false;
        }
    };

} // namespace tempo
