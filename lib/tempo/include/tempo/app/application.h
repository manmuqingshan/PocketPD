#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "tempo/bus/event_queue.h"
#include "tempo/bus/publisher.h"
#include "tempo/core/mixin.h"
#include "tempo/core/panic.h"
#include "tempo/core/type_list.h"
#include "tempo/diag/logger.h"
#include "tempo/hardware/display.h"
#include "tempo/hardware/stream.h"
#include "tempo/sched/background_task.h"
#include "tempo/sched/cooperative_scheduler.h"
#include "tempo/sched/periodic_task.h"
#include "tempo/sched/stage_scoped_task.h"
#include "tempo/sched/task.h"
#include "tempo/stage/conductor.h"
#include "tempo/ui/use_render.h"

namespace tempo {

    constexpr size_t DEFAULT_MAX_TASKS = 16;
    constexpr size_t DEFAULT_EVENT_QUEUE_CAP = 32;

    /**
     * @brief Single-core application orchestrator.
     *
     * Application is a thin wrapper around the scheduler, conductor, and event queues. It
     * does not own any hardware, drivers, app data, or other product-specific runtime data.
     *
     * Borrowed by reference: Clock, StreamWriter, and optionally Display (for UseRender stages).
     * Pass Display* into the constructor when any registered stage uses UseRender.
     *
     * Anything else (inputs, Storage, sensors) is wired from main into Tasks/Stages.
     *
     * Stage identity is the Stage's type. The Stages... pack defines the slot order for the
     * Conductor and StageMask.
     *
     * @tparam Event An event type of std::variant<...>.
     * @tparam Stages The compile-time list of Stage types.
     */
    template <typename TEvent, typename... Stages>
    class Application : public UseLog<Application<TEvent, Stages...>> {
    public:
        // clang-format off
        static constexpr size_t MaxTasks           = DEFAULT_MAX_TASKS;
        static constexpr size_t EventQueueCapacity = DEFAULT_EVENT_QUEUE_CAP;
        static constexpr size_t StageCount         = sizeof...(Stages);
        static constexpr const char* LOG_TAG = "Application";

        using Event           = TEvent;
        using Scheduler       = tempo::CooperativeScheduler<Event, MaxTasks, Stages...>;
        using Queue           = tempo::EventQueue<Event, EventQueueCapacity>;
        using Publisher       = tempo::QueuePublisher<Event, EventQueueCapacity>;

        using Conductor       = tempo::Conductor<Event, Stages...>;
        using Stage           = tempo::Stage<Event, Stages...>;
        using StageMask       = tempo::StageMask<Stages...>;

        using Task            = tempo::Task<Event, Stages...>;
        using PeriodicTask    = tempo::PeriodicTask<Event, Stages...>;
        using BackgroundTask  = tempo::BackgroundTask<Event, Stages...>;
        using StageScopedTask = tempo::StageScopedTask<Event, Stages...>;

        template <typename Derived>
        using UseLog          = tempo::UseLog<Derived>;

        template <typename Derived>
        using UsePublisher    = tempo::UsePublisher<Derived, Event>;

        template <typename Derived, typename View>
        using UseRender       = tempo::UseRender<Derived, View>;

        using tempo::UseLog<Application>::log;
        // clang-format on
    private:
        static inline Application* instance = nullptr;

        struct RenderHook {
            void (*render)(void* stage, uint32_t now_ms) = nullptr;
            void* stage = nullptr;
        };

        // —— External references

        const Clock& m_clock;
        StreamWriter& m_stream_writer;
        Display* m_display = nullptr;

        // —— Owned subsystems

        Queue m_task_queue;
        Queue m_isr_queue;

        Publisher m_task_publisher;
        Publisher m_isr_publisher;

        Scheduler m_scheduler;
        Conductor m_conductor;
        std::array<RenderHook, StageCount> m_render_hooks{};

        bool m_started = false;

        static const char* name_of_current_stage() {
            return instance ? instance->m_conductor.current_name() : "?";
        }

        const char* name_of_stage_at(size_t idx) const {
            return m_conductor.name_at(idx);
        }

    public:
        Application(const Clock& clock, StreamWriter& stream_writer, Display* display = nullptr)
            : m_clock(clock),
              m_stream_writer(stream_writer),
              m_display(display),
              m_task_publisher(m_task_queue),
              m_isr_publisher(m_isr_queue) {

            TEMPO_CHECK(instance == nullptr, "Only one Application may exist per build");
            instance = this;

            this->m_log_slot.attach(m_clock, m_stream_writer);
        }

        ~Application() = default;

        // ---- Registration (before start) ----
        template <typename T>
        bool add_task(T& task) {
            if constexpr (std::is_base_of_v<UseLogBase, T>) {
                auto& uselog = static_cast<tempo::UseLog<T>&>(task);
                uselog.m_log_slot.attach(m_clock, m_stream_writer);
            }

            if constexpr (std::is_base_of_v<UsePublisherBase, T>) {
                auto& usepublisher = static_cast<tempo::UsePublisher<T, Event>&>(task);
                usepublisher.m_publisher_slot.attach(m_task_publisher);
            }

            return m_scheduler.add(task);
        }

        template <typename S>
        void register_stage(S& stage) {
            if constexpr (std::is_base_of_v<UseLogBase, S>) {
                auto& uselog = static_cast<tempo::UseLog<S>&>(stage);
                uselog.m_log_slot.attach(m_clock, m_stream_writer);
            }

            if constexpr (std::is_base_of_v<UsePublisherBase, S>) {
                auto& usepublisher = static_cast<tempo::UsePublisher<S, Event>&>(stage);
                usepublisher.m_publisher_slot.attach(m_task_publisher);
            }

            if constexpr (std::is_base_of_v<UseRenderBase, S>) {
                TEMPO_CHECK(
                    m_display != nullptr, "UseRender stage requires Display* in Application ctor"
                );
                constexpr size_t idx = type_index_v<S, Stages...>;
                // Friend access to UseRender private attach / render_if_needed.
                stage.attach(*m_display);
                m_render_hooks[idx] = RenderHook{&render_hook<S>, &stage};
            }

            m_conductor.register_stage(stage);
        }

        /**
         * @brief Setup services and start the application.
         *
         * Unregistered stage slots fall back to a no-op NullStage, so calling start with no
         * stages registered is allowed but staging functionality is effectively disabled.
         *
         * @tparam InitialStage The stage type to enter on startup.
         */
        template <typename InitialStage>
        void start() {
            TEMPO_CHECK(!m_started, "Application::start called twice");

            m_scheduler.start();
            m_conductor.template start<InitialStage>(m_clock.now_ms());

            m_started = true;
            log.info("tempo: started, Stage={}", m_conductor.current_name());
        }

        /**
         * @brief Main tick loop.
         */
        void tick() {
            if (!m_started) {
                return;
            }

            const uint32_t now = m_clock.now_ms();

            // 1. Apply any pending Stage transition from the previous tick.
            const size_t before = m_conductor.current_index();
            if (m_conductor.apply_pending_transition(now)) {
                const size_t after = m_conductor.current_index();
                m_scheduler.notify_stage_changed(before, after);
                log.info("Stage {} -> {}", name_of_stage_at(before), name_of_stage_at(after));
            }

            const size_t current = m_conductor.current_index();

            // 2. Drain ISR queue first (more sensitive), then task queue. Per event, the
            // scheduler dispatches to every matching task before the conductor delivers the
            // same event to the active stage's on_event override.
            Event e;
            while (m_isr_queue.pop(e)) {
                m_scheduler.dispatch_event(e, current, now);
                m_conductor.dispatch_event(e, now);
            }
            while (m_task_queue.pop(e)) {
                m_scheduler.dispatch_event(e, current, now);
                m_conductor.dispatch_event(e, now);
            }

            // 3. Run periodic on_tick on every task whose stage filter matches.
            m_scheduler.tick(now, current);

            // 4. Give the current stage a chance to run its own on_tick.
            m_conductor.tick(now);

            // 5. Render UseRender stages once per tick (dirty and/or period).
            render_current(now);
        }

    private:
        /**
         * @brief Static so the function pointer is a friend of UseRender via Application.
         */
        template <typename S>
        static void render_hook(void* stage, uint32_t now_ms) {
            static_cast<S*>(stage)->render_if_needed(now_ms);
        }

        void render_current(uint32_t now_ms) {
            const size_t idx = m_conductor.current_index();
            if (idx >= StageCount) {
                return;
            }
            const RenderHook& hook = m_render_hooks[idx];
            if (hook.render != nullptr && hook.stage != nullptr) {
                hook.render(hook.stage, now_ms);
            }
        }

    public:

        // —— Accessors

        Queue& task_queue() {
            return m_task_queue;
        }
        Queue& isr_queue() {
            return m_isr_queue;
        }
        Publisher& task_publisher() {
            return m_task_publisher;
        }
        Publisher& isr_publisher() {
            return m_isr_publisher;
        }
        Scheduler& scheduler() {
            return m_scheduler;
        }
        Conductor& conductor() {
            return m_conductor;
        }

        size_t current_stage_index() const {
            return m_conductor.current_index();
        }
    };

} // namespace tempo
