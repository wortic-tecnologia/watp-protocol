# Como contribuir com o WATP

Obrigado pelo interesse. Antes de abrir um pull request:

1. Mudança de contrato (campo, tipo de mensagem, regra de hash ou de cadeia) começa por uma issue. O contrato em
   `WATP-CONTRATO-REAL.md` e as fixtures em `protocol/fixtures/` são a referência; uma mudança que altera o hash
   exige um novo snapshot, como descrito em `protocol/VERSIONING.md`.
2. Toda fixture nova entra com o resultado esperado. Rode `node scripts/watp-validate-contract-fixtures.mjs` e,
   no núcleo C, `./bin/watp-conformidade ../`.
3. No C: um domínio por arquivo, `-Wall -Wextra -Werror` sem aviso, e `make asan` limpo (AddressSanitizer e UBSan).
4. Não inclua segredo, token, chave ou dado de cliente em nenhum arquivo, nem em fixture.
5. Commits em português ou inglês, com mensagem que diga o que mudou e por quê.

Dúvidas sobre o protocolo: abra uma issue. Problemas de segurança: siga `SECURITY.md`.
