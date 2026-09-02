module;

#include <chrono>
#include <string>

export module farcaster.capability.profile_quality;

import farcaster.domain.identity;
import farcaster.foundation.result;

export namespace farcaster::capability::profile_quality {

/** @brief Provider-neutral normalized profile-quality observation. */
struct ProfileQualityScore final {
    double value{};
    std::string source{};
    std::chrono::system_clock::time_point observedAt{};
};

/** @brief Blocking source for a derived Farcaster profile-quality score. */
class IProfileQualitySource {
public:
    virtual ~IProfileQualitySource() = default;

    /** @brief Reads the quality score for one FID; NotFound means no score is published. */
    [[nodiscard]] virtual foundation::result::Result<ProfileQualityScore>
    get(domain::identity::Fid fid) = 0;
};

}
