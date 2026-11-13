import {createHmac,timingSafeEqual} from 'node:crypto';
import {exactAddress} from './neynar_id_registry.mjs';

const UUID=/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
const TOKEN=/^[A-Za-z0-9_-]{40,1024}\.[A-Za-z0-9_-]{43}$/;
const mac=(secret,message)=>createHmac('sha256',secret)
  .update(message).digest('base64url');

/**
 * Sponsor/payment authorization for a NEW wallet with no existing FID.
 *
 * The voucher is minted exclusively by a trusted server-side payment or
 * sponsorship verifier. Nothing in the untrusted client can create one.
 * This verifier never calls Neynar; it only binds the paid authorization to
 * the exact custody address and unique invoice ID.
 */
export function createPaidEnrollmentVerifier({secret,now=Date.now}) {
  if(typeof secret!=='string'||secret.length<32)
    throw new Error('strong server-only enrollment secret required');
  function issue({invoiceId,custodyAddress,expiresAtMs}) {
    if(!UUID.test(invoiceId))throw new Error('invalid invoice identifier');
    const wallet=exactAddress(custodyAddress);
    if(!Number.isSafeInteger(expiresAtMs)
       ||expiresAtMs<now()+60_000||expiresAtMs>now()+24*3600_000)
      throw new Error('invalid voucher expiry');
    const body=Buffer.from(JSON.stringify({
      v:1,invoice:invoiceId.toLowerCase(),wallet,exp:expiresAtMs
    })).toString('base64url');
    return body+'.'+mac(secret,body);
  }
  async function authorize(enrollment,custodyAddress) {
    if(typeof enrollment!=='string'||!TOKEN.test(enrollment))return null;
    let wallet;
    try{wallet=exactAddress(custodyAddress);}catch{return null;}
    const [body,proof]=enrollment.split('.');
    const expected=mac(secret,body);
    if(proof.length!==expected.length
       ||!timingSafeEqual(Buffer.from(proof),Buffer.from(expected)))
      return null;
    let parsed;
    try{parsed=JSON.parse(Buffer.from(body,'base64url').toString('utf8'));}catch{
      return null;
    }
    if(!parsed||parsed.v!==1||!UUID.test(parsed.invoice)
       ||parsed.wallet!==wallet||!Number.isSafeInteger(parsed.exp)
       ||parsed.exp<=now()||parsed.exp>now()+24*3600_000)
      return null;
    return {approved:true,subject:'paid-invoice:'+parsed.invoice};
  }
  return Object.freeze({issue,authorize});
}
