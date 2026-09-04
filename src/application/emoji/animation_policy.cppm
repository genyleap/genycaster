module;

#include <cstdint>

export module farcaster.application.emoji.animation_policy;

export namespace farcaster::application::emoji::animation {

/** @brief Product surfaces with distinct animation expectations. */
enum class Surface : std::uint8_t {
    Composer,
    Feed,
    CastDetail,
    PickerCell,
    PickerPreview,
};

/** @brief Allowed playback behavior for one emoji instance. */
enum class PlaybackMode : std::uint8_t {
    /** @brief Static first/representative frame; no animation. */
    Static,
    /** @brief Play once on activation, then remain on the final frame. */
    PlayOnce,
    /** @brief Loop while the instance remains visible and interactive. */
    Loop,
};

/**
 * @brief Effective per-instance motion decision.
 */
struct AnimationDecision final {
    PlaybackMode mode{PlaybackMode::Static};
    std::uint32_t loopCount{1U};
    bool pauseWhenInvisible{true};
};

/**
 * @brief Centralized emoji motion policy.
 *
 * The policy is pure logic with no global state. It combines surface context,
 * the global reduce-motion preference and visibility so every emoji instance
 * follows one consistent rule set.
 */
class MotionPolicy final {
public:
    /**
     * @brief Decides how one emoji instance should behave.
     *
     * @param surface Surface context (composer, feed, detail, picker...).
     * @param reduceMotion Global reduced-motion preference.
     * @param active Whether the instance is the focused/selected/hovered one.
     * @param visible Whether the instance is inside the visible viewport.
     */
    [[nodiscard]] static AnimationDecision decide(
        Surface surface,
        bool reduceMotion,
        bool active,
        bool visible) noexcept;
};

} // namespace farcaster::application::emoji::animation