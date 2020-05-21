#include <rand.h>
#include <rsa.h>
#include <string.h>
#ifdef USE_MBEDCRYPTO
#include <mbedtls/rsa.h>

static int rnd(void *ctx, unsigned char *buf, size_t n) {
  (void)ctx;
  random_buffer(buf, n);
  return 0;
}
#endif

static int pkcs1_v15_add_padding(const void *in, uint16_t in_len, uint8_t *out, uint16_t out_len) {
  if (out_len < 11 || in_len > out_len - 11) return -1;
  uint16_t pad_size = out_len - in_len - 3;
  memcpy(out + pad_size + 3, in, in_len);
  out[0] = 0x00;
  out[1] = 0x01;
  memset(out + 2, 0xFF, pad_size);
  out[2 + pad_size] = 0x00;
  return 0;
}

static int pkcs1_v15_remove_padding(const uint8_t *in, uint16_t in_len, uint8_t *out) {
  if (in_len < 11) return -1;
  if (in[0] != 0x00 || in[1] != 0x02) return -1;
  uint16_t i;
  for (i = 2; i < in_len; ++i)
    if (in[i] == 0x00) break;
  if (i == in_len || i - 2 < 8) return -1;
  memmove(out, in + i + 1, in_len - (i + 1));
  return in_len - (i + 1);
}

__attribute__((weak)) int rsa_generate_key(rsa_key_t *key, uint16_t nbits) {
#ifdef USE_MBEDCRYPTO
  mbedtls_rsa_context rsa;
  mbedtls_rsa_init(&rsa, MBEDTLS_RSA_PKCS_V15, 0);
  if (mbedtls_rsa_gen_key(&rsa, rnd, NULL, nbits, 65537) < 0) return -1;
  key->nbits = nbits;
  int pq_len = nbits / 16;
  if (mbedtls_rsa_export_raw(&rsa, NULL, 0, key->p, pq_len, key->q, pq_len, NULL, 0, key->e, 4) < 0) return -1;
#else
  (void)key;
#endif
  return 0;
}

__attribute__((weak)) int rsa_get_public_key(rsa_key_t *key, uint8_t *n) {
#ifdef USE_MBEDCRYPTO
  mbedtls_rsa_context rsa;
  mbedtls_rsa_init(&rsa, MBEDTLS_RSA_PKCS_V15, 0);
  int pq_len = key->nbits / 16;
  if (mbedtls_rsa_import_raw(&rsa, NULL, 0, key->p, pq_len, key->q, pq_len, NULL, 0, key->e, 4) < 0) return -1;
  if (mbedtls_rsa_complete(&rsa) < 0) return -1;
  if (mbedtls_rsa_export_raw(&rsa, n, pq_len * 2, NULL, 0, NULL, 0, NULL, 0, NULL, 0) < 0) return -1;
#else
  (void)key;
#endif
  return 0;
}

__attribute__((weak)) int rsa_private(rsa_key_t *key, const uint8_t *input, uint8_t *output) {
#ifdef USE_MBEDCRYPTO
  mbedtls_rsa_context rsa;
  mbedtls_rsa_init(&rsa, MBEDTLS_RSA_PKCS_V15, 0);
  int pq_len = key->nbits / 16;
  if (mbedtls_rsa_import_raw(&rsa, NULL, 0, key->p, pq_len, key->q, pq_len, NULL, 0, key->e, 4) < 0) return -1;
  if (mbedtls_rsa_complete(&rsa) < 0) return -1;
  if (mbedtls_rsa_private(&rsa, rnd, NULL, input, output) < 0) return -1;
#else
  (void)key;
  (void)input;
  (void)output;
#endif
  return 0;
}

int rsa_sign_pkcs_v15(rsa_key_t *key, const uint8_t *data, size_t len, uint8_t *sig) {
  if (pkcs1_v15_add_padding(data, len, sig, key->nbits / 8) < 0) return -1;
  return rsa_private(key, sig, sig);
}

int rsa_decrypt_pkcs_v15(rsa_key_t *key, const uint8_t *in, size_t *olen, uint8_t *out) {
  if (rsa_private(key, in, out) < 0) return -1;
  int len = pkcs1_v15_remove_padding(out, key->nbits / 8, out);
  if (len < 0) return -1;
  *olen = len;
  return 0;
}
