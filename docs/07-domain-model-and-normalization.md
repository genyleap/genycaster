# 07 — Domain Model and Provider Normalization

## Canonical-domain rule

The rest of the application sees only canonical internal models.

Provider DTOs are disposable infrastructure details.

```text
Hypersnap response ─┐
                    ├─ provider mapper → canonical domain model
Managed backend response ────┘
```

## Example

Canonical conceptual model:

```cpp
struct Cast {
    CastId id;
    Fid author;
    std::string text;
    std::optional<CastId> parent;
    std::vector<Embed> embeds;
    Timestamp created_at;
};
```

Forbidden canonical fields include vendor names such as:

```text
providerScore
hypersnapCursor
warpcastState
```

unless a value is explicitly modeled as external provenance/compatibility metadata outside the canonical entity.

## Three data planes

The architecture must distinguish:

### 1. Canonical protocol data

Examples:

- signed casts;
- identity;
- follows;
- reactions;
- signer/key information.

### 2. Derived network data

Examples:

- ranking score;
- recommendation score;
- search rank;
- trending weight;
- reputation signal;
- Mini App category/popularity.

Derived values must not be silently embedded into canonical protocol truth.

### 3. Product-private/local data

Examples:

- drafts;
- private messages;
- notification preferences;
- device sessions;
- locally organized collections;
- private notes.

## Reputation example

Bad:

```cpp
struct User {
    double provider_score;
};
```

Better conceptual model:

```cpp
struct ReputationSignal {
    ReputationKind kind;
    double value;
    Confidence confidence;
};
```

A managed-backend adapter may translate its score to a reputation signal. The canonical `User/Profile` does not become a provider model.

## Provenance

When useful, attach retrieval metadata separately:

```cpp
struct DataProvenance {
    ProviderId source;
    Timestamp fetched_at;
    DataConfidence confidence;
};
```

Provenance must not redefine protocol identity.

## Reconciliation rule

When two providers disagree:

- protocol-authenticated/canonical values win for protocol truth;
- derived values remain in derived models;
- local/private values remain local/private unless explicitly synchronized;
- provider precedence must not overwrite canonical truth with enrichment.
