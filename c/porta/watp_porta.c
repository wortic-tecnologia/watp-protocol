/* watp_porta.c: a porta pública do WATP (watp.wortic.com). Serve o site de uma pasta pelo wporta (index, docs, llms.txt,
 * robots.txt, sitemap.xml), a saúde do serviço e a especificação da API desta versão. Uso: watp-porta <porta> <pasta> [ip]
 * Um processo, uma conexão por vez: é a porta de leitura; a borda (wborda) põe o TLS na frente. */
#include "../watp.h"
#include "wporta.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

/* borda-json: a especificação da API desta versão (OpenAPI 3.1), para ferramentas de fora; só a rota que existe */
static const char *OPENAPI =
    "{\"openapi\":\"3.1.0\",\"info\":{\"title\":\"WATP: API publica desta versao\",\"version\":\"" WATP_VERSAO "\","   /* borda-json: OpenAPI */
    "\"description\":\"Nesta versao a porta publica responde a saude do servico. A API de mensagens do contrato ainda nao esta publicada.\"},"   /* borda-json: OpenAPI */
    "\"servers\":[{\"url\":\"https://watp.wortic.com\"}],\"paths\":{\"/api/v1/health\":{\"get\":{\"summary\":\"Saude do servico\","   /* borda-json: OpenAPI */
    "\"operationId\":\"health\",\"responses\":{\"200\":{\"description\":\"No ar, com a versao do nucleo e do contrato\","   /* borda-json: OpenAPI */
    "\"content\":{\"application/json\":{\"schema\":{\"type\":\"object\",\"required\":[\"status\",\"versao\",\"contrato\"],"   /* borda-json: OpenAPI */
    "\"properties\":{\"status\":{\"type\":\"string\",\"enum\":[\"ok\"]},\"versao\":{\"type\":\"string\"},\"contrato\":{\"type\":\"string\"}}}}}}}}}}}";   /* borda-json: OpenAPI */

static void resposta(int fd, int status, const char *frase, const char *tipo, const char *corpo) {
    char cab[512];
    int n = snprintf(cab, sizeof cab, "HTTP/1.1 %d %s\r\ncontent-type: %s\r\ncontent-length: %zu\r\n"
                                      "access-control-allow-origin: *\r\nx-content-type-options: nosniff\r\nconnection: close\r\n\r\n",
                     status, frase, tipo, strlen(corpo));
    if (write(fd, cab, (size_t)n) < 0) return;
    if (write(fd, corpo, strlen(corpo)) < 0) return;
}

static void atender(int fd) {
    char req[4096];
    ssize_t n = read(fd, req, sizeof req - 1);
    if (n <= 0) return;
    req[n] = 0;
    char metodo[16] = "", cam[1024] = "";
    if (sscanf(req, "%15s %1023s", metodo, cam) != 2) { resposta(fd, 400, "Bad Request", "text/plain; charset=utf-8", "pedido invalido\n"); return; }
    char *q = strchr(cam, '?');
    if (q) *q = 0;
    if (strcmp(metodo, "GET") && strcmp(metodo, "HEAD")) { resposta(fd, 405, "Method Not Allowed", "text/plain; charset=utf-8", "metodo nao permitido\n"); return; }
    if (!strcmp(cam, "/api/v1/health")) {   /* borda-json: a saúde do serviço, para o monitor da borda */
        char corpo[256];
        snprintf(corpo, sizeof corpo, "{\"status\":\"ok\",\"versao\":\"%s\",\"contrato\":\"%s\"}\n", WATP_VERSAO, WATP_SNAPSHOT);   /* borda-json: saúde */
        resposta(fd, 200, "OK", "application/json", corpo);
        return;
    }
    if (!strcmp(cam, "/openapi.json")) { resposta(fd, 200, "OK", "application/json", OPENAPI); return; }   /* borda-json: especificação */
    if (wporta_atender(fd, cam)) return;
    resposta(fd, 404, "Not Found", "text/plain; charset=utf-8", "nao encontrado\n");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "uso: watp-porta <porta> <pasta> [ip]\n"); return 2; }
    int porta = atoi(argv[1]);
    WportaMarca marcas[] = {{"{{VERSAO}}", WATP_VERSAO}, {"{{SNAPSHOT}}", WATP_SNAPSHOT}};
    int paginas = wporta_abrir(argv[2], marcas, 2);
    if (paginas <= 0) { fprintf(stderr, "watp-porta: a pasta %s não tem páginas\n", argv[2]); return 1; }
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return 1;
    int um = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &um, sizeof um);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t)porta);
    a.sin_addr.s_addr = inet_addr(argc > 3 ? argv[3] : "127.0.0.1");
    if (bind(s, (struct sockaddr *)&a, sizeof a) < 0 || listen(s, 64) < 0) { perror("watp-porta"); return 1; }
    signal(SIGPIPE, SIG_IGN);
    fprintf(stderr, "watp-porta: %d páginas, na porta %d\n", paginas, porta);
    for (;;) {
        int c = accept(s, NULL, NULL);
        if (c < 0) continue;
        atender(c);
        close(c);
    }
}
