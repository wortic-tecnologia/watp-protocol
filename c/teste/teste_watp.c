/* teste_watp.c: a prova da libwatp. Vetores (RFC 8032, RFC 4648, contrato), regras de URL, idempotência, cadeia,
 * registro, razão em jsonW e ida e volta de cada ponte (A2A, UCP, ACP, AP2, x402, MPP, L402, AG-UI, OTel).
 * Saída: uma linha por teste e o total. Código de saída diferente de 0 = falhou. */
#include "../watp.h"
#include <unistd.h>

static int total = 0, falhou = 0;

#define CHECA(cond, nome) do { total++; if (cond) printf("ok    %s\n", nome); else { falhou++; printf("FALHA %s\n", nome); } } while (0)

static void hex(const uint8_t *b, size_t n, char *o) { for (size_t i = 0; i < n; i++) snprintf(o + 2 * i, 3, "%02x", b[i]); }

/* um envelope com hash válido: payload em texto de borda, hash calculado; o texto de borda sai no Buf */
static void monta(Buf *b, const char *tipo, const char *payload, const char *anterior, const char *mom,
                  const char *id, const char *neg, const char *rem) {
    JVal *p = json_ler(payload, strlen(payload));   /* borda-json: payload de teste */
    Buf c = {0};
    watp_canon(&c, p);
    char h[65];
    watp_hash_mensagem(tipo, c.p, anterior, mom, h);
    json_livrar(p);   /* borda-json: libera o payload de teste */
    memset(b, 0, sizeof *b);
    buf_fmt(b, "{\"id\":\"%s\"", id);
    if (neg) buf_fmt(b, ",\"negotiation_id\":\"%s\"", neg);
    buf_fmt(b, ",\"type\":\"%s\",\"payload\":%s,\"hash\":\"%s\",\"previousHash\":", tipo, c.p, h);
    if (anterior) buf_fmt(b, "\"%s\"", anterior); else buf_str(b, "null");
    buf_fmt(b, ",\"timestamp\":\"%s\",\"sender\":\"%s\"}", mom, rem);   /* borda-json: texto de envelope de teste */
    buf_free(&c);
}

static int env(WatpEnv *e, const char *tipo, const char *payload, const char *anterior, const char *mom,
               const char *id, const char *neg, const char *rem) {
    Buf b; monta(&b, tipo, payload, anterior, mom, id, neg, rem);
    char erro[256] = "";
    int r = watp_env_de_borda(b.p, b.n, e, erro, sizeof erro);
    if (r) fprintf(stderr, "env de teste: %s (%d)\n", erro, r);
    buf_free(&b);
    return r;
}

static const char *PAGO = "{\"item\":\"Patch Cord Cat6 1.5m\",\"sku\":\"TC-PC6-150\",\"qty\":10,\"unit_price\":12.9,\"currency\":\"BRL\","
                          "\"accepted_by\":\"supplier\",\"payment_url\":\"https://produtos.exemplo.com.br/pagar/1234?key=abc\"}";
static const char *COMPRA = "{\"item\":\"Patch Cord Cat6 1.5m\",\"sku\":\"TC-PC6-150\",\"qty\":10,\"unit_price\":12.9,\"currency\":\"BRL\","
                            "\"buyer\":\"site-b\",\"buyer_name\":\"Site B\",\"buyer_url\":\"https://site-b.exemplo.com.br\"}";

static void testes_numeros_e_texto(void) {
    struct { double x; const char *t; } v[] = {{12.9, "12.9"}, {100, "100"}, {1e21, "1e+21"}, {1.5e-7, "1.5e-7"},
        {0.000001, "0.000001"}, {123456789012345678.0, "123456789012345680"}, {-0.0, "0"}, {0.1, "0.1"}, {1e-7, "1e-7"}};
    int ok = 1;
    for (size_t i = 0; i < sizeof v / sizeof v[0]; i++) { Buf b = {0}; watp_num_js(&b, v[i].x); if (strcmp(b.p, v[i].t)) ok = 0; buf_free(&b); }
    CHECA(ok, "numero como o JS (inteiro sem .0, expoente, zero negativo)");
    JVal *d = json_ler("{\"a\":1,\"b\":2,\"a\":3}", 19);   /* borda-json: teste de chave repetida */
    Buf c = {0}; watp_canon(&c, d);
    CHECA(!strcmp(c.p, "{\"a\":3,\"b\":2}"), "chave repetida: 1o lugar, ultimo valor (como o parse do JS)");
    buf_free(&c);
    json_livrar(d);   /* borda-json: libera a árvore do teste */
    Buf s = {0}; watp_texto_js(&s, "a\"b\\c\n\x01", 7);
    CHECA(!strcmp(s.p, "\"a\\\"b\\\\c\\n\\u0001\""), "escape de texto como o JS");
    buf_free(&s);
}

static void testes_url(void) {
    CHECA(watp_url_publica("https://site-b.exemplo.com.br", 0) && watp_url_publica("https://a.com/", 0), "https publica aceita");
    CHECA(!watp_url_publica("http://a.com", 0), "http recusado");
    CHECA(!watp_url_publica("https://localhost/x", 0) && !watp_url_publica("https://a.local", 0) && !watp_url_publica("https://x.internal", 0),
          "localhost, .local e .internal recusados");
    CHECA(!watp_url_publica("https://10.0.0.1", 0) && !watp_url_publica("https://172.16.0.1", 0) && !watp_url_publica("https://192.168.1.1", 0) &&
          !watp_url_publica("https://169.254.1.1", 0) && !watp_url_publica("https://127.0.0.1", 0), "IPv4 privado recusado");
    CHECA(watp_url_publica("https://172.32.0.1", 0), "172.32 e publico");
    CHECA(!watp_url_publica("https://u:p@a.com", 0) && !watp_url_publica("https://a.com/#x", 0), "credencial e fragmento recusados");
    CHECA(!watp_url_publica("https://a.com?x=1", 0) && watp_url_publica("https://a.com?x=1", 1), "query: recusada na URL de nó, aceita no pagamento");
    CHECA(!watp_url_publica("https://a.com:99999", 0) && watp_url_publica("https://a.com:8443", 0), "porta valida e invalida");
    CHECA(!watp_url_publica("https://", 0) && !watp_url_publica(NULL, 0), "vazio recusado");
}

static void testes_assinatura(void) {
    uint8_t seed[32], pub[32];
    uint8_t s1[32] = {0x9d,0x61,0xb1,0x9d,0xef,0xfd,0x5a,0x60,0xba,0x84,0x4a,0xf4,0x92,0xec,0x2c,0xc4,0x44,0x49,0xc5,0x69,0x7b,0x32,0x69,0x19,0x70,0x3b,0xac,0x03,0x1c,0xae,0x7f,0x60};
    memcpy(seed, s1, 32);
    uint8_t sig[64]; char hx[129];
    watp_ed_sinal_bruto(seed, (const uint8_t *)"", 0, sig);
    hex(sig, 64, hx);
    CHECA(!strcmp(hx, "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b"),
          "Ed25519: vetor 1 da RFC 8032");
    CHECA(watp_ed_gerar(seed, pub) == 0, "Ed25519: gerar chave");
    char a[137];
    watp_ed_assinar(seed, "no-a", "4c5ee5b2dc6a546afe29b11a0e7d20dd41d7dd61bcca4b119a1310c8c3b54475", a);
    CHECA(watp_ed_conferir(pub, "no-a", "4c5ee5b2dc6a546afe29b11a0e7d20dd41d7dd61bcca4b119a1310c8c3b54475", a), "Ed25519: assinatura do nó confere");
    CHECA(!watp_ed_conferir(pub, "no-b", "4c5ee5b2dc6a546afe29b11a0e7d20dd41d7dd61bcca4b119a1310c8c3b54475", a), "Ed25519: troca de remetente quebra");
    Buf h = {0};
    watp_hmac_assinar("secret-a", "{}", 2, &h);
    CHECA(watp_hmac_conferir("secret-a", "{}", 2, h.p) && !watp_hmac_conferir("secret-a", "{ }", 3, h.p), "HMAC legado: confere e recusa corpo trocado");
    buf_free(&h);
    CHECA(!watp_hmac_conferir("", "{}", 2, "sha256=00"), "HMAC: segredo vazio recusado");
}

static void testes_envelope(WatpEnv *base) {
    CHECA(env(base, "HASH_INTEREST", COMPRA, NULL, "2026-04-23T05:40:00.000000Z", "c89958da-d1d9-4378-a35b-3449f8d2d835", NULL, "site-b") == 0,
          "envelope de INTEREST aceito (negociação = o próprio hash)");
    char h[65]; strcpy(h, base->hash);
    WatpEnv e2 = {0};
    Buf b = {0};
    monta(&b, "HASH_INTEREST", COMPRA, NULL, "2026-04-23T05:40:00.000000Z", "c89958da-d1d9-4378-a35b-3449f8d2d835", NULL, "site-b");
    char *troca = strstr(b.p, "12.9");
    if (troca) memcpy(troca, "12.8", 4);   /* adulteração do payload: o hash não pode valer */
    char erro[256] = "";
    CHECA(watp_env_de_borda(b.p, b.n, &e2, erro, sizeof erro) == WATP_ERR_HASH && !strcmp(erro, "Invalid hash"), "payload adulterado: hash recusado");
    buf_free(&b);
    watp_env_livrar(&e2);
    Buf p; monta(&p, "HASH_INTEREST", "{\"sku\":\"SKU-1\",\"quantity\":2,\"price\":5.5,\"buyer_url\":\"https://site-b.exemplo.com.br\"}", NULL, "2026-04-23T05:40:00.000000Z", "id-alias", NULL, "site-b");
    WatpEnv e3 = {0};
    CHECA(watp_env_de_borda(p.p, p.n, &e3, erro, sizeof erro) == 0, "aliases quantity e price aceitos no payload");
    watp_env_livrar(&e3);
    buf_free(&p);
    Buf q; monta(&q, "HASH_CONCLUDED", PAGO, h, "2026-04-23T05:41:00.000000Z", "id-2", "neg-1", "produtos");
    WatpEnv e4 = {0};
    CHECA(watp_env_de_borda(q.p, q.n, &e4, erro, sizeof erro) == 0, "continuação com previousHash valida");
    watp_env_livrar(&e4);
    buf_free(&q);
    Buf r; monta(&r, "HASH_COUNTER", PAGO, NULL, "2026-04-23T05:43:00.000000Z", "id-3", "neg-1", "produtos");
    WatpEnv e5 = {0};
    CHECA(watp_env_de_borda(r.p, r.n, &e5, erro, sizeof erro) == WATP_ERR_CADEIA && !strcmp(erro, "HASH_COUNTER requires previousHash"),
          "continuação sem previousHash recusada");
    watp_env_livrar(&e5);
    buf_free(&r);
}

static void testes_estado(void) {
    WatpEnv i = {0}, c = {0}, c2 = {0}, ruim = {0};
    env(&i, "HASH_INTEREST", COMPRA, NULL, "2026-04-23T05:40:00.000000Z", "id-i", NULL, "site-b");
    env(&c, "HASH_COUNTER", PAGO, i.hash, "2026-04-23T05:41:00.000000Z", "id-c", i.hash, "produtos");
    env(&c2, "HASH_COUNTER", PAGO, c.hash, "2026-04-23T05:42:00.000000Z", "id-c2", i.hash, "produtos");
    env(&ruim, "HASH_COUNTER", PAGO, c.hash, "2026-04-23T05:43:00.000000Z", "id-r", "neg-x", "produtos");
    WatpEstado s; watp_estado_criar(&s);
    char erro[256] = "";
    CHECA(watp_aceitar(&s, &i, erro, sizeof erro) == WATP_OK, "aceitar: INTEREST abre a negociação");
    CHECA(watp_aceitar(&s, &i, erro, sizeof erro) == WATP_REPETIDO, "idempotência: o mesmo id e hash é retry");
    CHECA(watp_aceitar(&s, &c, erro, sizeof erro) == WATP_OK, "aceitar: contraproposta na cadeia");
    CHECA(watp_aceitar(&s, &c2, erro, sizeof erro) == WATP_OK, "cadeia: continuação a partir do último hash aceita");
    WatpEnv velho = {0};
    env(&velho, "HASH_COUNTER", PAGO, i.hash, "2026-04-23T05:44:00.000000Z", "id-v", i.hash, "produtos");
    CHECA(watp_aceitar(&s, &velho, erro, sizeof erro) == WATP_ERR_CADEIA, "cadeia: previousHash que não é o último recusado");
    watp_env_livrar(&velho);
    WatpEnv trocado; watp_env_copiar(&trocado, &c); free(trocado.id); trocado.id = strdup("id-c");
    free(trocado.hash); trocado.hash = strdup(c2.hash);
    CHECA(watp_aceitar(&s, &trocado, erro, sizeof erro) == WATP_ERR_CONFLITO, "replay: id reusado com outro conteúdo é conflito");
    watp_env_livrar(&trocado);
    CHECA(watp_aceitar(&s, &ruim, erro, sizeof erro) == WATP_ERR_CADEIA, "negociação desconhecida recusada");
    watp_estado_livrar(&s);
    watp_env_livrar(&i); watp_env_livrar(&c); watp_env_livrar(&c2); watp_env_livrar(&ruim);
}

static void testes_registro(void) {
    WatpRegistro *r = watp_registro_criar();
    char erro[256] = "";
    long long agora = 1800000000;
    CHECA(watp_registro_pors(r, "watp-produtos", "Produtos", "https://produtos.exemplo.com.br", "WooCommerce/9.x", agora, erro, sizeof erro) == 0, "registro: nó novo");
    CHECA(watp_registro_pors(r, "watp-local", "Local", "http://127.0.0.1", "", agora, erro, sizeof erro) == WATP_ERR_URL, "registro: URL local recusada");
    watp_registro_pors(r, "watp-antigo", "Antigo", "https://antigo.exemplo.com.br", "", agora - 27 * 3600, erro, sizeof erro);
    Buf o = {0};
    watp_registro_listar_borda(r, agora, &o);
    CHECA(strstr(o.p, "\"count\":1") && strstr(o.p, "watp-produtos") && !strstr(o.p, "watp-antigo"), "registro: lista só o visto nas últimas 26 horas");
    buf_free(&o);
    watp_registro_livrar(r);
}

static void testes_razao(WatpEnv *i, WatpEnv *c) {
    char caminho[256];
    snprintf(caminho, sizeof caminho, "/tmp/watp-teste-razao-%d.jsw", (int)getpid());
    unlink(caminho);
    CHECA(watp_jsonw_anexar(caminho, i) == 0 && watp_jsonw_anexar(caminho, c) == 0, "razão jsonW: gravar duas mensagens");
    WatpEstado s; watp_estado_criar(&s);
    int n = 0;
    CHECA(watp_jsonw_carregar(caminho, &s, &n) == 0 && n == 2, "razão jsonW: reler as duas");
    char erro[256] = "";
    CHECA(watp_aceitar(&s, i, erro, sizeof erro) == WATP_REPETIDO, "razão jsonW: o retry continua idempotente depois de reler");
    WatpEnv d = {0};
    env(&d, "HASH_COUNTER", PAGO, c->hash, "2026-04-23T05:42:00.000000Z", "id-d", i->hash, "produtos");
    CHECA(watp_aceitar(&s, &d, erro, sizeof erro) == WATP_OK, "razão jsonW: a cadeia segue depois de reler");
    watp_env_livrar(&d);
    watp_estado_livrar(&s);
    unlink(caminho);
}

static void testes_pontes(WatpEnv *c, WatpEnv *i) {
    uint8_t seed[32], pub[32];
    memset(seed, 7, 32);
    watp_ed_gerar(seed, pub);
    Buf o = {0};
    watp_a2a_para(c, &o);
    WatpEnv back = {0}; char erro[256] = "";
    int ra = watp_a2a_de(o.p, o.n, &back, erro, sizeof erro);
    if (ra) fprintf(stderr, "a2a_de: %d %s\n%s\n", ra, erro, o.p);
    CHECA(ra == 0 && watp_env_igual(c, &back), "A2A: ida e volta, hash revalidado");
    watp_env_livrar(&back); buf_free(&o);

    Buf u = {0}; watp_ucp_para(c, &u);
    char sku[128], moeda[4], link[512]; int64_t qtd, cent;
    CHECA(watp_checkout_resumo(u.p, u.n, sku, &qtd, &cent, moeda, link, sizeof link) == 0 && !strcmp(sku, "TC-PC6-150") && qtd == 10 && cent == 1290 &&
          !strcmp(moeda, "BRL") && !strcmp(link, "https://produtos.exemplo.com.br/pagar/1234?key=abc"), "UCP: resumo volta sku, quantidade, centavos, moeda e link");
    buf_free(&u);
    Buf a = {0}; watp_acp_para(c, &a);
    CHECA(watp_checkout_resumo(a.p, a.n, sku, &qtd, &cent, moeda, link, sizeof link) == 0 && qtd == 10 && cent == 1290 &&
          !strcmp(link, "https://produtos.exemplo.com.br/pagar/1234?key=abc"), "ACP: resumo volta os mesmos valores");
    buf_free(&a);

    Buf m = {0};
    CHECA(watp_ap2_mandato(c, seed, &m) == 0, "AP2: mandato assinado pela chave do nó");
    char hm[65] = "";
    CHECA(watp_ap2_mandato_de(m.p, m.n, pub, hm) == 0 && !strcmp(hm, c->hash), "AP2: a volta confere a assinatura e o checkout_hash");
    char *troca = strstr(m.p, "checkout_jwt\":\"");
    if (troca) { char *p = troca + 15; p[5] = p[5] == 'A' ? 'B' : 'A'; }
    CHECA(watp_ap2_mandato_de(m.p, m.n, pub, hm) != 0, "AP2: JWS adulterado recusado");
    buf_free(&m);

    Buf x = {0};
    CHECA(watp_x402_desafio(c, "", "eip155:8453", "USDC", "1290", &x) == WATP_ERR_SEM_DESTINO, "x402: sem destino, sem desafio");
    CHECA(watp_x402_desafio(c, "0xDESTINO", "eip155:8453", "USDC", "1290", &x) == 0, "x402: com destino, desafio");
    char val[32] = "";
    CHECA(watp_x402_valor(x.p, x.n, val, sizeof val) == 0 && !strcmp(val, "1290"), "x402: volta o valor atômico");
    buf_free(&x);

    Buf mp = {0};
    CHECA(watp_mpp_desafio(c, "", "BRL", &mp) == WATP_ERR_SEM_DESTINO, "MPP: sem destino, sem desafio");
    CHECA(watp_mpp_desafio(c, "destino-x", "BRL", &mp) == 0 && strstr(mp.p, "\"method\":\"tempo\"") && strstr(mp.p, c->hash), "MPP: desafio com o hash como id");
    buf_free(&mp);

    Buf l = {0};
    CHECA(watp_l402_desafio(c, "", "mac", &l) == WATP_ERR_SEM_DESTINO, "L402: sem fatura, sem desafio");
    CHECA(watp_l402_desafio(c, "lnbc1000", "mac-x", &l) == 0 && strstr(l.p, "lnbc1000"), "L402: desafio com a fatura dada");
    buf_free(&l);

    Buf g = {0};
    watp_agui_eventos(c, &g);
    char th[160] = "", rn[160] = "", hg[65] = "";
    CHECA(watp_agui_de(g.p, g.n, th, rn, hg) == 0 && !strcmp(th, c->negociacao) && !strcmp(rn, c->id) && !strcmp(hg, c->hash), "AG-UI: eventos voltam thread, run e hash");
    buf_free(&g);

    Buf sp = {0}; watp_otel_span(i, &sp);
    char tr[33] = "", spn[17] = "", nm[64] = "", hs[65] = "";
    CHECA(watp_otel_de(sp.p, sp.n, tr, spn, nm, hs) == 0 && strlen(tr) == 32 && !strncmp(spn, i->hash, 16) && !strcmp(hs, i->hash) && !strcmp(nm, "watp.HASH_INTEREST"),
          "OTel: span volta traceId, spanId, nome e hash");
    buf_free(&sp);

    Buf b64 = {0}, dec = {0};
    watp_b64url((const uint8_t *)"foobar", 6, &b64);
    watp_b64url_ler(b64.p, &dec);
    CHECA(!strcmp(b64.p, "Zm9vYmFy") && dec.n == 6 && !memcmp(dec.p, "foobar", 6), "base64 de URL: vetor da RFC 4648 e ida e volta");
    buf_free(&b64); buf_free(&dec);
}

int main(void) {
    testes_numeros_e_texto();
    testes_url();
    testes_assinatura();
    WatpEnv base = {0};
    testes_envelope(&base);
    WatpEnv i = {0}, c = {0};
    env(&i, "HASH_INTEREST", COMPRA, NULL, "2026-04-23T05:40:00.000000Z", "id-i", NULL, "site-b");
    env(&c, "HASH_CONCLUDED", PAGO, i.hash, "2026-04-23T05:41:00.000000Z", "id-c", i.hash, "produtos");
    testes_estado();
    testes_registro();
    testes_razao(&i, &c);
    testes_pontes(&c, &i);
    watp_env_livrar(&i); watp_env_livrar(&c); watp_env_livrar(&base);
    printf("\n%d testes, %d falhas\n", total, falhou);
    return falhou ? 1 : 0;
}
