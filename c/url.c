/* url.c: URL pública HTTPS do contrato WATP. Recusa: não-HTTPS, credencial, fragmento com texto, host vazio, localhost,
 * .local, .internal, IPv4 privado (10, 127, 172.16-31, 192.168, 169.254) e, na URL de nó, query com texto.
 * Simplificação: o parser não é o WHATWG inteiro (não normaliza IPv4 abreviado nem percent-encoding do host). */
#include "watp.h"
#include <ctype.h>

static int termina_em(const char *h, size_t n, const char *fim) {
    size_t m = strlen(fim);
    return n >= m && !strncmp(h + n - m, fim, m);
}

static int https_prefixo(const char *u) {   /* "https://" sem diferenciar maiúsculas (o JS põe o esquema em minúsculas) */
    const char *ref = "https://";
    for (size_t i = 0; ref[i]; i++) if (tolower((unsigned char)u[i]) != ref[i]) return 0;
    return 1;
}

int watp_url_publica(const char *url, int aceita_query) {
    if (!url || !https_prefixo(url)) return 0;
    const char *p = url + 8;
    size_t alen = strcspn(p, "/?#");
    if (alen == 0 || memchr(p, '@', alen)) return 0;               /* host vazio ou credencial na URL */
    const char *fim = p + alen;
    int query = 0, frag = 0;
    if (*fim == '?') { query = fim[1] && fim[1] != '#'; }
    const char *h = strchr(fim, '#');
    if (h && h[1]) frag = 1;                                        /* fragmento com texto */
    if (frag || (!aceita_query && query)) return 0;
    /* host e porta: IPv6 entre colchetes passa como hoje no runtime (não é IPv4 nem nome local) */
    size_t hlen = alen;
    if (p[0] == '[') return 1;
    const char *dois = memchr(p, ':', alen);
    if (dois) {
        hlen = (size_t)(dois - p);
        for (const char *q = dois + 1; q < p + alen; q++) {
            if (!isdigit((unsigned char)*q)) return 0;               /* porta só com dígitos */
        }
        long porta = strtol(dois + 1, NULL, 10);
        if (porta > 65535) return 0;
    }
    if (hlen == 0) return 0;
    char host[256];
    if (hlen >= sizeof host) return 0;
    for (size_t i = 0; i < hlen; i++) host[i] = (char)tolower((unsigned char)p[i]);
    host[hlen] = 0;
    if (!strcmp(host, "localhost") || termina_em(host, hlen, ".local") || termina_em(host, hlen, ".internal")) return 0;
    int partes = 0, ip = 1; unsigned oct[4] = {0};
    const char *q = host;
    while (ip && *q) {
        if (!isdigit((unsigned char)*q)) { ip = 0; break; }
        unsigned v = 0;
        while (isdigit((unsigned char)*q)) { v = v * 10 + (unsigned)(*q - '0'); q++; }
        if (partes < 4) oct[partes] = v;
        partes++;
        if (*q == '.') { q++; if (!*q) { ip = 0; break; } } else if (*q) { ip = 0; break; }
    }
    if (ip && partes == 4) {
        if (oct[0] == 10 || oct[0] == 127 || (oct[0] == 172 && oct[1] >= 16 && oct[1] <= 31) ||
            (oct[0] == 192 && oct[1] == 168) || (oct[0] == 169 && oct[1] == 254)) return 0;
    }
    return 1;
}
