// Smart Media Pipeline: bounded transient ingress. The user's original image
// is assembled in RAM only until it can be validated and optimized. Nothing
// original is written into QStorage in the optimized upload path.
// Memory budget is reserved BEFORE accepting a session, limiting DoS.
// Incomplete sessions are discarded on expiry or process termination.
import {createHash, randomUUID} from 'node:crypto';
import {SmartMediaError} from './policy.mjs';

const MIB = 1024 * 1024;
export function createSmartMediaIngress({
  now = () => Date.now(),
  maxSessions = 3,
  maxReservedBytes = 64 * MIB,
  maxSessionAgeMs = 30 * 60_000,
} = {}) {
  const sessions = new Map();
  // Retain only small completion receipts, never image bytes, so a lost HTTP
  // acknowledgement can be retried without writing a second cloud object.
  const receipts = new Map();
  let reservedBytes = 0;

  function sweep() {
    for (const [id, session] of sessions) {
      if (session.expiresAt > now() || session.completing) continue;
      reservedBytes -= session.sizeBytes;
      sessions.delete(id);
    }
    for (const [id, receipt] of receipts) {
      if (receipt.expiresAt <= now()) receipts.delete(id);
    }
  }
  const gc = setInterval(sweep, 60_000);
  gc.unref?.();

  function create({sizeBytes, fid, contentType, partCount, key}) {
    sweep();
    if (!Number.isSafeInteger(sizeBytes) || sizeBytes <= 0
        || !Number.isSafeInteger(partCount) || partCount < 1
        || sessions.size >= maxSessions
        || reservedBytes + sizeBytes > maxReservedBytes)
      throw new SmartMediaError('media_ingress_busy', 503);
    const id = randomUUID();
    sessions.set(id, {
      fid, contentType, sizeBytes, key, partCount,
      expiresAt: now() + maxSessionAgeMs,
      parts: new Map(), completing: false,
    });
    reservedBytes += sizeBytes;
    return id;
  }

  function lookup(ticket) {
    sweep();
    const session = sessions.get(ticket.sessionId);
    if (!session || session.fid !== ticket.fid
        || session.key !== ticket.key || session.sizeBytes !== ticket.sizeBytes
        || session.contentType !== ticket.contentType)
      throw new SmartMediaError('invalid_upload_ticket', 403);
    if (session.expiresAt <= now())
      throw new SmartMediaError('expired_upload_ticket', 403);
    return session;
  }

  function addPart(ticket, partNumber, body) {
    const session = lookup(ticket);
    if (session.completing) throw new SmartMediaError('upload_completing', 409);
    if (!Number.isSafeInteger(partNumber) || partNumber < 1
        || partNumber > session.partCount || session.parts.has(partNumber))
      throw new SmartMediaError('invalid_upload_part', 409);
    // Buffer is owned by the request's bounded readBody; do not duplicate it.
    const etag = '"sha256-' + createHash('sha256').update(body).digest('hex') + '"';
    session.parts.set(partNumber, {bytes:body, etag});
    return {partNumber, etag, sizeBytes:body.length};
  }

  async function complete(ticket, parts, handler) {
    sweep();
    const receipt = receipts.get(ticket.sessionId);
    if (receipt) {
      if (receipt.fid !== ticket.fid || receipt.key !== ticket.key
          || receipt.sizeBytes !== ticket.sizeBytes
          || !Array.isArray(parts) || parts.length !== receipt.etags.length
          || parts.some((part,index)=>part.partNumber !== index+1
              || part.etag !== receipt.etags[index]))
        throw new SmartMediaError('invalid_upload_ticket', 403);
      return receipt.outcome;
    }
    const session = lookup(ticket);
    if (session.completing) throw new SmartMediaError('upload_completing', 409);
    if (!Array.isArray(parts) || parts.length !== session.partCount)
      throw new SmartMediaError('invalid_upload_parts', 422);
    const buffers = [];
    let total = 0;
    for (let index = 0; index < parts.length; index++) {
      const actual = session.parts.get(index + 1);
      if (!actual || parts[index].partNumber !== index + 1
          || parts[index].etag !== actual.etag)
        throw new SmartMediaError('invalid_upload_parts', 422);
      buffers.push(actual.bytes);
      total += actual.bytes.length;
    }
    if (total !== session.sizeBytes)
      throw new SmartMediaError('upload_size_mismatch', 409);
    session.completing = true;
    try {
      const outcome = await handler(Buffer.concat(buffers, total));
      release(ticket.sessionId);
      if (receipts.size >= 256) receipts.delete(receipts.keys().next().value);
      receipts.set(ticket.sessionId, {
        fid:ticket.fid, key:ticket.key, sizeBytes:ticket.sizeBytes,
        etags:parts.map(part=>part.etag), outcome,
        expiresAt:session.expiresAt,
      });
      return outcome;
    } catch (error) {
      // An invalid source cannot be fixed by resubmitting the same immutable
      // multipart parts. Free its reserved memory immediately. A transient
      // storage failure, by contrast, can be retried within the ticket TTL.
      if ([413, 415, 422].includes(error?.statusCode))
        release(ticket.sessionId);
      throw error;
    } finally {
      if (sessions.get(ticket.sessionId) === session)
        session.completing = false;
    }
  }

  function release(id) {
    const session = sessions.get(id);
    if (!session) return;
    reservedBytes -= session.sizeBytes;
    sessions.delete(id);
  }

  return Object.freeze({
    create, addPart, complete,
    abort(ticket) {
      sweep();
      const receipt = receipts.get(ticket.sessionId);
      if (receipt) {
        if (receipt.key !== ticket.key || receipt.fid !== ticket.fid)
          throw new SmartMediaError('invalid_upload_ticket', 403);
        // Gateway must delete the finalized QStorage object before removing
        // the receipt; otherwise a failed delete would permanently orphan it.
        return 'completed';
      }
      const session = lookup(ticket);
      if (session.completing) throw new SmartMediaError('upload_completing', 409);
      release(ticket.sessionId);
      return 'staged';
    },
    forgetCompleted(ticket) {receipts.delete(ticket.sessionId);},
    forgetByKey(key) {
      for (const [id,receipt] of receipts)
        if (receipt.key === key) receipts.delete(id);
    },
    close() {clearInterval(gc); sessions.clear(); receipts.clear(); reservedBytes = 0;},
    status() { sweep(); return {sessions:sessions.size, reservedBytes,
      receipts:receipts.size}; },
  });
}

export async function uploadOptimizedObject(storage, {
  key, contentType, bytes, partSizeBytes,
}) {
  if (!Buffer.isBuffer(bytes) || bytes.length === 0
      || !Number.isSafeInteger(partSizeBytes) || partSizeBytes < 1)
    throw new SmartMediaError('invalid_image_payload', 422);
  let multipart = null;
  try {
    multipart = await storage.beginMultipartUpload({key, contentType});
    const parts = [];
    for (let index = 0, position = 0; position < bytes.length;
         index++, position += partSizeBytes) {
      const segment = bytes.subarray(position, Math.min(position + partSizeBytes,bytes.length));
      const receipt = await storage.uploadPart({
        key, providerUploadId:multipart.providerUploadId,
        partNumber:index + 1, body:segment,
      });
      parts.push({partNumber:index + 1, etag:receipt.etag});
    }
    await storage.completeMultipartUpload({
      key, providerUploadId:multipart.providerUploadId, parts,
    });
    const stored = await storage.headObject(key);
    if (stored.sizeBytes !== bytes.length || (
      stored.contentType
      && stored.contentType.split(';')[0].trim().toLowerCase() !== contentType
    )) throw new SmartMediaError('upload_verification_failed', 502);
    return stored;
  } catch (error) {
    if (multipart?.providerUploadId) {
      try {
        await storage.abortMultipartUpload({key,providerUploadId:multipart.providerUploadId});
      } catch {/* the provider may already have finalized the object */}
    }
    throw error;
  }
}
