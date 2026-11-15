# WATP: protocolo de negociação entre agentes

O WATP (Wortic Agentic Transfer Protocol) é o protocolo com que um agente propõe, contrapropõe e fecha uma compra com
outro agente. Cada negociação tem a sua cadeia de hash: cada mensagem inclui o hash da anterior, então a negociação
inteira pode ser conferida por quem não participou dela.

Esta versão reescreve o núcleo em C (`libwatp`), com o razão do disco em jsonW. JSON fica só na borda HTTP e nos
protocolos de fora.

> **English summary.** WATP is an open negotiation protocol between agents. Each negotiation has its own SHA-256 hash
> chain; each message commits to the previous hash. This repository holds the contract (`WATP-CONTRATO-REAL.md`), the
> JSON Schema, the public conformance fixtures and a validator, and, from this version, the C core (`c/`, library
> `libwatp`) that stores envelopes as jsonW lines and bridges to A2A, AP2, UCP, ACP, x402, MPP, L402, AG-UI and
> OpenTelemetry. The C code builds with `-Wall -Wextra -Werror` and passes its tests under AddressSanitizer and
> UndefinedBehaviorSanitizer. It is not a public blockchain: there is no global consensus, only one chain per
> negotiation and a registry of nodes.

## O que tem aqui

| Pasta | Conteúdo |
|---|---|
| `WATP-CONTRATO-REAL.md` | o que o runtime faz hoje, sem narrativa acima dele |
| `protocol/` | esquema do envelope, versionamento, fixtures de conformidade |
| `scripts/` | validador de referência em JavaScript (Node 20) |
| `c/` | núcleo em C: `libwatp`, o validador de conformidade, a porta do site e a prova |

## Envelope

Campos canônicos: `id`, `negotiation_id`, `type`, `payload`, `hash`, `previousHash`, `timestamp`, `sender`.
O leitor aceita os aliases `prev_hash` (no lugar de `previousHash`), `quantity` e `price` (dentro do payload).

O hash é o SHA-256 do texto `{"type":…,"payload":…,"previousHash":…,"timestamp":…}`. O payload entra na ordem de
chegada e número inteiro sai sem casas decimais (`100`, não `100.0`). Esse é o byte do runtime antigo, provado pelas
fixtures e pelo vetor de preço inteiro.

## Cadeia, registro e assinatura

- `HASH_INTEREST` abre a negociação. Os demais tipos continuam a partir do último hash. Um hash repetido não entra de novo.
- O `id` é a chave de idempotência. O mesmo id com outro conteúdo é replay e é recusado.
- Registro de nós: `node_id`, `name` e `url` obrigatórios, URL pública HTTPS sem credencial, fragmento, query ou host
  local e IP privado. A listagem mostra só os nós vistos nas últimas 26 horas.
- Assinatura legada: `X-WATP-Node-Signature: sha256=` mais o HMAC-SHA256 do corpo bruto, com o segredo do par.
- Assinatura forte por nó: Ed25519 sobre `WATP-ED25519-V1`, o remetente e o hash. Assim a assinatura não serve a outro remetente.

## Pontes

| Protocolo | O que a ponte faz |
|---|---|
| A2A 1.0 | Message com a parte de dados e o bloco `watp` nos metadados |
| AP2 mandate.1 | CheckoutMandate; o `checkout_jwt` é um JWS EdDSA assinado pela chave do nó |
| UCP 2026-08-25 | checkout com centavos inteiros e link de continuação |
| ACP 2026-04-17 | CheckoutSession com o mesmo resumo |
| x402 v2 | desafio 402 com valor atômico; só sai com destino configurado |
| MPP draft-httpauth-payment-01 | desafio Payment; só sai com destino configurado |
| L402 | desafio com macaroon e fatura; só sai com fatura dada |
| AG-UI 2026-11 | RunStarted, StateSnapshot e RunFinished ou RunError |
| OpenTelemetry OTLP | span com traceId da negociação e spanId do hash |

Cada ponte volta ao envelope e passa pela mesma validação. Os desafios de pagamento não saem sem destino: o
protocolo não define carteira, e a biblioteca não inventa uma.

## Compilar e provar (C)

O núcleo usa a biblioteca jsonW (`lib/`) e a máscara (`mascara/`) da casa, que não estão neste repositório. Copie-as para
`c/` antes do `make`. Dependências: gcc ou clang, OpenSSL (libcrypto) e libpq (cabeçalhos, para a lib jsonW).

```sh
cd c
make            # libwatp.a, bin/watp-conformidade, bin/teste_watp, bin/watp-porta
make asan       # as mesmas provas com AddressSanitizer e UBSan
./bin/watp-conformidade ../
./bin/teste_watp
```

`watp-conformidade` lê as fixtures de `protocol/` e reproduz as linhas do validador de referência. `watp-porta <porta> <pasta>`
serve o site de `c/porta` pelo wporta e responde `/api/v1/health`.

## Limites desta versão

- Não há consenso entre nós nem ledger público.
- A janela de tempo das mensagens novas não está no contrato e não é aplicada.
- O razão é um arquivo jsonW por nó. A replicação entre nós ainda não existe.
- Pagamentos reais (x402, MPP e L402) não estão ligados.
- Os SDKs TypeScript e Go têm Ed25519; o SDK Python ainda não recebeu essa mudança.

## Licença

A licença deste repositório ainda será confirmada. O texto em `LICENSE` é o da casa para o jsonW, copiado como ponto de partida.

---

## Português

WATP é o protocolo aberto de negociação entre agentes da Wortic. Contribuições: veja `CONTRIBUTING.md`. Vulnerabilidades: veja `SECURITY.md`.
