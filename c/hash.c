/* hash.c: o hash da mensagem WATP = SHA-256 do texto {type,payload,previousHash,timestamp} (contrato, seção de hash).
 * É o mesmo byte a byte do runtime antigo: as fixtures do repositório e o vetor 351a76ec (preço inteiro) provam isso. */
#include "watp.h"
#include "wsha256.h"

/* borda-json: o texto hasheado é o JSON.stringify do runtime; o payload já vem canônico */
int watp_hash_mensagem(const char *tipo, const char *payload_canon, const char *anterior, const char *momento, char out[65]) {
    Buf b = {0};
    buf_str(&b, "{\"type\":"); watp_texto_js(&b, tipo, strlen(tipo));          /* borda-json: campo type */
    buf_str(&b, ",\"payload\":"); buf_str(&b, payload_canon);                    /* borda-json: campo payload */
    buf_str(&b, ",\"previousHash\":");                                           /* borda-json: campo previousHash */
    if (anterior) watp_texto_js(&b, anterior, strlen(anterior)); else buf_str(&b, "null");
    buf_str(&b, ",\"timestamp\":"); watp_texto_js(&b, momento, strlen(momento)); /* borda-json: campo timestamp */
    buf_str(&b, "}");
    WSha256 s; uint8_t d[32];
    wsha256_inicio(&s); wsha256_mais(&s, b.p, b.n); wsha256_fim(&s, d);
    wsha256_hex(d, out);
    buf_free(&b);
    return 0;
}

int watp_hash_hex_valido(const char *s) {
    if (!s || strlen(s) != 64) return 0;
    for (int i = 0; i < 64; i++) if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) return 0;
    return 1;
}
