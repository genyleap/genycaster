FROM node:22-trixie-slim AS builder

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        cmake \
        g++ \
        git \
        libcurl4-openssl-dev \
        libssl-dev \
        ninja-build \
        pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
# Build context is services/, not media-gateway/. The gateway intentionally
# shares one server-only deployment policy with sibling services; copying
# it to the parent of /app preserves existing ../deployment_config.mjs imports.
COPY media-gateway/ ./
COPY deployment_config.mjs /deployment_config.mjs

# Reproducible, pinned libvips-based image processing for Smart Media Pipeline.
# The native binding is isolated in the gateway; no client needs Sharp.
RUN npm ci --omit=dev --no-audit --no-fund

RUN cmake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build/native --parallel --target genyleap_quilibrium_storage \
    && mkdir -p storage/native \
    && cp build/native/genyleap_quilibrium_storage.node storage/native/ \
    && node --test test/smart_media_pipeline.test.mjs test/gateway.test.mjs test/browser_image_proxy.test.mjs test/browser_farcaster_auth.test.mjs test/quilibrium_sdk_adapter.test.mjs test/browser_token_insights.test.mjs test/browser_miniapp_preview.test.mjs test/browser_link_preview.test.mjs test/browser_cast_views.test.mjs test/browser_cast_reactions.test.mjs test/channel_directory_gateway.test.mjs \
    && node --test test/native_addon.test.mjs

FROM node:22-trixie-slim AS runtime

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        libcurl4t64 \
        libssl3t64 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /deployment_config.mjs /deployment_config.mjs
COPY --from=builder /app/package.json ./package.json
COPY --from=builder /app/node_modules ./node_modules
COPY --from=builder /app/gateway.mjs ./gateway.mjs
COPY --from=builder /app/media_entitlements.mjs ./media_entitlements.mjs
COPY --from=builder /app/smart_media_pipeline ./smart_media_pipeline
COPY --from=builder /app/browser_image_proxy.mjs ./browser_image_proxy.mjs
COPY --from=builder /app/browser_farcaster_auth.mjs ./browser_farcaster_auth.mjs
COPY --from=builder /app/browser_token_insights.mjs ./browser_token_insights.mjs
COPY --from=builder /app/browser_miniapp_preview.mjs ./browser_miniapp_preview.mjs
COPY --from=builder /app/browser_link_preview.mjs ./browser_link_preview.mjs
COPY --from=builder /app/browser_cast_views.mjs ./browser_cast_views.mjs
COPY --from=builder /app/browser_cast_reactions.mjs ./browser_cast_reactions.mjs
COPY --from=builder /app/server.mjs ./server.mjs
COPY --from=builder /app/storage ./storage

# Build context files may be copied from a restrictive deployment umask.
# The unprivileged runtime must be able to import the browser image module.
RUN chmod 0644 /deployment_config.mjs \
    && chmod 0755 /app/storage/native \
    && find /app/smart_media_pipeline -type f -name "*.mjs" -exec chmod 0644 {} + \
    && chmod 0644 /app/media_entitlements.mjs \
    && chmod 0644 /app/browser_image_proxy.mjs /app/browser_farcaster_auth.mjs /app/browser_token_insights.mjs /app/browser_miniapp_preview.mjs /app/browser_link_preview.mjs /app/browser_cast_views.mjs /app/browser_cast_reactions.mjs /app/gateway.mjs /app/server.mjs

USER node
EXPOSE 8790
CMD ["node", "server.mjs"]
