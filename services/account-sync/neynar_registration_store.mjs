// PostgreSQL-owned, atomic Neynar FID lease state.
// A lease cannot be reused with another (principal,custody) tuple and an
// ambiguous network outcome can never silently trigger another purchase.
function normalized(row) {
  if (!row) return null;
  return {
    id:row.id,principal:row.principal,custody:row.custody,
    status:row.status,started:row.started===true,fid:row.fid||null,
    createdAt:Number(row.created_ms),
    reservedAt:row.reserved_ms===null?null:Number(row.reserved_ms),
    txHash:row.transaction_hash||null
  };
}
const FIELDS='id::text,principal,custody,status,started,fid::text,created_ms::text,reserved_ms::text,transaction_hash';
export function createNeynarRegistrationLedger(pool) {
  return Object.freeze({
    async acquire({id,principal,custody,createdAt}) {
      await pool.query(`INSERT INTO neynar_registration_leases
        (id,principal,custody,created_ms)
        VALUES($1::uuid,$2,$3,$4::bigint) ON CONFLICT DO NOTHING`,
        [id,principal,custody,createdAt]);
      const rows=await pool.query(`SELECT ${FIELDS} FROM neynar_registration_leases
        WHERE principal=$1 AND custody=$2`,[principal,custody]);
      return normalized(rows.rows[0]);
    },
    async get(id,principal) {
      const rows=await pool.query(`SELECT ${FIELDS} FROM neynar_registration_leases
        WHERE id=$1::uuid AND principal=$2`,[id,principal]);
      return normalized(rows.rows[0]);
    },
    async claimStart(id,principal) {
      const result=await pool.query(`UPDATE neynar_registration_leases
        SET started=true,updated_at=now()
        WHERE id=$1::uuid AND principal=$2 AND status='reserving'
          AND started=false RETURNING id`,[id,principal]);
      return result.rowCount===1;
    },
    async setReserved(id,principal,{fid,reservedAt}) {
      const result=await pool.query(`UPDATE neynar_registration_leases
        SET fid=$3::bigint,reserved_ms=$4::bigint,status='reserved',
            updated_at=now()
        WHERE id=$1::uuid AND principal=$2 AND status='reserving'
          AND started=true RETURNING id`,[id,principal,fid,reservedAt]);
      if(result.rowCount!==1)throw new Error('lease_reservation_conflict');
    },
    async claimSubmit(id,principal) {
      const result=await pool.query(`UPDATE neynar_registration_leases
        SET status='submitting',updated_at=now()
        WHERE id=$1::uuid AND principal=$2 AND status='reserved'
          AND reserved_ms>0 RETURNING id`,[id,principal]);
      return result.rowCount===1;
    },
    async setOutcome(id,principal,{status,txHash=null}) {
      const allowed=new Set(['reservation_unknown','submitted',
        'submission_unknown','rejected','already_registered','confirmed']);
      if(!allowed.has(status))throw new Error('invalid_neynar_status');
      const result=await pool.query(`UPDATE neynar_registration_leases
        SET status=$3,transaction_hash=COALESCE($4,transaction_hash),
            updated_at=now()
        WHERE id=$1::uuid AND principal=$2 AND
          (status<>'confirmed' OR $3='confirmed')
        RETURNING id`,[id,principal,status,txHash]);
      return result.rowCount===1;
    }
  });
}
