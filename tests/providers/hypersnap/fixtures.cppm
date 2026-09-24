module;

#include <cstdint>
#include <string>
#include <string_view>

export module farcaster.test.hypersnap.fixtures;

export namespace farcaster::test::hypersnap::fixtures {

/** @brief Owns a unique temporary SQLite database path for provider contract tests. */
class TemporaryDatabase final {
public:
    /** @brief Creates a unique path in the system temporary directory. */
    TemporaryDatabase();

    /** @brief Removes the database and SQLite sidecar files. */
    ~TemporaryDatabase();

    TemporaryDatabase(const TemporaryDatabase&) = delete;
    auto operator=(const TemporaryDatabase&) -> TemporaryDatabase& = delete;
    TemporaryDatabase(TemporaryDatabase&&) = delete;
    auto operator=(TemporaryDatabase&&) -> TemporaryDatabase& = delete;

    /** @brief Returns the database path text. */
    [[nodiscard]] const std::string& path() const noexcept;

private:
    std::string m_path{};
};

/** @brief Returns a representative Hypersnap v2 user response fixture. */
[[nodiscard]] const std::string& userResponse();

/** @brief Returns a v2 user response that omits banner enrichment. */
[[nodiscard]] const std::string& userWithoutBannerResponse();

/** @brief Returns a v2 user response with a synthetic FID username fallback. */
[[nodiscard]] const std::string& userWithSyntheticUsernameResponse();

/** @brief Returns a protocol-native USER_DATA_TYPE_URL response fixture. */
[[nodiscard]] const std::string& userUrlResponse();

/** @brief Returns a protocol-native USER_DATA_TYPE_BANNER response fixture. */
[[nodiscard]] const std::string& userBannerResponse();

/** @brief Returns the standard UserDataByFid response containing current profile fields. */
[[nodiscard]] const std::string& userDataByFidResponse();

/** @brief Returns a representative Hypersnap v2 cast response fixture. */
[[nodiscard]] std::string castResponse();

/** @brief Returns a representative Hypersnap v2 conversation response fixture. */
[[nodiscard]] std::string conversationResponse();

/** @brief Returns a representative Hypersnap v2 feed response fixture. */
[[nodiscard]] std::string feedResponse(std::string_view cursor);

/** @brief Returns a root post feed fixture without reply/quote semantics. */
[[nodiscard]] std::string rootPostFeedResponse();

/** @brief Returns a root quote-cast feed fixture. */
[[nodiscard]] std::string quoteFeedResponse();

/** @brief Returns a mixed reply/recast compatibility-feed fixture. */
[[nodiscard]] std::string repliesAndRecastsFeedResponse();

/** @brief Returns a reply feed whose root reply count still needs conversation hydration. */
[[nodiscard]] std::string repliesWithoutMetricsFeedResponse();

/** @brief Returns a standard Snapchain reactionsByFid Like response fixture. */
[[nodiscard]] std::string reactionListResponse(std::string_view cursor, std::string_view hash = "0xabc123");

/** @brief Returns a representative batch cast-interactions response. */
[[nodiscard]] std::string castInteractionsResponse();

/** @brief Returns a representative in-app notifications page. */
[[nodiscard]] std::string notificationResponse(std::string_view cursor = {});

/** @brief Returns a representative Hypersnap v2 channel feed response fixture. */
[[nodiscard]] std::string channelFeedResponse();

/** @brief Returns a representative Hypersnap v2 social graph response fixture. */
[[nodiscard]] std::string socialResponse(std::uint64_t fid, std::string_view username);

}
