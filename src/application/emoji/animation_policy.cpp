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
        // A completed one-shot Lottie often ends on a transparent frame:
        // the inline Unicode has already yielded to the animated renderer,
        // so the emoji appears to vanish from the cast. Keep visible feed
        // emoji looping just like detail emoji, and pause them when scrolled
        // offscreen, during a fling or while the app is inactive. Never
        // restart the animation on a transient viewport/motion change.
        return AnimationDecision{
            .mode = PlaybackMode::Loop,
            .loopCount = 0U,
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
