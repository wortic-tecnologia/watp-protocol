/* ponte_mpp.c: o desafio do MPP (draft-httpauth-payment-01, cabeçalho Payment). O id do desafio é o hash aceito, e o campo
 * request leva, em base64 de URL, o pedido de cobrança: moeda, centavos, destinatário e negociação. Sem destinatário, não há desafio.
 * O método é "tempo" (o trilho que o levantamento cita para o MPP); o runtime não tem outro trilho configurado. */
#include "watp.h"
#include "wsha256.h"

#define WATP_MPP_REALM "watp"

int watp_mpp_desafio(const WatpEnv *e, const char *destino, const char *moeda, Buf *o) {
    if (!destino || !*destino) return WATP_ERR_SEM_DESTINO;
    WatpCheckout d;
    if (watp_checkout_ler(e, &d)) return WATP_ERR_FORMA;
    int64_t cent, qt = (int64_t)d.qtd;
    if (watp_centavos(d.preco, &cent)) return WATP_ERR_FORMA;
    Buf req = {0}, b64 = {0};
    buf_str(&req, "{\"currency\":"); watp_texto_js(&req, moeda && *moeda ? moeda : d.moeda, strlen(moeda && *moeda ? moeda : d.moeda));   /* borda-json: pedido de cobrança */
    buf_fmt(&req, ",\"amountMinor\":%lld,\"quantity\":%lld", (long long)(cent * qt), (long long)qt);
    buf_str(&req, ",\"recipient\":"); watp_texto_js(&req, destino, strlen(destino));
    buf_str(&req, ",\"negotiation_id\":"); watp_texto_js(&req, e->negociacao, strlen(e->negociacao));
    buf_str(&req, "}");
    watp_b64url((const uint8_t *)req.p, req.n, &b64);
    buf_str(o, "{\"id\":"); watp_texto_js(o, e->hash, strlen(e->hash));
    buf_str(o, ",\"realm\":\"" WATP_MPP_REALM "\",\"method\":\"tempo\",\"intent\":\"charge\",\"request\":");
    watp_texto_js(o, b64.p, b64.n);
    buf_str(o, "}");
    buf_free(&req); buf_free(&b64);
    return WATP_OK;
}
