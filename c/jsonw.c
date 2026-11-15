/* jsonw.c: o razão do WATP em jsonW. Um arquivo (razao.jsw) com uma linha por mensagem aceita, no esquema abaixo.
 * Dentro do disco não há JSON: o payload é guardado como o texto canônico (o mesmo byte que foi hasheado).
 * Na subida, o razão é relido e reconstrói a idempotência e a cadeia (watp_estado_aplicar, sem checar de novo). */
#include "watp.h"
#include "wjsw.h"
#include <sys/stat.h>

#define WATP_ESQUEMA_RAZAO "id:texto,negociacao:texto,tipo:texto,hash:codigo,anterior:texto?,momento:texto,remetente:texto,payload:texto,assinatura:texto?"
enum { C_ID, C_NEG, C_TIPO, C_HASH, C_ANT, C_MOM, C_REM, C_PAYLOAD, C_ASS, C_N };

static void valor_texto(JwValor *v, const char *s) {
    memset(v, 0, sizeof *v);
    if (!s) { v->nulo = 1; v->tipo = JW_NULO; return; }
    v->tipo = JW_TEXTO; v->p = s; v->n = strlen(s);
}

int watp_jsonw_anexar(const char *caminho, const WatpEnv *e) {
    char erro[256] = "";
    JwEsquema *esq = jw_esquema(WATP_ESQUEMA_RAZAO, erro, sizeof erro);
    if (!esq) return WATP_ERR_IO;
    struct stat st;
    JwEscritor *w = stat(caminho, &st) == 0 ? jw_continuar(caminho, erro, sizeof erro) : jw_criar(caminho, esq);
    if (!w) { jw_esquema_free(esq); return WATP_ERR_IO; }
    JwValor c[C_N];
    valor_texto(&c[C_ID], e->id); valor_texto(&c[C_NEG], e->negociacao); valor_texto(&c[C_TIPO], e->tipo);
    valor_texto(&c[C_HASH], e->hash); valor_texto(&c[C_ANT], e->anterior); valor_texto(&c[C_MOM], e->momento);
    valor_texto(&c[C_REM], e->remetente); valor_texto(&c[C_PAYLOAD], e->payload); valor_texto(&c[C_ASS], e->assinatura);
    int r = jw_linha(w, c);
    if (r) { jw_abandonar(w); jw_esquema_free(esq); return WATP_ERR_IO; }
    r = jw_fechar(w);
    jw_esquema_free(esq);
    return r ? WATP_ERR_IO : WATP_OK;
}

static char *texto_do_campo(const JwValor *v) {
    if (v->nulo) return NULL;
    char *c = w_malloc(v->n + 1);
    memcpy(c, v->p, v->n);
    c[v->n] = 0;
    return c;
}

int watp_jsonw_carregar(const char *caminho, WatpEstado *s, int *n) {
    *n = 0;
    struct stat st;
    if (stat(caminho, &st) != 0) return WATP_OK;
    JwArquivo a;
    char erro[256] = "";
    if (jw_abrir(&a, caminho, erro, sizeof erro)) return WATP_ERR_IO;
    JwArena ar = {0};
    for (uint64_t i = 0; i < a.total; i++) {
        JwValor v[C_N];
        WatpEnv e = {0};
        int falta = 0;
        for (int k = 0; k < C_N && !falta; k++) if (jw_valor(&a, i, k, &v[k], &ar)) falta = 1;
        if (falta) { watp_env_livrar(&e); jw_arena_limpar(&ar); jw_fechar_arquivo(&a); jw_arena_free(&ar); return WATP_ERR_IO; }
        e.id = texto_do_campo(&v[C_ID]); e.negociacao = texto_do_campo(&v[C_NEG]); e.tipo = texto_do_campo(&v[C_TIPO]);
        e.hash = texto_do_campo(&v[C_HASH]); e.anterior = texto_do_campo(&v[C_ANT]); e.momento = texto_do_campo(&v[C_MOM]);
        e.remetente = texto_do_campo(&v[C_REM]); e.payload = texto_do_campo(&v[C_PAYLOAD]); e.assinatura = texto_do_campo(&v[C_ASS]);
        watp_estado_aplicar(s, &e);
        (*n)++;
        watp_env_livrar(&e);
        jw_arena_limpar(&ar);
    }
    jw_arena_free(&ar);
    jw_fechar_arquivo(&a);
    return WATP_OK;
}
