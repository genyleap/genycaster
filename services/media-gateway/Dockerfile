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
COPY . ./

RUN cmake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build/native --parallel --target genyleap_quilibrium_storage \
    && mkdir -p storage/native \
    && cp build/native/genyleap_quilibrium_storage.node storage/native/ \
    && node --test test/gateway.test.mjs test/browser_image_proxy.test.mjs test/browser_farcaster_auth.test.mjs test/quilibrium_sdk_adapter.test.mjs \
    && node --test test/native_addon.test.mjs

FROM node:22-trixie-slim AS runtime

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        libcurl4t64 \
        libssl3t64 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /app/package.json ./package.json
COPY --from=builder /app/gateway.mjs ./gateway.mjs
COPY --from=builder /app/browser_image_proxy.mjs ./browser_image_proxy.mjs
COPY --from=builder /app/browser_farcaster_auth.mjs ./browser_farcaster_auth.mjs
COPY --from=builder /app/server.mjs ./server.mjs
COPY --from=builder /app/storage ./storage

# Build context files may be copied from a restrictive deployment umask.
# The unprivileged runtime must be able to import the browser image module.
RUN chmod 0644 /app/browser_image_proxy.mjs /app/browser_farcaster_auth.mjs /app/gateway.mjs /app/server.mjs

USER node
EXPOSE 8790
CMD ["node", "server.mjs"]
