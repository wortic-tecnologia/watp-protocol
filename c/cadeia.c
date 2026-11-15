/* cadeia.c: a hash chain de cada negociação. HASH_INTEREST abre a negociação; os demais continuam a partir do último hash
 * (previousHash igual). Um hash já visto na negociação é repetição (WATP_REPETIDO). Negociação desconhecida ou cadeia
 * quebrada é WATP_ERR_CADEIA (409 na borda). */
#include "watp.h"
#include "mapa.h"

typedef struct { char *ultimo; WatpMapa *vistos; } Negociacao;   /* vistos: hash -> 1 */
struct WatpCadeia { WatpMapa *negs; };

WatpCadeia *watp_cadeia_criar(void) {
    WatpCadeia *c = w_calloc(1, sizeof *c);
    c->negs = watp_mapa_criar();
    return c;
}

static void liberar_negociacao(void *v) {
    Negociacao *n = v;
    free(n->ultimo);
    watp_mapa_livrar(n->vistos, NULL);
    free(n);
}

void watp_cadeia_livrar(WatpCadeia *c) {
    if (!c) return;
    watp_mapa_livrar(c->negs, liberar_negociacao);
    free(c);
}

int watp_cadeia_checar(const WatpCadeia *c, const WatpEnv *e, char *erro, size_t ne) {
    const Negociacao *n = watp_mapa_achar(c->negs, e->negociacao);
    if (n && watp_mapa_achar(n->vistos, e->hash)) return WATP_REPETIDO;
    if (!strcmp(e->tipo, "HASH_INTEREST")) {
        if (n) { if (erro && ne) snprintf(erro, ne, "negociação já aberta com outro hash"); return WATP_ERR_CADEIA; }
        return WATP_OK;
    }
    if (!n) { if (erro && ne) snprintf(erro, ne, "negociação desconhecida"); return WATP_ERR_CADEIA; }
    if (!e->anterior || strcmp(e->anterior, n->ultimo)) {
        if (erro && ne) snprintf(erro, ne, "previousHash não é o último hash da negociação");
        return WATP_ERR_CADEIA;
    }
    return WATP_OK;
}

int watp_cadeia_avancar(WatpCadeia *c, const WatpEnv *e) {
    Negociacao *n = watp_mapa_achar(c->negs, e->negociacao);
    if (!n) {
        n = w_calloc(1, sizeof *n);
        n->vistos = watp_mapa_criar();
        watp_mapa_pors(c->negs, e->negociacao, n);
    }
    free(n->ultimo);
    n->ultimo = w_malloc(strlen(e->hash) + 1);
    strcpy(n->ultimo, e->hash);
    watp_mapa_pors(n->vistos, e->hash, (void *)1);
    return WATP_OK;
}
