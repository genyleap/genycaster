module;

#include "emoji_controller.hpp"
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
