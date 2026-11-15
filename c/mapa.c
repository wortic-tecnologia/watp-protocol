/* mapa.c: mapa de texto para valor com endereçamento aberto (FNV-1a, cresce a meia carga). Base da idempotência e da cadeia. */
#include "watp.h"
#include "mapa.h"

typedef struct { char *k; void *v; } Par;
struct WatpMapa { Par *p; size_t cap, n; };

static uint64_t fnv(const char *s) {
    uint64_t h = 1469598103934665603ULL;
    for (const unsigned char *q = (const unsigned char *)s; *q; q++) { h ^= *q; h *= 1099511628211ULL; }
    return h;
}

WatpMapa *watp_mapa_criar(void) {
    WatpMapa *m = w_calloc(1, sizeof *m);
    m->cap = 16;
    m->p = w_calloc(m->cap, sizeof *m->p);
    return m;
}

static size_t posicao(const WatpMapa *m, const char *k) {
    size_t i = (size_t)(fnv(k) & (m->cap - 1));
    while (m->p[i].k && strcmp(m->p[i].k, k)) i = (i + 1) & (m->cap - 1);
    return i;
}

void *watp_mapa_achar(const WatpMapa *m, const char *k) {
    size_t i = posicao(m, k);
    return m->p[i].k ? m->p[i].v : NULL;
}

int watp_mapa_pors(WatpMapa *m, const char *k, void *v) {
    if (m->n * 2 >= m->cap) {
        Par *velho = m->p; size_t capv = m->cap;
        m->cap *= 2; m->p = w_calloc(m->cap, sizeof *m->p);
        for (size_t j = 0; j < capv; j++) if (velho[j].k) { size_t i = posicao(m, velho[j].k); m->p[i] = velho[j]; }
        free(velho);
    }
    size_t i = posicao(m, k);
    if (m->p[i].k) { m->p[i].v = v; return 0; }
    m->p[i].k = w_malloc(strlen(k) + 1);
    strcpy(m->p[i].k, k);
    m->p[i].v = v;
    m->n++;
    return 1;
}

void watp_mapa_passar(const WatpMapa *m, void (*fn)(const char *k, void *v, void *ctx), void *ctx) {
    for (size_t j = 0; j < m->cap; j++) if (m->p[j].k) fn(m->p[j].k, m->p[j].v, ctx);
}

void watp_mapa_livrar(WatpMapa *m, void (*liberar)(void *v)) {
    if (!m) return;
    for (size_t j = 0; j < m->cap; j++) if (m->p[j].k) { if (liberar) liberar(m->p[j].v); free(m->p[j].k); }
    free(m->p);
    free(m);
}
