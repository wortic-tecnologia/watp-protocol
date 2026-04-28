# WATP Protocol Changelog

## 2026-04-23

- schema do envelope recebeu descricoes operacionais para SDKs e conectores
- schema passou a explicitar regras ja verdadeiras no runtime: continuidade exige `negotiation_id` e hash anterior; eventos nao cancelados exigem identidade de item, quantidade e preco
- `payment_url` foi documentado como campo opcional e aditivo em `HASH_CONCLUDED`
- aliases de compatibilidade ficaram explicitados: `prev_hash`, `quantity` e `price`
- workspace do protocolo ganhou `README.md` e `VERSIONING.md` para orientar implementacoes novas

## 2026-04-22

- snapshot de contrato real congelado no workspace
- fixtures iniciais de mensagens validas e invalidas foram publicados
