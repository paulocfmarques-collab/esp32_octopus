# Contribuindo

Obrigado por contribuir com o ESP32 Octopus.

## Fluxo sugerido

1. Faça um fork do projeto.
2. Crie uma branch para sua alteração.
3. Mantenha o código limpo, documentado e compatível com o projeto.
4. Teste a compilação e o comportamento esperado.
5. Abra um Pull Request com descrição clara.

## Padrões

- priorize clareza e legibilidade;
- mantenha nomes de funções e variáveis em português quando a base do projeto já estiver em português;
- documente alterações relevantes no README e/ou CHANGELOG;
- evite alterar a arquitetura sem necessidade e sem justificar a mudança.

## Reportando problemas

Abra uma issue com:

- descrição do problema;
- passos para reproduzir;
- placa ESP32 e ambiente utilizado;
- logs relevantes.

## Código e qualidade

- prefira funções pequenas e bem nomeadas;
- mantenha as regras de hardware e GPIO centralizadas em `defines.h`;
- valide entradas antes de aplicar alterações de rede, armazenamento ou OTA.
