/* watp.h: libwatp, o WATP (Wortic Agentic Transfer Protocol) em C e jsonW.
 * Contrato: watp-protocol/WATP-CONTRATO-REAL.md (snapshot-2026-04-23). Cada função devolve 0 = ok ou um WATP_*.
 * Por dentro, o envelope é um registro C (WatpEnv) guardado em disco como linha jsonW (watp_jsonw.c).
 * A borda (HTTP e protocolos de fora) fala texto de borda: ver as linhas marcadas borda-json.
 * Hash: SHA-256 do texto canônico {type,payload,previousHash,timestamp}, igual ao runtime antigo (watp_hash.c). */
#ifndef WATP_H
#define WATP_H
#include "comum.h"
#include "json.h" /* borda-json: leitor do texto que chega da borda (HTTP e protocolos de fora) */
#include <ctype.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define WATP_VERSAO "0.1.0"
#define WATP_SNAPSHOT "snapshot-2026-04-23"

enum { WATP_OK = 0, WATP_REPETIDO = 1, WATP_ERR_CAMPO = 2, WATP_ERR_HASH = 3, WATP_ERR_CADEIA = 4,
       WATP_ERR_URL = 5, WATP_ERR_CONFLITO = 6, WATP_ERR_ASSINATURA = 7, WATP_ERR_SEM_DESTINO = 8,
       WATP_ERR_IO = 9, WATP_ERR_FORMA = 10 };

/* o envelope: o que o runtime guarda por mensagem. Textos terminados em 0 e donos do registro (watp_env_livrar) */
typedef struct {
    char *id, *negociacao, *tipo, *hash, *anterior, *momento, *remetente;
    char *payload;     /* texto canônico do payload (ordem de chegada; é o que entra no hash) */
    char *assinatura;  /* transporte: "sha256=<hex>" (legado) ou "ed25519=<hex>"; NULL se não veio */
} WatpEnv;

/* ---- canon.c: texto canônico do payload e dos números, igual ao do runtime (borda-json: hash do runtime antigo) ---- */
void watp_num_js(Buf *o, double x);                       /* borda-json: número como o JSON.stringify do JS; inteiro sem .0 */
void watp_texto_js(Buf *o, const char *s, size_t n);      /* borda-json: string com o escape do JS */
int  watp_canon(Buf *o, const JVal *v);                   /* borda-json: valor na ordem de chegada; chave repetida = último valor */

/* ---- tempo.c: momento ISO UTC e nanossegundos Unix; centavos exatos de um preço ---- */
int watp_iso_ns(const char *iso, int64_t *ns);
void watp_ns_iso(int64_t ns, char out[32]);
int watp_centavos(double preco, int64_t *c);

/* ---- hash.c ---- */
int watp_hash_mensagem(const char *tipo, const char *payload_canon, const char *anterior, const char *momento, char out[65]);
int watp_hash_hex_valido(const char *s);

/* ---- valida.c: as regras do validador de referência, com as mesmas mensagens. 0 = válida; senão a razão ---- */
int watp_valida(const JVal *m, char *erro, size_t ne);

/* ---- envelope.c ---- */
int  watp_env_de_borda(const char *s, size_t n, WatpEnv *e, char *erro, size_t ne);   /* borda-json: lê e valida (prev_hash/quantity/price) */
int  watp_env_para_borda(const WatpEnv *e, Buf *o);                                    /* borda-json: emite só os nomes canônicos */
void watp_env_livrar(WatpEnv *e);
int  watp_env_copiar(WatpEnv *d, const WatpEnv *s);
int  watp_env_igual(const WatpEnv *a, const WatpEnv *b);

/* ---- url.c: URL pública HTTPS (sem IP privado, localhost, .local, .internal, credencial, fragmento) ---- */
int watp_url_publica(const char *url, int aceita_query);   /* 1 = vale */

/* ---- hmac.c: assinatura legada do nó, X-WATP-Node-Signature = sha256=hex(HMAC-SHA256(segredo, corpo bruto)) ---- */
int watp_hmac_assinar(const char *segredo, const char *corpo, size_t ncorpo, Buf *o);
int watp_hmac_conferir(const char *segredo, const char *corpo, size_t ncorpo, const char *assinatura);

/* ---- ed25519.c: assinatura forte por nó (OpenSSL). Assina "WATP-ED25519-V1\n" + remetente + "\n" + hash ---- */
int watp_ed_gerar(uint8_t semente[32], uint8_t pub[32]);
int watp_ed_assinar(const uint8_t semente[32], const char *remetente, const char *hash, char out[137]);   /* "ed25519=" + 128 hex */
int watp_ed_conferir(const uint8_t pub[32], const char *remetente, const char *hash, const char *assinatura);
int watp_ed_sinal_bruto(const uint8_t semente[32], const uint8_t *m, size_t nm, uint8_t sig[64]);   /* Ed25519 de um texto cru */
int watp_ed_verificar_bruto(const uint8_t pub[32], const uint8_t *m, size_t nm, const uint8_t sig[64]);   /* 1 = vale */

/* ---- idem.c: idempotência por id ---- */
typedef struct WatpIdem WatpIdem;
WatpIdem *watp_idem_criar(void);
void watp_idem_livrar(WatpIdem *t);
int watp_idem_ver(const WatpIdem *t, const char *id, const char *hash);   /* 0 = novo; 1 = igual já visto; -1 = id reusado */
int watp_idem_pors(WatpIdem *t, const char *id, const char *hash);

/* ---- cadeia.c: uma hash chain por negociação ---- */
typedef struct WatpCadeia WatpCadeia;
WatpCadeia *watp_cadeia_criar(void);
void watp_cadeia_livrar(WatpCadeia *c);
int watp_cadeia_checar(const WatpCadeia *c, const WatpEnv *e, char *erro, size_t ne);
int watp_cadeia_avancar(WatpCadeia *c, const WatpEnv *e);

/* ---- aceitar.c: o caminho de uma mensagem nova (valida, idempotência, cadeia). WATP_OK = nova; WATP_REPETIDO = retry ---- */
typedef struct { WatpIdem *idem; WatpCadeia *cadeia; } WatpEstado;
int watp_estado_criar(WatpEstado *s);
void watp_estado_livrar(WatpEstado *s);
int watp_aceitar(WatpEstado *s, const WatpEnv *e, char *erro, size_t ne);
int watp_estado_aplicar(WatpEstado *s, const WatpEnv *e);   /* põe no estado sem checar (releitura do razão) */

/* ---- registro.c: registry de nós (upsert por node_id; lista só o visto nas últimas 26 horas) ---- */
typedef struct WatpRegistro WatpRegistro;
WatpRegistro *watp_registro_criar(void);
void watp_registro_livrar(WatpRegistro *r);
int watp_registro_pors(WatpRegistro *r, const char *node_id, const char *nome, const char *url, const char *plataforma,
                      long long agora, char *erro, size_t ne);
int watp_registro_listar_borda(const WatpRegistro *r, long long agora, Buf *o);   /* borda-json: {"nodes":[...],"count":n} */

/* ---- jsonw.c: o razão em jsonW (um arquivo; uma linha por mensagem aceita) ---- */
int watp_jsonw_anexar(const char *caminho, const WatpEnv *e);
int watp_jsonw_carregar(const char *caminho, WatpEstado *s, int *n);

/* ---- pontes: o envelope em cada protocolo de comércio com agentes (ida e volta testada) ---- */
int watp_a2a_para(const WatpEnv *e, Buf *o);                                  /* borda-json: A2A Message */
int watp_a2a_de(const char *s, size_t n, WatpEnv *e, char *erro, size_t ne);  /* borda-json: volta ao envelope e revalida */
/* o que um checkout precisa do envelope: sku, nome do item, quantidade, preço unitário, moeda e link de continuação */
typedef struct { char sku[128], item[256], moeda[4], link[512]; double qtd, preco; } WatpCheckout;
int watp_checkout_ler(const WatpEnv *e, WatpCheckout *d);   /* 0 = ok; qty e unit_price têm de ser números */
int watp_ucp_para(const WatpEnv *e, Buf *o);                                  /* borda-json: UCP checkout */
int watp_acp_para(const WatpEnv *e, Buf *o);                                  /* borda-json: ACP CheckoutSession */
int watp_checkout_resumo(const char *s, size_t n, char sku[128], int64_t *qtd, int64_t *centavos, char moeda[4], char *link, size_t nlink);
/* AP2: o checkout_jwt é um JWS EdDSA (Ed25519) assinado pela chave do nó; o checkout_hash é o hash aceito */
int watp_ap2_mandato(const WatpEnv *e, const uint8_t semente[32], Buf *o);   /* borda-json: AP2 CheckoutMandate */
int watp_ap2_mandato_de(const char *s, size_t n, const uint8_t pub[32], char hash[65]);   /* confere a assinatura e devolve o hash */
/* x402 e MPP e L402: o desafio sai só com destino configurado (pay_to, destino, fatura); vazio = WATP_ERR_SEM_DESTINO */
int watp_x402_desafio(const WatpEnv *e, const char *pay_to, const char *rede, const char *ativo, const char *valor, Buf *o);
int watp_x402_valor(const char *s, size_t n, char *valor, size_t nvalor);
int watp_mpp_desafio(const WatpEnv *e, const char *destino, const char *moeda, Buf *o);
int watp_l402_desafio(const WatpEnv *e, const char *fatura, const char *macaroon, Buf *o);
/* base64 de URL sem preenchimento (RFC 4648 seção 5), para JWS e para o campo request do MPP */
int watp_b64url(const uint8_t *in, size_t n, Buf *o);
int watp_b64url_ler(const char *s, Buf *o);
int watp_agui_eventos(const WatpEnv *e, Buf *o);                              /* borda-json: RunStarted, StateSnapshot, RunFinished */
int watp_agui_de(const char *s, size_t n, char thread[160], char run[160], char hash[65]);
int watp_otel_span(const WatpEnv *e, Buf *o);                                 /* borda-json: OTLP Span */
int watp_otel_de(const char *s, size_t n, char trace[33], char span[17], char nome[64], char hash[65]);

#endif
