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
        // Feed emoji play once. The UI restores the native Unicode fallback
        // when the authored animation completes, rather than leaving a
        // transparent last Lottie frame in the inline layout indefinitely.
        // This preserves a legible cast without dozens of perpetual loops.
        return AnimationDecision{
            .mode = PlaybackMode::PlayOnce,
            .loopCount = 1U,
            .pauseWhenInvisible = true,
        };
    case Surface::PickerCell:
        // Special-picker cells animate as soon as they enter the viewport.
        // Playing once keeps the initial reveal lively without leaving dozens
        // of persistent loops competing with scrolling and asset decoding.
        return AnimationDecision{
            .mode = active ? PlaybackMode::PlayOnce : PlaybackMode::Static,
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
