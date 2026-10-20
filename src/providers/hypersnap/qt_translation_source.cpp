#include <QtTranslation>

// Translation-tool-only provider-specific source metadata. These IDs remain
// stable and their runtime translations remain in the existing checked-in TS
// catalogs. This file is supplied to lupdate, not linked into domain or core.
namespace {
void registerReadProviderTranslationMetadata()
{
    //% "Hosted Farcaster feed requires a configured read endpoint or Neynar API key. Hypersnap remains available."
    (void)qtTrId("genycaster.setup.farcaster_not_configured");

    //% "Not configured: add a Neynar API key or a compatible hosted read gateway."
    (void)qtTrId("genycaster.setup.farcaster_missing_key");

    //% "Select your data network. Farcaster uses a hosted service; Hypersnap uses independently operated nodes."
    (void)qtTrId("genycaster.setup.connection_mode_help");

    //% "Uses Hypersnap for live reads. Sign-in and publishing permissions are verified separately."
    (void)qtTrId("genycaster.setup.hypersnap_explainer");

    //% "Farcaster hosted read API, when a compatible endpoint is configured; otherwise Hypersnap remains the fallback. Sign-in is separate."
    (void)qtTrId("genycaster.setup.farcaster_explainer");

    //% "Choose your preferred connection. Reads currently use Hypersnap and signed writes use Snapchain. Switching does not migrate your identity or keys."
    (void)qtTrId("genycaster.setup.network_settings_explainer");
}
} // namespace
