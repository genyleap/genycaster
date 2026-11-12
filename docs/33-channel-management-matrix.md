# Channel management — capability and safety matrix

Status: **in development**. The public channel directory exists in the source,
but a documented endpoint alone is not proof of working writes.

## Domain semantics

- Discover lists public channels, not verified memberships.
- Following is the list of channels explicitly followed by this FID.
- Member lists channels for which the selected provider has verified membership.
- Moderate requires an authoritative lead or moderator role and write-side
  authorization by the selected provider.
- Invitations are distinct from follows and memberships.

## Provider matrix

| Capability | HyperSnap v2 | Neynar v2 |
|---|---|---|
| Search and trending | GET channel/search, channel/trending | GET channel/search, channel/trending with API key |
| Channel lookup | GET channel | GET channel |
| Member list | GET channel/members | GET channel/member/list |
| Viewer memberships | GET user/memberships/list, channels array | GET user/memberships/list, members array containing channel |
| Following list | No canonical channel-follow list in channel docs | GET user/channels |
| Pending invites | Compatibility endpoint always returns empty | GET channel/member/invite/list |
| Follow/unfollow REST | HTTP 501 Not Implemented | POST/DELETE channel/follow with managed signer |
| Invite/respond/remove REST | HTTP 501 Not Implemented | POST/PUT channel/member/invite; DELETE channel/member |

The directory and composer must not share a mutable membership/search state.
Public discovery must not promote a user to a verified channel member.

## Write safety contract

- Public read operations route to the selected provider and valid endpoint.
- Neynar management requires an API credential restricted to the canonical
  host, an approved managed signer UUID for the exact logged-in FID,
  explicit write-provider registration, and a confirmation prompt.
- Inviting/removing a user requires a verified current lead/moderator role.
- The UI never grants a role optimistically after a REST request.
- Unverified or unimplemented operations are explicitly unavailable.
- Do not emulate a HyperSnap channel follow by forging a normal FID-follow
  LinkAdd. The canonical codec expects a user FID, not a channel parent URL.

## Outstanding implementation requirements

- The current ProviderProfilesController registers HyperSnap v2 and Snapchain
  v1 but not a Neynar managed management writer. This must be integrated into
  the provider registry, selected-provider configuration, secure signer
  bootstrap and tests before Neynar mutations are actually available.
- HyperSnap channel follow/management REST returns HTTP 501. A supported,
  interoperable signed channel-follow write must be specified and tested
  before exposing the action.
- Pagination and end-to-end UI/permission tests remain for large member and
  invitation lists. Source-level checks are not a release certification.
- No new WebAssembly build or site deployment has been performed.

Provider references:
- https://hypersnap.org/docs/reference/reads/channels
- https://docs.neynar.com/reference/
