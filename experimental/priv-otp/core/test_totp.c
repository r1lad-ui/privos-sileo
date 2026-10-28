/* Host tests use real OpenSSL HMAC-SHA256 and PUBLIC RFC 6238 test data only.
 * No production secret is generated, read, stored or printed.
 * https://www.rfc-editor.org/rfc/rfc6238#appendix-A (32-byte SHA256 seed)
 * https://www.rfc-editor.org/rfc/rfc6238#appendix-B (expected SHA256 outputs)
 *
 * Build/run (from this directory, output outside the repository):
 * cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Wconversion -Werror \
 *    priv_totp.c test_totp.c -lcrypto -o /tmp/priv-test-totp
 * /tmp/priv-test-totp
 */
#include "priv_totp.h"

#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t PUBLIC_RFC_KEY[PRIV_TOTP_SECRET_BYTES] =
    "12345678901234567890123456789012";
static unsigned int assertions;

#define CHECK(condition) do { \
    ++assertions; \
    if (!(condition)) { \
        fprintf(stderr, "FAIL at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

typedef struct {
    unsigned int calls;
    unsigned int fail_on_call;
    uint64_t last_counter;
} crypto_audit;

static int openssl_hmac(void *context, const uint8_t *key, size_t key_length,
    const uint8_t *message, size_t message_length, uint8_t digest[PRIV_TOTP_DIGEST_BYTES])
{
    crypto_audit *audit = (crypto_audit *)context;
    unsigned int written = 0u;
    size_t index;
    CHECK(key_length == PRIV_TOTP_SECRET_BYTES);
    CHECK(message_length == 8u);
    if (audit != NULL) {
        ++audit->calls;
        audit->last_counter = UINT64_C(0);
        for (index = 0u; index < message_length; ++index) {
            audit->last_counter = (audit->last_counter << 8u) | (uint64_t)message[index];
        }
        if (audit->calls == audit->fail_on_call) {
            /* Deliberate provider failure, including partial output, must
             * never accept a code or partially mutate caller-owned data. */
            digest[0] = UINT8_C(42);
            return 0;
        }
    }
    return HMAC(EVP_sha256(), key, (int)key_length, message, message_length,
        digest, &written) != NULL && written == PRIV_TOTP_DIGEST_BYTES;
}

static const priv_totp_crypto OPENSSL_CRYPTO = {openssl_hmac, NULL};

static void expect_failure(const priv_totp_params *params,
    const priv_totp_crypto *crypto, const uint8_t *key, size_t key_length,
    int64_t when, const char *code, size_t length,
    priv_totp_replay_state *state, priv_totp_status expected)
{
    unsigned char before[sizeof(*state)];
    memcpy(before, state, sizeof(before));
    CHECK(priv_totp_verify(params, crypto, key, key_length, when, code, length, state) == expected);
    CHECK(memcmp(before, state, sizeof(before)) == 0);
}

static void test_rfc6238_sha256(void)
{
    static const struct {int64_t when; const char *expected;} vectors[] = {
        {INT64_C(59), "46119246"},
        {INT64_C(1111111109), "68084774"},
        {INT64_C(1111111111), "67062674"},
        {INT64_C(1234567890), "91819424"},
        {INT64_C(2000000000), "90698825"},
        {INT64_C(20000000000), "77737706"}
    };
    const priv_totp_params params = {30u, 8u, 0u, INT64_C(0)};
    size_t index;
    for (index = 0u; index < sizeof(vectors) / sizeof(vectors[0]); ++index) {
        char code[PRIV_TOTP_OUTPUT_BYTES];
        priv_totp_replay_state state = PRIV_TOTP_REPLAY_STATE_INIT;
        CHECK(priv_totp_generate(&params, &OPENSSL_CRYPTO, PUBLIC_RFC_KEY,
            sizeof(PUBLIC_RFC_KEY), vectors[index].when, code, sizeof(code)) == PRIV_TOTP_OK);
        CHECK(strcmp(code, vectors[index].expected) == 0);
        CHECK(priv_totp_verify(&params, &OPENSSL_CRYPTO, PUBLIC_RFC_KEY,
            sizeof(PUBLIC_RFC_KEY), vectors[index].when, code, 8u, &state) == PRIV_TOTP_OK);
        CHECK(state.has_accepted == 1u);
        CHECK(state.last_accepted_counter == (uint64_t)vectors[index].when / UINT64_C(30));
        expect_failure(&params, &OPENSSL_CRYPTO, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
            vectors[index].when, code, 8u, &state, PRIV_TOTP_REJECTED);
    }
}

static void test_profile_boundaries(void)
{
    /* Independent fixtures from Python's stdlib hmac/hashlib with the same
     * PUBLIC RFC seed, not output copied from the C engine under test. */
    static const struct {int64_t when; const char *expected;} fixtures[] = {
        {INT64_C(0), "920136"}, {INT64_C(59), "920136"},
        {INT64_C(60), "119246"}, {INT64_C(119), "119246"},
        {INT64_C(120), "882438"}, {INT64_C(1560), "021280"},
        {INT64_C(1234567890), "450756"},
        {INT64_C(257698037820), "447045"}, /* Counter > UINT32_MAX. */
        {INT64_MAX, "539680"}
    };
    size_t index;
    CHECK(PRIV_TOTP_PROFILE.period_seconds == 60u);
    CHECK(PRIV_TOTP_PROFILE.digits == 6u);
    CHECK(PRIV_TOTP_PROFILE.previous_steps == 0u);
    CHECK(PRIV_TOTP_PROFILE.epoch_seconds == INT64_C(0));
    for (index = 0u; index < sizeof(fixtures) / sizeof(fixtures[0]); ++index) {
        char code[10];
        priv_totp_replay_state state = PRIV_TOTP_REPLAY_STATE_INIT;
        memset(code, 'X', sizeof(code));
        CHECK(priv_totp_generate_profile(&OPENSSL_CRYPTO, PUBLIC_RFC_KEY,
            sizeof(PUBLIC_RFC_KEY), fixtures[index].when, code, 7u) == PRIV_TOTP_OK);
        CHECK(strcmp(code, fixtures[index].expected) == 0);
        CHECK(code[7] == 'X' && code[8] == 'X' && code[9] == 'X');
        CHECK(priv_totp_verify_profile(&OPENSSL_CRYPTO, PUBLIC_RFC_KEY,
            sizeof(PUBLIC_RFC_KEY), fixtures[index].when, code, 6u, &state) == PRIV_TOTP_OK);
        CHECK(state.last_accepted_counter == (uint64_t)fixtures[index].when / UINT64_C(60));
    }
    {
        char code[PRIV_TOTP_OUTPUT_BYTES];
        priv_totp_params offset = {60u, 6u, 0u, INT64_C(1000)};
        CHECK(priv_totp_generate(&offset, &OPENSSL_CRYPTO, PUBLIC_RFC_KEY,
            sizeof(PUBLIC_RFC_KEY), INT64_C(1059), code, sizeof(code)) == PRIV_TOTP_OK);
        CHECK(strcmp(code, "920136") == 0);
        CHECK(priv_totp_generate(&offset, &OPENSSL_CRYPTO, PUBLIC_RFC_KEY,
            sizeof(PUBLIC_RFC_KEY), INT64_C(1060), code, sizeof(code)) == PRIV_TOTP_OK);
        CHECK(strcmp(code, "119246") == 0);
        offset.epoch_seconds = INT64_MAX;
        CHECK(priv_totp_generate(&offset, &OPENSSL_CRYPTO, PUBLIC_RFC_KEY,
            sizeof(PUBLIC_RFC_KEY), INT64_MAX, code, sizeof(code)) == PRIV_TOTP_OK);
        CHECK(strcmp(code, "920136") == 0);
    }
}

static void test_strict_input(void)
{
    static const char valid_without_nul[6] = {'1','1','9','2','4','6'};
    static const char *invalid[] = {" 19246", "11924 ", "+19246", "-19246",
        "1192a6", "1192\n6", "1192\r6", "1192\t6", "\xff" "19246"};
    priv_totp_replay_state state = PRIV_TOTP_REPLAY_STATE_INIT;
    char embedded_nul[6] = {'1','1','9','\0','4','6'};
    size_t index;
    for (index = 0u; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
        expect_failure(&PRIV_TOTP_PROFILE, &OPENSSL_CRYPTO, PUBLIC_RFC_KEY,
            sizeof(PUBLIC_RFC_KEY), INT64_C(60), invalid[index], 6u, &state, PRIV_TOTP_REJECTED);
    }
    expect_failure(&PRIV_TOTP_PROFILE, &OPENSSL_CRYPTO, PUBLIC_RFC_KEY,
        sizeof(PUBLIC_RFC_KEY), INT64_C(60), embedded_nul, 6u, &state, PRIV_TOTP_REJECTED);
    for (index = 0u; index < 6u; ++index) {
        expect_failure(&PRIV_TOTP_PROFILE, &OPENSSL_CRYPTO, PUBLIC_RFC_KEY,
            sizeof(PUBLIC_RFC_KEY), INT64_C(60), valid_without_nul, index, &state, PRIV_TOTP_REJECTED);
    }
    expect_failure(&PRIV_TOTP_PROFILE, &OPENSSL_CRYPTO, PUBLIC_RFC_KEY,
        sizeof(PUBLIC_RFC_KEY), INT64_C(60), "119246\n", 7u, &state, PRIV_TOTP_REJECTED);
    expect_failure(&PRIV_TOTP_PROFILE, &OPENSSL_CRYPTO, PUBLIC_RFC_KEY,
        sizeof(PUBLIC_RFC_KEY), INT64_C(60), "119246", SIZE_MAX, &state, PRIV_TOTP_REJECTED);
    CHECK(priv_totp_verify_profile(&OPENSSL_CRYPTO, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(60), valid_without_nul, sizeof(valid_without_nul), &state) == PRIV_TOTP_OK);
    CHECK(state.last_accepted_counter == UINT64_C(1));
}

static void test_windows_replay_and_clock(void)
{
    priv_totp_params grace = PRIV_TOTP_PROFILE;
    priv_totp_replay_state state = PRIV_TOTP_REPLAY_STATE_INIT;
    priv_totp_replay_state zero = PRIV_TOTP_REPLAY_STATE_INIT;
    crypto_audit audit = {0u, 0u, UINT64_C(0)};
    priv_totp_crypto provider = {openssl_hmac, &audit};
    grace.previous_steps = 1u;
    expect_failure(&PRIV_TOTP_PROFILE, &provider, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(120), "119246", 6u, &state, PRIV_TOTP_REJECTED);
    CHECK(priv_totp_verify(&grace, &provider, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(120), "119246", 6u, &state) == PRIV_TOTP_OK);
    CHECK(state.last_accepted_counter == UINT64_C(1)); /* Accepted previous, not now. */
    expect_failure(&grace, &provider, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(120), "119246", 6u, &state, PRIV_TOTP_REJECTED);
    CHECK(priv_totp_verify(&grace, &provider, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(120), "882438", 6u, &state) == PRIV_TOTP_OK);
    CHECK(state.last_accepted_counter == UINT64_C(2));
    expect_failure(&grace, &provider, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(60), "119246", 6u, &state, PRIV_TOTP_REJECTED); /* Clock rollback. */
    expect_failure(&grace, &provider, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(179), "882438", 6u, &state, PRIV_TOTP_REJECTED);
    expect_failure(&grace, &provider, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(180), "119246", 6u, &state, PRIV_TOTP_REJECTED); /* Two periods old. */
    expect_failure(&grace, &provider, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(60), "882438", 6u, &zero, PRIV_TOTP_REJECTED); /* Future period. */

    audit.calls = 0u;
    CHECK(priv_totp_verify(&grace, &provider, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(0), "920136", 6u, &zero) == PRIV_TOTP_OK);
    CHECK(zero.has_accepted == 1u && zero.last_accepted_counter == UINT64_C(0));
    CHECK(audit.calls == 1u && audit.last_counter == UINT64_C(0)); /* No underflow. */
    expect_failure(&grace, &provider, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(59), "920136", 6u, &zero, PRIV_TOTP_REJECTED);

    state.last_accepted_counter = UINT64_MAX;
    state.has_accepted = 1u;
    expect_failure(&grace, &provider, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_MAX, "539680", 6u, &state, PRIV_TOTP_REJECTED);
}

static void test_crypto_failures_and_complete_comparison(void)
{
    priv_totp_params grace = PRIV_TOTP_PROFILE;
    priv_totp_replay_state state = PRIV_TOTP_REPLAY_STATE_INIT;
    crypto_audit audit = {0u, 1u, UINT64_C(0)};
    priv_totp_crypto provider = {openssl_hmac, &audit};
    char output[9] = "sentinel";
    grace.previous_steps = 1u;
    CHECK(priv_totp_generate_profile(&provider, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(120), output, sizeof(output)) == PRIV_TOTP_CRYPTO_ERROR);
    CHECK(memcmp(output, "sentinel", sizeof(output)) == 0);
    audit.calls = 0u;
    expect_failure(&grace, &provider, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(120), "882438", 6u, &state, PRIV_TOTP_CRYPTO_ERROR);
    audit.calls = 0u; audit.fail_on_call = 2u;
    /* Current candidate matches, but second HMAC failure still rejects. */
    expect_failure(&grace, &provider, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(120), "882438", 6u, &state, PRIV_TOTP_CRYPTO_ERROR);
    CHECK(audit.calls == 2u);
    audit.calls = 0u; audit.fail_on_call = 0u;
    CHECK(priv_totp_verify(&grace, &provider, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(120), "882438", 6u, &state) == PRIV_TOTP_OK);
    CHECK(audit.calls == 2u);
    audit.calls = 0u;
    expect_failure(&grace, &provider, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(120), "000000", 6u, &state, PRIV_TOTP_REJECTED);
    CHECK(audit.calls == 2u);
}

static void test_real_adjacent_collision(void)
{
    /* Real HMAC-SHA256 collision after six-digit truncation, found using the
     * PUBLIC RFC seed. Both counters 398541 and 398542 produce 728233. */
    priv_totp_params grace = PRIV_TOTP_PROFILE;
    priv_totp_replay_state state = PRIV_TOTP_REPLAY_STATE_INIT;
    char code[PRIV_TOTP_OUTPUT_BYTES];
    grace.previous_steps = 1u;
    CHECK(priv_totp_generate_profile(&OPENSSL_CRYPTO, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(398541) * INT64_C(60), code, sizeof(code)) == PRIV_TOTP_OK);
    CHECK(strcmp(code, "728233") == 0);
    CHECK(priv_totp_generate_profile(&OPENSSL_CRYPTO, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(398542) * INT64_C(60), code, sizeof(code)) == PRIV_TOTP_OK);
    CHECK(strcmp(code, "728233") == 0);
    CHECK(priv_totp_verify(&grace, &OPENSSL_CRYPTO, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(398542) * INT64_C(60), code, 6u, &state) == PRIV_TOTP_OK);
    CHECK(state.last_accepted_counter == UINT64_C(398542)); /* Prefer newest. */
    state.last_accepted_counter = UINT64_C(398541);
    expect_failure(&grace, &OPENSSL_CRYPTO, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(398542) * INT64_C(60), code, 6u, &state, PRIV_TOTP_REJECTED);
}

static void test_bad_arguments_and_times(void)
{
    priv_totp_params params = PRIV_TOTP_PROFILE;
    priv_totp_replay_state state = PRIV_TOTP_REPLAY_STATE_INIT;
    priv_totp_crypto no_callback = {NULL, NULL};
    char output[9] = "sentinel";
    static const uint32_t bad_periods[] = {0u, 3601u, UINT32_MAX};
    static const uint32_t bad_digits[] = {0u, 5u, 9u, UINT32_MAX};
    size_t index;
    for (index = 0u; index < sizeof(bad_periods) / sizeof(bad_periods[0]); ++index) {
        params.period_seconds = bad_periods[index];
        expect_failure(&params, &OPENSSL_CRYPTO, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
            INT64_C(60), "119246", 6u, &state, PRIV_TOTP_BAD_ARGUMENT);
    }
    params = PRIV_TOTP_PROFILE;
    for (index = 0u; index < sizeof(bad_digits) / sizeof(bad_digits[0]); ++index) {
        params.digits = bad_digits[index];
        expect_failure(&params, &OPENSSL_CRYPTO, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
            INT64_C(60), "119246", 6u, &state, PRIV_TOTP_BAD_ARGUMENT);
    }
    params = PRIV_TOTP_PROFILE; params.previous_steps = 2u;
    expect_failure(&params, &OPENSSL_CRYPTO, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(60), "119246", 6u, &state, PRIV_TOTP_BAD_ARGUMENT);
    params = PRIV_TOTP_PROFILE; params.epoch_seconds = INT64_C(-1);
    expect_failure(&params, &OPENSSL_CRYPTO, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(60), "119246", 6u, &state, PRIV_TOTP_BAD_ARGUMENT);
    params = PRIV_TOTP_PROFILE; params.epoch_seconds = INT64_C(1000);
    expect_failure(&params, &OPENSSL_CRYPTO, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(999), "119246", 6u, &state, PRIV_TOTP_BAD_TIME);
    expect_failure(&PRIV_TOTP_PROFILE, &OPENSSL_CRYPTO, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_MIN, "119246", 6u, &state, PRIV_TOTP_BAD_TIME);
    expect_failure(NULL, &OPENSSL_CRYPTO, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(60), "119246", 6u, &state, PRIV_TOTP_BAD_ARGUMENT);
    expect_failure(&PRIV_TOTP_PROFILE, NULL, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(60), "119246", 6u, &state, PRIV_TOTP_BAD_ARGUMENT);
    expect_failure(&PRIV_TOTP_PROFILE, &no_callback, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(60), "119246", 6u, &state, PRIV_TOTP_BAD_ARGUMENT);
    expect_failure(&PRIV_TOTP_PROFILE, &OPENSSL_CRYPTO, NULL, sizeof(PUBLIC_RFC_KEY),
        INT64_C(60), "119246", 6u, &state, PRIV_TOTP_BAD_ARGUMENT);
    expect_failure(&PRIV_TOTP_PROFILE, &OPENSSL_CRYPTO, PUBLIC_RFC_KEY, 31u,
        INT64_C(60), "119246", 6u, &state, PRIV_TOTP_BAD_ARGUMENT);
    expect_failure(&PRIV_TOTP_PROFILE, &OPENSSL_CRYPTO, PUBLIC_RFC_KEY, 33u,
        INT64_C(60), "119246", 6u, &state, PRIV_TOTP_BAD_ARGUMENT);
    expect_failure(&PRIV_TOTP_PROFILE, &OPENSSL_CRYPTO, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(60), NULL, 6u, &state, PRIV_TOTP_BAD_ARGUMENT);
    CHECK(priv_totp_verify_profile(&OPENSSL_CRYPTO, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(60), "119246", 6u, NULL) == PRIV_TOTP_BAD_ARGUMENT);
    state.has_accepted = 2u;
    expect_failure(&PRIV_TOTP_PROFILE, &OPENSSL_CRYPTO, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(60), "119246", 6u, &state, PRIV_TOTP_BAD_ARGUMENT);
    state.has_accepted = 0u; state.last_accepted_counter = UINT64_C(1);
    expect_failure(&PRIV_TOTP_PROFILE, &OPENSSL_CRYPTO, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(60), "119246", 6u, &state, PRIV_TOTP_BAD_ARGUMENT);
    CHECK(priv_totp_generate_profile(&OPENSSL_CRYPTO, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(60), output, 6u) == PRIV_TOTP_BAD_ARGUMENT);
    CHECK(memcmp(output, "sentinel", sizeof(output)) == 0);
    CHECK(priv_totp_generate_profile(&OPENSSL_CRYPTO, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(-1), output, sizeof(output)) == PRIV_TOTP_BAD_TIME);
    CHECK(memcmp(output, "sentinel", sizeof(output)) == 0);
    CHECK(priv_totp_generate_profile(&OPENSSL_CRYPTO, PUBLIC_RFC_KEY, sizeof(PUBLIC_RFC_KEY),
        INT64_C(60), NULL, sizeof(output)) == PRIV_TOTP_BAD_ARGUMENT);
}

int main(void)
{
    test_rfc6238_sha256();
    test_profile_boundaries();
    test_strict_input();
    test_windows_replay_and_clock();
    test_crypto_failures_and_complete_comparison();
    test_real_adjacent_collision();
    test_bad_arguments_and_times();
    printf("PASS: 7 test groups, %u assertions; real OpenSSL HMAC-SHA256; public RFC data only.\n", assertions);
    return EXIT_SUCCESS;
}
