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
    case Surface::CastDetail:
    case Surface::Feed:
    case Surface::PickerPreview:
        // One authored animation cycle per exposure, then a resting frame.
        // A feed with many emoji must never hold unbounded frame timers.
        return AnimationDecision{
            .mode = PlaybackMode::PlayOnce,
            .loopCount = 1U,
            .pauseWhenInvisible = true,
        };
    case Surface::PickerCell:
        // Unfocused picker cells stay static; focused cells play one cycle.
        return AnimationDecision{
            .mode = active ? PlaybackMode::PlayOnce : PlaybackMode::Static,
            .loopCount = 1U,
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
