FROM node:22-alpine

WORKDIR /app
COPY --chown=node:node package.json gateway.mjs server.mjs ./

USER node
ENV NODE_ENV=production \
    GENYLEAP_SNAPCHAIN_HOST=0.0.0.0 \
    GENYLEAP_SNAPCHAIN_PORT=8787
EXPOSE 8787

CMD ["node", "server.mjs"]
