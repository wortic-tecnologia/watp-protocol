/* ponte_acp.c: a negociação aceita vira uma CheckoutSession do ACP (versão 2026-04-17). Os valores vêm do mesmo leitor do UCP.
 * O link de continuação vai em links (type continue_url), a mesma convenção do UCP: o ACP não tem campo de pagamento próprio. */
#include "watp.h"

#define WATP_ACP_VERSAO "2026-04-17"

int watp_acp_para(const WatpEnv *e, Buf *o) {
    WatpCheckout d;
    if (watp_checkout_ler(e, &d)) return WATP_ERR_FORMA;
    int64_t cent, qt = (int64_t)d.qtd;
    if (watp_centavos(d.preco, &cent)) return WATP_ERR_FORMA;   /* preço com mais de 2 casas não entra */
    int64_t total = cent * qt;
    int pronto = !strcmp(e->tipo, "HASH_CONCLUDED");
    buf_str(o, "{\"id\":"); watp_texto_js(o, e->negociacao, strlen(e->negociacao));   /* borda-json: id da sessão */
    buf_fmt(o, ",\"protocol\":{\"version\":\"%s\"},\"capabilities\":{},", WATP_ACP_VERSAO);   /* borda-json: protocol.version */
    buf_str(o, pronto ? "\"status\":\"ready_for_payment\"" : "\"status\":\"not_ready_for_payment\"");
    buf_str(o, ",\"currency\":"); watp_texto_js(o, d.moeda, strlen(d.moeda));
    buf_str(o, ",\"line_items\":[{\"id\":"); watp_texto_js(o, d.sku, strlen(d.sku));
    buf_str(o, ",\"title\":"); watp_texto_js(o, d.item, strlen(d.item));
    buf_fmt(o, ",\"quantity\":%lld,\"unit_price\":%lld}],", (long long)qt, (long long)cent);
    buf_str(o, "\"fulfillment_details\":null,\"selected_fulfillment_options\":[],");
    buf_fmt(o, "\"totals\":[{\"type\":\"total\",\"display_text\":\"Total\",\"amount\":%lld}],", (long long)total);
    buf_str(o, "\"fulfillment_options\":[],\"marketing_consent_options\":[],\"messages\":[],\"links\":[");
    if (d.link[0]) { buf_str(o, "{\"type\":\"continue_url\",\"url\":"); watp_texto_js(o, d.link, strlen(d.link)); buf_str(o, "}"); }
    buf_str(o, "]}");
    return WATP_OK;
}
