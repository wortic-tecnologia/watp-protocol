/* canon.c: o texto canônico que entra no hash, igual ao JSON.stringify do JS (o validador de referência do WATP).
 * Números: o menor texto que lê de volta o mesmo double, na forma do ECMAScript (inteiro sem .0). Strings: o escape do JS.
 * Objetos: na ordem de chegada; chave repetida fica no 1º lugar com o último valor (como o JSON.parse do JS). */
#include "watp.h"
#include <math.h>
#include <stdio.h>

/* borda-json: a forma de número do JS, a partir dos dígitos do menor %e que volta igual */
void watp_num_js(Buf *o, double x) {
    if (!isfinite(x)) { buf_str(o, "null"); return; }   /* borda-json: o JS escreve null para o que não é finito */
    if (x == 0) { buf_str(o, "0"); return; }
    if (x < 0) { buf_str(o, "-"); x = -x; }
    char e[40]; int p;
    for (p = 1; p <= 17; p++) {
        snprintf(e, sizeof e, "%.*e", p - 1, x);
        if (strtod(e, NULL) == x) break;
    }
    char dig[20]; size_t k = 0; const char *q = e;
    for (; *q && *q != 'e'; q++) if (*q >= '0' && *q <= '9') dig[k++] = *q;
    int ex = atoi(q + 1);
    while (k > 1 && dig[k - 1] == '0') k--;
    dig[k] = 0;
    int n = ex + 1; char t[64];
    if ((int)k <= n && n <= 21) {                       /* inteiro: dígitos e zeros */
        buf_add(o, dig, k);
        for (int i = 0; i < n - (int)k; i++) buf_add(o, "0", 1);
    } else if (0 < n && n <= 21) {                      /* ponto dentro dos dígitos */
        buf_add(o, dig, (size_t)n); buf_add(o, ".", 1); buf_add(o, dig + n, k - (size_t)n);
    } else if (-6 < n && n <= 0) {                      /* 0.000ddd */
        buf_str(o, "0.");
        for (int i = 0; i < -n; i++) buf_add(o, "0", 1);
        buf_add(o, dig, k);
    } else {                                            /* notação com expoente */
        buf_add(o, dig, 1);
        if (k > 1) { buf_add(o, ".", 1); buf_add(o, dig + 1, k - 1); }
        snprintf(t, sizeof t, "e%c%d", n - 1 < 0 ? '-' : '+', (n - 1 < 0 ? -(n - 1) : (n - 1)));
        buf_str(o, t);
    }
}

/* borda-json: a string com o escape do JSON.stringify (\b \f \n \r \t \" \\ e \u00xx; o UTF-8 passa cru) */
void watp_texto_js(Buf *o, const char *s, size_t n) {
    buf_add(o, "\"", 1);
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
        case '"': buf_str(o, "\\\""); break;
        case '\\': buf_str(o, "\\\\"); break;
        case '\b': buf_str(o, "\\b"); break;
        case '\f': buf_str(o, "\\f"); break;
        case '\n': buf_str(o, "\\n"); break;
        case '\r': buf_str(o, "\\r"); break;
        case '\t': buf_str(o, "\\t"); break;
        default:
            if (c < 0x20) buf_fmt(o, "\\u%04x", c);
            else buf_add(o, &s[i], 1);
        }
    }
    buf_add(o, "\"", 1);
}

/* borda-json: valor canônico; o último valor de cada chave vale, na posição da 1ª ocorrência */
int watp_canon(Buf *o, const JVal *v) {
    if (!v) return WATP_ERR_FORMA;
    switch (v->t) {
    case J_NULO: buf_str(o, "null"); return 0;
    case J_BOOL: buf_str(o, v->b ? "true" : "false"); return 0;
    case J_NUM: watp_num_js(o, v->num); return 0;
    case J_TEXTO: watp_texto_js(o, v->s ? v->s : "", v->n); return 0;
    case J_LISTA:
        buf_add(o, "[", 1);
        for (size_t i = 0; i < v->n; i++) {
            if (i) buf_add(o, ",", 1);
            if (watp_canon(o, v->itens[i])) return WATP_ERR_FORMA;
        }
        buf_add(o, "]", 1);
        return 0;
    case J_OBJETO:
        buf_add(o, "{", 1);
        for (size_t i = 0, primeiro = 1; i < v->n; i++) {
            size_t ultimo = i; int repetida = 0;
            for (size_t j = 0; j < i; j++) if (!strcmp(v->chaves[j], v->chaves[i])) { repetida = 1; break; }
            if (repetida) continue;
            for (size_t j = i; j < v->n; j++) if (!strcmp(v->chaves[j], v->chaves[i])) ultimo = j;
            if (!primeiro) buf_add(o, ",", 1);
            primeiro = 0;
            watp_texto_js(o, v->chaves[i], strlen(v->chaves[i]));
            buf_add(o, ":", 1);
            if (watp_canon(o, v->itens[ultimo])) return WATP_ERR_FORMA;
        }
        buf_add(o, "}", 1);
        return 0;
    }
    return WATP_ERR_FORMA;
}
