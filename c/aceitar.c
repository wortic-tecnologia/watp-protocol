/* aceitar.c: o caminho de uma mensagem nova, na ordem do contrato: idempotência por id, depois a cadeia da negociação.
 * O envelope já chega validado (watp_env_de_borda); aqui só se decide se ele entra. */
#include "watp.h"

int watp_estado_criar(WatpEstado *s) {
    s->idem = watp_idem_criar();
    s->cadeia = watp_cadeia_criar();
    return WATP_OK;
}

void watp_estado_livrar(WatpEstado *s) {
    watp_idem_livrar(s->idem);
    watp_cadeia_livrar(s->cadeia);
    s->idem = NULL;
    s->cadeia = NULL;
}

/* põe no estado sem checar (usado ao reler o razão do disco, que já foi aceito uma vez) */
int watp_estado_aplicar(WatpEstado *s, const WatpEnv *e) {
    watp_idem_pors(s->idem, e->id, e->hash);
    return watp_cadeia_avancar(s->cadeia, e);
}

int watp_aceitar(WatpEstado *s, const WatpEnv *e, char *erro, size_t ne) {
    int v = watp_idem_ver(s->idem, e->id, e->hash);
    if (v == 1) return WATP_REPETIDO;
    if (v == -1) {
        if (erro && ne) snprintf(erro, ne, "id reusado com outro conteúdo");
        return WATP_ERR_CONFLITO;
    }
    int r = watp_cadeia_checar(s->cadeia, e, erro, ne);
    if (r != WATP_OK) return r;
    watp_estado_aplicar(s, e);
    return WATP_OK;
}
