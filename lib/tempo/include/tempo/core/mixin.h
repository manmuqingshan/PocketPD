/**
 * @file mixin.h
 * @brief Non-template bases for Application capability detection.
 *
 */
#pragma once

namespace tempo {

    /** @brief Marker base for `UseLog<Derived>`. */
    class UseLogBase {
    protected:
        UseLogBase() = default;
    };

    /** @brief Marker base for `UsePublisher<Derived, Event>`. */
    class UsePublisherBase {
    protected:
        UsePublisherBase() = default;
    };

    /** @brief Marker base for `UseRender<Derived, View>`. */
    class UseRenderBase {
    protected:
        UseRenderBase() = default;
    };

} // namespace tempo
