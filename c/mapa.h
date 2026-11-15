/* mapa.h: mapa de texto para valor (mapa.c). Usado pela idempotência (id -> hash) e pela cadeia (negociação -> hashes). */
#ifndef WATP_MAPA_H
#define WATP_MAPA_H
#include <stddef.h>

typedef struct WatpMapa WatpMapa;
WatpMapa *watp_mapa_criar(void);
void *watp_mapa_achar(const WatpMapa *m, const char *k);                 /* NULL se não há */
int watp_mapa_pors(WatpMapa *m, const char *k, void *v);                 /* 1 = chave nova; 0 = trocou o valor */
void watp_mapa_passar(const WatpMapa *m, void (*fn)(const char *k, void *v, void *ctx), void *ctx);
void watp_mapa_livrar(WatpMapa *m, void (*liberar)(void *v));            /* a chave é liberada; o valor, por liberar */

#endif
