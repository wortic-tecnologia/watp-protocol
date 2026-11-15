/* ponte_l402.c: o desafio L402 (Lightning). A fatura (bolt11) e o macaroon vêm de um nó Lightning; a casa não tem esse nó,
 * então sem fatura não há desafio (WATP_ERR_SEM_DESTINO). A ponte só monta e lê o par, sem gerar fatura.
 * Esquema: esquema_http, macaroon, invoice (perfil l402 da casa). */
#include "watp.h"

int watp_l402_desafio(const WatpEnv *e, const char *fatura, const char *macaroon, Buf *o) {
    (void)e;
    if (!fatura || !*fatura || !macaroon || !*macaroon) return WATP_ERR_SEM_DESTINO;
    buf_str(o, "{\"esquema_http\":\"L402\",\"macaroon\":");   /* borda-json: desafio L402 */
    watp_texto_js(o, macaroon, strlen(macaroon));
    buf_str(o, ",\"invoice\":"); watp_texto_js(o, fatura, strlen(fatura));
    buf_str(o, "}");
    return WATP_OK;
}
