/* ponte_a2a.c: o envelope WATP como mensagem A2A v1.0 (Message, com a parte de dados e o bloco watp nos metadados).
 * A volta monta de novo o envelope e passa pela mesma validação de borda: o hash tem de continuar valendo. */
#include "watp.h"

static void campo_texto(Buf *o, const char *k, const char *v) {   /* borda-json: "k":"v" ou "k":null */
    buf_fmt(o, "\"%s\":", k);
    if (v) watp_texto_js(o, v, strlen(v)); else buf_str(o, "null");   /* borda-json: texto escapado */
}

int watp_a2a_para(const WatpEnv *e, Buf *o) {
    buf_str(o, "{");
    campo_texto(o, "messageId", e->id); buf_str(o, ",");
    campo_texto(o, "contextId", e->negociacao); buf_str(o, ",");
    buf_str(o, "\"taskId\":null,\"role\":\"ROLE_AGENT\",\"parts\":[{\"data\":");   /* borda-json: Part.data */
    buf_str(o, e->payload);
    buf_str(o, ",\"mediaType\":\"application/json\"}],\"metadata\":{\"watp\":{");   /* borda-json: Part.mediaType */
    campo_texto(o, "type", e->tipo); buf_str(o, ",");
    campo_texto(o, "negotiation_id", e->negociacao); buf_str(o, ",");
    campo_texto(o, "hash", e->hash); buf_str(o, ",");
    campo_texto(o, "previousHash", e->anterior); buf_str(o, ",");
    campo_texto(o, "timestamp", e->momento); buf_str(o, ",");
    campo_texto(o, "sender", e->remetente);
    if (e->assinatura) { buf_str(o, ","); campo_texto(o, "signature", e->assinatura); }
    buf_str(o, "}}}");   /* borda-json: fecha watp, metadata e a Message */
    return WATP_OK;
}

static const char *texto(const JVal *o, const char *k) { return json_texto_campo(o, k); }

int watp_a2a_de(const char *s, size_t n, WatpEnv *e, char *erro, size_t ne) {
    JVal *m = json_ler(s, n);                                       /* borda-json: a Message que chegou de outro agente */
    if (!m || m->t != J_OBJETO) { json_livrar(m); if (erro && ne) snprintf(erro, ne, "Message A2A inválida"); return WATP_ERR_FORMA; }
    const JVal *partes = json_campo(m, "parts"), *meta = json_campo(m, "metadata");
    const JVal *w = meta && meta->t == J_OBJETO ? json_campo(meta, "watp") : NULL;
    const JVal *dados = partes && partes->t == J_LISTA && partes->n > 0 && partes->itens[0]->t == J_OBJETO ? json_campo(partes->itens[0], "data") : NULL;
    const char *id = texto(m, "messageId");
    if (!w || w->t != J_OBJETO || !dados || !id) {
        json_livrar(m);
        if (erro && ne) snprintf(erro, ne, "a Message A2A não traz messageId, a parte de dados e o bloco watp");
        return WATP_ERR_FORMA;
    }
    Buf payload = {0}, env = {0};
    watp_canon(&payload, dados);
    buf_str(&env, "{"); campo_texto(&env, "id", id); buf_str(&env, ",");
    campo_texto(&env, "negotiation_id", texto(w, "negotiation_id")); buf_str(&env, ",");
    campo_texto(&env, "type", texto(w, "type")); buf_str(&env, ",");
    buf_str(&env, "\"payload\":"); buf_str(&env, payload.p);
    buf_str(&env, ","); campo_texto(&env, "hash", texto(w, "hash")); buf_str(&env, ",");
    campo_texto(&env, "previousHash", texto(w, "previousHash")); buf_str(&env, ",");
    campo_texto(&env, "timestamp", texto(w, "timestamp")); buf_str(&env, ",");
    campo_texto(&env, "sender", texto(w, "sender"));
    if (texto(w, "signature")) { buf_str(&env, ","); campo_texto(&env, "signature", texto(w, "signature")); }
    buf_str(&env, "}");
    int r = watp_env_de_borda(env.p, env.n, e, erro, ne);
    buf_free(&payload); buf_free(&env);
    json_livrar(m);
    return r;
}
