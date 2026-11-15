/* valida.c: as regras do validador de referência (scripts/watp-validate-contract-fixtures.mjs), na mesma ordem e com as
 * mesmas mensagens. Acrescenta a regra do contrato: continuação exige previousHash (como o SDK TypeScript).
 * Os parseInt/parseFloat e o String() do JS são copiados aqui, porque o validador de referência depende deles. */
#include "watp.h"
#include <ctype.h>
#include <math.h>

static const JVal *cp(const JVal *o, const char *k) { return json_campo(o, k); }   /* borda-json: campo do texto de entrada */
static int e_objeto(const JVal *v) { return v && v->t == J_OBJETO; }
static int tem_texto(const JVal *v) {                                             /* string com algo que não é espaço */
    if (!v || v->t != J_TEXTO) return 0;
    for (size_t i = 0; i < v->n; i++) if (!isspace((unsigned char)v->s[i])) return 1;
    return 0;
}
static int nulo_ou_ausente(const JVal *v) { return !v || v->t == J_NULO; }

/* String(v) do JS para um valor do texto de entrada (Array e Object seguem o toString do JS) */
static void js_str(Buf *o, const JVal *v) {
    if (!v) return;
    switch (v->t) {
    case J_NULO: buf_str(o, "null"); break;
    case J_BOOL: buf_str(o, v->b ? "true" : "false"); break;
    case J_NUM: watp_num_js(o, v->num); break;
    case J_TEXTO: buf_add(o, v->s ? v->s : "", v->n); break;
    case J_LISTA:
        for (size_t i = 0; i < v->n; i++) {
            if (i) buf_add(o, ",", 1);
            if (v->itens[i]->t != J_NULO) js_str(o, v->itens[i]);
        }
        break;
    case J_OBJETO: buf_str(o, "[object Object]"); break;
    }
}

static double js_parse_int(const char *s) {          /* parseInt(s, 10) */
    while (isspace((unsigned char)*s)) s++;
    int neg = 0;
    if (*s == '+' || *s == '-') { neg = *s == '-'; s++; }
    if (!isdigit((unsigned char)*s)) return NAN;
    double v = 0;
    while (isdigit((unsigned char)*s)) { v = v * 10 + (*s - '0'); s++; }
    return neg ? -v : v;
}

static double js_parse_float(const char *s) {        /* parseFloat(s) */
    while (isspace((unsigned char)*s)) s++;
    const char *ini = s, *p = s;
    if (*p == '+' || *p == '-') p++;
    if (!strncmp(p, "Infinity", 8)) return *ini == '-' ? -INFINITY : INFINITY;
    int dig = 0;
    while (isdigit((unsigned char)*p)) { p++; dig = 1; }
    if (*p == '.') { p++; while (isdigit((unsigned char)*p)) { p++; dig = 1; } }
    if (!dig) return NAN;
    if (*p == 'e' || *p == 'E') {
        const char *q = p + 1;
        if (*q == '+' || *q == '-') q++;
        if (isdigit((unsigned char)*q)) { while (isdigit((unsigned char)*q)) q++; p = q; }
    }
    size_t n = (size_t)(p - ini);
    char *c = w_malloc(n + 1);
    memcpy(c, ini, n); c[n] = 0;
    double v = strtod(c, NULL);
    free(c);
    return v;
}

static int falha(char *erro, size_t ne, int cod, const char *msg) {
    if (erro && ne) snprintf(erro, ne, "%s", msg);
    return cod;
}

/* qty ?? quantity ?? 0 (ou price ?? unit_price): o primeiro que não é nulo */
static double numero_do_payload(const JVal *pl, const char *a, const char *b, int inteiro) {
    const JVal *v = cp(pl, a);
    if (nulo_ou_ausente(v)) v = cp(pl, b);
    if (nulo_ou_ausente(v)) return 0;   /* ?? 0 do validador de referência: qty 0 e price 0 */
    Buf s = {0};
    js_str(&s, v);
    double r = inteiro ? js_parse_int(s.p ? s.p : "") : js_parse_float(s.p ? s.p : "");
    buf_free(&s);
    return r;
}

static int payload_valido(const char *tipo, const JVal *pl, char *erro, size_t ne) {
    if (!strcmp(tipo, "HASH_CANCELLED")) return 0;
    int nome = tem_texto(cp(pl, "sku")) || (cp(pl, "item") && cp(pl, "item")->t == J_TEXTO ? tem_texto(cp(pl, "item")) : tem_texto(cp(pl, "product_name")));
    if (!nome) return falha(erro, ne, WATP_ERR_CAMPO, "sku or product_name is required");
    double qty = numero_do_payload(pl, "qty", "quantity", 1);
    if (!isfinite(qty) || qty <= 0) return falha(erro, ne, WATP_ERR_CAMPO, "qty must be greater than zero");
    double preco = numero_do_payload(pl, "unit_price", "price", 0);
    if (!isfinite(preco) || preco < 0) return falha(erro, ne, WATP_ERR_CAMPO, "unit_price must be zero or greater");
    if (!strcmp(tipo, "HASH_INTEREST")) {
        Buf u = {0};
        js_str(&u, nulo_ou_ausente(cp(pl, "buyer_url")) ? NULL : cp(pl, "buyer_url"));
        int ok = watp_url_publica(u.p ? u.p : "", 0);
        buf_free(&u);
        if (!ok) return falha(erro, ne, WATP_ERR_URL, "buyer_url is required for HASH_INTEREST and must be a public HTTPS URL");
    }
    const JVal *pu = cp(pl, "payment_url");
    int verdadeiro = pu && (pu->t == J_TEXTO ? pu->n > 0 : pu->t == J_NUM ? (pu->num != 0 && !isnan(pu->num)) :
                            pu->t == J_BOOL ? pu->b : pu->t != J_NULO);
    if (verdadeiro) {
        Buf u = {0};
        js_str(&u, pu);
        int ok = watp_url_publica(u.p ? u.p : "", 1);
        buf_free(&u);
        if (!ok) return falha(erro, ne, WATP_ERR_URL, "payment_url must be a public HTTPS URL");
    }
    return 0;
}

int watp_valida(const JVal *m, char *erro, size_t ne) {
    const JVal *t = e_objeto(m) ? cp(m, "type") : NULL;
    if (!e_objeto(m) || !tem_texto(t)) return falha(erro, ne, WATP_ERR_CAMPO, "Invalid message: type is required");
    const char *tipo = t->s;
    const char *tipos[] = {"HASH_INTEREST", "HASH_PROPOSAL", "HASH_COUNTER", "HASH_CONCLUDED", "HASH_CANCELLED", "HASH_WAITING_HUMAN"};
    int tipo_ok = 0;
    for (size_t i = 0; i < sizeof tipos / sizeof tipos[0]; i++) if (!strcmp(tipo, tipos[i])) tipo_ok = 1;
    if (!tipo_ok) { char msg[200]; snprintf(msg, sizeof msg, "Invalid message type: %s", tipo); return falha(erro, ne, WATP_ERR_CAMPO, msg); }
    const JVal *pl = cp(m, "payload");
    if (!e_objeto(pl)) return falha(erro, ne, WATP_ERR_CAMPO, "payload must be an object");
    if (!tem_texto(cp(m, "id"))) return falha(erro, ne, WATP_ERR_CAMPO, "id is required");
    if (!tem_texto(cp(m, "sender"))) return falha(erro, ne, WATP_ERR_CAMPO, "sender is required");
    const JVal *h = cp(m, "hash");
    if (!h || h->t != J_TEXTO || !watp_hash_hex_valido(h->s)) return falha(erro, ne, WATP_ERR_CAMPO, "hash and timestamp are required");
    const JVal *ts = cp(m, "timestamp");
    if (!tem_texto(ts)) return falha(erro, ne, WATP_ERR_CAMPO, "hash and timestamp are required");
    int r = payload_valido(tipo, pl, erro, ne);
    if (r) return r;
    if (strcmp(tipo, "HASH_INTEREST") && !tem_texto(cp(m, "negotiation_id")))
        return falha(erro, ne, WATP_ERR_CAMPO, "negotiation_id is required for non-INTEREST messages");
    const JVal *pv = cp(m, "previousHash"), *pvp = cp(m, "prev_hash");
    Buf anterior = {0};
    if (!nulo_ou_ausente(pv)) {
        Buf s = {0}; js_str(&s, pv);
        if (!watp_hash_hex_valido(s.p ? s.p : "")) { buf_free(&s); return falha(erro, ne, WATP_ERR_CADEIA, "previousHash must be a 64-char hex string when present"); }
        buf_free(&s);
    }
    if (!nulo_ou_ausente(pvp)) {
        Buf s = {0}; js_str(&s, pvp);
        if (!watp_hash_hex_valido(s.p ? s.p : "")) { buf_free(&s); return falha(erro, ne, WATP_ERR_CADEIA, "prev_hash must be a 64-char hex string when present"); }
        buf_free(&s);
    }
    if (!nulo_ou_ausente(pv)) js_str(&anterior, pv); else if (!nulo_ou_ausente(pvp)) js_str(&anterior, pvp);
    if (strcmp(tipo, "HASH_INTEREST") && !anterior.p) {
        buf_free(&anterior);
        char msg[120]; snprintf(msg, sizeof msg, "%s requires previousHash", tipo);
        return falha(erro, ne, WATP_ERR_CADEIA, msg);
    }
    Buf canon = {0};
    if (watp_canon(&canon, pl)) { buf_free(&anterior); buf_free(&canon); return falha(erro, ne, WATP_ERR_FORMA, "payload nao canonizou"); }
    char calc[65];
    watp_hash_mensagem(tipo, canon.p, anterior.p, ts->s, calc);
    buf_free(&anterior); buf_free(&canon);
    if (strcmp(calc, h->s)) return falha(erro, ne, WATP_ERR_HASH, "Invalid hash");
    return WATP_OK;
}
