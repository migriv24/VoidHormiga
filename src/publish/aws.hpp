/* aws.hpp — AWS Signature Version 4, and an S3 object PUT/GET/DELETE over it.
 *
 * ── WHY THIS IS THE FIRST AWS THING BUILT ────────────────────────────────────
 *
 * okf/concepts/platform/data-planes.md phases B/E and web-platform.md §7 both bottom out
 * in the same operation: **put a small file in a bucket.**
 *
 *   - the published index (`site/index/<name>.json`, a few KB) — pushed on its own so
 *     that updating one contact costs that file, not a site deploy;
 *   - the encrypted backup (`platform/backup.cpp`) — an opaque blob;
 *   - later, `hol_uploads` — a visitor's profile picture, in a DIFFERENT bucket,
 *     because the org's own fliers and a stranger's upload are different trust.
 *
 * All three are one signed HTTP request. Signing is the whole of the work, and
 * everything else on the AWS roadmap — Cognito, DynamoDB, Lambda — signs the
 * same way. So this is the piece that unblocks the rest, and it is deliberately
 * built before anything that needs an account.
 *
 * ── NO NEW DEPENDENCY, AND NO NEW CRYPTO ─────────────────────────────────────
 *
 * SigV4 is SHA-256 and HMAC-SHA-256 in a specific order. We already vendor
 * libsodium for the vault and the backup, and it has both
 * (`crypto_hash_sha256`, `crypto_auth_hmacsha256`). So there is no AWS SDK, no
 * new vendored library, and — importantly — **no hand-rolled primitive.** What
 * is written here is the ORDER, which is a specification, not cryptography.
 *
 * ── HOW THIS IS TESTED WITHOUT AN AWS ACCOUNT ────────────────────────────────
 *
 * `tests/aws_smoke.cpp`, and it is worth being precise about what it does and
 * does not prove, because a signing implementation that is confidently wrong
 * fails identically to a bad credential:
 *
 *   PROVEN OFFLINE — the primitives against RFC 4231 and FIPS 180-4 vectors;
 *   the canonical request and string-to-sign built exactly as the spec lays them
 *   out, character for character, which is readable and checkable; the signing
 *   key derivation chain; and that a changed byte anywhere changes the signature.
 *
 *   NOT PROVEN OFFLINE — that AWS agrees. Nothing here can establish that, and
 *   claiming otherwise would be the mistake the Cloudflare deploy already taught
 *   this project once: the local half was exercised end to end and the remote
 *   half was wrong by one HTTP verb, found in a minute by an operator with real
 *   credentials because the vendor's own error reached the log verbatim.
 *
 * So the same play: the operator drives the first real call, and every AWS
 * error is passed through UNPARSED. AWS's SigV4 failures are unusually good —
 * they return the canonical request they computed, so a mismatch is diffable
 * against ours rather than guessable.
 */
#pragma once

#include <functional>
#include <map>
#include <string>

namespace hormiga::aws {

/* Credentials. `session_token` is set only for temporary credentials (STS,
 * an instance role); an ordinary long-lived key pair leaves it empty. */
struct Credentials {
    std::string access_key_id;
    std::string secret_access_key;
    std::string session_token;
};

/* Everything a signature is over. Kept as a value so the whole computation is
 * a pure function of it — which is what makes it testable against a vector. */
struct Request {
    std::string method = "PUT";
    std::string host;                          // bucket.s3.us-west-2.amazonaws.com
    std::string path = "/";                    // must already be URI-encoded
    std::map<std::string, std::string> query;  // signed, sorted, encoded here
    std::map<std::string, std::string> headers;// extra headers to sign
    std::string payload;                       // the body; hashed
    std::string region = "us-east-1";
    std::string service = "s3";
    std::string amz_date;                      // YYYYMMDDTHHMMSSZ; now() if empty
};

/* The intermediate values, exposed because they are what a failure is diagnosed
 * with: AWS returns ITS canonical request in a 403, and comparing the two is the
 * difference between a fix and a guess. */
struct Signed {
    std::string canonical_request;
    std::string string_to_sign;
    std::string signature;       // lowercase hex
    std::string authorization;   // the full Authorization header value
    std::string amz_date;
    std::map<std::string, std::string> headers;  // everything to send, signed
};

/* Sign a request. Pure: same inputs, same output, no clock unless `amz_date` is
 * empty and no network ever. */
Signed sign(const Request& req, const Credentials& creds);

/* ── the pieces, exported because the tests check them individually ─────────
 * A signing bug is otherwise a single opaque hex string that is simply wrong,
 * and knowing WHICH of the four steps drifted is most of the diagnosis. */
std::string sha256_hex(const std::string& data);
std::string hmac_sha256(const std::string& key, const std::string& data); // raw
std::string hex(const std::string& raw);
std::string uri_encode(const std::string& s, bool encode_slash);
std::string signing_key(const std::string& secret, const std::string& date,
                        const std::string& region, const std::string& service);

/* ── the object store ───────────────────────────────────────────────────────
 *
 * S3 over curl, the same transport every other holiday here uses — it ships with
 * Windows 10+, needs nothing vendored, and its stderr is the diagnosis. The
 * secret never touches argv: the Authorization header goes into a curl config
 * file written beside the database, used, and deleted, exactly as the Cloudflare
 * deploy does. */
struct Config {
    Credentials creds;
    std::string bucket;
    std::string region = "us-east-1";
    /* Empty = real AWS (`<bucket>.s3.<region>.amazonaws.com`). Set it for an
     * S3-compatible store — Cloudflare R2, MinIO, a test double — which is what
     * keeps this generalizable rather than an AWS dependency wearing a holon's
     * hat. */
    std::string endpoint;
    std::string work_dir;                                  // where the temp files go
    std::function<std::string(const std::string&)> shell;  // the curl transport
    std::function<void(const std::string&)> progress;
};

struct Result {
    bool ok = false;
    std::string error;    // the vendor's own words, unparsed
    std::string body;     // for a GET
    long long bytes = 0;
};

/* Upload one object. `content_type` may be empty. */
Result put_object(const Config& cfg, const std::string& key,
                  const std::string& body, const std::string& content_type = "");

/* Upload a local file, streamed by curl rather than read into memory — a
 * backup of a real `.miga` is tens of megabytes. */
Result put_file(const Config& cfg, const std::string& key,
                const std::string& path, const std::string& content_type = "");

Result get_object(const Config& cfg, const std::string& key);

/* Can these credentials reach this bucket? Lists one key.
 *
 * NOT a credential validator in the abstract — the same rule the Cloudflare
 * panel already follows, learned from the field agent: a green light that does not
 * predict the operation is worse than no light. This performs the smallest real
 * operation against the actual bucket. */
Result check_access(const Config& cfg);

} // namespace hormiga::aws
