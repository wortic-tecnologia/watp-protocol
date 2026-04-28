# WATP Contrato Real

Status: interno, canônico para implementação

Data de congelamento: 2026-04-22

Objetivo: descrever o que o WATP realmente implementa hoje, sem narrativa acima do runtime.

## O que o WATP é hoje

O WATP não é uma blockchain pública.

O WATP é hoje uma rede federada de descoberta e negociação, onde cada negociação mantém sua própria cadeia de mensagens com hash.

Isso significa:

- não existe consenso global entre todos os nós
- não existe bloco compartilhado por toda a rede
- não existe mineração
- não existe ledger universal imutável para todos os participantes
- existe uma hash chain por negociação
- existe um registry central para discovery de nós

## Papéis de nó observáveis

- Registry: site-a expõe o registry central de discovery
- Buyer/Marketplace: site-b descobre fornecedores e inicia negociações
- Supplier: plugin WooCommerce canônico e também os nós demo podem receber interesse e responder

## Modelo de autenticação real

### Público

- GET /api/v1/health
- GET /api/v1/catalog
- GET /api/v1/registry/nodes

### Protegido por Bearer da rede

- POST /api/v1/messages
- POST /api/v1/registry/register

### Protegido por sessão autenticada do painel ou Bearer

- GET /api/v1/messages/{id} nos nós demo
- GET /api/v1/negotiations nos nós demo
- GET /api/v1/negotiations/{id} nos nós demo
- POST /api/v1/negotiations/{id}/approve nos nós demo

### Limitação atual

O modelo atual ainda usa um segredo Bearer compartilhado pela rede para autenticar escrita e leitura protegida entre nós confiáveis.

Nos writes críticos, o runtime exige HMAC por peer conhecido, salvo quando `WATP_ALLOW_UNSIGNED_PEERS=1` for explicitamente habilitado para compatibilidade legada.

Isso reduz spoofing operacional, mas ainda não é identidade criptográfica forte por nó com chave pública, certificado ou rotação automática.

## Registry real

### Registro de nó

Endpoint:

- POST /api/v1/registry/register

Body aceito:

```json
{
  "node_id": "watp-produtos",
  "name": "Produtos Supplier",
  "url": "https://produtos.williamschons.com.br",
  "platform": "WooCommerce/9.x"
}
```

Regras atuais:

- node_id, name e url são obrigatórios
- url precisa ser uma base URL pública em HTTPS
- localhost, IP privado, domínio local/internal, query string, fragment e credenciais na URL são rejeitados
- o registry faz upsert por node_id
- registry/nodes retorna apenas nós vistos nas últimas 26 horas

### Discovery de nós

Endpoint:

- GET /api/v1/registry/nodes

Resposta base:

```json
{
  "registry": "site-a",
  "nodes": [
    {
      "node_id": "watp-produtos",
      "name": "Produtos Supplier",
      "url": "https://produtos.williamschons.com.br",
      "platform": "WooCommerce/9.x",
      "registered_at": "2026-04-22 00:00:00",
      "last_seen": "2026-04-22 00:30:00"
    }
  ],
  "count": 1,
  "timestamp": "2026-04-22T00:30:00Z"
}
```

No consumidor atual, site-b filtra novamente os nós e só usa URLs públicas HTTPS válidas antes do fan-out.

## Envelope real de mensagem

Campos observáveis hoje:

- id: obrigatório no contrato de produção; usado para idempotência e replay protection
- negotiation_id: obrigatório para tipos diferentes de HASH_INTEREST
- type: obrigatório
- payload: obrigatório
- hash: obrigatório
- previousHash: opcional no primeiro evento; obrigatório na continuidade da negociação
- prev_hash: alias aceito por alguns nós demo ao receber
- timestamp: obrigatório
- sender: obrigatório no contrato de produção; deve identificar o peer emissor
- signature: HMAC obrigatório para peers conhecidos nos writes críticos; ainda não é X.509 ou assinatura assimétrica ponta a ponta

### Tipos aceitos no runtime

- HASH_INTEREST
- HASH_PROPOSAL
- HASH_COUNTER
- HASH_CONCLUDED
- HASH_CANCELLED
- HASH_WAITING_HUMAN

Observação:

o plugin Woo canônico hoje valida e trata os tipos HASH_INTEREST, HASH_PROPOSAL, HASH_COUNTER, HASH_CONCLUDED e HASH_CANCELLED. O tipo HASH_WAITING_HUMAN está ativo nos nós demo.

## Payload mínimo válido

Para todos os tipos exceto HASH_CANCELLED:

- sku ou item ou product_name deve existir
- qty deve ser maior que zero
- unit_price ou price deve ser zero ou maior

Campos recorrentes observados:

- item
- sku
- qty
- quantity
- unit_price
- price
- currency
- buyer
- buyer_name
- buyer_url
- billing
- reason
- accepted_by
- approved_by
- counter_by
- agent
- message
- payment_url

## Algoritmo real de hash

Hash atual:

- algoritmo: SHA-256
- entrada: JSON canônico com type, payload, previousHash e timestamp

Estrutura canônica:

```json
{
  "type": "HASH_INTEREST",
  "payload": { "sku": "ABC-1", "qty": 10, "unit_price": 12.5 },
  "previousHash": null,
  "timestamp": "2026-04-22T00:00:00.000000Z"
}
```

Regras operacionais atuais:

- incoming hash é recomputado e comparado
- previousHash é exigido na continuidade da negociação
- a mensagem é deduplicada por negotiation_id + hash
- `id` de mensagem é usado para short-circuit de retry idempotente e conflito de replay
- timestamp fora da janela operacional é rejeitado para mensagens novas
- hash inválido gera 400
- previousHash inválido gera 409

## Semântica operacional por tipo

### HASH_INTEREST

- abre uma nova negociação
- pode criar pedido Woo pendente no plugin supplier
- deve carregar buyer_url quando se espera resposta do supplier para o comprador

### HASH_PROPOSAL

- representa proposta formal na negociação

### HASH_COUNTER

- representa contraproposta e avança a hash chain

### HASH_WAITING_HUMAN

- usado nos nós demo para escalar a decisão ao humano

### HASH_CONCLUDED

- encerra a negociação como concluída
- nos nós demo pode gerar contrato local
- no plugin Woo atualiza estado local, alinha o pedido Woo aos termos finais e mantém o pedido aguardando pagamento quando aplicável
- quando o supplier Woo fecha a negociação e o pedido ainda precisa de pagamento, o payload pode incluir `payment_url` com o link oficial de pagamento do pedido

### HASH_CANCELLED

- encerra a negociação como cancelada
- não exige payload mínimo de sku/qty/preço

## Contratos e limites atuais

O runtime atual não oferece ainda:

- blockchain pública
- consenso distribuído entre todos os nós
- identidade criptográfica por nó com chave própria
- verificação real de signature/X.509
- contrato juridicamente assinado no sentido forte
- SDK oficial empacotado para múltiplas linguagens

Os contratos dos nós demo são hoje artefatos locais derivados da hash chain da negociação, não um instrumento jurídico com assinatura forte verificada.

## Definição prática para próximos passos

Antes de abrir SDK público, qualquer implementação nova deve respeitar este contrato real.

A próxima evolução técnica natural é:

1. identidade por nó
2. assinatura por emissor
3. contrato público versionado
4. SDKs em cima desse contrato congelado
