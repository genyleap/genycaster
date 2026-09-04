module;

#include <cstdint>

module farcaster.application.emoji.animation_policy;

namespace farcaster::application::emoji::animation {

AnimationDecision MotionPolicy::decide(
    const Surface surface,
    const bool reduceMotion,
    const bool active,
    const bool visible) noexcept
{
    // Reduced motion is absolute: never loop, never play automatically.
    if (reduceMotion) {
        return AnimationDecision{
            .mode = PlaybackMode::Static,
            .loopCount = 1U,
            .pauseWhenInvisible = true,
        };
    }

    if (!visible) {
        return AnimationDecision{
            .mode = PlaybackMode::Static,
            .loopCount = 1U,
            .pauseWhenInvisible = true,
        };
    }

    switch (surface) {
    case Surface::Composer:
        // Composer emoji are few and inside an interactive editor; gentle loops
        // keep them lively without a timer per glyph (visibility still gates).
        return AnimationDecision{
            .mode = PlaybackMode::Loop,
            .loopCount = 0U, // infinite
            .pauseWhenInvisible = true,
        };
    case Surface::CastDetail:
        // Detail view is the focus of attention; loop while visible.
        return AnimationDecision{
            .mode = PlaybackMode::Loop,
            .loopCount = 0U,
            .pauseWhenInvisible = true,
        };
    case Surface::Feed:
        // Feed emoji play once on entry and stay on the final frame. Replaying
        // on hover is allowed where the UI exposes hover/focus.
        return active
            ? AnimationDecision{
                  .mode = PlaybackMode::PlayOnce,
                  .loopCount = 1U,
                  .pauseWhenInvisible = true,
              }
            : AnimationDecision{
                  .mode = PlaybackMode::PlayOnce,
                  .loopCount = 1U,
                  .pauseWhenInvisible = true,
              };
    case Surface::PickerCell:
        // Picker cells stay static until hover/focus/selection activates them.
        return active
            ? AnimationDecision{
                  .mode = PlaybackMode::PlayOnce,
                  .loopCount = 1U,
                  .pauseWhenInvisible = true,
              }
            : AnimationDecision{
                  .mode = PlaybackMode::Static,
                  .loopCount = 1U,
                  .pauseWhenInvisible = true,
              };
    case Surface::PickerPreview:
        // The dedicated preview cell may loop while the picker is open.
        return AnimationDecision{
            .mode = PlaybackMode::Loop,
            .loopCount = 0U,
            .pauseWhenInvisible = true,
        };
    }
    return AnimationDecision{
        .mode = PlaybackMode::Static,
        .loopCount = 1U,
        .pauseWhenInvisible = true,
    };
}

} // namespace farcaster::application::emoji::animation