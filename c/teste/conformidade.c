/* conformidade.c: o validador de conformidade do WATP em C. Mesmas linhas e mesmo veredito do validador de referência
 * (scripts/watp-validate-contract-fixtures.mjs do watp-protocol): cada fixture válida passa, cada inválida reprova
 * com a mensagem esperada; depois os exemplos do esquema, o vetor HMAC e o vetor do hash com preço inteiro.
 * Uso: watp-conformidade <raiz do repositório watp-protocol>. Saída 0 = conforme. */
#include "../watp.h"

typedef struct { const char *arquivo; int passa; const char *erro_esperado; } Caso;

static const Caso CASOS[] = {
    {"valid-hash-interest.json", 1, NULL},
    {"valid-hash-concluded-payment-url.json", 1, NULL},
    {"invalid-hash-interest-missing-buyer-url.json", 0, "buyer_url is required for HASH_INTEREST and must be a public HTTPS URL"},
    {"invalid-hash-counter-missing-negotiation-id.json", 0, "negotiation_id is required for non-INTEREST messages"},
    {"invalid-hash-concluded-invalid-payment-url.json", 0, "payment_url must be a public HTTPS URL"},
};

static char *ler_tudo(const char *caminho, size_t *n) {
    FILE *f = fopen(caminho, "rb");
    if (!f) return NULL;
    Buf b = {0};
    char t[4096]; size_t r;
    while ((r = fread(t, 1, sizeof t, f)) > 0) buf_add(&b, t, r);
    fclose(f);
    *n = b.n;
    return b.p;
}

/* confere um texto de envelope: 0 = válido; senão a mensagem do validador */
static int confere(const char *texto, size_t n, char *erro, size_t ne) {
    JVal *m = json_ler(texto, n);   /* borda-json: fixture do repositório público, lida como veio */
    if (!m) { snprintf(erro, ne, "texto ilegível"); return WATP_ERR_FORMA; }
    int r = watp_valida(m, erro, ne);
    json_livrar(m);   /* borda-json: libera a árvore lida da borda */
    return r;
}

static int falhas = 0;

static void caso_envelope(const char *raiz, const Caso *c) {
    char caminho[1024], erro[256] = "";
    snprintf(caminho, sizeof caminho, "%s/protocol/fixtures/%s", raiz, c->arquivo);
    size_t n = 0;
    char *t = ler_tudo(caminho, &n);
    if (!t) { falhas++; fprintf(stderr, "FAIL %s: fixture não abre\n", c->arquivo); return; }
    int r = confere(t, n, erro, sizeof erro);
    free(t);
    if (c->passa && r != 0) { falhas++; fprintf(stderr, "FAIL %s: expected valid, got \"%s\"\n", c->arquivo, erro); return; }
    if (!c->passa) {
        if (r == 0) { falhas++; fprintf(stderr, "FAIL %s: expected failure, got success\n", c->arquivo); return; }
        if (strcmp(erro, c->erro_esperado)) { falhas++; fprintf(stderr, "FAIL %s: expected \"%s\", got \"%s\"\n", c->arquivo, c->erro_esperado, erro); return; }
    }
    printf("OK %s\n", c->arquivo);
}

static void exemplos_do_esquema(const char *raiz) {
    char caminho[1024], erro[256];
    snprintf(caminho, sizeof caminho, "%s/protocol/watp-message-envelope.schema.json", raiz);
    size_t n = 0;
    char *t = ler_tudo(caminho, &n);
    JVal *esq = t ? json_ler(t, n) : NULL;   /* borda-json: esquema público do repositório */
    free(t);
    const JVal *ex = esq ? json_campo(esq, "examples") : NULL;   /* borda-json: campo examples do esquema */
    if (!ex || ex->t != J_LISTA) { falhas++; fprintf(stderr, "FAIL watp-message-envelope.schema.json: examples must be an array\n"); }
    else {
        for (size_t i = 0; i < ex->n; i++) {
            Buf b = {0};
            watp_canon(&b, ex->itens[i]);   /* borda-json: exemplo volta a texto para a mesma leitura */
            int r = confere(b.p, b.n, erro, sizeof erro);
            buf_free(&b);
            if (r) { falhas++; fprintf(stderr, "FAIL watp-message-envelope.schema.json example %zu: %s\n", i + 1, erro); }
            else printf("OK watp-message-envelope.schema.json example %zu\n", i + 1);
        }
    }
    if (esq) json_livrar(esq);   /* borda-json: libera o esquema lido */
}

static void vetores(void) {
    Buf a = {0};
    watp_hmac_assinar("secret-a", "{}", 2, &a);
    int ok = a.p && !strcmp(a.p, "sha256=e94d4c3acf8e7f838218bb8aa3ab6e98f6a78b0755421a81efd1d96497e19411") &&
             watp_hmac_conferir("secret-a", "{}", 2, a.p) && !watp_hmac_conferir("wrong-secret", "{}", 2, a.p);
    buf_free(&a);
    if (!ok) { falhas++; fprintf(stderr, "FAIL shared HMAC signature vector\n"); }
    const char *pl = "{\"sku\":\"SKU-100\",\"qty\":1,\"unit_price\":100.0,\"currency\":\"BRL\"}";   /* borda-json: payload de vetor */
    JVal *p = json_ler(pl, strlen(pl));   /* borda-json: payload de vetor lido como veio */
    Buf canon = {0};
    watp_canon(&canon, p);   /* borda-json: texto canônico do payload */
    char h[65];
    watp_hash_mensagem("HASH_INTEREST", canon.p, NULL, "2026-04-28T00:00:00.000000Z", h);
    json_livrar(p);   /* borda-json: libera o payload de vetor */
    buf_free(&canon);
    if (strcmp(h, "351a76ec467a22fcdde02586af196e866f0fd07e3609b3b145be568b4e02d56a")) {
        falhas++; fprintf(stderr, "FAIL PHP-compatible whole-number float hash vector\n");
    }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "uso: watp-conformidade <raiz do watp-protocol>\n"); return 2; }
    for (size_t i = 0; i < sizeof CASOS / sizeof CASOS[0]; i++) caso_envelope(argv[1], &CASOS[i]);
    if (falhas) return 1;
    exemplos_do_esquema(argv[1]);
    vetores();
    if (falhas) return 1;
    puts("Contract fixtures: OK");
    return 0;
}
