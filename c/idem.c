/* idem.c: idempotência por id de mensagem. Um id já visto com o mesmo hash é retry (WATP_REPETIDO, nada a gravar);
 * um id já visto com outro hash é replay com conteúdo trocado (conflito). */
#include "watp.h"
#include "mapa.h"

struct WatpIdem { WatpMapa *m; };   /* id -> hash (texto próprio) */

WatpIdem *watp_idem_criar(void) {
    WatpIdem *t = w_calloc(1, sizeof *t);
    t->m = watp_mapa_criar();
    return t;
}

void watp_idem_livrar(WatpIdem *t) {
    if (!t) return;
    watp_mapa_livrar(t->m, free);
    free(t);
}

int watp_idem_ver(const WatpIdem *t, const char *id, const char *hash) {
    const char *h = watp_mapa_achar(t->m, id);
    if (!h) return 0;
    return strcmp(h, hash) ? -1 : 1;
}

int watp_idem_pors(WatpIdem *t, const char *id, const char *hash) {
    const char *h = watp_mapa_achar(t->m, id);
    if (h) return strcmp(h, hash) ? -1 : 1;
    char *c = w_malloc(strlen(hash) + 1);
    strcpy(c, hash);
    watp_mapa_pors(t->m, id, c);
    return 0;
}
