/* ponte_ap2.c: o mandato de compra do AP2 (CheckoutMandate, mandate.1). O checkout_jwt é um JWS compacto EdDSA (Ed25519)
 * assinado pela chave do nó: prova que o nó aceitou estes termos. Na volta, confere a assinatura e o checkout_hash. */
#include "watp.h"

#define WATP_VCT "mandate.checkout.1"

static void cabecalho_jws(Buf *o) {
    buf_str(o, "{\"alg\":\"EdDSA\",\"typ\":\"JWT\"}");   /* borda-json: cabeçalho do JWS (RFC 7515) */
}

int watp_ap2_mandato(const WatpEnv *e, const uint8_t semente[32], Buf *o) {
    Buf cab = {0}, pay = {0}, entrada = {0}, jwt = {0}, sig64 = {0};
    cabecalho_jws(&cab);
    buf_str(&pay, "{\"vct\":\"" WATP_VCT "\",\"negotiation_id\":");   /* borda-json: corpo do JWS */
    watp_texto_js(&pay, e->negociacao, strlen(e->negociacao));
    buf_str(&pay, ",\"checkout_hash\":"); watp_texto_js(&pay, e->hash, strlen(e->hash));
    buf_str(&pay, ",\"terms\":"); buf_str(&pay, e->payload);
    buf_str(&pay, "}");
    watp_b64url((const uint8_t *)cab.p, cab.n, &entrada);
    buf_str(&entrada, ".");
    watp_b64url((const uint8_t *)pay.p, pay.n, &entrada);
    uint8_t sig[64];
    if (watp_ed_sinal_bruto(semente, (const uint8_t *)entrada.p, entrada.n, sig)) {
        buf_free(&cab); buf_free(&pay); buf_free(&entrada);
        return WATP_ERR_ASSINATURA;
    }
    buf_add(&jwt, entrada.p, entrada.n);
    buf_str(&jwt, ".");
    watp_b64url(sig, 64, &jwt);
    buf_str(o, "{\"vct\":\"" WATP_VCT "\",\"checkout_jwt\":");   /* borda-json: AP2 CheckoutMandate */
    watp_texto_js(o, jwt.p, jwt.n);
    buf_str(o, ",\"checkout_hash\":"); watp_texto_js(o, e->hash, strlen(e->hash));
    buf_str(o, "}");
    buf_free(&cab); buf_free(&pay); buf_free(&entrada); buf_free(&jwt); buf_free(&sig64);
    return WATP_OK;
}

int watp_ap2_mandato_de(const char *s, size_t n, const uint8_t pub[32], char hash[65]) {
    JVal *m = json_ler(s, n);                                    /* borda-json: o mandato que chegou */
    const char *jwt = m ? json_texto_campo(m, "checkout_jwt") : NULL, *ch = m ? json_texto_campo(m, "checkout_hash") : NULL;
    if (!jwt || !ch) { if (m) json_livrar(m); return WATP_ERR_FORMA; }
    const char *p1 = strchr(jwt, '.');
    const char *p2 = p1 ? strchr(p1 + 1, '.') : NULL;
    if (!p2 || strchr(p2 + 1, '.')) { json_livrar(m); return WATP_ERR_FORMA; }
    size_t nentrada = (size_t)(p2 - jwt);
    Buf sig = {0}, pay = {0};
    if (watp_b64url_ler(p2 + 1, &sig) || sig.n != 64) { buf_free(&sig); json_livrar(m); return WATP_ERR_ASSINATURA; }
    if (!watp_ed_verificar_bruto(pub, (const uint8_t *)jwt, nentrada, (const uint8_t *)sig.p)) {
        buf_free(&sig); json_livrar(m); return WATP_ERR_ASSINATURA;
    }
    char pay64[1024];
    size_t npay = (size_t)(p2 - p1 - 1);
    if (npay >= sizeof pay64) { buf_free(&sig); json_livrar(m); return WATP_ERR_FORMA; }
    memcpy(pay64, p1 + 1, npay); pay64[npay] = 0;
    if (watp_b64url_ler(pay64, &pay)) { buf_free(&sig); json_livrar(m); return WATP_ERR_FORMA; }
    JVal *corpo = json_ler(pay.p, pay.n);                        /* borda-json: corpo do JWS */
    const char *dentro = corpo ? json_texto_campo(corpo, "checkout_hash") : NULL;
    int ok = dentro && !strcmp(dentro, ch) && strlen(ch) == 64;
    if (ok) snprintf(hash, 65, "%s", ch);
    if (corpo) json_livrar(corpo);
    buf_free(&sig); buf_free(&pay); json_livrar(m);
    return ok ? WATP_OK : WATP_ERR_ASSINATURA;
}
