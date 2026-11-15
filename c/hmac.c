/* hmac.c: a assinatura legada do nó. O cabeçalho X-WATP-Node-Signature é sha256=hex(HMAC-SHA256(segredo, corpo bruto)).
 * Vetor compartilhado com os SDKs (TypeScript, Python, Go): HMAC("{}", "secret-a") = e94d4c3a...97411. */
#include "watp.h"
#include "wsha256.h"

void whmac_sha256(const uint8_t *chave, size_t nchave, const void *d, size_t n, uint8_t out[32]);   /* lib/whmac.c */
int wmemcmp_constante(const void *a, const void *b, size_t n);                                     /* lib/whmac.c: 0 = iguais */

static int segredo_vazio(const char *s) {
    if (!s) return 1;
    for (const char *p = s; *p; p++) if (!isspace((unsigned char)*p)) return 0;
    return 1;
}

int watp_hmac_assinar(const char *segredo, const char *corpo, size_t ncorpo, Buf *o) {
    if (segredo_vazio(segredo)) return WATP_ERR_ASSINATURA;
    uint8_t m[32]; char hex[65];
    whmac_sha256((const uint8_t *)segredo, strlen(segredo), corpo, ncorpo, m);
    wsha256_hex(m, hex);
    buf_str(o, "sha256=");
    buf_str(o, hex);
    return WATP_OK;
}

int watp_hmac_conferir(const char *segredo, const char *corpo, size_t ncorpo, const char *assinatura) {
    if (!assinatura || !*assinatura || segredo_vazio(segredo)) return 0;
    Buf esperada = {0};
    watp_hmac_assinar(segredo, corpo, ncorpo, &esperada);
    int ok = esperada.n == strlen(assinatura) && !wmemcmp_constante(esperada.p, assinatura, esperada.n);   /* tempo constante */
    buf_free(&esperada);
    return ok;
}
