/* ponte_otel.c: cada mensagem WATP vira um span OTLP (formato JSON do OpenTelemetry, otlp-1). O traceId é a negociação
 * (os 32 primeiros hex do SHA-256 do id da negociação), o spanId é o hash da mensagem (16 hex), e o pai é o hash anterior.
 * Os atributos levam o hash, o tipo, o remetente e a negociação, para reconstruir a prova do lado de quem observa. */
#include "watp.h"
#include "wsha256.h"

static const JVal *atributo(const JVal *attrs, const char *chave) {
    if (!attrs || attrs->t != J_LISTA) return NULL;
    for (size_t i = 0; i < attrs->n; i++) {
        const char *k = json_texto_campo(attrs->itens[i], "key");
        if (k && !strcmp(k, chave)) return json_campo(attrs->itens[i], "value");
    }
    return NULL;
}

static void atrib(Buf *o, const char *chave, const char *valor, int virgula) {
    if (virgula) buf_str(o, ",");
    buf_str(o, "{\"key\":"); watp_texto_js(o, chave, strlen(chave));                 /* borda-json: KeyValue */
    buf_str(o, ",\"value\":{\"stringValue\":"); watp_texto_js(o, valor, strlen(valor));
    buf_str(o, "}}");
}

int watp_otel_span(const WatpEnv *e, Buf *o) {
    int64_t ns;
    if (watp_iso_ns(e->momento, &ns)) return WATP_ERR_FORMA;
    uint8_t d[32]; char hex[65], trace[33], span[17], pai[17];
    WSha256 s;
    wsha256_inicio(&s); wsha256_mais(&s, e->negociacao, strlen(e->negociacao)); wsha256_fim(&s, d);
    wsha256_hex(d, hex);
    memcpy(trace, hex, 32); trace[32] = 0;
    memcpy(span, e->hash, 16); span[16] = 0;
    buf_str(o, "{\"traceId\":"); watp_texto_js(o, trace, 32);                       /* borda-json: OTLP traceId */
    buf_str(o, ",\"spanId\":"); watp_texto_js(o, span, 16);                          /* borda-json: OTLP spanId */
    if (e->anterior) { memcpy(pai, e->anterior, 16); pai[16] = 0; buf_str(o, ",\"parentSpanId\":"); watp_texto_js(o, pai, 16); }
    buf_str(o, ",\"name\":"); {
        Buf nome = {0};
        buf_str(&nome, "watp."); buf_str(&nome, e->tipo);
        watp_texto_js(o, nome.p, nome.n);
        buf_free(&nome);
    }
    char ini[32];
    snprintf(ini, sizeof ini, "%lld", (long long)ns);
    buf_fmt(o, ",\"startTimeUnixNano\":\"%s\",\"endTimeUnixNano\":\"%s\",\"kind\":1,\"attributes\":[", ini, ini);   /* borda-json: 64 bits como texto */
    atrib(o, "watp.hash", e->hash, 0);
    atrib(o, "watp.negotiation_id", e->negociacao, 1);
    atrib(o, "watp.type", e->tipo, 1);
    atrib(o, "watp.sender", e->remetente, 1);
    buf_str(o, "]}");
    return WATP_OK;
}

int watp_otel_de(const char *s, size_t n, char trace[33], char span[17], char nome[64], char hash[65]) {
    JVal *sp = json_ler(s, n);                                 /* borda-json: span que chegou */
    if (!sp || sp->t != J_OBJETO) { if (sp) json_livrar(sp); return WATP_ERR_FORMA; }
    const char *t = json_texto_campo(sp, "traceId"), *sid = json_texto_campo(sp, "spanId"), *nm = json_texto_campo(sp, "name");
    const JVal *v = atributo(json_campo(sp, "attributes"), "watp.hash");
    const char *h = v && v->t == J_OBJETO ? json_texto_campo(v, "stringValue") : NULL;
    int ok = t && sid && nm && h && strlen(t) == 32 && strlen(sid) == 16 && strlen(h) == 64 && !strncmp(h, sid, 16);
    if (ok) {
        snprintf(trace, 33, "%s", t);
        snprintf(span, 17, "%s", sid);
        snprintf(nome, 64, "%s", nm);
        snprintf(hash, 65, "%s", h);
    }
    json_livrar(sp);
    return ok ? WATP_OK : WATP_ERR_FORMA;
}
