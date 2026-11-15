/* ponte_agui.c: a negociação como eventos AG-UI (protocolo de interação agente-usuário, perfil agui 2026-11).
 * Cada mensagem aceita vira: RunStarted (threadId = negociação, runId = id), StateSnapshot (o hash e o estado) e, se a
 * negociação fechou, RunFinished (concluída) ou RunError (cancelada). Em andamento, só os dois primeiros. */
#include "watp.h"

int watp_agui_eventos(const WatpEnv *e, Buf *o) {
    int64_t ns;
    if (watp_iso_ns(e->momento, &ns)) return WATP_ERR_FORMA;
    long long ms = (long long)(ns / 1000000);
    buf_str(o, "[{\"type\":\"RunStarted\",\"threadId\":");   /* borda-json: evento AG-UI */
    watp_texto_js(o, e->negociacao, strlen(e->negociacao));
    buf_str(o, ",\"runId\":"); watp_texto_js(o, e->id, strlen(e->id));
    buf_fmt(o, ",\"timestamp\":%lld},", ms);
    buf_str(o, "{\"type\":\"StateSnapshot\",\"snapshot\":{\"type\":"); watp_texto_js(o, e->tipo, strlen(e->tipo));
    buf_str(o, ",\"hash\":"); watp_texto_js(o, e->hash, strlen(e->hash));
    buf_str(o, ",\"previousHash\":");
    if (e->anterior) watp_texto_js(o, e->anterior, strlen(e->anterior)); else buf_str(o, "null");
    buf_str(o, ",\"sender\":"); watp_texto_js(o, e->remetente, strlen(e->remetente));
    buf_fmt(o, "},\"timestamp\":%lld}", ms);
    if (!strcmp(e->tipo, "HASH_CONCLUDED")) {
        buf_str(o, ",{\"type\":\"RunFinished\",\"threadId\":"); watp_texto_js(o, e->negociacao, strlen(e->negociacao));
        buf_str(o, ",\"runId\":"); watp_texto_js(o, e->id, strlen(e->id));
        buf_fmt(o, ",\"timestamp\":%lld}", ms);
    } else if (!strcmp(e->tipo, "HASH_CANCELLED")) {
        buf_fmt(o, ",{\"type\":\"RunError\",\"message\":\"negociação cancelada\",\"code\":\"HASH_CANCELLED\",\"timestamp\":%lld}", ms);
    }
    buf_str(o, "]");
    return WATP_OK;
}

/* lê de volta: thread e run do RunStarted, e o hash do StateSnapshot */
int watp_agui_de(const char *s, size_t n, char thread[160], char run[160], char hash[65]) {
    JVal *a = json_ler(s, n);                                  /* borda-json: eventos que chegaram */
    if (!a || a->t != J_LISTA) { if (a) json_livrar(a); return WATP_ERR_FORMA; }
    int achou_inicio = 0, achou_hash = 0;
    for (size_t i = 0; i < a->n; i++) {
        const JVal *ev = a->itens[i];
        if (ev->t != J_OBJETO) continue;
        const char *tipo = json_texto_campo(ev, "type");
        if (!tipo) continue;
        if (!strcmp(tipo, "RunStarted")) {
            const char *t = json_texto_campo(ev, "threadId"), *r = json_texto_campo(ev, "runId");
            if (t && r) { snprintf(thread, 160, "%s", t); snprintf(run, 160, "%s", r); achou_inicio = 1; }
        } else if (!strcmp(tipo, "StateSnapshot")) {
            const JVal *snap = json_campo(ev, "snapshot");
            const char *h = snap && snap->t == J_OBJETO ? json_texto_campo(snap, "hash") : NULL;
            if (h) { snprintf(hash, 65, "%s", h); achou_hash = 1; }
        }
    }
    json_livrar(a);
    return achou_inicio && achou_hash ? WATP_OK : WATP_ERR_FORMA;
}
