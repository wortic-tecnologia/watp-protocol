/* ponte_x402.c: o desafio HTTP 402 do x402 (PaymentRequired, versão 2) para uma negociação aceita.
 * Sem destino de recebimento (pay_to vazio) não há desafio: o contrato não tem carteira; a ponte fica pronta e desligada.
 * O valor sai nas unidades atômicas do ativo escolhido por quem configura (a conversão de moeda não existe no runtime). */
#include "watp.h"

int watp_x402_desafio(const WatpEnv *e, const char *pay_to, const char *rede, const char *ativo, const char *valor, Buf *o) {
    if (!pay_to || !*pay_to || !rede || !*rede || !ativo || !*ativo) return WATP_ERR_SEM_DESTINO;
    if (!valor || !*valor) return WATP_ERR_SEM_DESTINO;
    for (const char *p = valor; *p; p++) if (!isdigit((unsigned char)*p)) return WATP_ERR_FORMA;
    buf_str(o, "{\"x402Version\":2,\"error\":\"PAYMENT-SIGNATURE header is required\",");   /* borda-json: x402 v2 */
    buf_str(o, "\"resource\":{\"description\":\"negociação WATP\",\"mimeType\":\"application/json\"},\"accepts\":[{");
    buf_str(o, "\"scheme\":\"exact\",\"network\":"); watp_texto_js(o, rede, strlen(rede));
    buf_str(o, ",\"amount\":"); watp_texto_js(o, valor, strlen(valor));
    buf_str(o, ",\"asset\":"); watp_texto_js(o, ativo, strlen(ativo));
    buf_str(o, ",\"payTo\":"); watp_texto_js(o, pay_to, strlen(pay_to));
    buf_str(o, ",\"maxTimeoutSeconds\":600,\"extra\":{\"watp_hash\":");
    watp_texto_js(o, e->hash, strlen(e->hash));
    buf_str(o, ",\"negotiation_id\":"); watp_texto_js(o, e->negociacao, strlen(e->negociacao));
    buf_str(o, "}}]}");
    return WATP_OK;
}

int watp_x402_valor(const char *s, size_t n, char *valor, size_t nvalor) {
    JVal *d = json_ler(s, n);                                 /* borda-json: o desafio que chegou */
    const JVal *acc = d ? json_campo(d, "accepts") : NULL;
    const char *amt = acc && acc->t == J_LISTA && acc->n > 0 && acc->itens[0]->t == J_OBJETO ? json_texto_campo(acc->itens[0], "amount") : NULL;
    int r = amt ? WATP_OK : WATP_ERR_FORMA;
    if (amt) snprintf(valor, nvalor, "%s", amt);
    if (d) json_livrar(d);
    return r;
}
