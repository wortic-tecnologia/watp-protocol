/* tempo.c: o momento da mensagem (ISO 8601 UTC, AAAA-MM-DDTHH:MM:SS.ffffffZ) em nanossegundos Unix, e de volta.
 * centavos: o preço unitário (double do payload) em centavos inteiros, só quando cabe exatamente (sem arredondar em silêncio). */
#include "watp.h"
#include <math.h>
#include <time.h>

int watp_iso_ns(const char *iso, int64_t *ns) {
    struct tm t = {0};
    int ms = 0, lidos = 0;
    if (sscanf(iso, "%4d-%2d-%2dT%2d:%2d:%2d%n", &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec, &lidos) < 6)
        return WATP_ERR_FORMA;
    t.tm_year -= 1900; t.tm_mon -= 1;
    const char *p = iso + lidos;
    if (*p == '.') {
        int digitos = 0; p++;
        while (isdigit((unsigned char)*p)) {
            if (digitos < 9) { ms = ms * 10 + (*p - '0'); digitos++; }
            p++;
        }
        while (digitos < 9) { ms *= 10; digitos++; }   /* fração em nanossegundos */
    }
    if (*p != 'Z') return WATP_ERR_FORMA;
    *ns = (int64_t)timegm(&t) * 1000000000LL + ms;
    return WATP_OK;
}

void watp_ns_iso(int64_t ns, char out[32]) {
    time_t s = (time_t)(ns / 1000000000LL);
    struct tm g;
    gmtime_r(&s, &g);
    strftime(out, 21, "%Y-%m-%dT%H:%M:%S", &g);
    snprintf(out + 19, 13, ".%06lldZ", (long long)((ns % 1000000000LL) / 1000));
}

int watp_centavos(double preco, int64_t *c) {
    if (!isfinite(preco) || preco < 0) return WATP_ERR_CAMPO;
    double x = preco * 100.0;
    double r = llround(x);
    if (fabs(x - r) > 1e-6) return WATP_ERR_FORMA;   /* mais de 2 casas: não arredonda em silêncio */
    *c = (int64_t)r;
    return WATP_OK;
}
