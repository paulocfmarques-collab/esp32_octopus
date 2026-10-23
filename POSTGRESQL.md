# PostgreSQL e netscan — Octopus v1.4.3

## Confirmação da senha e do login

Abra **Configuração** em `/config` e informe as credenciais. **Salvar configuração**
confirma a gravação e a leitura de retorno das prefs, indicando se a senha foi
alterada, mantida (campo vazio) ou removida (checkbox). A indicação de senha salva
confirma apenas sua presença. Isso altera a senha usada pelo Octopus, não a
senha do usuário no servidor PostgreSQL.

**Salvar e testar login** salva primeiro e inicia teste em tarefa separada.
O estado abaixo do formulário mostra autenticação, execução de SELECT 1 e
leitura das colunas da tabela fixa `public.netscan`. Distingue login recusado de
login confirmado com tabela ausente/inacessível. O teste não modifica linhas e
não comprova autorização de INSERT/UPDATE; o envio real confirma a escrita.
Erros aparecem na Serial e `/log.txt` com data e hora.

O resultado também aparece em **Status** (`/`), que não inclui os formulários
de configuração. Os links do cabeçalho navegam entre as páginas. O teste impede
outra varredura e mudança de credenciais enquanto estiver ativo. Salvar invalida
o resultado anterior de login. Não há campos de banco ou tabela: **LogsRede**
e **public.netscan** são fixos. Ao carregar prefs antigas, o firmware preserva
servidor, porta, usuário e senha, adotando LogsRede no lugar do banco anterior.

Os eventos SQL gravados em `/log.txt` incluem data/hora local e uptime.
Antes de sincronizar o relógio, aparece `[SEM_NTP]` com uptime. O horário do log
é o do ESP32; `data_leitura` continua usando o relógio do PostgreSQL.

## Preparação

Execute [sql/netscan.sql](sql/netscan.sql) no banco **LogsRede**. O nome é enviado
com essa capitalização exata. Se for necessário criar o banco, use o identificador
entre aspas: `CREATE DATABASE "LogsRede";`. O firmware não cria o banco.
Se já existe
`public.netscan`, confirme que `mac` tem chave primária ou restrição UNIQUE,
que os tipos são compatíveis e que o usuário tem SELECT, INSERT e UPDATE.
O script CREATE IF NOT EXISTS não altera uma tabela existente. MACs de dados
antigos em maiúsculas precisam ser normalizados, resolvendo duplicidades antes;
o firmware passa a enviar somente minúsculas e não migra registros antigos.

No PostgreSQL, permita conexões TCP do IP do Octopus no `listen_addresses`,
firewall e `pg_hba.conf`. Use um usuário limitado à tabela. Não é necessário
criar API intermediária. Acesso direto TCP sem TLS; configuração `hostssl`
obrigatória não é suportada por este cliente.

O cliente aceita SCRAM-SHA-256 (4096 a 100000 iterações), MD5 e autenticação
trust já autorizada pelo servidor. Recusa solicitação de senha em texto claro.
SCRAM-SHA-256-PLUS e outros métodos não são suportados. Use nomes de banco,
usuário e senha ASCII; não há normalização SASLprep para senhas Unicode.
O protocolo segue a [documentação oficial PostgreSQL](https://www.postgresql.org/docs/current/protocol-flow.html)
e o [fluxo SASL/SCRAM](https://www.postgresql.org/docs/current/sasl-authentication.html).

## Configuração no painel

Na porta 80, abra **Configuração**, seção **Descoberta da rede e PostgreSQL**.
Configure servidor (padrão 192.168.0.20), porta (5432), usuário e senha. Banco
LogsRede e tabela public.netscan aparecem somente como informação fixa. Ative
**Enviar ao PostgreSQL** e salve. Envio começa desabilitado até configurar as
credenciais. A senha fica nas prefs; o status não a
expõe. Campo vazio mantém a senha, e o checkbox de apagar remove a salva.

Marque a agenda automática e escolha intervalo em minutos (1 a 1440, padrão 15).
As mesmas opções de agenda estão em `netscan_interval:15`, `netscan_auto:on` e
`netscan_auto:off`. Salvamento não reinicia o ESP32. `netscan` dispara coleta
manual; `netscan_status` mostra agenda, contagem e último resultado do banco.

## Dados e falhas

Cada varredura completa tenta salvar `/netscan.json` e, com SQL habilitado,
abre conexão e transação para inserir/atualizar todos os registros por MAC.
`ip`, `response_time`, `ping_replied`, `discovery` e `data_leitura` são atualizados
com `ON CONFLICT(mac) DO UPDATE`. `response_time` usa milissegundos ou NULL sem
resposta ICMP; MAC é minúsculo. `data_leitura=CURRENT_TIMESTAMP` usa o instante
da transação no servidor, não o timestamp NTP do ESP32.

Conexão tem timeout de 3 s; autenticação e cada troca SQL têm limite de 10 s,
statement_timeout 2 s e lock_timeout 1 s no servidor. O lote tem limite de 120 s
entre comandos (uma troca em andamento pode acrescentar até 10 s). A resolução
DNS do SDK pode exceder o timeout TCP; prefira o endereço IP configurado.
Mensagens de resposta são limitadas a 2047 bytes.

Falhas são registradas com etapa, servidor, banco e motivo, incluindo SQLSTATE
quando enviado pelo PostgreSQL. A senha é removida do texto antes do registro.
O log é `/log.txt`, também visível na Serial; não haver SD gravável impede apenas
a cópia persistente. O estado em RAM aparece no painel e comando de status.

Sem COMMIT, a desconexão desfaz a transação. Se a conexão cair durante a
confirmação do COMMIT, o estado final precisa ser conferido no servidor. O JSON
é independente do envio SQL. Falhas de gravação JSON não impedem tentativa SQL
de uma varredura completa. Não existe fila de reenvio histórico: a próxima
varredura enviará seus dados atuais. Erro de banco não altera o resultado JSON.

Compilação, autenticação real e inserção no servidor precisam ser validadas no
equipamento após configurar o banco. Nenhuma senha foi fornecida nesta conversa.
