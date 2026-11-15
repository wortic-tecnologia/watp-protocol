/* b64.c: base64 de URL sem preenchimento (RFC 4648, seção 5). Usado no JWS do AP2 e no campo request do MPP. */
#include "watp.h"

static const char ABC[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

int watp_b64url(const uint8_t *in, size_t n, Buf *o) {
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = (uint32_t)in[i] << 16;
        if (i + 1 < n) v |= (uint32_t)in[i + 1] << 8;
        if (i + 2 < n) v |= in[i + 2];
        buf_add(o, &ABC[v >> 18 & 63], 1);
        buf_add(o, &ABC[v >> 12 & 63], 1);
        if (i + 1 < n) buf_add(o, &ABC[v >> 6 & 63], 1);
        if (i + 2 < n) buf_add(o, &ABC[v & 63], 1);
    }
    return WATP_OK;
}

static int valor_do_digito(char c) {
    const char *p = strchr(ABC, c);
    return (c && p) ? (int)(p - ABC) : -1;
}

int watp_b64url_ler(const char *s, Buf *o) {
    size_t n = strlen(s);
    if (n % 4 == 1) return WATP_ERR_FORMA;   /* 1 caractere sobrando não é base64 válido */
    uint32_t acc = 0; int bits = 0;
    for (size_t i = 0; i < n; i++) {
        int d = valor_do_digito(s[i]);
        if (d < 0) return WATP_ERR_FORMA;
        acc = (acc << 6) | (uint32_t)d;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            uint8_t b = (uint8_t)(acc >> bits);
            buf_add(o, &b, 1);
        }
    }
    return WATP_OK;
}
