#include <QtTranslation>

namespace {

void registerTranslationSourceMetadata()
{
    //% "Application version %1"
    (void)qtTrId("genycaster.update.version_number");
    //% "Alpha release channel"
    (void)qtTrId("genycaster.update.channel_alpha");
    //% "Beta release channel"
    (void)qtTrId("genycaster.update.channel_beta");
    //% "Release candidate"
    (void)qtTrId("genycaster.update.channel_rc");
    //% "Stable release channel"
    (void)qtTrId("genycaster.update.channel_stable");
    //% "You are using a prerelease build. Features may change as testing continues."
    (void)qtTrId("genycaster.update.prerelease_info");
    //% "You already have the latest available build."
    (void)qtTrId("genycaster.update.up_to_date");
    //% "Build identifier %1"
    (void)qtTrId("genycaster.update.build_label");
    //% "Loaded profile posts"
    (void)qtTrId("genycaster.feed_pulse.loaded_profile_posts");
    //% "Loaded feed snapshot"
    (void)qtTrId("genycaster.feed_pulse.loaded_feed_snapshot");
    //% "offline"
    (void)qtTrId("genycaster.feed_pulse.offline");
    //% "cached"
    (void)qtTrId("genycaster.feed_pulse.cached");
    //% "waiting for data"
    (void)qtTrId("genycaster.feed_pulse.waiting_for_data");
    //% "Profile snapshot"
    (void)qtTrId("genycaster.feed_pulse.profile_snapshot");
    //% "Feed pulse"
    (void)qtTrId("genycaster.feed_pulse.title");
    //% "Posts loaded"
    (void)qtTrId("genycaster.feed_pulse.posts_loaded");
    //% "Authors loaded"
    (void)qtTrId("genycaster.feed_pulse.authors_loaded");
    //% "Interactions"
    (void)qtTrId("genycaster.feed_pulse.interactions");
    //% "Avg / post"
    (void)qtTrId("genycaster.feed_pulse.avg_per_post");
    //% "What would you like to do today?"
    (void)qtTrId("genycaster.home.prompt");
    //% "Write and publish a new cast"
    (void)qtTrId("genycaster.home.compose_description");
    //% "Open apps and tools"
    (void)qtTrId("genycaster.home.apps_description");
    //% "Write a new cast or message…"
    (void)qtTrId("genycaster.home.composer_placeholder");
    //% "Find people, casts and channels"
    (void)qtTrId("genycaster.home.search_description");
    //% "Social workspace"
    (void)qtTrId("genycaster.settings.social_workspace");
    //% "Tune the desktop navigation and context columns. You can also drag their separators directly in the main window."
    (void)qtTrId("genycaster.settings.social_workspace_description");
    //% "Navigation width"
    (void)qtTrId("genycaster.settings.navigation_width");
    //% "Primary destinations and account controls"
    (void)qtTrId("genycaster.settings.navigation_width_description");
    //% "Context sidebar width"
    (void)qtTrId("genycaster.settings.context_sidebar_width");
    //% "Profile, activity, search, and feed context"
    (void)qtTrId("genycaster.settings.context_sidebar_width_description");
    //% "Reset social layout"
    (void)qtTrId("genycaster.settings.reset_social_layout");
    //% "Search people, casts, and channels"
    (void)qtTrId("genycaster.social.search_placeholder");
    //% "Your profile"
    (void)qtTrId("genycaster.social.your_profile");
    //% "Sign in to personalize"
    (void)qtTrId("genycaster.social.sign_in_to_personalize");
    //% "%1 followers"
    (void)qtTrId("genycaster.social.followers_count");
    //% "%1 following"
    (void)qtTrId("genycaster.social.following_count");
    //% "Open profile"
    (void)qtTrId("genycaster.social.open_profile");
    //% "People in your orbit"
    (void)qtTrId("genycaster.social.people_in_your_orbit");

    //% "Content & loading"
    (void)qtTrId("genycaster.settings.content_loading");
    //% "Control how GenyCaster requests, hydrates, previews, and reveals casts. These settings apply to future requests."
    (void)qtTrId("genycaster.settings.content_loading_description");
    //% "Posts per refresh"
    (void)qtTrId("genycaster.settings.posts_per_refresh");
    //% "Number of casts requested for each refresh or page. Range: 1–100."
    (void)qtTrId("genycaster.settings.posts_per_refresh_description");
    //% "Reply / quote context"
    (void)qtTrId("genycaster.settings.context_hydration_limit");
    //% "Maximum referenced parent or quoted casts hydrated per page. Range: 1–100."
    (void)qtTrId("genycaster.settings.context_hydration_limit_description");
    //% "Buffer new posts"
    (void)qtTrId("genycaster.settings.buffer_new_posts");
    //% "Keep the current timeline stable and show a “Show N posts” banner when a refresh finds newer casts."
    (void)qtTrId("genycaster.settings.buffer_new_posts_description");
    //% "Prefetch link previews"
    (void)qtTrId("genycaster.settings.link_preview_prefetch");
    //% "Resolve visible web links in advance for richer cards. Disable to reduce background API and network work."
    (void)qtTrId("genycaster.settings.link_preview_prefetch_description");
    //% "Provider safety limits still apply. Higher values may increase latency, bandwidth, memory use, and API cost."
    (void)qtTrId("genycaster.settings.read_api_note");
    //% "Show %1 posts"
    (void)qtTrId("genycaster.feed_surface.show_new_posts");
    //% "Loading original cast…"
    (void)qtTrId("genycaster.cast.loading_original_cast");
    //% "Loading quoted cast…"
    (void)qtTrId("genycaster.embed.loading_quoted_cast");
    //% "Mini App presentation"
    (void)qtTrId("genycaster.miniapp.presentation_title");
    //% "Saved for the next app you open. Changing this does not reload a running app."
    (void)qtTrId("genycaster.miniapp.presentation_description");
    //% "Separate window"
    (void)qtTrId("genycaster.miniapp.mode_window");
    //% "Drawer from bottom"
    (void)qtTrId("genycaster.miniapp.mode_bottom");
    //% "Drawer from left"
    (void)qtTrId("genycaster.miniapp.mode_left");
    //% "Drawer from right"
    (void)qtTrId("genycaster.miniapp.mode_right");
    //% "Open in Genycaster"
    (void)qtTrId("genycaster.miniapp.open_internal");
    //% "The internal Mini App host is not enabled in this build."
    (void)qtTrId("genycaster.miniapp.unavailable");
    //% "Sign in to Genycaster before opening this app."
    (void)qtTrId("genycaster.miniapp.sign_in_first");
    //% "Loading app…"
    (void)qtTrId("genycaster.miniapp.loading");
    //% "This app needs a valid public HTTPS address."
    (void)qtTrId("genycaster.miniapp.invalid_url");
    //% "The app manifest is missing or invalid."
    (void)qtTrId("genycaster.miniapp.invalid_manifest");
    //% "Could not load the app. Check your connection and try again."
    (void)qtTrId("genycaster.miniapp.load_failed");
    //% "The app did not become ready. You can retry or close it."
    (void)qtTrId("genycaster.miniapp.ready_timeout");
    //% "This app requires host capabilities or chains that Genycaster does not support yet. Reload cannot add them."
    (void)qtTrId("genycaster.miniapp.unsupported_requirements");
    //% "The app sent too many or oversized requests and was stopped."
    (void)qtTrId("genycaster.miniapp.rpc_limit");
    //% "Up to four Mini Apps can stay open at once. Close one in Task Manager to open another."
    (void)qtTrId("genycaster.miniapp.session_limit");
    //% "Task Manager"
    (void)qtTrId("genycaster.task_manager.title");
    //% "Live Mini App resource usage and session controls."
    (void)qtTrId("genycaster.task_manager.subtitle");
    //% "SDK activity"
    (void)qtTrId("genycaster.task_manager.sdk_rate");
    //% "Focus"
    (void)qtTrId("genycaster.task_manager.focus");
    //% "Minimize"
    (void)qtTrId("genycaster.task_manager.minimize");
    //% "Terminate"
    (void)qtTrId("genycaster.task_manager.terminate");
    //% "No Mini Apps are running"
    (void)qtTrId("genycaster.task_manager.empty");
    //% "Open a Mini App to see its live resource usage here."
    (void)qtTrId("genycaster.task_manager.empty_description");
    (void)qtTrId("genycaster.task_manager.foreground");
    (void)qtTrId("genycaster.task_manager.minimized");
    (void)qtTrId("genycaster.task_manager.error");
    //% "Apps"
    (void)qtTrId("genycaster.apps.title");
    //% "Discover and save Farcaster Mini Apps, then launch them in the Genycaster host."
    (void)qtTrId("genycaster.apps.description");
    //% "Native Apps"
    (void)qtTrId("genycaster.apps.native");
    //% "Native Genycaster Apps are in development."
    (void)qtTrId("genycaster.apps.native_description");
    //% "My MiniApps"
    (void)qtTrId("genycaster.apps.my_miniapps");
    //% "Search Mini Apps"
    (void)qtTrId("genycaster.apps.search_placeholder");
    //% "All Apps"
    (void)qtTrId("genycaster.apps.all_apps");
    //% "For You"
    (void)qtTrId("genycaster.apps.for_you");
    //% "Personalized discovery needs the configured Mini App API."
    (void)qtTrId("genycaster.apps.personalization_requires_provider");
    //% "My FID"
    (void)qtTrId("genycaster.apps.my_fid");
    //% "Discovering Mini Apps…"
    (void)qtTrId("genycaster.apps.discovering");
    //% "No Mini Apps saved yet"
    (void)qtTrId("genycaster.apps.my_empty");
    //% "Add apps from Explorer to keep them here in Genycaster."
    (void)qtTrId("genycaster.apps.my_empty_description");
    //% "No Mini Apps found"
    (void)qtTrId("genycaster.apps.explorer_empty");
    //% "Try another search or refresh the catalogue."
    (void)qtTrId("genycaster.apps.explorer_empty_description");
    //% "Remove"
    (void)qtTrId("genycaster.apps.remove");
    //% "Save"
    (void)qtTrId("genycaster.apps.save");
    //% "Saved"
    (void)qtTrId("genycaster.apps.saved");
    //% "Genycaster"
    (void)qtTrId("genycaster.app.genycaster");
    //% "Explorer"
    (void)qtTrId("genycaster.bottom_nav.explorer");
    //% "Home"
    (void)qtTrId("genycaster.bottom_nav.home");
    //% "Notifications"
    (void)qtTrId("genycaster.bottom_nav.notifications");
    //% "Store"
    (void)qtTrId("genycaster.bottom_nav.store");
    //% "Cached · refreshing in background"
    (void)qtTrId("genycaster.bridge.cached_refreshing_in_background");
    //% "Checking local identity…"
    (void)qtTrId("genycaster.bridge.checking_local_identity");
    //% "Choose a canonical cast before publishing a reply."
    (void)qtTrId("genycaster.bridge.choose_a_canonical_cast_before_publishing_a_reply");
    //% "Enable a verified Farcaster signer before changing follows."
    (void)qtTrId("genycaster.bridge.enable_a_verified_farcaster_signer_before_changing_follows");
    //% "Enable a verified Farcaster signer before deleting a cast."
    (void)qtTrId("genycaster.bridge.enable_a_verified_farcaster_signer_before_deleting_a_cast");
    //% "Enable a verified Farcaster signer before interacting."
    (void)qtTrId("genycaster.bridge.enable_a_verified_farcaster_signer_before_interacting");
    //% "Enable a verified Farcaster signer before publishing."
    (void)qtTrId("genycaster.bridge.enable_a_verified_farcaster_signer_before_publishing");
    //% "Enable a verified Farcaster signer before replying."
    (void)qtTrId("genycaster.bridge.enable_a_verified_farcaster_signer_before_replying");
    //% "Enter a channel id."
    (void)qtTrId("genycaster.bridge.enter_a_channel_id");
    //% "Enter a Farcaster username."
    (void)qtTrId("genycaster.bridge.enter_a_farcaster_username");
    //% "Enter something to search."
    (void)qtTrId("genycaster.bridge.enter_a_search_query");
    //% "Loading the next page…"
    (void)qtTrId("genycaster.bridge.loading_the_next_page");
    //% "Local runtime unavailable"
    (void)qtTrId("genycaster.bridge.local_runtime_unavailable");
    //% "Only valid HTTP(S) links can be opened externally."
    (void)qtTrId("genycaster.bridge.only_valid_http_s_links_can_be_opened_externally");
    //% "Only your own canonical Farcaster cast can be deleted."
    (void)qtTrId("genycaster.bridge.only_your_own_canonical_farcaster_cast_can_be_deleted");
    //% "Open a profile before loading profile posts."
    (void)qtTrId("genycaster.bridge.open_a_profile_before_loading_profile_posts");
    //% "Open an identity before reading Following."
    (void)qtTrId("genycaster.bridge.open_an_identity_before_reading_following");
    //% "Open an identity first."
    (void)qtTrId("genycaster.bridge.open_an_identity_first");
    //% "Preparing a canonical Farcaster cast…"
    (void)qtTrId("genycaster.bridge.preparing_a_canonical_farcaster_cast");
    //% "Preparing a canonical Farcaster reply…"
    (void)qtTrId("genycaster.bridge.preparing_a_canonical_farcaster_reply");
    //% "Preparing the Farcaster cast deletion…"
    (void)qtTrId("genycaster.bridge.preparing_the_farcaster_cast_deletion");
    //% "Preparing the Farcaster follow…"
    (void)qtTrId("genycaster.bridge.preparing_the_farcaster_follow");
    //% "Preparing the Farcaster interaction…"
    (void)qtTrId("genycaster.bridge.preparing_the_farcaster_interaction");
    //% "Preparing the Farcaster unfollow…"
    (void)qtTrId("genycaster.bridge.preparing_the_farcaster_unfollow");
    //% "Preparing to remove the Farcaster interaction…"
    (void)qtTrId("genycaster.bridge.preparing_to_remove_the_farcaster_interaction");
    //% "Reading cached cast…"
    (void)qtTrId("genycaster.bridge.reading_cached_cast");
    //% "Reading cached channel…"
    (void)qtTrId("genycaster.bridge.reading_cached_channel");
    //% "Reading cached feed…"
    (void)qtTrId("genycaster.bridge.reading_cached_feed");
    //% "Reading cached followers…"
    (void)qtTrId("genycaster.bridge.reading_cached_followers");
    //% "Reading cached following…"
    (void)qtTrId("genycaster.bridge.reading_cached_following");
    //% "Reading cached likes…"
    (void)qtTrId("genycaster.bridge.reading_cached_likes");
    //% "Reading cached popular casts…"
    (void)qtTrId("genycaster.bridge.reading_cached_popular_casts");
    //% "Reading cached posts…"
    (void)qtTrId("genycaster.bridge.reading_cached_posts");
    //% "Reading cached profile…"
    (void)qtTrId("genycaster.bridge.reading_cached_profile");
    //% "Reading cached quotes…"
    (void)qtTrId("genycaster.bridge.reading_cached_quotes");
    //% "Reading cached replies…"
    (void)qtTrId("genycaster.bridge.reading_cached_replies");
    //% "Ready to read"
    (void)qtTrId("genycaster.bridge.ready_to_read");
    //% "The reply target is not available as a canonical cast in the current presentation state."
    (void)qtTrId("genycaster.bridge.reply_target_not_available_as_canonical_cast");
    //% "Sign in before changing follows."
    (void)qtTrId("genycaster.bridge.sign_in_before_changing_follows");
    //% "Sign in before deleting a cast."
    (void)qtTrId("genycaster.bridge.sign_in_before_deleting_a_cast");
    //% "Sign in before enabling interactions."
    (void)qtTrId("genycaster.bridge.sign_in_before_enabling_interactions");
    //% "Sign in before interacting with a cast."
    (void)qtTrId("genycaster.bridge.sign_in_before_interacting_with_a_cast");
    //% "Sign in before opening your account profile."
    (void)qtTrId("genycaster.bridge.sign_in_before_opening_your_account_profile");
    //% "Sign in before publishing a cast."
    (void)qtTrId("genycaster.bridge.sign_in_before_publishing_a_cast");
    //% "Sign in before replying to a cast."
    (void)qtTrId("genycaster.bridge.sign_in_before_replying_to_a_cast");
    //% "Sign in is not configured in this build. A verified account is required to continue."
    (void)qtTrId("genycaster.bridge.sign_in_is_not_configured_guest_reading_remains_available");
    //% "Starting local read runtime…"
    (void)qtTrId("genycaster.bridge.starting_local_read_runtime");
    //% "That cast identifier is empty."
    (void)qtTrId("genycaster.bridge.that_cast_identifier_is_empty");
    //% "That Farcaster follow target is invalid."
    (void)qtTrId("genycaster.bridge.that_farcaster_follow_target_is_invalid");
    //% "That Farcaster profile identifier is invalid."
    (void)qtTrId("genycaster.bridge.that_farcaster_profile_identifier_is_invalid");
    //% "That Farcaster reaction target is invalid."
    (void)qtTrId("genycaster.bridge.that_farcaster_reaction_target_is_invalid");
    //% "That reply target has an empty cast identifier."
    (void)qtTrId("genycaster.bridge.that_reply_target_has_an_empty_cast_identifier");
    //% "The authentication approval link is invalid."
    (void)qtTrId("genycaster.bridge.the_authentication_approval_link_is_invalid");
    //% "The local read runtime is still starting."
    (void)qtTrId("genycaster.bridge.the_local_read_runtime_is_still_starting");
    //% "The local runtime is still starting."
    (void)qtTrId("genycaster.bridge.the_local_runtime_is_still_starting");
    //% "The system could not open that link."
    (void)qtTrId("genycaster.bridge.the_system_could_not_open_that_link");
    //% "The system could not open the Farcaster approval link."
    (void)qtTrId("genycaster.bridge.the_system_could_not_open_the_farcaster_approval_link");
    //% "Write something before publishing."
    (void)qtTrId("genycaster.bridge.write_something_before_publishing");
    //% "Write something before replying."
    (void)qtTrId("genycaster.bridge.write_something_before_replying");
    //% "Copy cast ID"
    (void)qtTrId("genycaster.cast.copy_cast_id");
    //% "Copy text"
    (void)qtTrId("genycaster.cast.copy_text");
    //% "Delete"
    (void)qtTrId("genycaster.cast.delete");
    //% "Open author profile"
    (void)qtTrId("genycaster.cast.open_author_profile");
    //% "Open conversation context"
    (void)qtTrId("genycaster.cast.open_conversation_context");
    //% "Open original cast"
    (void)qtTrId("genycaster.cast.open_original_cast");
    //% "Publish failed · saved locally"
    (void)qtTrId("genycaster.cast.publish_failed_saved_locally");
    //% "Published"
    (void)qtTrId("genycaster.cast.published");
    //% "Publishing · saved locally"
    (void)qtTrId("genycaster.cast.publishing_saved_locally");
    //% "Queued for retry · saved locally"
    (void)qtTrId("genycaster.cast.queued_for_retry_saved_locally");
    //% "Reply failed · saved locally"
    (void)qtTrId("genycaster.cast.reply_failed_saved_locally");
    //% "Reply in a conversation"
    (void)qtTrId("genycaster.cast.reply_in_a_conversation");
    //% "Reply published"
    (void)qtTrId("genycaster.cast.reply_published");
    //% "Replying · saved locally"
    (void)qtTrId("genycaster.cast.replying_saved_locally");
    //% "Replying to another cast"
    (void)qtTrId("genycaster.cast.replying_to_another_cast");
    //% "Replying to %1"
    (void)qtTrId("genycaster.cast.replying_to_value");
    //% "Unknown"
    (void)qtTrId("genycaster.cast.unknown");
    //% "%1 embeds"
    (void)qtTrId("genycaster.cast.value_embeds");
    //% "View conversation"
    (void)qtTrId("genycaster.cast.more_options");
    (void)qtTrId("genycaster.cast.view_conversation");
    //% "Waiting for write authority · saved locally"
    (void)qtTrId("genycaster.cast.waiting_for_write_authority_saved_locally");
    //% "Bold"
    (void)qtTrId("genycaster.composer.bold");
    //% "Italic"
    (void)qtTrId("genycaster.composer.italic");
    //% "Bulleted list"
    (void)qtTrId("genycaster.composer.list");
    //% "Nothing to preview yet."
    (void)qtTrId("genycaster.composer.preview_empty");
    //% "Quote"
    (void)qtTrId("genycaster.composer.quote");
    //% "Conversation"
    (void)qtTrId("genycaster.detail.conversation");
    //% "Open channel %1"
    (void)qtTrId("genycaster.detail.open_channel");
    //% "Back to feed"
    (void)qtTrId("genycaster.detail.back_back_to_feed");
    //% "Canonical detail"
    (void)qtTrId("genycaster.detail.canonical_detail");
    //% "Delete cast"
    (void)qtTrId("genycaster.detail.delete_cast");
    //% "FID %1"
    (void)qtTrId("genycaster.detail.fid_value");
    //% "Not following"
    (void)qtTrId("genycaster.search.not_following");
    //% "Does not follow you"
    (void)qtTrId("genycaster.search.not_followed_by");
    //% "Relationship unknown"
    (void)qtTrId("genycaster.search.relationship_unknown");
    //% "No matching posts in the loaded feed"
    (void)qtTrId("genycaster.feed.filtered_empty");
    //% "These tabs filter the loaded feed. Load more posts or change the source; unavailable data is not an empty category."
    (void)qtTrId("genycaster.feed.filtered_description");
    //% "Look through more posts"
    (void)qtTrId("genycaster.feed.more_filtered");
    //% "Follow"
    (void)qtTrId("genycaster.detail.follow");
    //% "Followed"
    (void)qtTrId("genycaster.detail.followed");
    //% "Followers"
    (void)qtTrId("genycaster.detail.followers");
    //% "Following"
    (void)qtTrId("genycaster.detail.following");
    //% "No detail cached yet."
    (void)qtTrId("genycaster.detail.no_detail_cached_yet");
    //% "Refresh"
    (void)qtTrId("genycaster.detail.refresh");
    //% "Refreshing…"
    (void)qtTrId("genycaster.detail.refreshing");
    //% "Unfollow"
    (void)qtTrId("genycaster.detail.unfollow");
    //% "External link"
    (void)qtTrId("genycaster.embed.external_link");
    //% "Image"
    (void)qtTrId("genycaster.embed.image");
    //% "Image embed"
    (void)qtTrId("genycaster.embed.image_embed");
    //% "Preview unavailable"
    (void)qtTrId("genycaster.embed.media_unavailable");
    //% "Open link"
    (void)qtTrId("genycaster.embed.open_link");
    //% "Open original image"
    (void)qtTrId("genycaster.embed.open_original_image_external");
    //% "Open quoted cast"
    (void)qtTrId("genycaster.embed.open_quoted_cast");
    //% "Open in your default app"
    (void)qtTrId("genycaster.embed.open_safely_in_your_default_app_external");
    //% "Play video"
    (void)qtTrId("genycaster.embed.play_video");
    //% "Quoted cast"
    (void)qtTrId("genycaster.embed.quoted_cast");
    //% "Video"
    (void)qtTrId("genycaster.embed.video");
    //% "View quoted cast"
    (void)qtTrId("genycaster.embed.view_quoted_cast_next");
    //% "Browse Explore"
    (void)qtTrId("genycaster.empty.browse_explore");
    //% "Public discovery is temporarily unavailable. Retry when the service is available."
    (void)qtTrId("genycaster.empty.explore_is_temporarily_unavailable_open_an_identity_fo");
    //% "Explore is temporarily unavailable. Your Home feed remains independent while discovery recovers."
    (void)qtTrId("genycaster.empty.explore_is_temporarily_unavailable_your_home_feed_rema");
    //% "Genycaster checks local state first, then refreshes from the selected source."
    (void)qtTrId("genycaster.empty.genycaster_checks_local_state_first_then_refreshes_fro");
    //% "Live reading is not configured"
    (void)qtTrId("genycaster.empty.live_reading_is_not_configured");
    //% "Loading your reading surface…"
    (void)qtTrId("genycaster.empty.loading_your_reading_surface");
    //% "No saved posts for offline reading"
    (void)qtTrId("genycaster.empty.no_saved_posts_for_offline_reading");
    //% "Nothing here yet"
    (void)qtTrId("genycaster.empty.nothing_here_yet");
    //% "Open an identity, choose another feed, or refresh this surface."
    (void)qtTrId("genycaster.empty.open_an_identity_choose_another_feed_or_refresh_this_s");
    //% "Open an identity for your Home feed, or use Explore for network-wide discovery."
    (void)qtTrId("genycaster.empty.open_an_identity_for_your_home_feed_or_use_explore_for");
    //% "Open Home"
    (void)qtTrId("genycaster.empty.open_home");
    //% "Open Home for this identity, or use Explore when you want network-wide discovery."
    (void)qtTrId("genycaster.empty.open_home_for_this_identity_or_use_explore_when_you_wa");
    //% "Open identity"
    (void)qtTrId("genycaster.empty.open_identity");
    //% "Open your Farcaster home"
    (void)qtTrId("genycaster.empty.open_your_farcaster_home");
    //% "Reconnect and refresh once to populate the local cache."
    (void)qtTrId("genycaster.empty.reconnect_and_refresh_once_to_populate_the_local_cache");
    //% "Retry"
    (void)qtTrId("genycaster.empty.retry");
    //% "The live source did not return this feed"
    (void)qtTrId("genycaster.empty.the_live_source_did_not_return_this_feed");
    //% "This feed could not be loaded"
    (void)qtTrId("genycaster.empty.this_feed_could_not_be_loaded");
    //% "Explore could not refresh"
    (void)qtTrId("genycaster.empty.trending_is_unavailable_from_this_source");
    //% "Try again"
    (void)qtTrId("genycaster.empty.try_again");
    //% "Your device may still be online. Retry or use another available reading surface."
    (void)qtTrId("genycaster.empty.your_device_may_still_be_online_retry_or_use_another_a");
    //% "Like · %1 likes"
    (void)qtTrId("genycaster.engagement.like_value_likes");
    //% "Recast · %1 recasts"
    (void)qtTrId("genycaster.engagement.recast_value_recasts");
    //% "Reply · %1 replies"
    (void)qtTrId("genycaster.engagement.reply_value_replies");
    //% "Undo recast · %1 recasts"
    (void)qtTrId("genycaster.engagement.undo_recast_value_recasts");
    //% "Unlike · %1 likes"
    (void)qtTrId("genycaster.engagement.unlike_value_likes");
    //% "%1 replies"
    (void)qtTrId("genycaster.engagement.value_replies");
    //% "A network-wide view from the selected live source"
    (void)qtTrId("genycaster.feed_header.a_network_wide_view_from_the_selected_live_source");
    //% "Casts scoped to the channel you opened"
    (void)qtTrId("genycaster.feed_header.casts_scoped_to_the_channel_you_opened");
    //% "Channel"
    (void)qtTrId("genycaster.feed_header.channel");
    //% "Discover people, channels, and casts"
    (void)qtTrId("genycaster.feed_header.discover_people_channels_and_casts");
    //% "Explore"
    (void)qtTrId("genycaster.feed_header.explore");
    //% "Home"
    (void)qtTrId("genycaster.feed_header.home");
    //% "Open"
    (void)qtTrId("genycaster.feed_header.open");
    //% "Recent casts from the people this identity follows"
    (void)qtTrId("genycaster.feed_header.recent_casts_from_the_people_this_identity_follows");
    //% "Refresh"
    (void)qtTrId("genycaster.feed_header.refresh");
    //% "Refreshing…"
    (void)qtTrId("genycaster.feed_header.refreshing");
    //% "Use @ for profiles, / for channels, or paste a cast id."
    (void)qtTrId("genycaster.feed_header.search_hint");
    //% "Search people, @user, /channel, or cast id"
    (void)qtTrId("genycaster.feed_header.search_people_channels_casts");
    //% "Explore search"
    (void)qtTrId("genycaster.feed_header.universal_search");
    //% "Loading more"
    (void)qtTrId("genycaster.feed_surface.loading_more");
    //% "Account"
    (void)qtTrId("genycaster.header.account");
    //% "Genycaster"
    (void)qtTrId("genycaster.header.genycaster");
    //% "New cast"
    (void)qtTrId("genycaster.header.new_cast");
    //% "Open your signed-in account session"
    (void)qtTrId("genycaster.header.open_your_signed_in_account_session");
    //% "Sign in"
    (void)qtTrId("genycaster.header.sign_in");
    //% "Sign in to interact on Farcaster"
    (void)qtTrId("genycaster.header.sign_in_to_interact_on_farcaster");
    //% "Add"
    (void)qtTrId("genycaster.locale.add_currency");
    //% "Also show"
    (void)qtTrId("genycaster.locale.additional_currencies");
    //% "Optional quote currencies are shown only when a trusted valuation is available. They do not replace the primary currency."
    (void)qtTrId("genycaster.locale.additional_currencies_description");
    //% "%1 = %2"
    (void)qtTrId("genycaster.locale.asset_quote");
    //% "Calendar"
    (void)qtTrId("genycaster.locale.calendar");
    //% "Automatic calendar"
    (void)qtTrId("genycaster.locale.calendar_auto");
    //% "Gregorian"
    (void)qtTrId("genycaster.locale.calendar_gregorian");
    //% "Islamic Civil"
    (void)qtTrId("genycaster.locale.calendar_islamic_civil");
    //% "Solar Hijri (Jalali)"
    (void)qtTrId("genycaster.locale.calendar_jalali");
    //% "Julian"
    (void)qtTrId("genycaster.locale.calendar_julian");
    //% "Revised Julian (Milanković)"
    (void)qtTrId("genycaster.locale.calendar_milankovic");
    //% "Shahanshahi (Imperial)"
    (void)qtTrId("genycaster.locale.calendar_shahanshahi");
    //% "Close"
    (void)qtTrId("genycaster.locale.close");
    //% "%1B"
    (void)qtTrId("genycaster.locale.compact_billion");
    //% "%1M"
    (void)qtTrId("genycaster.locale.compact_million");
    //% "%1K"
    (void)qtTrId("genycaster.locale.compact_thousand");
    //% "Currency"
    (void)qtTrId("genycaster.locale.currency");
    //% "Automatic (%1)"
    (void)qtTrId("genycaster.locale.currency_auto_value");
    //% "Currency selection controls formatting only. Token and FX conversion require a timestamped quote source and never happen inside localization."
    (void)qtTrId("genycaster.locale.currency_formatting_only");
    //% "IRT — Toman"
    (void)qtTrId("genycaster.locale.currency_irt_toman");
    //% "Done"
    (void)qtTrId("genycaster.locale.done");
    //% "Language"
    (void)qtTrId("genycaster.locale.language");
    //% "System language"
    (void)qtTrId("genycaster.locale.language_system");
    //% "Preview"
    (void)qtTrId("genycaster.locale.preview");
    //% "System regional format"
    (void)qtTrId("genycaster.locale.region_system");
    //% "Regional format"
    (void)qtTrId("genycaster.locale.regional_format");
    //% "Controls numbers, dates and automatic local currency independently from the UI language."
    (void)qtTrId("genycaster.locale.regional_format_description");
    //% "1 day ago"
    (void)qtTrId("genycaster.locale.relative_day");
    //% "%1 days ago"
    (void)qtTrId("genycaster.locale.relative_days");
    //% "1 hour ago"
    (void)qtTrId("genycaster.locale.relative_hour");
    //% "%1 hours ago"
    (void)qtTrId("genycaster.locale.relative_hours");
    //% "1 minute ago"
    (void)qtTrId("genycaster.locale.relative_minute");
    //% "%1 minutes ago"
    (void)qtTrId("genycaster.locale.relative_minutes");
    //% "1 month ago"
    (void)qtTrId("genycaster.locale.relative_month");
    //% "%1 months ago"
    (void)qtTrId("genycaster.locale.relative_months");
    //% "now"
    (void)qtTrId("genycaster.locale.relative_now");
    //% "1 week ago"
    (void)qtTrId("genycaster.locale.relative_week");
    //% "%1 weeks ago"
    (void)qtTrId("genycaster.locale.relative_weeks");
    //% "1 year ago"
    (void)qtTrId("genycaster.locale.relative_year");
    //% "%1 years ago"
    (void)qtTrId("genycaster.locale.relative_years");
    //% "%1، %2 %3 %4 · %5"
    (void)qtTrId("genycaster.locale.shahanshahi_long");
    //% "%1 %2 %3 · %4"
    (void)qtTrId("genycaster.locale.shahanshahi_medium");
    //% "%1/%2/%3 · %4"
    (void)qtTrId("genycaster.locale.shahanshahi_short");
    //% "Time zone"
    (void)qtTrId("genycaster.locale.time_zone");
    //% "System time zone"
    (void)qtTrId("genycaster.locale.time_zone_system");
    //% "Calendar"
    (void)qtTrId("genycaster.locale.timestamp_calendar");
    //% "Timestamp display"
    (void)qtTrId("genycaster.locale.timestamp_display");
    //% "Smart keeps recent activity relative and uses your selected calendar for older dates. Calendar always shows the selected calendar; Relative always shows elapsed time."
    (void)qtTrId("genycaster.locale.timestamp_display_description");
    //% "Relative"
    (void)qtTrId("genycaster.locale.timestamp_relative");
    //% "Smart"
    (void)qtTrId("genycaster.locale.timestamp_smart");
    //% "%1 toman"
    (void)qtTrId("genycaster.locale.toman_amount");
    //% "Back to feed"
    (void)qtTrId("genycaster.profile.back_back_to_feed");
    //% "Farcaster profile"
    (void)qtTrId("genycaster.profile.farcaster_profile");
    //% "Follow"
    (void)qtTrId("genycaster.profile.follow");
    //% "Followed"
    (void)qtTrId("genycaster.profile.followed");
    //% "Follows you"
    (void)qtTrId("genycaster.profile.follows_you");
    //% "Likes"
    (void)qtTrId("genycaster.profile.likes");
    //% "Loading more profile posts"
    (void)qtTrId("genycaster.profile.loading_more_profile_posts");
    //% "No liked casts here yet"
    (void)qtTrId("genycaster.profile.no_liked_casts_here_yet");
    //% "No popular casts available yet"
    (void)qtTrId("genycaster.profile.no_popular_casts_available_yet");
    //% "No posts here yet"
    (void)qtTrId("genycaster.profile.no_posts_here_yet");
    //% "No quotes here yet"
    (void)qtTrId("genycaster.profile.no_quotes_here_yet");
    //% "No replies here yet"
    (void)qtTrId("genycaster.profile.no_replies_here_yet");
    //% "Open profile link"
    (void)qtTrId("genycaster.profile.open_profile_link");
    //% "Popular"
    (void)qtTrId("genycaster.profile.popular");
    //% "Posts"
    (void)qtTrId("genycaster.profile.posts");
    //% "Quotes"
    (void)qtTrId("genycaster.profile.quotes");
    //% "Refresh"
    (void)qtTrId("genycaster.profile.refresh");
    //% "Refreshing…"
    (void)qtTrId("genycaster.profile.refreshing");
    //% "Replies"
    (void)qtTrId("genycaster.profile.replies");
    //% "Saved profile data is empty. Retry when the live source responds."
    (void)qtTrId("genycaster.profile.saved_profile_data_is_empty_retry_when_the_live_source");
    //% "Try another profile tab or refresh."
    (void)qtTrId("genycaster.profile.try_another_profile_tab_or_refresh");
    //% "Unfollow"
    (void)qtTrId("genycaster.profile.unfollow");
    //% "%1 Followers"
    (void)qtTrId("genycaster.profile.value_followers");
    //% "%1 Following"
    (void)qtTrId("genycaster.profile.value_following");
    //% "Unknown"
    (void)qtTrId("genycaster.profile_list.unknown");
    //% "Save casts, threads, profiles, and collections into a fast local library."
    (void)qtTrId("genycaster.roadmap.bookmarks_description");
    //% "Bookmarks"
    (void)qtTrId("genycaster.roadmap.bookmarks_title");
    //% "Profiles, analytics, publishing tools, customer conversations, and commerce for teams."
    (void)qtTrId("genycaster.roadmap.business_description");
    //% "Business"
    (void)qtTrId("genycaster.roadmap.business_title");
    //% "Private conversation with a native desktop workflow and local-first history."
    (void)qtTrId("genycaster.roadmap.messages_description");
    //% "Messages"
    (void)qtTrId("genycaster.roadmap.messages_title");
    //% "A focused inbox for mentions, replies, reactions, follows, and important activity."
    (void)qtTrId("genycaster.roadmap.notifications_description");
    //% "Notifications"
    (void)qtTrId("genycaster.roadmap.notifications_title");
    //% "Discover apps, digital goods, memberships, and Farcaster-native commerce in one place."
    (void)qtTrId("genycaster.roadmap.store_description");
    //% "Store"
    (void)qtTrId("genycaster.roadmap.store_title");
    //% "Cast"
    (void)qtTrId("genycaster.runtime.cast");
    //% "Cast deletion"
    (void)qtTrId("genycaster.runtime.cast_deletion");
    //% "Cast deletion could not be scheduled."
    (void)qtTrId("genycaster.runtime.cast_deletion_could_not_be_scheduled");
    //% "Cast deletion is unavailable in this runtime."
    (void)qtTrId("genycaster.runtime.cast_deletion_is_unavailable_in_this_runtime");
    //% "Checking the protocol signer against the network…"
    (void)qtTrId("genycaster.runtime.checking_the_protocol_signer_against_the_network");
    //% "Publishing is not configured for this Genycaster build yet. Reading remains available."
    (void)qtTrId("genycaster.runtime.client_attribution_not_configured");
    //% " confirmed by authoritative Farcaster state."
    (void)qtTrId("genycaster.runtime.confirmed_by_authoritative_farcaster_state");
    //% "could not reach live source"
    (void)qtTrId("genycaster.runtime.could_not_reach_live_source");
    //% "Custody wallet approval is required to enable interactions."
    (void)qtTrId("genycaster.runtime.custody_wallet_approval_is_required_to_enable_interactions");
    //% "Durable CAST_ADD composition is not available."
    (void)qtTrId("genycaster.runtime.durable_cast_add_composition_is_not_available");
    //% "Durable CAST_REMOVE composition is not available."
    (void)qtTrId("genycaster.runtime.durable_cast_remove_composition_is_not_available");
    //% "Durable follow composition is not available."
    (void)qtTrId("genycaster.runtime.durable_follow_composition_is_not_available");
    //% "Durable PublishCast composition is not available."
    (void)qtTrId("genycaster.runtime.durable_publishcast_composition_is_not_available");
    //% "Durable reaction composition is not available."
    (void)qtTrId("genycaster.runtime.durable_reaction_composition_is_not_available");
    //% " failed. The signed operation remains recorded locally."
    (void)qtTrId("genycaster.runtime.failed_the_signed_operation_remains_recorded_locally");
    //% "Farcaster could not verify this sign-in. Start a new request and approve the matching account."
    (void)qtTrId("genycaster.runtime.farcaster_could_not_verify_this_sign_in_start_a_new_request_");
    //% "Farcaster is rate limiting this write. It is saved locally for retry."
    (void)qtTrId("genycaster.runtime.farcaster_is_rate_limiting_this_write_it_is_saved_locally_fo");
    //% "Farcaster rejected the canonical protocol message."
    (void)qtTrId("genycaster.runtime.farcaster_rejected_the_canonical_protocol_message");
    //% "Follow"
    (void)qtTrId("genycaster.runtime.follow");
    //% "Follow change could not be scheduled."
    (void)qtTrId("genycaster.runtime.follow_change_could_not_be_scheduled");
    //% "Follow changes are unavailable in this runtime."
    (void)qtTrId("genycaster.runtime.follow_changes_are_unavailable_in_this_runtime");
    //% "Genycaster could not reach the selected live source. Check the source or connection and retry."
    (void)qtTrId("genycaster.runtime.genycaster_could_not_reach_the_selected_live_source_check_th");
    //% "Interaction signer is not enabled yet."
    (void)qtTrId("genycaster.runtime.interaction_signer_is_not_enabled_yet");
    //% "Interaction signing could not be verified safely. Reading is unaffected."
    (void)qtTrId("genycaster.runtime.interaction_signing_could_not_be_verified_safely_reading_is_");
    //% "Interaction signing is ready."
    (void)qtTrId("genycaster.runtime.interaction_signing_is_ready");
    //% "Interaction signing is temporarily unavailable. Reading is unaffected."
    (void)qtTrId("genycaster.runtime.interaction_signing_is_temporarily_unavailable_reading_is_un");
    //% "Interaction signing is unavailable."
    (void)qtTrId("genycaster.runtime.interaction_signing_is_unavailable");
    //% " is paused until the verified signer is available again."
    (void)qtTrId("genycaster.runtime.is_paused_until_the_verified_signer_is_available_again");
    //% " is saved locally. A bounded network retry is scheduled."
    (void)qtTrId("genycaster.runtime.is_saved_locally_a_bounded_network_retry_is_scheduled");
    //% "Like"
    (void)qtTrId("genycaster.runtime.like");
    //% "live format mismatch"
    (void)qtTrId("genycaster.runtime.live_format_mismatch");
    //% "live read timed out"
    (void)qtTrId("genycaster.runtime.live_read_timed_out");
    //% "live source unavailable"
    (void)qtTrId("genycaster.runtime.live_source_unavailable");
    //% "Loading live data…"
    (void)qtTrId("genycaster.runtime.loading_live_data");
    //% "Nothing was found for that identity or item."
    (void)qtTrId("genycaster.runtime.nothing_was_found_for_that_identity_or_item");
    //% "Protocol write"
    (void)qtTrId("genycaster.runtime.protocol_write");
    //% "Publish could not be scheduled."
    (void)qtTrId("genycaster.runtime.publish_could_not_be_scheduled");
    //% "Publishing is unavailable in this runtime."
    (void)qtTrId("genycaster.runtime.publishing_is_unavailable_in_this_runtime");
    //% "Reaction could not be scheduled."
    (void)qtTrId("genycaster.runtime.reaction_could_not_be_scheduled");
    //% "Reactions are unavailable in this runtime."
    (void)qtTrId("genycaster.runtime.reactions_are_unavailable_in_this_runtime");
    //% "Recast"
    (void)qtTrId("genycaster.runtime.recast");
    //% "Recast removal"
    (void)qtTrId("genycaster.runtime.recast_removal");
    //% "Refresh did not complete"
    (void)qtTrId("genycaster.runtime.refresh_did_not_complete");
    //% "refresh failed"
    (void)qtTrId("genycaster.runtime.refresh_failed");
    //% "Reply"
    (void)qtTrId("genycaster.runtime.reply");
    //% "Reply could not be scheduled."
    (void)qtTrId("genycaster.runtime.reply_could_not_be_scheduled");
    //% "Replying is unavailable in this runtime."
    (void)qtTrId("genycaster.runtime.replying_is_unavailable_in_this_runtime");
    //% "Saved data · "
    (void)qtTrId("genycaster.runtime.saved_data");
    //% "Saved data · refreshing"
    (void)qtTrId("genycaster.runtime.saved_data_refreshing");
    //% " saved locally before network submission."
    (void)qtTrId("genycaster.runtime.saved_locally_before_network_submission");
    //% " saved locally. Network submission is not configured in this runtime."
    (void)qtTrId("genycaster.runtime.saved_locally_network_submission_is_not_configured_in_this_r");
    //% "Sign in before enabling interactions."
    (void)qtTrId("genycaster.runtime.sign_in_before_enabling_interactions");
    //% "Sign in could not be completed."
    (void)qtTrId("genycaster.runtime.sign_in_could_not_be_completed");
    //% "Secure Genycaster client attribution is not configured yet. Reading remains available."
    (void)qtTrId("genycaster.runtime.signer_attestation_not_configured");
    //% "Snapchain rejected or contradicted the signer authorization state."
    (void)qtTrId("genycaster.runtime.snapchain_rejected_or_contradicted_the_signer_authorization_");
    //% "Submitting %1 to Farcaster…"
    (void)qtTrId("genycaster.runtime.submitting_value_to_farcaster");
    //% "That read request is not valid."
    (void)qtTrId("genycaster.runtime.that_read_request_is_not_valid");
    //% "The authentication worker is unavailable."
    (void)qtTrId("genycaster.runtime.the_authentication_worker_is_unavailable");
    //% "The custody approval or network confirmation timed out."
    (void)qtTrId("genycaster.runtime.the_custody_approval_or_network_confirmation_timed_out");
    //% "The custody wallet did not authorize this signer request."
    (void)qtTrId("genycaster.runtime.the_custody_wallet_did_not_authorize_this_signer_request");
    //% "The live source did not answer in time. This does not mean your device is offline."
    (void)qtTrId("genycaster.runtime.the_live_source_did_not_answer_in_time_this_does_not_mean_yo");
    //% "The live source is rate limited. Saved data remains available."
    (void)qtTrId("genycaster.runtime.the_live_source_is_rate_limited_saved_data_remains_available");
    //% "The live source returned data this build cannot decode yet."
    (void)qtTrId("genycaster.runtime.the_live_source_returned_data_this_build_cannot_decode_yet");
    //% "The local account session could not be saved."
    (void)qtTrId("genycaster.runtime.the_local_account_session_could_not_be_saved");
    //% "The local cache could not be updated."
    (void)qtTrId("genycaster.runtime.the_local_cache_could_not_be_updated");
    //% "The network is temporarily unavailable. The write is saved locally for retry."
    (void)qtTrId("genycaster.runtime.the_network_is_temporarily_unavailable_the_write_is_saved_lo");
    //% "The network reported authoritative state that conflicts with this write."
    (void)qtTrId("genycaster.runtime.the_network_reported_authoritative_state_that_conflicts_with");
    //% "The platform secure store or signer metadata could not be accessed."
    (void)qtTrId("genycaster.runtime.the_platform_secure_store_or_signer_metadata_could_not_be_ac");
    //% "The protocol signer expired. Enable interactions again to continue."
    (void)qtTrId("genycaster.runtime.the_protocol_signer_expired_enable_interactions_again_to_con");
    //% "The protocol signer has been revoked."
    (void)qtTrId("genycaster.runtime.the_protocol_signer_has_been_revoked");
    //% "The protocol write could not be completed safely."
    (void)qtTrId("genycaster.runtime.the_protocol_write_could_not_be_completed_safely");
    //% "The read could not be completed."
    (void)qtTrId("genycaster.runtime.the_read_could_not_be_completed");
    //% "The read worker is unavailable."
    (void)qtTrId("genycaster.runtime.the_read_worker_is_unavailable");
    //% "The secure signer, wallet, or Snapchain endpoint is currently unavailable."
    (void)qtTrId("genycaster.runtime.the_secure_signer_wallet_or_snapchain_endpoint_is_currently_");
    //% "The selected live source is temporarily unavailable. Your connection may still be fine."
    (void)qtTrId("genycaster.runtime.the_selected_live_source_is_temporarily_unavailable_your_con");
    //% "The selected source rejected this read."
    (void)qtTrId("genycaster.runtime.the_selected_source_rejected_this_read");
    //% "The sign-in proof could not be validated safely."
    (void)qtTrId("genycaster.runtime.the_sign_in_proof_could_not_be_validated_safely");
    //% "The signer authorization configuration is invalid."
    (void)qtTrId("genycaster.runtime.the_signer_authorization_configuration_is_invalid");
    //% "The signer operation could not be completed safely."
    (void)qtTrId("genycaster.runtime.the_signer_operation_could_not_be_completed_safely");
    //% "The write could not be saved safely to the durable outbox."
    (void)qtTrId("genycaster.runtime.the_write_could_not_be_saved_safely_to_the_durable_outbox");
    //% "This build is missing a required signer capability or endpoint configuration."
    (void)qtTrId("genycaster.runtime.this_build_is_missing_a_required_signer_capability_or_endpoi");
    //% "Sign-in configuration could not be used. A verified account is required to continue."
    (void)qtTrId("genycaster.runtime.this_build_is_missing_valid_sign_in_with_farcaster_app_confi");
    //% "This interaction does not satisfy the current Farcaster protocol contract."
    (void)qtTrId("genycaster.runtime.this_interaction_does_not_satisfy_the_current_farcaster_prot");
    //% "This interaction needs the current verified Farcaster signer and exact required scope."
    (void)qtTrId("genycaster.runtime.this_interaction_needs_the_current_verified_farcaster_signer");
    //% "This read is not available in the current build."
    (void)qtTrId("genycaster.runtime.this_read_is_not_available_in_the_current_build");
    //% "This runtime has no compatible Snapchain write endpoint. The signed write remains queued locally."
    (void)qtTrId("genycaster.runtime.this_runtime_has_no_compatible_snapchain_write_endpoint_the_");
    //% "This saved page cannot continue on the current route. Refresh from the start."
    (void)qtTrId("genycaster.runtime.this_saved_page_cannot_continue_on_the_current_route_refresh");
    //% "Unfollow"
    (void)qtTrId("genycaster.runtime.unfollow");
    //% "Unlike"
    (void)qtTrId("genycaster.runtime.unlike");
    //% "Up to date"
    (void)qtTrId("genycaster.runtime.up_to_date");
    //% "Waiting for custody approval and network confirmation…"
    (void)qtTrId("genycaster.runtime.waiting_for_custody_approval_and_network_confirmation");
    //% "you"
    (void)qtTrId("genycaster.runtime.you");
    //% "You"
    (void)qtTrId("genycaster.runtime.you_2");
    //% "Back to feed"
    (void)qtTrId("genycaster.section.back_back_to_feed");
    //% "Appearance"
    (void)qtTrId("genycaster.settings.appearance");
    //% "Text size"
    (void)qtTrId("genycaster.settings.text_size");
    //% "Interface size"
    (void)qtTrId("genycaster.settings.interface_size");
    //% "Adjust text and control sizing without changing the window size."
    (void)qtTrId("genycaster.settings.scale_description");
    //% "Choose a mode and color current. Every control keeps the same Geny geometry."
    (void)qtTrId("genycaster.settings.appearance_description");
    //% "Color theme"
    (void)qtTrId("genycaster.settings.color_theme");
    //% "Shape Genycaster around the way you read. Appearance and regional preferences stay on this device."
    (void)qtTrId("genycaster.settings.description");
    //% "Language & region"
    (void)qtTrId("genycaster.settings.language_region");
    //% "Mode"
    (void)qtTrId("genycaster.settings.mode");
    //% "Dark"
    (void)qtTrId("genycaster.settings.mode_dark");
    //% "Light"
    (void)qtTrId("genycaster.settings.mode_light");
    //% "System"
    (void)qtTrId("genycaster.settings.mode_system");
    //% "Open settings"
    (void)qtTrId("genycaster.settings.open");
    //% "Power Save"
    (void)qtTrId("genycaster.settings.power_save");
    //% "Reduce background animation and animated media to save battery."
    (void)qtTrId("genycaster.settings.power_save_description");
    //% "Animated profile pictures"
    (void)qtTrId("genycaster.settings.animate_avatars");
    //% "Keep GIF avatars moving while browsing and scrolling."
    (void)qtTrId("genycaster.settings.animate_avatars_description");
    //% "Reduce motion"
    (void)qtTrId("genycaster.settings.reduce_motion");
    //% "Removes repeated and spatial animation while keeping status changes visible."
    (void)qtTrId("genycaster.settings.reduce_motion_description");
    //% "Reset all settings"
    (void)qtTrId("genycaster.settings.reset_all");
    //% "Ember"
    (void)qtTrId("genycaster.settings.theme_ember");
    //% "Geny"
    (void)qtTrId("genycaster.settings.theme_geny");
    //% "Ocean"
    (void)qtTrId("genycaster.settings.theme_ocean");
    //% "Violet"
    (void)qtTrId("genycaster.settings.theme_violet");
    //% "Settings"
    (void)qtTrId("genycaster.settings.title");
    //% "Approve sign in"
    (void)qtTrId("genycaster.shell.approve_sign_in");
    //% "Approve the request in a Farcaster-compatible client, then return here. Genycaster verifies the signed identity proof before creating a local session."
    (void)qtTrId("genycaster.shell.approve_the_request_in_a_farcaster_compatible_client_t");
    //% "Cancel"
    (void)qtTrId("genycaster.shell.cancel");
    //% "Close"
    (void)qtTrId("genycaster.shell.close");
    //% "In development"
    (void)qtTrId("genycaster.shell.coming_soon");
    //% "Creating a secure Farcaster sign-in request…"
    (void)qtTrId("genycaster.shell.creating_a_secure_farcaster_sign_in_request");
    //% "Enable interactions"
    (void)qtTrId("genycaster.shell.enable_interactions");
    //% "Enable publishing with recovery phrase"
    (void)qtTrId("genycaster.shell.enable_publishing_with_recovery_phrase");
    //% "Enabling…"
    (void)qtTrId("genycaster.shell.enabling");
    //% "Followers"
    (void)qtTrId("genycaster.shell.followers");
    //% "Following"
    (void)qtTrId("genycaster.shell.following");
    //% "New cast"
    (void)qtTrId("genycaster.shell.new_cast");
    //% "Open Farcaster"
    (void)qtTrId("genycaster.shell.open_farcaster");
    //% "Publish"
    (void)qtTrId("genycaster.shell.publish");
    //% "Publishing…"
    (void)qtTrId("genycaster.shell.publishing");
    //% "Reply"
    (void)qtTrId("genycaster.shell.reply");
    //% "Replying…"
    (void)qtTrId("genycaster.shell.replying");
    //% "Revoke signer"
    (void)qtTrId("genycaster.shell.revoke_signer");
    //% "Revoking…"
    (void)qtTrId("genycaster.shell.revoking");
    //% "Saved locally first, then refreshed from the network"
    (void)qtTrId("genycaster.shell.saved_locally_first_then_refreshed_from_the_network");
    //% "Sign in to continue"
    (void)qtTrId("genycaster.shell.sign_in_to_interact");
    //% "Sign in with Farcaster"
    (void)qtTrId("genycaster.shell.sign_in_with_farcaster");
    //% "Could not reach Farcaster Relay. In Network setup, try Direct instead of System proxy, then restart and retry."
    (void)qtTrId("genycaster.setup.farcaster_relay_unreachable");
    //% "Use your Farcaster identity or Genyleap account. Each sign-in is verified independently; neither changes your read network."
    (void)qtTrId("genycaster.setup.account_methods_explainer");
    //% "Product notice summary"
    (void)qtTrId("genycaster.setup.legal_summary_label");
    //% "Hosted / centralized access"
    (void)qtTrId("genycaster.setup.centralized_service_label");
    //% "Decentralized"
    (void)qtTrId("genycaster.setup.decentralized_label");
    //% "Farcaster · Hosted read node"
    (void)qtTrId("genycaster.setup.farcaster_hosted_node");
    //% "Independent Farcaster read adapter configured. Sign-in credentials are separate."
    (void)qtTrId("genycaster.setup.farcaster_configured");
    //% "Accepted"
    (void)qtTrId("genycaster.setup.document_accepted");
    //% "Scroll to the end, then confirm this document."
    (void)qtTrId("genycaster.setup.confirm_document");
    //% "Open each document, scroll through it and confirm. Both confirmations are required to continue."
    (void)qtTrId("genycaster.setup.legal_click_to_review");
    //% "Choose how you connect"
    (void)qtTrId("genycaster.setup.connection_mode_title");
    //% "I reviewed the document"
    (void)qtTrId("genycaster.setup.mark_document_reviewed");
    //% "Open the official document and review the summary to continue."
    (void)qtTrId("genycaster.setup.open_governing_document");
    //% "Farcaster recovery could not be verified. Check your connection and try again."
    (void)qtTrId("genycaster.setup.recovery_failed_retry");
    //% "Genyleap rejected this sign-in. Start a new request or try Farcaster sign-in."
    (void)qtTrId("genycaster.setup.auth_rejected_recover");
    //% "The sign-in service could not complete the verification. Retry or use the other sign-in method."
    (void)qtTrId("genycaster.setup.auth_provider_response_invalid");
    //% "Another sign-in request conflicts with this one. Cancel and start again."
    (void)qtTrId("genycaster.setup.auth_conflict");
    //% "No linked Farcaster identity was found. Connect your existing profile after signing in."
    (void)qtTrId("genycaster.setup.auth_missing_link");
    //% "Secure account setup"
    (void)qtTrId("genycaster.setup.workspace_label");
    //% "Setup steps"
    (void)qtTrId("genycaster.setup.progress_label");
    //% "I agree to the Terms of Use"
    (void)qtTrId("genycaster.setup.explicit_terms_agreement");
    //% "I have read and acknowledge the Privacy Policy"
    (void)qtTrId("genycaster.setup.explicit_privacy_agreement");
    //% "Open and review the Terms before enabling this agreement."
    (void)qtTrId("genycaster.setup.review_terms_first");
    //% "Open and review the Privacy Policy before acknowledging it."
    (void)qtTrId("genycaster.setup.review_privacy_first");
    //% "Choose a connection"
    (void)qtTrId("genycaster.setup.choose_network");
    //% "Your profile"
    (void)qtTrId("genycaster.setup.profile_choice");
    //% "Use existing"
    (void)qtTrId("genycaster.setup.existing_profile");
    //% "Create new"
    (void)qtTrId("genycaster.setup.new_profile");
    //% "Register your new FID in a compatible app, then return here to connect it. Genycaster never asks for your recovery phrase."
    (void)qtTrId("genycaster.setup.profile_creation_browser");
    //% "Find a registration app"
    (void)qtTrId("genycaster.setup.open_registration");
    //% "I have a profile — connect"
    (void)qtTrId("genycaster.setup.already_created");
    //% "Advanced: choose read node"
    (void)qtTrId("genycaster.setup.advanced_nodes");
    //% "Connect the Farcaster identity you already own."
    (void)qtTrId("genycaster.setup.existing_profile_body");
    //% "Register a new Farcaster identity in a compatible app."
    (void)qtTrId("genycaster.setup.new_profile_body");
    //% "Continue in a compatible registration app, then come back to link your profile."
    (void)qtTrId("genycaster.setup.registration_handoff");
    //% "Sign in directly with Farcaster using a verified wallet signature, or use your Genyleap/OpenProof account. Publishing requires separate signer permission."
    (void)qtTrId("genycaster.setup.authentication_explainer");
    //% "Continue with Genyleap"
    (void)qtTrId("genycaster.shell.continue_with_genyleap");
    //% "Open Genyleap"
    (void)qtTrId("genycaster.shell.open_genyleap");
    //% "Connecting securely to Genyleap…"
    (void)qtTrId("genycaster.shell.connecting_securely_to_genyleap");
    //% "Approve Genyleap sign-in in your browser. If you’re already signed in on genyleap.com, you won’t need to sign in again."
    (void)qtTrId("genycaster.shell.approve_genyleap_sign_in_in_your_browser");
    //% "Link your Farcaster account"
    (void)qtTrId("genycaster.shell.link_your_farcaster_account");
    //% "Connect your Farcaster identity to continue. Genycaster requires it for your profile and protocol actions."
    (void)qtTrId("genycaster.shell.genyleap_verified_link_farcaster_to_continue");
    //% "Verified by OpenProof"
    (void)qtTrId("genycaster.shell.verified_by_openproof");
    //% "Signed in with Farcaster · verified by OpenProof"
    (void)qtTrId("genycaster.shell.farcaster_sign_in_openproof_verification");
    //% "You signed in with Farcaster. OpenProof verified the signed proof and linked FID. Publishing requires a separately authorized Farcaster signer."
    (void)qtTrId("genycaster.shell.farcaster_sign_in_openproof_description");
    //% "OpenProof owns your account identity; Farcaster is the required linked protocol identity."
    (void)qtTrId("genycaster.shell.openproof_account_farcaster_protocol_identity");
    //% "Sign out"
    (void)qtTrId("genycaster.shell.sign_out");
    //% "Signed-in account"
    (void)qtTrId("genycaster.shell.signed_in_account");
    //% "The approval link is kept transient and opens through your system browser or Farcaster client. It is not stored in local session data."
    (void)qtTrId("genycaster.shell.the_approval_link_is_kept_transient_and_opens_through_");
    //% "this cast"
    (void)qtTrId("genycaster.shell.this_cast");
    //% "This verified session identifies your account. Protocol writes remain locked until Genycaster has a valid Farcaster signer for this FID."
    (void)qtTrId("genycaster.shell.this_verified_session_identifies_your_account_protocol");
    //% "What’s happening?"
    (void)qtTrId("genycaster.shell.whats_happening");
    //% "Your Farcaster account"
    (void)qtTrId("genycaster.shell.your_farcaster_account");
    //% "Your Genyleap account"
    (void)qtTrId("genycaster.shell.your_genyleap_account");
    //% "Set up Genycaster"
    (void)qtTrId("genycaster.setup.title");
    //% "Sign in to continue. OpenProof verifies your Genyleap identity, and a linked Farcaster account is required for Genycaster."
    (void)qtTrId("genycaster.setup.subtitle");
    //% "Review and accept the Terms of Use and Privacy Policy before signing in."
    (void)qtTrId("genycaster.setup.legal_required_before_sign_in");
    //% "Welcome"
    (void)qtTrId("genycaster.setup.welcome");
    //% "Privacy & Terms"
    (void)qtTrId("genycaster.setup.privacy_terms");
    //% "A private, verified social client"
    (void)qtTrId("genycaster.setup.welcome_eyebrow");
    //% "Welcome to Genycaster"
    (void)qtTrId("genycaster.setup.welcome_title");
    //% "Set up your verified identity, personalize the interface, and choose how Genycaster should look and speak before you enter."
    (void)qtTrId("genycaster.setup.welcome_body");
    //% "Verified identity"
    (void)qtTrId("genycaster.setup.welcome_identity_title");
    //% "Connect through OpenProof and the Farcaster identity required for social actions."
    (void)qtTrId("genycaster.setup.welcome_identity_body");
    //% "Your interface"
    (void)qtTrId("genycaster.setup.welcome_personal_title");
    //% "Choose appearance, color, motion, and regional preferences before the first session."
    (void)qtTrId("genycaster.setup.welcome_personal_body");
    //% "Clear control"
    (void)qtTrId("genycaster.setup.welcome_control_title");
    //% "No guest mode, no hidden setup path, and no interaction before the required checks are complete."
    (void)qtTrId("genycaster.setup.welcome_control_body");
    //% "Setup stays on this device except for the account authentication and protocol actions you explicitly approve."
    (void)qtTrId("genycaster.setup.welcome_note");
    //% "Before you continue"
    (void)qtTrId("genycaster.setup.legal_title");
    //% "Review the Genyleap Terms of Use and Privacy Policy. Scroll through this notice before the consent controls are unlocked."
    (void)qtTrId("genycaster.setup.legal_intro");
    //% "Terms of Use"
    (void)qtTrId("genycaster.setup.terms_title");
    //% "Current official website terms"
    (void)qtTrId("genycaster.setup.terms_updated");
    //% "Genyleap's published Terms of Use cover website information and product resources. Website information is general and product scope may change. Downloads and repositories can carry their own licenses, release notes, and compatibility requirements. Sending a project enquiry does not create a contract or guarantee acceptance. Review the full Terms before continuing."
    (void)qtTrId("genycaster.setup.terms_body");
    //% "Open full Terms of Use"
    (void)qtTrId("genycaster.setup.open_terms");
    //% "Privacy Policy"
    (void)qtTrId("genycaster.setup.privacy_title");
    //% "Current official website privacy notice"
    (void)qtTrId("genycaster.setup.privacy_updated");
    //% "The published Genyleap Privacy notice covers information you choose to submit through contact and project forms and basic abuse-prevention data. Genycaster additionally uses the Genyleap/OpenProof account and linked Farcaster identity you choose during setup to authenticate you and enable protocol actions. Appearance and locale preferences are stored locally in the app unless a feature explicitly says otherwise. Review the full Privacy notice before continuing."
    (void)qtTrId("genycaster.setup.privacy_body");
    //% "Open full Privacy Policy"
    (void)qtTrId("genycaster.setup.open_privacy");
    //% "The links above open Genyleap's current published website terms and privacy notice. Product or repository-specific terms still apply where stated."
    (void)qtTrId("genycaster.setup.legal_governing_note");
    //% "Scroll to the end to unlock consent."
    (void)qtTrId("genycaster.setup.scroll_to_continue");
    //% "You reached the end. Review and confirm both items below."
    (void)qtTrId("genycaster.setup.legal_read_complete");
    //% "I agree to the Genyleap Terms of Use."
    (void)qtTrId("genycaster.setup.accept_terms");
    //% "I have read and understood the Genyleap Privacy Policy."
    (void)qtTrId("genycaster.setup.acknowledge_privacy");
    //% "I have read and agree to the Terms of Use and Privacy Policy."
    (void)qtTrId("genycaster.setup.single_legal_consent");
    //% "Agree & Continue"
    (void)qtTrId("genycaster.setup.agree_continue");
    //% "Back"
    (void)qtTrId("genycaster.setup.back");
    //% "Continue"
    (void)qtTrId("genycaster.setup.continue");
    //% "Finish setup"
    (void)qtTrId("genycaster.setup.finish");
    //% "Restore your Farcaster account"
    (void)qtTrId("genycaster.setup.recovery_title");
    //% "Enter your Farcaster recovery words during setup. Genycaster verifies ownership on Optimism and authorizes a publishing signer."
    (void)qtTrId("genycaster.setup.recovery_description");
    //% "Use only your 24-word Farcaster recovery phrase on a trusted device and HTTPS site. The phrase is never uploaded or stored."
    (void)qtTrId("genycaster.setup.recovery_privacy");
    //% "Continue with recovery words"
    (void)qtTrId("genycaster.setup.recovery_button");
    //% "Verify your account and enable publishing in Setup. No separate Sign In."
    (void)qtTrId("genycaster.setup.recovery_button_body");
    //% "Other sign-in methods"
    (void)qtTrId("genycaster.setup.other_sign_in_methods");
    //% "Preparing your Farcaster account"
    (void)qtTrId("genycaster.setup.recovery_working");
    //% "Validating account ownership and signer registration. Wait for network confirmation."
    (void)qtTrId("genycaster.setup.recovery_working_body");
    //% "Farcaster custody verified on Optimism"
    (void)qtTrId("genycaster.setup.recovery_verified");
    //% "Remembered on this device · provider session not refreshed"
    (void)qtTrId("genycaster.setup.remembered_device_label");
    //% "Undo"
    (void)qtTrId("genycaster.editor.undo");
    //% "Redo"
    (void)qtTrId("genycaster.editor.redo");
    //% "Cut"
    (void)qtTrId("genycaster.editor.cut");
    //% "Paste"
    (void)qtTrId("genycaster.editor.paste");
    //% "Select All"
    (void)qtTrId("genycaster.editor.select_all");
    //% "Viewing only — publishing is locked"
    (void)qtTrId("genycaster.setup.viewer_title");
    //% "Sign in to browse posts and profiles. Posting, likes, follows and reactions remain disabled. You can enable full access later in Account using your Farcaster recovery words."
    (void)qtTrId("genycaster.setup.viewer_description");
    //% "Sign in to continue"
    (void)qtTrId("genycaster.setup.sign_in_required");
    //% "Account verified"
    (void)qtTrId("genycaster.setup.account_verified");
    //% "Your Genyleap/OpenProof identity is verified and the required Farcaster identity is linked."
    (void)qtTrId("genycaster.setup.account_verified_body");
    //% "Verified account ready for Genycaster."
    (void)qtTrId("genycaster.setup.verified_account_ready");
    //% "Consent recorded"
    (void)qtTrId("genycaster.setup.consent_recorded");
    //% "Account"
    (void)qtTrId("genycaster.side_nav.account");
    //% "Bookmarks"
    (void)qtTrId("genycaster.side_nav.bookmarks");
    //% "Business"
    (void)qtTrId("genycaster.side_nav.business");
    //% "Explorer"
    (void)qtTrId("genycaster.side_nav.explorer");
    //% "Genycaster"
    (void)qtTrId("genycaster.side_nav.genycaster");
    //% "Home"
    (void)qtTrId("genycaster.side_nav.home");
    //% "Messages"
    (void)qtTrId("genycaster.side_nav.messages");
    //% "No identity open"
    (void)qtTrId("genycaster.side_nav.no_identity_open");
    //% "Notifications"
    (void)qtTrId("genycaster.side_nav.notifications");
    //% "Reading remains available without signing in"
    (void)qtTrId("genycaster.side_nav.reading_remains_available_without_signing_in");
    //% "Store"
    (void)qtTrId("genycaster.side_nav.store");
    //% "Viewing"
    (void)qtTrId("genycaster.side_nav.viewing");
    //% "Data issue"
    (void)qtTrId("genycaster.status.data_issue");
    //% "Details"
    (void)qtTrId("genycaster.status.details");
    //% "Disconnected"
    (void)qtTrId("genycaster.status.disconnected");
    //% "Hide details"
    (void)qtTrId("genycaster.status.hide_details");
    //% "Live"
    (void)qtTrId("genycaster.status.live");
    //% "Live source: responded"
    (void)qtTrId("genycaster.status.live_source_responded");
    //% "Live source: unavailable"
    (void)qtTrId("genycaster.status.live_source_unavailable");
    //% "Network: %1"
    (void)qtTrId("genycaster.status.network_value");
    //% "No network connection · showing saved data when available"
    (void)qtTrId("genycaster.status.no_network_connection_showing_saved_data_when_availabl");
    //% "Offline"
    (void)qtTrId("genycaster.status.offline");
    //% "Reachable"
    (void)qtTrId("genycaster.status.reachable");
    //% "Read runtime: unavailable"
    (void)qtTrId("genycaster.status.read_runtime_unavailable");
    //% "Ready"
    (void)qtTrId("genycaster.status.ready");
    //% "Retry"
    (void)qtTrId("genycaster.status.retry");
    //% "Saved"
    (void)qtTrId("genycaster.status.saved");
    //% "Setup issue"
    (void)qtTrId("genycaster.status.setup_issue");
    //% "Showing saved data · live refresh will retry automatically"
    (void)qtTrId("genycaster.status.showing_saved_data_live_refresh_will_retry_automatical");
    //% "Source issue"
    (void)qtTrId("genycaster.status.source_issue");
    //% "Syncing"
    (void)qtTrId("genycaster.status.syncing");
    //% "Unknown"
    (void)qtTrId("genycaster.status.unknown");
    //% "Farcaster cast output is limited to 1024 UTF-8 bytes."
    (void)qtTrId("genycaster.bridge.farcaster_casts_are_limited_to_1024_utf_8_bytes");
    //% "The semantic mention layout is invalid."
    (void)qtTrId("genycaster.bridge.invalid_semantic_mentions");
    (void)qtTrId("genycaster.bridge.too_many_cast_embeds");
    (void)qtTrId("genycaster.bridge.invalid_cast_embeds");
    //% "Auto Detect"
    (void)qtTrId("genycaster.composer.auto_detect");
    //% "Autosaved"
    (void)qtTrId("genycaster.composer.autosaved");
    //% "Code block"
    (void)qtTrId("genycaster.composer.code_block");
    //% "%1 formatting element(s) will be simplified in other Farcaster clients."
    (void)qtTrId("genycaster.composer.compatibility_simplified_count");
    //% "Copy code"
    (void)qtTrId("genycaster.composer.copy_code");
    //% "Published output is clean, protocol-safe text in every Farcaster client."
    (void)qtTrId("genycaster.composer.cross_client_safe");
    //% "Draft restored"
    (void)qtTrId("genycaster.composer.draft_restored");
    //% "Editor %1"
    (void)qtTrId("genycaster.composer.editor_count");
    //% "Embed"
    (void)qtTrId("genycaster.composer.embed");
    //% "Farcaster Preview"
    (void)qtTrId("genycaster.composer.farcaster_preview");
    //% "Heading"
    (void)qtTrId("genycaster.composer.heading");
    (void)qtTrId("genycaster.composer.ordered_list");
    (void)qtTrId("genycaster.composer.separator");
    (void)qtTrId("genycaster.composer.list_placeholder");
    (void)qtTrId("genycaster.composer.ordered_list_placeholder");
    (void)qtTrId("genycaster.composer.heading_placeholder");
    //% "Inline code"
    (void)qtTrId("genycaster.composer.inline_code");
    //% "Line numbers"
    (void)qtTrId("genycaster.composer.line_numbers");
    //% "This will publish as a Farcaster long cast."
    (void)qtTrId("genycaster.composer.formatting_simplified_count");
    (void)qtTrId("genycaster.composer.long_cast_notice");
    //% "Write or paste Markdown. It is parsed into the same rich document used by Visual mode."
    (void)qtTrId("genycaster.composer.markdown_placeholder");
    //% "Media"
    (void)qtTrId("genycaster.composer.media");
    //% "Choose photo or video"
    (void)qtTrId("genycaster.media.choose_file");
    //% "Select an image or video from your device. It uploads before the cast is published."
    (void)qtTrId("genycaster.media.choose_file_hint");
    //% "Media upload is unavailable until your Farcaster signer is ready."
    (void)qtTrId("genycaster.media.upload_unavailable");
    //% "Waiting to upload…"
    (void)qtTrId("genycaster.media.queued");
    //% "Preparing media…"
    (void)qtTrId("genycaster.media.preparing");
    //% "Authorizing upload…"
    (void)qtTrId("genycaster.media.authorizing");
    //% "Uploading…"
    (void)qtTrId("genycaster.media.uploading");
    //% "Verifying upload…"
    (void)qtTrId("genycaster.media.verifying");
    //% "Cancelling upload…"
    (void)qtTrId("genycaster.media.cancelling");
    //% "Upload cancelled"
    (void)qtTrId("genycaster.media.cancelled");
    //% "Upload complete"
    (void)qtTrId("genycaster.media.completed");
    //% "Media upload failed."
    (void)qtTrId("genycaster.media.upload_failed");
    //% "Cancel upload"
    (void)qtTrId("genycaster.media.cancel");
    //% "Retry upload"
    (void)qtTrId("genycaster.media.retry");
    //% "Images and videos"
    (void)qtTrId("genycaster.media.images_and_videos");
    //% "Images"
    (void)qtTrId("genycaster.media.images");
    //% "Videos"
    (void)qtTrId("genycaster.media.videos");
    //% "The uploaded media could not be inserted into this cast."
    (void)qtTrId("genycaster.media.insert_failed");
    //% "Unsupported file type. Choose JPG, PNG, WebP, GIF, AVIF, MP4, WebM, or MOV."
    (void)qtTrId("genycaster.media.unsupported_file_type");
    //% "The selected file is not readable."
    (void)qtTrId("genycaster.media.file_unreadable");
    //% "Buffering"
    (void)qtTrId("genycaster.player.buffering");
    //% "Exit full screen"
    (void)qtTrId("genycaster.player.exit_fullscreen");
    //% "Full screen"
    (void)qtTrId("genycaster.player.fullscreen");
    //% "Mute"
    (void)qtTrId("genycaster.player.mute");
    //% "Open original"
    (void)qtTrId("genycaster.player.open_original");
    //% "Video quality"
    (void)qtTrId("genycaster.player.quality");
    //% "Auto"
    (void)qtTrId("genycaster.player.quality_auto");
    //% "Original"
    (void)qtTrId("genycaster.player.quality_original");
    //% "Pause"
    (void)qtTrId("genycaster.player.pause");
    //% "Paused"
    (void)qtTrId("genycaster.player.paused");
    //% "Play"
    (void)qtTrId("genycaster.player.play");
    //% "Playing"
    (void)qtTrId("genycaster.player.playing");
    //% "Replay"
    (void)qtTrId("genycaster.player.replay");
    //% "Resume"
    (void)qtTrId("genycaster.player.resume");
    //% "Retry"
    (void)qtTrId("genycaster.player.retry");
    //% "Seek"
    (void)qtTrId("genycaster.player.seek");
    //% "Unmute"
    (void)qtTrId("genycaster.player.unmute");
    //% "Video unavailable"
    (void)qtTrId("genycaster.player.video_unavailable");
    //% "Video controls"
    (void)qtTrId("genycaster.player.video_controls");
    //% "Volume"
    (void)qtTrId("genycaster.player.volume");
    //% "Link"
    (void)qtTrId("genycaster.composer.link");
    //% "Mention"
    (void)qtTrId("genycaster.composer.mention");
    //% "URL"
    (void)qtTrId("genycaster.composer.url");
    //% "Username"
    (void)qtTrId("genycaster.composer.username");
    //% "Farcaster FID"
    (void)qtTrId("genycaster.composer.fid");
    //% "Optional title"
    (void)qtTrId("genycaster.composer.title_optional");
    //% "Apply"
    (void)qtTrId("genycaster.composer.apply");
    //% "Published %1 / %2 bytes"
    (void)qtTrId("genycaster.composer.published_count");
    //% "Rich metadata saved"
    (void)qtTrId("genycaster.composer.published_metadata_saved");
    //% "Quote"
    (void)qtTrId("genycaster.composer.quote_placeholder");
    //% "Clear block content"
    (void)qtTrId("genycaster.composer.clear_block");
    //% "Remove block"
    (void)qtTrId("genycaster.composer.remove_block");
    //% "Structured rich document · safe Farcaster output"
    (void)qtTrId("genycaster.composer.rich_engine_subtitle");
    //% "Saved"
    (void)qtTrId("genycaster.composer.saved");
    //% "Unsaved changes"
    (void)qtTrId("genycaster.composer.unsaved");
    //% "Visual"
    (void)qtTrId("genycaster.composer.visual");
    //% "Trust Score"
    (void)qtTrId("genycaster.profile.trust_score");
    //% "Wrap"
    (void)qtTrId("genycaster.composer.wrap");
    //% "Rendering quality"
    (void)qtTrId("genycaster.rendering.title");
    //% "Experimental"
    (void)qtTrId("genycaster.rendering.experimental");
    //% "Rendering profiles are experimental while we qualify quality and energy behavior across GPUs."
    (void)qtTrId("genycaster.rendering.experimental_description");
    //% "Choose how Genycaster balances battery, frame pacing, and visual fidelity."
    (void)qtTrId("genycaster.rendering.subtitle");
    //% "Efficiency"
    (void)qtTrId("genycaster.rendering.efficiency");
    //% "Lowest GPU and battery cost with lightweight effects and geometry anti-aliasing."
    (void)qtTrId("genycaster.rendering.efficiency_description");
    //% "Adaptive"
    (void)qtTrId("genycaster.rendering.adaptive");
    //% "Recommended. Adjusts effect quality from observed frame pacing while keeping the interface stable."
    (void)qtTrId("genycaster.rendering.adaptive_description");
    //% "Fidelity"
    (void)qtTrId("genycaster.rendering.fidelity");
    //% "Maximum visual quality, 4× MSAA, full effects, and higher media decode quality."
    (void)qtTrId("genycaster.rendering.fidelity_description");
    //% "Graphics backend"
    (void)qtTrId("genycaster.rendering.backend");
    //% "Frame interval"
    (void)qtTrId("genycaster.rendering.frame_interval");
    //% "Display"
    (void)qtTrId("genycaster.rendering.display");
    //% "MSAA"
    (void)qtTrId("genycaster.rendering.msaa");
    //% "Off"
    (void)qtTrId("genycaster.rendering.off");
    //% "Restart Genycaster to apply the selected MSAA level. Live effect and media quality changes are already active."
    (void)qtTrId("genycaster.rendering.restart_required");
    //% "Target frame rate"
    (void)qtTrId("genycaster.rendering.frame_target");
    //% "Send feedback"
    (void)qtTrId("genycaster.feedback.title");
    //% "Tell us what feels broken, slow, confusing, or worth improving."
    (void)qtTrId("genycaster.feedback.subtitle");
    //% "Bug"
    (void)qtTrId("genycaster.feedback.bug");
    //% "Performance"
    (void)qtTrId("genycaster.feedback.performance");
    //% "Improve"
    (void)qtTrId("genycaster.feedback.improvement");
    //% "Idea"
    (void)qtTrId("genycaster.feedback.idea");
    //% "What happened, what did you expect, and what would make this better?"
    (void)qtTrId("genycaster.feedback.placeholder");
    //% "Feedback transport is not connected yet. This interface is ready; server delivery will be wired in a later build."
    (void)qtTrId("genycaster.feedback.backend_pending");
    //% "Copy"
    (void)qtTrId("genycaster.feedback.copy");
    //% "Done"
    (void)qtTrId("genycaster.feedback.done");
    //% "Feedback statistics"
    (void)qtTrId("genycaster.feedback.stats");
    //% "Reports"
    (void)qtTrId("genycaster.feedback.stats_total");
    //% "Accepted"
    (void)qtTrId("genycaster.feedback.stats_accepted");
    //% "Resolved"
    (void)qtTrId("genycaster.feedback.stats_resolved");
    //% "%1 points earned"
    (void)qtTrId("genycaster.feedback.points");
    //% "Recent reports"
    (void)qtTrId("genycaster.feedback.recent_reports");
    //% "Sign in to send feedback and view your reports."
    (void)qtTrId("genycaster.feedback.authentication_required");
    //% "Daily report limit reached. Try again later."
    (void)qtTrId("genycaster.feedback.limit_reached");
    //% "Please describe the issue in at least 10 characters."
    (void)qtTrId("genycaster.feedback.invalid");
    //% "Feedback couldn't be sent. Please try again."
    (void)qtTrId("genycaster.feedback.unavailable");
    //% "Submitted"
    (void)qtTrId("genycaster.feedback.status_submitted");
    //% "Under review"
    (void)qtTrId("genycaster.feedback.status_triaged");
    //% "Accepted"
    (void)qtTrId("genycaster.feedback.status_accepted");
    //% "Resolved"
    (void)qtTrId("genycaster.feedback.status_resolved");
    //% "Not accepted"
    (void)qtTrId("genycaster.feedback.status_rejected");
    //% "Thanks! Your feedback has been sent."
    (void)qtTrId("genycaster.feedback.sent");
    //% "Send report"
    (void)qtTrId("genycaster.feedback.submit");
    //% "Emoji"
    (void)qtTrId("genycaster.composer.emoji");
    //% "Classic"
    (void)qtTrId("genycaster.emoji.classic");
    //% "Animated"
    (void)qtTrId("genycaster.emoji.animated");
    //% "More"
    (void)qtTrId("genycaster.emoji.more");
    //% "Add an emoji to the current cast."
    (void)qtTrId("genycaster.composer.emoji_subtitle");
    //% "New thread"
    (void)qtTrId("genycaster.composer.thread_title");
    //% "Publish thread"
    (void)qtTrId("genycaster.composer.publish_thread");
    //% "Publishing thread…"
    (void)qtTrId("genycaster.composer.publishing_thread");
    //% "Add cast"
    (void)qtTrId("genycaster.composer.add_cast");
    //% "Remove this cast"
    (void)qtTrId("genycaster.composer.remove_thread_cast");
    //% "Continue the thread · Cast %1"
    (void)qtTrId("genycaster.composer.thread_placeholder");
    //% "Add another cast to turn this into a Farcaster thread."
    (void)qtTrId("genycaster.composer.thread_hint");
    //% "Each cast is a normal Farcaster CastAdd linked to the previous cast as its parent."
    (void)qtTrId("genycaster.composer.thread_protocol_hint");
    //% "Preparing a canonical Farcaster thread…"
    (void)qtTrId("genycaster.bridge.preparing_a_canonical_farcaster_thread");
    //% "A thread must contain between 2 and 12 casts."
    (void)qtTrId("genycaster.bridge.thread_cast_count_invalid");
    //% "Every cast in a thread must contain text or an embed."
    (void)qtTrId("genycaster.bridge.thread_cast_empty");
    //% "The Farcaster thread could not be scheduled."
    (void)qtTrId("genycaster.runtime.thread_could_not_be_scheduled");
    //% "My Assets"
    (void)qtTrId("genycaster.context.my_assets");
    //% "GENY utility · Base"
    (void)qtTrId("genycaster.context.geny_utility");
    //% "Checking your GENY balance…"
    (void)qtTrId("genycaster.context.checking_geny");
    //% "No verified Farcaster EVM addresses were found"
    (void)qtTrId("genycaster.context.wallet_needed");
    //% "You are not a GENY holder yet"
    (void)qtTrId("genycaster.context.not_geny_holder");
    //% "GENY balance is temporarily unavailable"
    (void)qtTrId("genycaster.context.geny_balance_unavailable");
    //% "GENY is more than a token — it powers identity, access, and status across Genycaster."
    (void)qtTrId("genycaster.context.geny_membership_hint");
    //% "GENY is read across your verified Farcaster EVM addresses on Base. No signing or transaction is required."
    (void)qtTrId("genycaster.context.geny_asset_explainer");
    //% "Market cap"
    (void)qtTrId("genycaster.context.market_cap");
    //% "24h volume"
    (void)qtTrId("genycaster.context.volume_24h");
    //% "Liquidity"
    (void)qtTrId("genycaster.context.liquidity");
    //% "Move asset up"
    (void)qtTrId("genycaster.settings.move_asset_up");
    //% "Move asset down"
    (void)qtTrId("genycaster.settings.move_asset_down");
    //% "Copy link"
    (void)qtTrId("genycaster.cast.copy_link");

    //% "Buyers 24h"
    (void)qtTrId("genycaster.context.buyers_24h");
    //% "Fully diluted value"
    (void)qtTrId("genycaster.context.fdv");
    //% "GENY holder"
    (void)qtTrId("genycaster.context.geny_holder");
    //% "Sellers 24h"
    (void)qtTrId("genycaster.context.sellers_24h");
    //% "NFTs"
    (void)qtTrId("genycaster.home.visual_filter_nfts");
    //% "All engagement"
    (void)qtTrId("genycaster.profile.sort_all");
    //% "Most liked"
    (void)qtTrId("genycaster.profile.sort_likes");
    //% "Newest"
    (void)qtTrId("genycaster.profile.sort_newest");
    //% "Oldest"
    (void)qtTrId("genycaster.profile.sort_oldest");
    //% "Most recast"
    (void)qtTrId("genycaster.profile.sort_recasts");
    //% "Most replies"
    (void)qtTrId("genycaster.profile.sort_replies");
    //% "Cast"
    (void)qtTrId("genycaster.search.cast");
    //% "Channel"
    (void)qtTrId("genycaster.search.channel");
    //% "Profile"
    (void)qtTrId("genycaster.search.profile");
    //% "Search shortcuts"
    (void)qtTrId("genycaster.search.shortcuts");
    //% "Custom accent"
    (void)qtTrId("genycaster.settings.custom_color");
    //% "Pick a preset or enter any hex color."
    (void)qtTrId("genycaster.settings.custom_color_description");
    //% "Enter a valid color such as #FF4FA3."
    (void)qtTrId("genycaster.settings.custom_color_invalid");
    //% "#FF4FA3"
    (void)qtTrId("genycaster.settings.custom_color_placeholder");
    //% "Mint"
    (void)qtTrId("genycaster.settings.theme_mint");
    //% "Monochrome"
    (void)qtTrId("genycaster.settings.theme_mono");
    //% "Rose"
    (void)qtTrId("genycaster.settings.theme_rose");    //% "Notifications"
    (void)qtTrId("genycaster.notifications.title");
    //% "Replies, mentions, reactions, and new followers from Farcaster."
    (void)qtTrId("genycaster.notifications.subtitle");
    //% "Someone"
    (void)qtTrId("genycaster.notifications.someone");
    //% "%1 replied to you"
    (void)qtTrId("genycaster.notifications.replied");
    //% "%1 mentioned you"
    (void)qtTrId("genycaster.notifications.mentioned");
    //% "%1 followed you"
    (void)qtTrId("genycaster.notifications.followed");
    //% "%1 reacted to your cast"
    (void)qtTrId("genycaster.notifications.reacted");
    //% "%1 and %2 people reacted to your cast"
    (void)qtTrId("genycaster.notifications.reacted_many");
    //% "%1 recast your cast"
    (void)qtTrId("genycaster.notifications.recasted");
    //% "%1 and %2 people recast your cast"
    (void)qtTrId("genycaster.notifications.recasted_many");
    //% "New Farcaster activity"
    (void)qtTrId("genycaster.notifications.activity");
    //% "In reply to your cast"
    (void)qtTrId("genycaster.notifications.in_reply_to_your_cast");
    //% "Sign in to load your Farcaster notifications."
    (void)qtTrId("genycaster.notifications.sign_in_required");
    //% "Your notifications are account-specific"
    (void)qtTrId("genycaster.notifications.sign_in_title");
    //% "Sign in to see replies, mentions, reactions, and followers."
    (void)qtTrId("genycaster.notifications.sign_in_description");
    //% "Sign in"
    (void)qtTrId("genycaster.notifications.sign_in");
    //% "Notifications could not refresh"
    (void)qtTrId("genycaster.notifications.could_not_refresh");
    //% "You are all caught up"
    (void)qtTrId("genycaster.notifications.empty_title");
    //% "New replies, mentions, reactions, and followers will appear here."
    (void)qtTrId("genycaster.notifications.empty_description");

}

} // namespace
