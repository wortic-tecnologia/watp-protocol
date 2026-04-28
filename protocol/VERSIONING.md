# WATP Contract Snapshot e Compatibilidade

## Objetivo

Dar um alvo estavel para SDKs e conectores antes de existir um pacote oficial versionado por linguagem.

## Como versionar hoje

O runtime atual ainda nao publica um versionamento semantico de protocolo no wire.

Por isso, a referencia recomendada para SDKs nesta fase e um snapshot datado do contrato:

- `snapshot-2026-04-23`

Esse snapshot corresponde a:

- schema em `watp-message-envelope.schema.json`
- fixtures em `protocol/fixtures/`
- comportamento documentado nos docs atuais do workspace

## O que conta como compatibilidade

### Mudanca compativel

- adicionar campo opcional novo
- adicionar fixture nova
- esclarecer descricao ou exemplo sem mudar semantica do runtime

### Mudanca que exige novo snapshot

- tornar obrigatorio um campo antes opcional
- trocar nome canonico de campo emitido
- mudar a regra do hash
- mudar os tipos aceitos do runtime
- mudar a semantica de continuidade da cadeia

## Matriz de estabilidade por campo

| Campo | Status | Emissao recomendada | Leitura recomendada | Observacao |
| --- | --- | --- | --- | --- |
| `id` | estavel, opcional | sim | sim | recomendado para deduplicacao e rastreio |
| `type` | estavel | sim | sim | enum observado no runtime atual |
| `payload` | estavel | sim | sim | negocio vai dentro do hash |
| `hash` | estavel | sim | sim | SHA-256 do objeto canonico |
| `timestamp` | estavel | sim | sim | entra no hash |
| `negotiation_id` | estavel em continuacao | sim | sim | obrigatorio apos o primeiro `HASH_INTEREST` |
| `previousHash` | canonico estavel | sim | sim | usar em escrita nova |
| `prev_hash` | alias de compatibilidade | nao | sim | aceitar em leitura apenas |
| `qty` | canonico estavel | sim | sim | usar em escrita nova |
| `quantity` | alias de compatibilidade | nao | sim | aceitar em leitura apenas |
| `unit_price` | canonico estavel | sim | sim | usar em escrita nova |
| `price` | alias de compatibilidade | nao | sim | aceitar em leitura apenas |
| `payment_url` | opcional aditivo | so em `HASH_CONCLUDED` | sim | usar quando a plataforma devolver link oficial de pagamento |
| `signature` | opaco, nao contratual | opcional | sim | persistido hoje, sem verificacao forte fim a fim |

## Regra de ouro para SDKs

Emissao nova deve ser canonica.

Leitura deve ser tolerante aos aliases e aos campos opcionais aditivos.

## Fora de escopo deste snapshot

- assinatura forte por no
- X.509
- consenso global
- semver publico de protocolo
- SDK oficial GitHub pronto para terceiros
