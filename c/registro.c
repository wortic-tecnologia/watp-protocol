/* registro.c: o registry de nós do WATP. Upsert por node_id (a 2ª chamada atualiza nome, URL e plataforma, e mantém
 * registered_at). Listagem: só os nós vistos nas últimas 26 horas, ordenados por node_id. */
#include "watp.h"
#include "mapa.h"
#include <time.h>

#define WATP_JANELA_S (26 * 3600)

typedef struct { char *id, *nome, *url, *plataforma; long long registrado, visto; } No;
struct WatpRegistro { WatpMapa *nos; };

WatpRegistro *watp_registro_criar(void) {
    WatpRegistro *r = w_calloc(1, sizeof *r);
    r->nos = watp_mapa_criar();
    return r;
}

static void liberar_no(void *v) {
    No *n = v;
    free(n->id); free(n->nome); free(n->url); free(n->plataforma); free(n);
}

void watp_registro_livrar(WatpRegistro *r) {
    if (!r) return;
    watp_mapa_livrar(r->nos, liberar_no);
    free(r);
}

static char *copia(const char *s) {
    char *c = w_malloc(strlen(s) + 1);
    strcpy(c, s);
    return c;
}

static int texto_vazio(const char *s) {
    if (!s) return 1;
    for (const char *p = s; *p; p++) if (!isspace((unsigned char)*p)) return 0;
    return 1;
}

int watp_registro_pors(WatpRegistro *r, const char *node_id, const char *nome, const char *url, const char *plataforma,
                       long long agora, char *erro, size_t ne) {
    if (texto_vazio(node_id) || texto_vazio(nome) || texto_vazio(url)) {
        if (erro && ne) snprintf(erro, ne, "node_id, name e url são obrigatórios");
        return WATP_ERR_CAMPO;
    }
    if (!watp_url_publica(url, 0)) {
        if (erro && ne) snprintf(erro, ne, "url precisa ser uma base URL pública em HTTPS, sem query nem fragmento");
        return WATP_ERR_URL;
    }
    No *n = watp_mapa_achar(r->nos, node_id);
    if (!n) {
        n = w_calloc(1, sizeof *n);
        n->id = copia(node_id);
        n->registrado = agora;
        watp_mapa_pors(r->nos, node_id, n);
    } else {
        free(n->nome); free(n->url); free(n->plataforma);
    }
    n->nome = copia(nome);
    n->url = copia(url);
    n->plataforma = copia(plataforma ? plataforma : "");
    n->visto = agora;
    return WATP_OK;
}

static void iso(long long t, const char *fmt, char out[32]) {
    time_t tt = (time_t)t;
    struct tm g;
    gmtime_r(&tt, &g);
    strftime(out, 32, fmt, &g);
}

typedef struct { No **v; size_t n, cap; long long corte; } Colhe;

static void colher(const char *k, void *v, void *ctx) {
    (void)k;
    Colhe *c = ctx;
    No *n = v;
    if (n->visto < c->corte) return;
    if (c->n == c->cap) { c->cap = c->cap ? c->cap * 2 : 16; c->v = w_realloc(c->v, c->cap * sizeof *c->v); }
    c->v[c->n++] = n;
}

static int por_id(const void *a, const void *b) {
    const No *x = *(No *const *)a, *y = *(No *const *)b;
    return strcmp(x->id, y->id);
}

/* borda-json: a resposta de GET /api/v1/registry/nodes */
int watp_registro_listar_borda(const WatpRegistro *r, long long agora, Buf *o) {
    Colhe c = {0};
    c.corte = agora - WATP_JANELA_S;
    watp_mapa_passar(r->nos, colher, &c);
    qsort(c.v, c.n, sizeof *c.v, por_id);
    buf_str(o, "{\"registry\":\"watp\",\"nodes\":[");
    for (size_t i = 0; i < c.n; i++) {
        No *n = c.v[i];
        char reg[32], vis[32];
        iso(n->registrado, "%Y-%m-%d %H:%M:%S", reg);
        iso(n->visto, "%Y-%m-%d %H:%M:%S", vis);
        if (i) buf_str(o, ",");
        buf_str(o, "{\"node_id\":"); watp_texto_js(o, n->id, strlen(n->id));
        buf_str(o, ",\"name\":"); watp_texto_js(o, n->nome, strlen(n->nome));
        buf_str(o, ",\"url\":"); watp_texto_js(o, n->url, strlen(n->url));
        buf_str(o, ",\"platform\":"); watp_texto_js(o, n->plataforma, strlen(n->plataforma));
        buf_str(o, ",\"registered_at\":"); watp_texto_js(o, reg, strlen(reg));
        buf_str(o, ",\"last_seen\":"); watp_texto_js(o, vis, strlen(vis));
        buf_str(o, "}");
    }
    char agora_iso[32];
    iso(agora, "%Y-%m-%dT%H:%M:%SZ", agora_iso);
    buf_fmt(o, "],\"count\":%zu,\"timestamp\":\"%s\"}", c.n, agora_iso);
    free(c.v);
    return WATP_OK;
}
