/*!
 * @file        ui/app/emoji_runtime.cppm
 * @brief       Executable emoji runtime composition.
 *
 * @details
 * Owns asset provider, cache and playback components outside the public
 * Qt presentation surface.
 *
 * @author      Kambiz Asadzadeh
 * @since       24 Sep 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;

#include "emoji_controller_fwd.hpp"
#include <memory>
#include <filesystem>

export module farcaster.runtime.emoji;

export namespace farcaster::runtime::emoji {

/**
 * @brief Executable-owned emoji asset composition, isolated from presentation.
 *
 * Borrows a GUI-thread controller which must outlive it. Owns bounded asynchronous
 * cache/download/validation work and drains callbacks before releasing dependencies.
 * Missing assets report failure through the controller while Unicode remains usable.
 */
class EmojiRuntime final {
public:
    explicit EmojiRuntime(EmojiController& controller, std::filesystem::path cacheRoot = {});
    ~EmojiRuntime();
    EmojiRuntime(const EmojiRuntime&) = delete;
    auto operator=(const EmojiRuntime&) -> EmojiRuntime& = delete;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

}
