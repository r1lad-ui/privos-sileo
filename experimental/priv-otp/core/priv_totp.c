#include "priv_totp.h"

const priv_totp_params PRIV_TOTP_PROFILE = {
    PRIV_TOTP_PROFILE_PERIOD_SECONDS, PRIV_TOTP_PROFILE_DIGITS, 0u, INT64_C(0)
};

static void clear_bytes(void *buffer, size_t length)
{
    volatile uint8_t *bytes = (volatile uint8_t *)buffer;
    while (length != 0u) {
        *bytes++ = 0u;
        --length;
    }
}

static int valid_params(const priv_totp_params *params)
{
    return params != NULL && params->period_seconds >= 1u &&
        params->period_seconds <= PRIV_TOTP_MAX_PERIOD_SECONDS &&
        params->digits >= 6u && params->digits <= PRIV_TOTP_MAX_DIGITS &&
        params->previous_steps <= 1u && params->epoch_seconds >= INT64_C(0);
}

static priv_totp_status prepare(
    const priv_totp_params *params, const priv_totp_crypto *crypto,
    const uint8_t *secret, size_t secret_length, int64_t unix_seconds,
    uint64_t *counter)
{
    if (!valid_params(params) || crypto == NULL ||
        crypto->hmac_sha256 == NULL || secret == NULL ||
        secret_length != PRIV_TOTP_SECRET_BYTES) {
        return PRIV_TOTP_BAD_ARGUMENT;
    }
    if (unix_seconds < INT64_C(0) || unix_seconds < params->epoch_seconds) {
        return PRIV_TOTP_BAD_TIME;
    }
    *counter = ((uint64_t)unix_seconds - (uint64_t)params->epoch_seconds) /
        (uint64_t)params->period_seconds;
    return PRIV_TOTP_OK;
}

static priv_totp_status code_for_counter(
    const priv_totp_crypto *crypto, const uint8_t *secret,
    uint64_t counter, uint32_t digits,
    char output[PRIV_TOTP_OUTPUT_BYTES])
{
    uint8_t message[8];
    uint8_t digest[PRIV_TOTP_DIGEST_BYTES] = {0};
    uint32_t value;
    uint32_t modulus = UINT32_C(1);
    uint32_t offset;
    size_t index;

    for (index = 0u; index < sizeof(message); ++index) {
        message[sizeof(message) - 1u - index] = (uint8_t)(counter & UINT64_C(255));
        counter >>= 8u;
    }
    if (crypto->hmac_sha256(crypto->context, secret, PRIV_TOTP_SECRET_BYTES,
            message, sizeof(message), digest) != 1) {
        clear_bytes(digest, sizeof(digest));
        clear_bytes(message, sizeof(message));
        return PRIV_TOTP_CRYPTO_ERROR;
    }

    /* RFC 6238 SHA256 dynamic truncation: last tag byte gives a 4-bit offset.
     * offset<=15, so every read is within the 32-byte HMAC result. */
    offset = (uint32_t)(digest[PRIV_TOTP_DIGEST_BYTES - 1u] & UINT8_C(15));
    value = ((uint32_t)(digest[offset] & UINT8_C(127)) << 24u) |
        ((uint32_t)digest[offset + 1u] << 16u) |
        ((uint32_t)digest[offset + 2u] << 8u) |
        (uint32_t)digest[offset + 3u];
    for (index = 0u; index < digits; ++index) {
        modulus *= UINT32_C(10);
    }
    value %= modulus;
    output[digits] = '\0';
    for (index = digits; index != 0u; --index) {
        output[index - 1u] = (char)('0' + (value % UINT32_C(10)));
        value /= UINT32_C(10);
    }
    clear_bytes(digest, sizeof(digest));
    clear_bytes(message, sizeof(message));
    return PRIV_TOTP_OK;
}

static uint32_t equal_digits(const char *left, const char *right, size_t digits)
{
    /* Volatile accumulation preserves every iteration under optimization.
     * The loop bound depends on the public profile, not the matching prefix.
     * Platform integration still owns compiler/crypto timing review. */
    volatile uint32_t difference = UINT32_C(0);
    size_t index;
    for (index = 0u; index < digits; ++index) {
        difference |= (uint32_t)((uint8_t)left[index] ^ (uint8_t)right[index]);
    }
    return (uint32_t)(difference == UINT32_C(0));
}

priv_totp_status priv_totp_generate(
    const priv_totp_params *params, const priv_totp_crypto *crypto,
    const uint8_t *secret, size_t secret_length, int64_t unix_seconds,
    char *output, size_t output_capacity)
{
    uint64_t counter;
    char temporary[PRIV_TOTP_OUTPUT_BYTES] = {0};
    size_t index;
    priv_totp_status status = prepare(params, crypto, secret, secret_length,
        unix_seconds, &counter);
    if (status != PRIV_TOTP_OK) {
        return status;
    }
    if (output == NULL || output_capacity < (size_t)params->digits + 1u) {
        return PRIV_TOTP_BAD_ARGUMENT;
    }
    status = code_for_counter(crypto, secret, counter, params->digits, temporary);
    if (status == PRIV_TOTP_OK) {
        for (index = 0u; index <= params->digits; ++index) {
            output[index] = temporary[index];
        }
    }
    clear_bytes(temporary, sizeof(temporary));
    return status;
}

priv_totp_status priv_totp_verify(
    const priv_totp_params *params, const priv_totp_crypto *crypto,
    const uint8_t *secret, size_t secret_length, int64_t unix_seconds,
    const char *code, size_t code_length, priv_totp_replay_state *state)
{
    uint64_t counter;
    uint64_t matched_counter = UINT64_C(0);
    uint32_t matched = 0u;
    uint32_t replayed_match = 0u;
    uint32_t candidates;
    uint32_t step;
    size_t index;
    priv_totp_status status = prepare(params, crypto, secret, secret_length,
        unix_seconds, &counter);
    if (status != PRIV_TOTP_OK) {
        return status;
    }
    if (code == NULL || state == NULL || state->has_accepted > 1u ||
        (state->has_accepted == 0u && state->last_accepted_counter != UINT64_C(0))) {
        return PRIV_TOTP_BAD_ARGUMENT;
    }
    if (code_length != params->digits) {
        return PRIV_TOTP_REJECTED;
    }
    for (index = 0u; index < code_length; ++index) {
        if ((uint8_t)code[index] < (uint8_t)'0' ||
            (uint8_t)code[index] > (uint8_t)'9') {
            return PRIV_TOTP_REJECTED;
        }
    }

    candidates = 1u + (uint32_t)(params->previous_steps == 1u && counter != UINT64_C(0));
    for (step = 0u; step < candidates; ++step) {
        char expected[PRIV_TOTP_OUTPUT_BYTES] = {0};
        uint64_t candidate = counter - (uint64_t)step;
        uint64_t selection_mask;
        uint32_t same;
        uint32_t consumed;
        status = code_for_counter(crypto, secret, candidate, params->digits, expected);
        if (status != PRIV_TOTP_OK) {
            clear_bytes(expected, sizeof(expected));
            return status;
        }
        same = equal_digits(code, expected, code_length);
        consumed = (uint32_t)(state->has_accepted != 0u &&
            candidate <= state->last_accepted_counter);
        replayed_match |= same & consumed;
        /* Descending counter order: select the first (newest) matching period,
         * but continue through every configured candidate before committing. */
        selection_mask = UINT64_C(0) - (uint64_t)(same & (matched ^ 1u));
        matched_counter = (matched_counter & ~selection_mask) |
            (candidate & selection_mask);
        matched |= same;
        clear_bytes(expected, sizeof(expected));
    }
    if (matched == 0u || replayed_match != 0u) {
        return PRIV_TOTP_REJECTED;
    }
    state->last_accepted_counter = matched_counter;
    state->has_accepted = 1u;
    return PRIV_TOTP_OK;
}

priv_totp_status priv_totp_generate_profile(
    const priv_totp_crypto *crypto,
    const uint8_t *secret, size_t secret_length, int64_t unix_seconds,
    char *output, size_t output_capacity)
{
    return priv_totp_generate(&PRIV_TOTP_PROFILE, crypto, secret, secret_length,
        unix_seconds, output, output_capacity);
}

priv_totp_status priv_totp_verify_profile(
    const priv_totp_crypto *crypto,
    const uint8_t *secret, size_t secret_length, int64_t unix_seconds,
    const char *code, size_t code_length, priv_totp_replay_state *state)
{
    return priv_totp_verify(&PRIV_TOTP_PROFILE, crypto, secret, secret_length,
        unix_seconds, code, code_length, state);
}
