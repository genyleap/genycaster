import { createHash, createHmac } from 'node:crypto';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

function required(value, name) {
  const text = String(value ?? '').trim();
  if (!text) throw new Error(`${name} is required.`);
  return text;
}

function normalizeEndpoint(value) {
  const endpoint = new URL(required(value, 'storage endpoint'));
  if (endpoint.protocol !== 'https:' && endpoint.protocol !== 'http:') {
    throw new Error('storage endpoint must use http or https.');
  }
  if (endpoint.username || endpoint.password || endpoint.search || endpoint.hash) {
    throw new Error('storage endpoint must be a credential-free root URL.');
  }
  endpoint.pathname = endpoint.pathname.replace(/\/+$/, '');
  return endpoint.toString().replace(/\/$/, '');
}

function positiveExpiry(value, fallback, name) {
  const parsed = Number(String(value ?? fallback));
  if (!Number.isSafeInteger(parsed) || parsed < 1 || parsed > 604800) {
    throw new Error(`${name} must be between 1 and 604800 seconds.`);
  }
  return parsed;
}

function loadNativeAddon(env = process.env) {
  const defaultPath = fileURLToPath(new URL('./native/genyleap_quilibrium_storage.node', import.meta.url));
  const configuredPath = String(env.GENYLEAP_MEDIA_QUILIBRIUM_ADDON || '').trim();
  const addonPath = configuredPath ? path.resolve(configuredPath) : defaultPath;
  const require = createRequire(import.meta.url);
  try {
    return require(addonPath);
  } catch (error) {
    throw new Error(`Unable to load Quilibrium SDK native addon at ${addonPath}: ${error.message}`);
  }
}


function awsPercentEncode(value) {
  return encodeURIComponent(String(value))
    .replace(/[!'()*]/g, (character) =>
      `%${character.charCodeAt(0).toString(16).toUpperCase()}`,
    );
}

function sha256Hex(value) {
  return createHash('sha256').update(value).digest('hex');
}

function hmacSha256(key, value) {
  return createHmac('sha256', key).update(value).digest();
}

function compatibleReadPresign(config, key, method, expiresSeconds, now = new Date()) {
  const endpoint = new URL(config.endpoint);
  const normalizedMethod = String(method || '').toUpperCase();
  if (normalizedMethod !== 'GET' && normalizedMethod !== 'HEAD') {
    throw new Error('QStorage compatible read presigning requires GET or HEAD.');
  }

  const endpointPath = endpoint.pathname.replace(/\/+$/, '');
  const objectPath = [config.bucket, ...String(key).split('/')]
    .map(awsPercentEncode)
    .join('/');
  const canonicalUri = `${endpointPath}/${objectPath}`.replace(/\/{2,}/g, '/');

  const amzDate = now.toISOString().replace(/[:-]|\.\d{3}/g, '');
  const dateStamp = amzDate.slice(0, 8);
  const service = 's3';
  const scope = `${dateStamp}/${config.region}/${service}/aws4_request`;

  const parameters = [
    ['X-Amz-Algorithm', 'AWS4-HMAC-SHA256'],
    ['X-Amz-Credential', `${config.accessKeyId}/${scope}`],
    ['X-Amz-Date', amzDate],
    ['X-Amz-Expires', String(expiresSeconds)],
    ['X-Amz-SignedHeaders', 'host'],
  ];
  if (config.sessionToken) {
    parameters.push(['X-Amz-Security-Token', config.sessionToken]);
  }

  const canonicalQuery = parameters
    .map(([name, value]) => [awsPercentEncode(name), awsPercentEncode(value)])
    .sort(([leftName, leftValue], [rightName, rightValue]) =>
      leftName === rightName
        ? leftValue.localeCompare(rightValue)
        : leftName.localeCompare(rightName),
    )
    .map(([name, value]) => `${name}=${value}`)
    .join('&');

  const payloadHash = sha256Hex(Buffer.alloc(0));
  const canonicalHeaders = `host:${endpoint.host}\n`;
  const canonicalRequest = [
    normalizedMethod,
    canonicalUri,
    canonicalQuery,
    canonicalHeaders,
    'host',
    payloadHash,
  ].join('\n');

  const stringToSign = [
    'AWS4-HMAC-SHA256',
    amzDate,
    scope,
    sha256Hex(canonicalRequest),
  ].join('\n');

  const dateKey = hmacSha256(Buffer.from(`AWS4${config.secretAccessKey}`), dateStamp);
  const regionKey = hmacSha256(dateKey, config.region);
  const serviceKey = hmacSha256(regionKey, service);
  const signingKey = hmacSha256(serviceKey, 'aws4_request');
  const signature = createHmac('sha256', signingKey).update(stringToSign).digest('hex');

  return `${endpoint.origin}${canonicalUri}?${canonicalQuery}&X-Amz-Signature=${signature}`;
}

function xmlDecode(value) {
  return value
    .replaceAll('&lt;', '<')
    .replaceAll('&gt;', '>')
    .replaceAll('&quot;', '"')
    .replaceAll('&apos;', "'")
    .replaceAll('&amp;', '&');
}

function xmlEscape(value) {
  return String(value)
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;');
}

function xmlElement(document, name) {
  const expression = new RegExp(`<${name}(?:\\s[^>]*)?>([\\s\\S]*?)<\\/${name}>`, 'i');
  const match = expression.exec(document);
  return match ? xmlDecode(match[1].trim()) : '';
}

function providerError(operation, status, body = '') {
  const providerCode = xmlElement(body, 'Code');
  const suffix = providerCode ? ` (${providerCode})` : '';
  const error = new Error(`QStorage ${operation} failed with HTTP ${status}${suffix}`);
  error.statusCode = status >= 500 ? 503 : 502;
  return error;
}

async function boundedText(response, maxBytes = 64 * 1024) {
  const text = await response.text();
  return text.length > maxBytes ? text.slice(0, maxBytes) : text;
}

export function loadQuilibriumStorageConfig(env = process.env) {
  return Object.freeze({
    endpoint: normalizeEndpoint(env.GENYLEAP_MEDIA_STORAGE_ENDPOINT || 'https://qstorage.quilibrium.com'),
    region: required(env.GENYLEAP_MEDIA_STORAGE_REGION || 'q-world-1', 'GENYLEAP_MEDIA_STORAGE_REGION'),
    bucket: required(env.GENYLEAP_MEDIA_STORAGE_BUCKET, 'GENYLEAP_MEDIA_STORAGE_BUCKET'),
    accessKeyId: required(env.GENYLEAP_MEDIA_STORAGE_ACCESS_KEY_ID, 'GENYLEAP_MEDIA_STORAGE_ACCESS_KEY_ID'),
    secretAccessKey: required(env.GENYLEAP_MEDIA_STORAGE_SECRET_ACCESS_KEY, 'GENYLEAP_MEDIA_STORAGE_SECRET_ACCESS_KEY'),
    sessionToken: String(env.GENYLEAP_MEDIA_STORAGE_SESSION_TOKEN || '').trim(),
    verificationUrlLifetimeSeconds: positiveExpiry(
      env.GENYLEAP_MEDIA_STORAGE_VERIFICATION_URL_LIFETIME_SECONDS,
      60,
      'GENYLEAP_MEDIA_STORAGE_VERIFICATION_URL_LIFETIME_SECONDS',
    ),
  });
}

export class QuilibriumSdkStorage {
  constructor(config, dependencies = {}) {
    this.config = config;
    this.native = dependencies.native ?? loadNativeAddon(dependencies.env ?? process.env);
    this.fetchImpl = dependencies.fetchImpl ?? globalThis.fetch;
    this.signer = this.native.createQStorageSigner({
      endpoint: config.endpoint,
      region: config.region,
      bucket: config.bucket,
      accessKeyId: config.accessKeyId,
      secretAccessKey: config.secretAccessKey,
      sessionToken: config.sessionToken,
    });
  }

  /**
   * Legacy S3-compatible direct PUT presigning. Kept for provider portability,
   * but the QStorage gateway path does not use it because live QStorage PUT
   * query-auth interoperability currently returns AccessDenied.
   */
  createUpload({ key, contentType, expiresSeconds = 900 }) {
    const presigned = this.native.presignPut(this.signer, key, contentType, expiresSeconds);
    return {
      method: 'PUT',
      uploadUrl: presigned.url,
      headers: { ...presigned.requiredHeaders },
    };
  }

  createDownload({ key, method = 'GET', expiresSeconds = 900 }) {
    return compatibleReadPresign(this.config, key, method, expiresSeconds);
  }

  presignGet({ key, expiresSeconds = 900 }) {
    return compatibleReadPresign(this.config, key, 'GET', expiresSeconds);
  }

  presignHead({ key, expiresSeconds = 900 }) {
    return compatibleReadPresign(this.config, key, 'HEAD', expiresSeconds);
  }

  async beginMultipartUpload({ key, contentType }) {
    const signed = this.native.signCreateMultipart(this.signer, key, contentType);
    const response = await this.fetchImpl(signed.url, {
      method: 'POST',
      headers: { ...signed.headers },
      redirect: 'manual',
    });
    const body = await boundedText(response);
    if (!response.ok) throw providerError('CreateMultipartUpload', response.status, body);

    const uploadId = xmlElement(body, 'UploadId');
    if (!uploadId) {
      const error = new Error('QStorage CreateMultipartUpload response did not contain an UploadId.');
      error.statusCode = 502;
      throw error;
    }
    return { providerUploadId: uploadId };
  }

  async uploadPart({ key, providerUploadId, partNumber, body }) {
    const bytes = Buffer.isBuffer(body) ? body : Buffer.from(body);
    const signed = this.native.signUploadPart(
      this.signer,
      key,
      providerUploadId,
      partNumber,
      bytes,
    );
    const response = await this.fetchImpl(signed.url, {
      method: 'PUT',
      headers: { ...signed.headers },
      body: bytes,
      redirect: 'manual',
    });
    const responseBody = response.ok ? '' : await boundedText(response);
    if (!response.ok) throw providerError('UploadPart', response.status, responseBody);

    const etag = String(response.headers.get('etag') || '').trim();
    if (!etag || etag.length > 512 || /[\r\n]/.test(etag)) {
      const error = new Error('QStorage UploadPart response did not contain a valid ETag.');
      error.statusCode = 502;
      throw error;
    }
    return { etag };
  }

  async completeMultipartUpload({ key, providerUploadId, parts }) {
    const completionXml = Buffer.from(
      '<CompleteMultipartUpload>' +
      parts.map((part) =>
        `<Part><PartNumber>${part.partNumber}</PartNumber><ETag>${xmlEscape(part.etag)}</ETag></Part>`).join('') +
      '</CompleteMultipartUpload>',
      'utf8',
    );
    const signed = this.native.signCompleteMultipart(
      this.signer,
      key,
      providerUploadId,
      completionXml,
    );
    const response = await this.fetchImpl(signed.url, {
      method: 'POST',
      headers: { ...signed.headers },
      body: completionXml,
      redirect: 'manual',
    });
    const body = await boundedText(response);
    if (!response.ok) throw providerError('CompleteMultipartUpload', response.status, body);
    if (/<Error(?:\s|>)/i.test(body)) throw providerError('CompleteMultipartUpload', 502, body);
    return { etag: String(response.headers.get('etag') || xmlElement(body, 'ETag') || '') };
  }

  async abortMultipartUpload({ key, providerUploadId }) {
    const signed = this.native.signAbortMultipart(this.signer, key, providerUploadId);
    const response = await this.fetchImpl(signed.url, {
      method: 'DELETE',
      headers: { ...signed.headers },
      redirect: 'manual',
    });
    if (!response.ok && response.status !== 404) {
      throw providerError('AbortMultipartUpload', response.status, await boundedText(response));
    }
  }

  async deleteObject(key) {
    let signed;

    if (typeof this.native.signDeleteObject === 'function') {
      signed = this.native.signDeleteObject(this.signer, key);
    } else {
      // Compatibility path for containers built with an older native addon.
      // QStorage accepts normal Authorization-header SigV4 DELETE requests, so
      // we can safely sign this operation in JavaScript until the addon image
      // is rebuilt with signDeleteObject exported.
      const endpoint = new URL(this.config.endpoint);
      const endpointPath = endpoint.pathname.replace(/\/+$/, '');
      const objectPath = [this.config.bucket, ...String(key).split('/')]
        .map(awsPercentEncode)
        .join('/');
      const canonicalUri = `${endpointPath}/${objectPath}`.replace(/\/{2,}/g, '/');

      const now = new Date();
      const amzDate = now.toISOString().replace(/[:-]|\.\d{3}/g, '');
      const dateStamp = amzDate.slice(0, 8);
      const service = 's3';
      const scope = `${dateStamp}/${this.config.region}/${service}/aws4_request`;
      const payloadHash = sha256Hex(Buffer.alloc(0));

      const canonicalHeaderEntries = [
        ['host', endpoint.host],
        ['x-amz-content-sha256', payloadHash],
        ['x-amz-date', amzDate],
      ];
      if (this.config.sessionToken) {
        canonicalHeaderEntries.push(['x-amz-security-token', this.config.sessionToken]);
      }
      canonicalHeaderEntries.sort(([left], [right]) => left.localeCompare(right));

      const canonicalHeaders = canonicalHeaderEntries
        .map(([name, value]) => `${name}:${value}\n`)
        .join('');
      const signedHeaders = canonicalHeaderEntries
        .map(([name]) => name)
        .join(';');

      const canonicalRequest = [
        'DELETE',
        canonicalUri,
        '',
        canonicalHeaders,
        signedHeaders,
        payloadHash,
      ].join('\n');

      const stringToSign = [
        'AWS4-HMAC-SHA256',
        amzDate,
        scope,
        sha256Hex(canonicalRequest),
      ].join('\n');

      const dateKey = hmacSha256(
        Buffer.from(`AWS4${this.config.secretAccessKey}`),
        dateStamp,
      );
      const regionKey = hmacSha256(dateKey, this.config.region);
      const serviceKey = hmacSha256(regionKey, service);
      const signingKey = hmacSha256(serviceKey, 'aws4_request');
      const signature = createHmac('sha256', signingKey)
        .update(stringToSign)
        .digest('hex');

      const headers = {
        'x-amz-content-sha256': payloadHash,
        'x-amz-date': amzDate,
        authorization:
          `AWS4-HMAC-SHA256 Credential=${this.config.accessKeyId}/${scope}, ` +
          `SignedHeaders=${signedHeaders}, Signature=${signature}`,
      };
      if (this.config.sessionToken) {
        headers['x-amz-security-token'] = this.config.sessionToken;
      }

      signed = {
        url: `${endpoint.origin}${canonicalUri}`,
        headers,
      };
    }

    const response = await this.fetchImpl(signed.url, {
      method: 'DELETE',
      headers: { ...signed.headers },
      redirect: 'manual',
    });
    if (!response.ok && response.status !== 404) {
      throw providerError('DeleteObject', response.status, await boundedText(response));
    }
  }

  async readImageObject(key, maximumBytes) {
    // Serve permanent Cast image URLs with real image bytes, not a temporary
    // QStorage redirect that external Farcaster scrapers can treat as a link.
    // Only the trusted storage endpoint is contacted; never follow redirects.
    if (!Number.isSafeInteger(maximumBytes) || maximumBytes < 1) {
      throw new Error('A positive image read limit is required.');
    }
    const url = compatibleReadPresign(
      this.config, key, 'GET', this.config.verificationUrlLifetimeSeconds);
    const response = await this.fetchImpl(url, {
      method: 'GET',
      redirect: 'manual',
      signal: AbortSignal.timeout(20_000),
    });
    if (response.status === 404) {
      const error = new Error('Image object not found.');
      error.statusCode = 404;
      throw error;
    }
    if (!response.ok) {
      throw providerError('GetObject', response.status, await boundedText(response));
    }
    const lengthHeader = response.headers.get('content-length');
    const length = lengthHeader === null ? null : Number(lengthHeader);
    if (length !== null && (!Number.isSafeInteger(length) || length < 0
        || length > maximumBytes)) {
      await response.body?.cancel();
      const error = new Error('Image object exceeds the configured size limit.');
      error.statusCode = 413;
      throw error;
    }
    const chunks = [];
    let total = 0;
    if (response.body) {
      for await (const chunk of response.body) {
        total += chunk.length;
        if (total > maximumBytes) {
          // Exiting the async iterator cancels the upstream stream safely.
          const error = new Error('Image object exceeds the configured size limit.');
          error.statusCode = 413;
          throw error;
        }
        chunks.push(Buffer.from(chunk));
      }
    }
    if (length !== null && length !== total) {
      const error = new Error('Image object was truncated during retrieval.');
      error.statusCode = 502;
      throw error;
    }
    return Buffer.concat(chunks, total);
  }

  async headObject(key) {
    // QStorage rejects UNSIGNED-PAYLOAD query authentication for reads and its
    // HEAD interoperability is inconsistent. Verify with an authenticated GET
    // whose canonical payload hash is SHA256(""); cancel the body immediately
    // after headers arrive so successful multipart uploads are not downloaded.
    const url = compatibleReadPresign(
      this.config,
      key,
      'GET',
      this.config.verificationUrlLifetimeSeconds,
    );
    const response = await this.fetchImpl(url, {
      method: 'GET',
      redirect: 'manual',
    });
    if (!response.ok) {
      throw providerError(
        'GetObject verification',
        response.status,
        await boundedText(response),
      );
    }

    const length = Number.parseInt(response.headers.get('content-length') || '', 10);
    const result = {
      sizeBytes: Number.isSafeInteger(length) && length >= 0 ? length : null,
      contentType: response.headers.get('content-type') || '',
      etag: response.headers.get('etag') || '',
    };
    if (response.body) {
      await response.body.cancel().catch(() => {});
    }
    return result;
  }
}
