/* envelope.c: o envelope WATP na borda (texto de fora) e o registro C. Entrada aceita os aliases do runtime antigo
 * (prev_hash; quantity e price dentro do payload). O payload fica exatamente como chegou: é o que foi hasheado,
 * e mudar uma letra dele muda o hash. Saída: só os nomes canônicos do envelope. */
#include "watp.h"

static char *dup(const char *s) {
    size_t n = strlen(s);
    char *c = w_malloc(n + 1);
    memcpy(c, s, n + 1);
    return c;
}

static const JVal *cp(const JVal *o, const char *k) { return json_campo(o, k); }   /* borda-json: campo do texto de entrada */

static int texto_de(const JVal *v, char **out) {
    if (!v || v->t != J_TEXTO) { *out = NULL; return 0; }
    char *c = w_malloc(v->n + 1);
    memcpy(c, v->s ? v->s : "", v->n);
    c[v->n] = 0;
    *out = c;
    return 1;
}

void watp_env_livrar(WatpEnv *e) {
    if (!e) return;
    free(e->id); free(e->negociacao); free(e->tipo); free(e->hash); free(e->anterior);
    free(e->momento); free(e->remetente); free(e->payload); free(e->assinatura);
    memset(e, 0, sizeof *e);
}

int watp_env_copiar(WatpEnv *d, const WatpEnv *s) {
    memset(d, 0, sizeof *d);
    d->id = dup(s->id); d->negociacao = dup(s->negociacao); d->tipo = dup(s->tipo); d->hash = dup(s->hash);
    d->anterior = s->anterior ? dup(s->anterior) : NULL;
    d->momento = dup(s->momento); d->remetente = dup(s->remetente); d->payload = dup(s->payload);
    d->assinatura = s->assinatura ? dup(s->assinatura) : NULL;
    return WATP_OK;
}

int watp_env_igual(const WatpEnv *a, const WatpEnv *b) {
    if (strcmp(a->id, b->id) || strcmp(a->negociacao, b->negociacao) || strcmp(a->tipo, b->tipo) ||
        strcmp(a->hash, b->hash) || strcmp(a->momento, b->momento) || strcmp(a->remetente, b->remetente) ||
        strcmp(a->payload, b->payload)) return 0;
    if ((a->anterior == NULL) != (b->anterior == NULL) || (a->anterior && strcmp(a->anterior, b->anterior))) return 0;
    return 1;
}

/* borda-json: lê o texto de entrada, valida pelas regras do contrato e preenche o registro C */
int watp_env_de_borda(const char *s, size_t n, WatpEnv *e, char *erro, size_t ne) {
    memset(e, 0, sizeof *e);
    JVal *r = json_ler(s, n);                                  /* borda-json: leitor do texto da borda */
    if (!r) { if (erro && ne) snprintf(erro, ne, "texto de borda inválido"); return WATP_ERR_FORMA; }
    int c = watp_valida(r, erro, ne);
    if (c) { json_livrar(r); return c; }
    texto_de(cp(r, "id"), &e->id);
    texto_de(cp(r, "type"), &e->tipo);
    texto_de(cp(r, "hash"), &e->hash);
    texto_de(cp(r, "timestamp"), &e->momento);
    texto_de(cp(r, "sender"), &e->remetente);
    if (!texto_de(cp(r, "negotiation_id"), &e->negociacao)) e->negociacao = dup(e->hash);   /* INTEREST sem id: a própria mensagem abre a negociação */
    const JVal *pv = cp(r, "previousHash"), *pvp = cp(r, "prev_hash");
    const JVal *ant = (pv && pv->t != J_NULO) ? pv : ((pvp && pvp->t != J_NULO) ? pvp : NULL);
    if (ant && ant->t != J_TEXTO) { json_livrar(r); watp_env_livrar(e); return WATP_ERR_FORMA; }   /* borda-json: previousHash é texto */
    if (ant) texto_de(ant, &e->anterior);
    Buf canon = {0};
    watp_canon(&canon, cp(r, "payload"));                      /* borda-json: payload na ordem de chegada */
    e->payload = canon.p ? canon.p : dup("{}");
    if (!canon.p) { /* payload vazio nunca passa a validação; cópia de segurança */ }
    texto_de(cp(r, "signature"), &e->assinatura);
    json_livrar(r);
    return WATP_OK;
}

/* borda-json: a saída com os nomes canônicos; o payload sai exatamente como foi hasheado */
int watp_env_para_borda(const WatpEnv *e, Buf *o) {
    buf_str(o, "{\"id\":");
    watp_texto_js(o, e->id, strlen(e->id));                    /* borda-json: id */
    buf_str(o, ",\"negotiation_id\":");
    watp_texto_js(o, e->negociacao, strlen(e->negociacao));    /* borda-json: negotiation_id */
    buf_str(o, ",\"type\":");
    watp_texto_js(o, e->tipo, strlen(e->tipo));                /* borda-json: type */
    buf_str(o, ",\"payload\":"); buf_str(o, e->payload);       /* borda-json: payload */
    buf_str(o, ",\"hash\":");
    watp_texto_js(o, e->hash, strlen(e->hash));                /* borda-json: hash */
    buf_str(o, ",\"previousHash\":");                          /* borda-json: previousHash */
    if (e->anterior) watp_texto_js(o, e->anterior, strlen(e->anterior)); else buf_str(o, "null");
    buf_str(o, ",\"timestamp\":");
    watp_texto_js(o, e->momento, strlen(e->momento));          /* borda-json: timestamp */
    buf_str(o, ",\"sender\":");
    watp_texto_js(o, e->remetente, strlen(e->remetente));      /* borda-json: sender */
    if (e->assinatura) { buf_str(o, ",\"signature\":"); watp_texto_js(o, e->assinatura, strlen(e->assinatura)); }   /* borda-json: signature */
    buf_str(o, "}");
    return WATP_OK;
}
