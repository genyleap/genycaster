// Deterministic tests for Data & Storage policy and network-usage accounting.
#include <cassert>
#include <cstdint>
#include <string_view>

import farcaster.presentation.media.storage_policy;

namespace {
namespace storage = farcaster::presentation::media::storage_policy;
constexpr std::uint64_t mib = 1024U * 1024U;
}

int main()
{
    storage::RuntimeState state{};

    const auto defaults = state.snapshot();
    assert(defaults.diskCacheEnabled);
    assert(defaults.cacheBudgetBytes == 1024U * mib);
    assert(storage::retentionDays(defaults, storage::MediaCategory::Photos) == 30);
    assert(storage::retentionDays(defaults, storage::MediaCategory::VideoPreviews) == 7);
    assert(storage::downloadRule(
               defaults, storage::MediaCategory::Photos, storage::NetworkClass::Wifi)
               .enabled);
    assert(!storage::downloadRule(
                defaults, storage::MediaCategory::Nfts, storage::NetworkClass::Cellular)
                .enabled);
    assert(storage::downloadRule(
               defaults, storage::MediaCategory::Photos, storage::NetworkClass::Cellular)
               .maximumBytes == 6U * mib);

    auto edited = defaults;
    edited.diskCacheEnabled = false;
    edited.cacheBudgetBytes = 2048U * mib;
    edited.retentionDays[static_cast<std::size_t>(storage::MediaCategory::Nfts)] = 0;
    edited.downloadRules[static_cast<std::size_t>(storage::NetworkClass::Cellular)]
                        [static_cast<std::size_t>(storage::MediaCategory::Nfts)] =
        storage::DownloadRule{.enabled = true, .maximumBytes = 16U * mib};
    state.apply(edited);
    state.setNetworkClass(storage::NetworkClass::Cellular);

    const auto applied = state.snapshot();
    assert(!applied.diskCacheEnabled);
    assert(applied.cacheBudgetBytes == 2048U * mib);
    assert(applied.networkClass == storage::NetworkClass::Cellular);
    assert(storage::retentionDays(applied, storage::MediaCategory::Nfts) == 0);
    assert(storage::downloadRule(
               applied, storage::MediaCategory::Nfts, storage::NetworkClass::Cellular)
               .enabled);
    assert(storage::downloadRule(
               applied, storage::MediaCategory::Nfts, storage::NetworkClass::Cellular)
               .maximumBytes == 16U * mib);

    state.recordDownloadedBytes(storage::MediaCategory::Photos, storage::NetworkClass::Wifi, 1234U);
    state.recordDownloadedBytes(storage::MediaCategory::Nfts, storage::NetworkClass::Cellular, 5678U);
    state.recordDownloadedBytes(storage::MediaCategory::Count, storage::NetworkClass::Wifi, 99U);
    state.recordDownloadedBytes(storage::MediaCategory::Photos, storage::NetworkClass::Count, 99U);
    state.recordDownloadedBytes(storage::MediaCategory::Photos, storage::NetworkClass::Wifi, 0U);

    const auto firstDrain = state.takeUsageDeltas();
    assert(firstDrain.bytes[static_cast<std::size_t>(storage::NetworkClass::Wifi)]
                           [static_cast<std::size_t>(storage::MediaCategory::Photos)] == 1234U);
    assert(firstDrain.bytes[static_cast<std::size_t>(storage::NetworkClass::Cellular)]
                           [static_cast<std::size_t>(storage::MediaCategory::Nfts)] == 5678U);

    const auto secondDrain = state.takeUsageDeltas();
    for (const auto& network : secondDrain.bytes) {
        for (const auto bytes : network) {
            assert(bytes == 0U);
        }
    }

    assert(storage::mediaCategoryFromId("photos") == storage::MediaCategory::Photos);
    assert(storage::mediaCategoryFromId("miniapps") == storage::MediaCategory::MiniApps);
    assert(storage::mediaCategoryFromId("unexpected") == storage::MediaCategory::Other);
    assert(storage::mediaCategoryId(storage::MediaCategory::AnimatedEmoji) ==
           std::string_view{"animated-emoji"});
    assert(storage::networkClassId(storage::NetworkClass::Cellular) ==
           std::string_view{"cellular"});

    return 0;
}
