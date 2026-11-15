/* ponte_ucp.c: a negociação aceita vira um checkout UCP (perfil ucp, versão 2026-08-25): line_items em centavos inteiros,
 * totais i64 e o link de continuação (o comprador paga na página). Também lê o resumo de volta (sku, quantidade, preço, moeda, link). */
#include "watp.h"

#define WATP_UCP_VERSAO "2026-08-25"

/* o que o checkout precisa do payload guardado (texto canônico). Quantidade e preço têm de ser números */
int watp_checkout_ler(const WatpEnv *e, WatpCheckout *d) {
    memset(d, 0, sizeof *d);
    JVal *p = json_ler(e->payload, strlen(e->payload));   /* borda-json: o payload guardado (texto canônico) */
    if (!p) return WATP_ERR_FORMA;
    const JVal *q = json_campo(p, "qty"), *pr = json_campo(p, "unit_price");
    if (!q || q->t == J_NULO) q = json_campo(p, "quantity");
    if (!pr || pr->t == J_NULO) pr = json_campo(p, "price");
    int ok = q && q->t == J_NUM && pr && pr->t == J_NUM;
    if (ok) {
        const char *s = json_texto_campo(p, "sku"), *it = json_texto_campo(p, "item"), *m = json_texto_campo(p, "currency");
        const char *l = json_texto_campo(p, "payment_url");
        snprintf(d->sku, sizeof d->sku, "%s", s ? s : "");
        snprintf(d->item, sizeof d->item, "%s", it ? it : (s ? s : "item"));
        snprintf(d->moeda, sizeof d->moeda, "%s", m && strlen(m) == 3 ? m : "BRL");
        snprintf(d->link, sizeof d->link, "%s", l ? l : "");
        d->qtd = q->num;
        d->preco = pr->num;
    }
    json_livrar(p);
    return ok ? WATP_OK : WATP_ERR_FORMA;
}

int watp_ucp_para(const WatpEnv *e, Buf *o) {
    WatpCheckout d;
    if (watp_checkout_ler(e, &d)) return WATP_ERR_FORMA;
    int64_t cent, qt = (int64_t)d.qtd;
    if (watp_centavos(d.preco, &cent)) return WATP_ERR_FORMA;   /* preço com mais de 2 casas não entra */
    int64_t total = cent * qt;
    int fechado = !strcmp(e->tipo, "HASH_CONCLUDED");
    buf_fmt(o, "{\"ucp\":{\"version\":\"%s\"},", WATP_UCP_VERSAO);   /* borda-json: UCP, versão do perfil */
    buf_str(o, "\"id\":"); watp_texto_js(o, e->negociacao, strlen(e->negociacao));   /* borda-json: id do checkout */
    buf_str(o, fechado ? ",\"status\":\"ready_for_complete\"" : ",\"status\":\"in_progress\"");
    buf_str(o, ",\"currency\":"); watp_texto_js(o, d.moeda, strlen(d.moeda));
    buf_str(o, ",\"line_items\":[{\"id\":"); watp_texto_js(o, d.sku, strlen(d.sku));
    buf_str(o, ",\"item\":{\"id\":"); watp_texto_js(o, d.sku, strlen(d.sku));
    buf_str(o, ",\"title\":"); watp_texto_js(o, d.item, strlen(d.item));
    buf_fmt(o, ",\"price\":%lld},\"quantity\":%lld,", (long long)cent, (long long)qt);
    buf_fmt(o, "\"totals\":[{\"type\":\"subtotal\",\"amount\":%lld}]}],", (long long)total);
    buf_fmt(o, "\"totals\":[{\"type\":\"total\",\"amount\":%lld}],", (long long)total);
    buf_str(o, "\"links\":[");
    if (d.link[0]) { buf_str(o, "{\"type\":\"continue_url\",\"url\":"); watp_texto_js(o, d.link, strlen(d.link)); buf_str(o, "}"); }
    buf_str(o, "]}");
    return WATP_OK;
}

/* o resumo do checkout (UCP ou ACP): aceita o preço nos dois formatos (item.price do UCP, unit_price do ACP) */
int watp_checkout_resumo(const char *s, size_t n, char sku[128], int64_t *qtd, int64_t *centavos, char moeda[4], char *link, size_t nlink) {
    JVal *c = json_ler(s, n);                                  /* borda-json: o checkout que voltou do protocolo */
    if (!c || c->t != J_OBJETO) { if (c) json_livrar(c); return WATP_ERR_FORMA; }
    const JVal *li = json_campo(c, "line_items");
    if (!li || li->t != J_LISTA || li->n < 1 || li->itens[0]->t != J_OBJETO) { json_livrar(c); return WATP_ERR_FORMA; }
    const JVal *it = li->itens[0];
    const char *id = json_texto_campo(it, "id"), *cur = json_texto_campo(c, "currency");
    const JVal *q = json_campo(it, "quantity");
    const JVal *pr = json_campo(it, "unit_price");
    if (!pr) { const JVal *item = json_campo(it, "item"); if (item && item->t == J_OBJETO) pr = json_campo(item, "price"); }
    if (!id || !q || q->t != J_NUM || !pr || pr->t != J_NUM) { json_livrar(c); return WATP_ERR_FORMA; }
    snprintf(sku, 128, "%s", id);
    *qtd = (int64_t)q->num;
    *centavos = (int64_t)pr->num;
    snprintf(moeda, 4, "%s", cur ? cur : "");
    const JVal *links = json_campo(c, "links");
    const char *u = links && links->t == J_LISTA && links->n > 0 && links->itens[0]->t == J_OBJETO ? json_texto_campo(links->itens[0], "url") : NULL;
    snprintf(link, nlink, "%s", u ? u : "");
    json_livrar(c);
    return WATP_OK;
}
