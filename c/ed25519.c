/* ed25519.c: a assinatura forte por nó (evolução do HMAC legado; o contrato pede identidade e assinatura por emissor).
 * Ed25519 pela libcrypto do OpenSSL, como a casa já usa no whttpsig. A mensagem assinada amarra o remetente ao hash:
 * "WATP-ED25519-V1\n" + remetente + "\n" + hash. Quem troca de remetente não aproveita a assinatura. */
#include "watp.h"
#include <openssl/evp.h>
#include <openssl/rand.h>

static EVP_PKEY *chave_privada(const uint8_t s[32]) { return EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, s, 32); }

int watp_ed_gerar(uint8_t semente[32], uint8_t pub[32]) {
    if (RAND_bytes(semente, 32) != 1) return WATP_ERR_ASSINATURA;
    EVP_PKEY *k = chave_privada(semente);
    size_t np = 32;
    int ok = k && EVP_PKEY_get_raw_public_key(k, pub, &np) == 1 && np == 32;
    EVP_PKEY_free(k);
    return ok ? WATP_OK : WATP_ERR_ASSINATURA;
}

/* o que vai assinado: texto fixo, remetente e hash (com \n entre eles, para não haver ambiguidade) */
static void mensagem(Buf *b, const char *rem, const char *hash) {
    buf_str(b, "WATP-ED25519-V1\n");
    buf_str(b, rem);
    buf_str(b, "\n");
    buf_str(b, hash);
}

/* assina um texto cru (usado pelos testes com os vetores da RFC 8032) */
int watp_ed_sinal_bruto(const uint8_t semente[32], const uint8_t *m, size_t nm, uint8_t sig[64]) {
    EVP_PKEY *k = chave_privada(semente);
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    size_t sl = 64;
    int ok = k && ctx && EVP_DigestSignInit(ctx, NULL, NULL, NULL, k) == 1 && EVP_DigestSign(ctx, sig, &sl, m, nm) == 1 && sl == 64;
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(k);
    return ok ? WATP_OK : WATP_ERR_ASSINATURA;
}

int watp_ed_assinar(const uint8_t semente[32], const char *remetente, const char *hash, char out[137]) {
    Buf b = {0};
    mensagem(&b, remetente, hash);
    uint8_t sig[64];
    int r = watp_ed_sinal_bruto(semente, (const uint8_t *)b.p, b.n, sig);
    buf_free(&b);
    if (r) return r;
    memcpy(out, "ed25519=", 8);
    for (int i = 0; i < 64; i++) snprintf(out + 8 + 2 * i, 3, "%02x", sig[i]);
    return WATP_OK;
}

int watp_ed_verificar_bruto(const uint8_t pub[32], const uint8_t *m, size_t nm, const uint8_t sig[64]) {
    EVP_PKEY *k = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, pub, 32);
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    int ok = k && ctx && EVP_DigestVerifyInit(ctx, NULL, NULL, NULL, k) == 1 && EVP_DigestVerify(ctx, sig, 64, m, nm) == 1;
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(k);
    return ok;
}

int watp_ed_conferir(const uint8_t pub[32], const char *remetente, const char *hash, const char *assinatura) {
    if (!assinatura || strncmp(assinatura, "ed25519=", 8) || strlen(assinatura) != 136) return 0;
    uint8_t sig[64];
    for (int i = 0; i < 64; i++) {
        unsigned v;
        if (sscanf(assinatura + 8 + 2 * i, "%2x", &v) != 1) return 0;
        sig[i] = (uint8_t)v;
    }
    Buf b = {0};
    mensagem(&b, remetente, hash);
    int ok = watp_ed_verificar_bruto(pub, (const uint8_t *)b.p, b.n, sig);
    buf_free(&b);
    return ok;
}
