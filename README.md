# WATP Protocol Workspace

## Objetivo

Este diretorio concentra o contrato real que SDKs e conectores devem consumir hoje.

Ele documenta o runtime atual do WATP sem prometer:

- consenso global
- blockchain publica
- assinatura criptografica forte por no
- X.509 ponta a ponta
- SDK oficial congelado em multiplas linguagens

## Arquivos principais

- `watp-message-envelope.schema.json`: schema do envelope observado no runtime
- `fixtures/valid-hash-interest.json`: exemplo minimo de abertura de negociacao
- `fixtures/valid-hash-concluded-payment-url.json`: exemplo de fechamento com `payment_url`
- `VERSIONING.md`: politica de snapshots e compatibilidade para SDKs
- `CHANGELOG.md`: historico objetivo das mudancas documentais do contrato

## Regra pratica para novos SDKs

1. emitir os nomes canonicos `previousHash`, `qty` e `unit_price`
2. aceitar em leitura os aliases `prev_hash`, `quantity` e `price`
3. exigir `negotiation_id` e hash anterior em toda continuacao da cadeia
4. tratar `payment_url` como campo opcional e aditivo, valido quando o supplier consegue devolver um link oficial de pagamento
5. tratar `signature` como opaco; nao depender dela para autenticidade forte nesta versao

## Perfil minimo de interoperabilidade

### Escrita canonica

Um emissor novo deve preferir:

- `id`
- `type`
- `payload`
- `hash`
- `timestamp`
- `negotiation_id` em qualquer evento apos `HASH_INTEREST`
- `previousHash` em qualquer evento apos `HASH_INTEREST`

### Leitura compativel

Um consumidor robusto deve aceitar:

- `previousHash` ou `prev_hash`
- `qty` ou `quantity`
- `unit_price` ou `price`

## Escopo e limites

Este diretorio nao define:

- bearer headers e HMAC de trust layer como contrato publico de SDK
- registro de nos como discovery descentralizado
- fluxo financeiro universal entre plataformas commerce

Esses pontos permanecem acoplados ao runtime atual e devem ser tratados como integracao de adaptador, nao como promessa publica de protocolo.
