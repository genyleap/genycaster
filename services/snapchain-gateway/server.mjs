import { createGateway, loadConfig } from './gateway.mjs';

let config;
try {
  config = loadConfig();
} catch (error) {
  console.error(`Genyleap Snapchain Gateway configuration error: ${error.message}`);
  process.exit(2);
}

const server = createGateway(config);
server.listen(config.port, config.host, () => {
  console.log(`Genyleap Snapchain Gateway listening on ${config.host}:${config.port}`);
});

function shutdown(signal) {
  console.log(`Genyleap Snapchain Gateway received ${signal}; shutting down.`);
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
