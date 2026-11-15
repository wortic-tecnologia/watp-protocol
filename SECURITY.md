# Política de segurança

## Como reportar

Não abra issue pública para vulnerabilidade. Envie o relato ao mantenedor pelo canal de contato indicado no perfil
da organização `wortic-tecnologia` no GitHub, com:

- a versão ou o commit afetado;
- os passos para reproduzir, com a entrada exata (envelope, URL ou assinatura);
- o efeito esperado (hash aceito indevidamente, replay, assinatura falsa, vazamento de segredo).

O relato é lido e respondido. Enquanto a correção não sai, não divulgue o detalhe.

## Escopo

Entram: a conferência do hash e da cadeia, a idempotência, a validação de URL do registro, a assinatura HMAC legada,
a assinatura Ed25519 por nó e a validação dos mandatos e desafios das pontes.

Não entram: a implantação do runtime de terceiros e os sistemas de quem usa o protocolo.

## Limites conhecidos

O WATP desta versão não tem consenso entre nós, não aplica janela de tempo e não replica o razão. Esses pontos estão
descritos no `README.md` e não são falhas escondidas.
