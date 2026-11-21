import {createHealthServer} from './server.mjs';

const port = Number(process.env.GENYCASTER_HEALTH_PORT || 8798);
if (!Number.isInteger(port) || port < 1 || port > 65535)
  throw new Error('Invalid GENYCASTER_HEALTH_PORT');
createHealthServer().listen(port, '127.0.0.1');
