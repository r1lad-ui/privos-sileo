#ifndef PRIV_TOTP_H
#define PRIV_TOTP_H

/* Portable C11 TOTP engine, SHA256 only. No clock, storage, random generation,
 * logging, platform unlock APIs or cryptographic primitive implementation.
 * Specification: https://www.rfc-editor.org/rfc/rfc6238#section-4.2
 * Validation: https://www.rfc-editor.org/rfc/rfc6238#section-5.2
 */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PRIV_TOTP_SECRET_BYTES 32u
#define PRIV_TOTP_DIGEST_BYTES 32u
#define PRIV_TOTP_MAX_DIGITS 8u
#define PRIV_TOTP_OUTPUT_BYTES (PRIV_TOTP_MAX_DIGITS + 1u)
#define PRIV_TOTP_PROFILE_DIGITS 6u
#define PRIV_TOTP_PROFILE_PERIOD_SECONDS 60u
#define PRIV_TOTP_MAX_PERIOD_SECONDS 3600u

typedef enum {
    PRIV_TOTP_OK = 0,
    PRIV_TOTP_REJECTED = 1,
    PRIV_TOTP_BAD_ARGUMENT = 2,
    PRIV_TOTP_BAD_TIME = 3,
    PRIV_TOTP_CRYPTO_ERROR = 4
} priv_totp_status;

/* The trusted callback MUST calculate HMAC-SHA256 using the supplied key and
 * message, write exactly 32 digest bytes, and return exactly 1 on success.
 * Any other return value fails closed. It must not retain these pointers or
 * modify key/message/state. Its own context and key handling are its owner's
 * responsibility. The engine always supplies a 32-byte key and 8-byte message.
 */
typedef int (*priv_totp_hmac_sha256_fn)(
    void *context,
    const uint8_t *key, size_t key_length,
    const uint8_t *message, size_t message_length,
    uint8_t digest[PRIV_TOTP_DIGEST_BYTES]);

typedef struct {
    priv_totp_hmac_sha256_fn hmac_sha256;
    void *context;
} priv_totp_crypto;

/* Supported configurable range: period 1..3600 seconds, 6..8 digits,
 * previous_steps 0 or 1, nonnegative epoch. Use a 30-second/8-digit instance
 * for RFC 6238 SHA256 vectors. Configuration is trusted provisioning input;
 * never obtain it from an unauthenticated code submission.
 */
typedef struct {
    uint32_t period_seconds;
    uint32_t digits;
    uint32_t previous_steps;
    int64_t epoch_seconds;
} priv_totp_params;

/* Explicit application profile: SHA256, 32-byte secret, 6 digits, 60 seconds,
 * Unix epoch, CURRENT period only. The profile wrappers below cannot change it.
 * An integration may explicitly copy the params and set previous_steps=1:
 * this tolerates late typing, but extends validity to almost 120 seconds and
 * doubles the number of candidate codes. No future period is ever accepted.
 * RFC 6238 recommends 30 seconds by default; 60 seconds is this project's
 * deliberate profile choice, not the RFC default.
 */
extern const priv_totp_params PRIV_TOTP_PROFILE;

/* Initialize once for a NEW secret/configuration. Counter zero is valid.
 * has_accepted must be 0 or 1; last_accepted_counter must be zero when it is 0.
 * This structure contains no secret. It is not a serialization/file format.
 *
 * The caller MUST bind this state to one secret and configuration, serialize
 * concurrent verifications, and durably commit successful state BEFORE
 * granting authorization. Resetting, losing or rolling back it permits replay.
 * Persist its fields explicitly; do not persist C struct padding. An error
 * leaves every byte of this structure unchanged. Rate limiting, trusted time,
 * secret protection and atomic durable persistence belong to the integration.
 */
typedef struct {
    uint64_t last_accepted_counter;
    uint32_t has_accepted;
} priv_totp_replay_state;

#define PRIV_TOTP_REPLAY_STATE_INIT { UINT64_C(0), 0u }

/* On success writes exactly digits ASCII bytes and a trailing NUL. out_capacity
 * must be at least digits+1. On any failure, output is unchanged. Inputs/output
 * must not overlap each other or the callback context. No secret is copied into
 * long-lived state; temporary digest and code buffers are cleared on return.
 */
priv_totp_status priv_totp_generate(
    const priv_totp_params *params, const priv_totp_crypto *crypto,
    const uint8_t *secret, size_t secret_length, int64_t unix_seconds,
    char *output, size_t output_capacity);

/* code_length must equal digits exactly and every byte must be ASCII '0'..'9'.
 * The input need not be NUL-terminated; no strlen, trimming or numeric coercion
 * is performed. Wrong syntax, wrong code and replay all return REJECTED.
 * Comparison visits every digit, with no content-dependent early exit, and all
 * configured candidate periods are evaluated before accepting any match.
 *
 * Success updates state with the newest matching counter, strictly greater
 * than the last accepted counter. If a matching eligible-window counter was
 * already consumed, reject even if its digits collide with a newer candidate.
 * This is monotonic COUNTER replay protection, not a promise that six-digit
 * values never recur. No forward/backward clock resynchronization is performed.
 * All pointers and buffers must remain valid and not overlap for the call.
 */
priv_totp_status priv_totp_verify(
    const priv_totp_params *params, const priv_totp_crypto *crypto,
    const uint8_t *secret, size_t secret_length, int64_t unix_seconds,
    const char *code, size_t code_length, priv_totp_replay_state *state);

priv_totp_status priv_totp_generate_profile(
    const priv_totp_crypto *crypto,
    const uint8_t *secret, size_t secret_length, int64_t unix_seconds,
    char *output, size_t output_capacity);

priv_totp_status priv_totp_verify_profile(
    const priv_totp_crypto *crypto,
    const uint8_t *secret, size_t secret_length, int64_t unix_seconds,
    const char *code, size_t code_length, priv_totp_replay_state *state);

#ifdef __cplusplus
}
#endif
#endif
