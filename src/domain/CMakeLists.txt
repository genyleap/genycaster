add_library(farcaster_domain)
add_library(farcaster::domain ALIAS farcaster_domain)

target_sources(farcaster_domain
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            identity.cppm
            cast.cppm
            profile.cppm
            social.cppm
            channel.cppm
            feed.cppm
            session.cppm
            signer.cppm
            write.cppm
            valuation.cppm
            market.cppm
            rich_document.cppm
            media.cppm
    PRIVATE
        session.cpp
        signer.cpp
        rich_document.cpp
)

target_link_libraries(farcaster_domain PUBLIC farcaster::foundation)
target_compile_features(farcaster_domain PUBLIC cxx_std_26)
set_property(TARGET farcaster_domain PROPERTY CXX_SCAN_FOR_MODULES ON)
farcaster_enable_warnings(farcaster_domain)
