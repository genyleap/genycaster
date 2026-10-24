# Per-query indexed continuation affinity

## Feature gate

| Item | Contract |
|---|---|
| Responsibility | Prevent reusing an indexed read cursor for another FID/channel even if endpoint, provider ID and capability match |
| Origin | Adapter-private persistence correctness |
| Mandatory | Required for user-scoped indexed read pagination |
| Public contract | Same canonical `ContinuationToken`; provider-private `continuation::Store` now also binds selector identity |
| Canonical invariants | No raw cursor, node URL, selector encoding or provider tokens in Cast/Profile/FeedEntry |
| Dependencies | Existing SQLite continuation store, concrete read adapter, capability registry |
| Expected failures | Wrong capability/provider/endpoint/FID/channel returns Conflict before HTTP network request |
| Local state | Versioned `indexed-v3:` payload, strict endpoint and selector prefix |
| Offline | Cached canonical data survives; invalid legacy cursors require new first page |
| Provider policy | No change to routing, write pinning, signer, auth, privacy or rate limits |
| Failover | Continuation cannot silently migrate between any different dataset or node |
| Tests | Indexed contract and no-network replay checks; asynchronous/blocking scope guards in JS |

## Token scope

`indexed-v3:<endpoint>\\n<selector_byte_length>:<selector>\\n<opaque_provider_cursor>`

The backing continuation map independently stores capability and provider ID.
The adapter accepts only canonical semantic selectors: `fid:<FID>` and
`channel_ids:<id>`. Pagination limit, transport cursor, ranking fallback
and temporary sort options do **not** change scope. Aggregate routes with no
user/channel selector bind an explicitly empty selector identity.

The same scope is applied to blocking and asynchronous user feeds, liked
casts, social graph, notifications and channel feeds. Previous `indexed-v2`
continuations are intentionally rejected; restart from page one. Existing
cached canonical posts and profiles remain usable offline.

This does not implement a network-wide index or secure provider-credential
vault. Multi-node live validation and full native/WebAssembly release builds
remain outstanding.
