import { createServer } from 'node:http';
import { createClient, HubError, InvalidTokenError } from '@farcasterorg/quick-auth';
import { createHandler } from './handler.mjs';

const domain = process.env.GENYCASTER_QUICK_AUTH_DOMAIN;
const internalSecret = process.env.GENYCASTER_QUICK_AUTH_INTERNAL_SECRET;
if (!domain || !internalSecret || internalSecret.length < 32) {
  throw new Error('Set verification domain and a random internal secret (at least 32 characters).');
}
const port = Number(process.env.GENYCASTER_QUICK_AUTH_PORT ?? '8789');
if (!Number.isInteger(port) || port < 1 || port > 65535) {
  throw new Error('Invalid port');
}
const hubUrl = process.env.GENYCASTER_QUICK_AUTH_HUB_URL;
const client = hubUrl ? createClient({ hubUrl }) : createClient();
const handler = createHandler({
  verifyToken: (args) => client.verifyToken(args),
  domain,
  internalSecret,
  hubErrorClass: HubError,
  invalidTokenClass: InvalidTokenError,
});
const server = createServer(handler);
// Auth sidecar is intentionally not exposed publicly. Use a trusted local reverse-proxy/app adapter.
server.listen(port, '127.0.0.1');
