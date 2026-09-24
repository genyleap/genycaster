import { createMediaGateway, loadConfig } from './gateway.mjs';
import { createStorageAdapter } from './storage/index.mjs';

let config;
let storage;
try {
  config = loadConfig();
  storage = createStorageAdapter();
} catch (error) {
  console.error(`Genyleap Media Gateway configuration error: ${error.message}`);
  process.exit(2);
}

const server = createMediaGateway(config, storage);
server.listen(config.port, config.host, () => {
  console.log(`Genyleap Media Gateway listening on ${config.host}:${config.port}.`);
});

function shutdown(signal) {
  console.log(`Genyleap Media Gateway received ${signal}; shutting down.`);
  server.close((error) => {
    if (error) {
      console.error(error);
      process.exit(1);
    }
    process.exit(0);
  });
}

process.on('SIGTERM', () => shutdown('SIGTERM'));
process.on('SIGINT', () => shutdown('SIGINT'));
