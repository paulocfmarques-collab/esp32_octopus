#include "PgClient.h"
#include <lwip/sockets.h>
#include <errno.h>
#include <esp_random.h>
#include <mbedtls/md.h>
#include <mbedtls/base64.h>

namespace {
uint32_t ler32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
void por32(uint8_t* p, uint32_t n) { p[0] = n >> 24; p[1] = n >> 16; p[2] = n >> 8; p[3] = n; }
String base64(const uint8_t* dados, size_t n) {
    unsigned char out[128]; size_t len = 0;
    if (mbedtls_base64_encode(out, sizeof(out) - 1, &len, dados, n)) return "";
    out[len] = 0; return reinterpret_cast<char*>(out);
}
bool hmac(const uint8_t* key, size_t n, const String& texto, uint8_t* out) {
    return mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), key, n,
        reinterpret_cast<const uint8_t*>(texto.c_str()), texto.length(), out) == 0;
}
String hex(const uint8_t* dados, size_t n) {
    String out; out.reserve(n * 2);
    for (size_t i = 0; i < n; ++i) { char b[3]; snprintf(b, sizeof(b), "%02x", dados[i]); out += b; }
    return out;
}
String atributo(const String& msg, char chave) {
    for (size_t i = 0; i + 2 <= msg.length();) {
        int fim = msg.indexOf(',', i); if (fim < 0) fim = msg.length();
        if (msg[i] == chave && msg[i + 1] == '=') return msg.substring(i + 2, fim);
        i = fim + 1;
    }
    return "";
}
// PBKDF2-HMAC-SHA256 de um bloco (32 bytes), com pausas para o idle/watchdog.
bool derivar(const char* senha, const uint8_t* salt, size_t saltLen, uint32_t iteracoes, uint8_t* out) {
    mbedtls_md_context_t ctx; mbedtls_md_init(&ctx);
    bool ok = mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1) == 0;
    uint8_t u[32], proximo[32], indice[4] = {0, 0, 0, 1};
    if (ok) ok = mbedtls_md_hmac_starts(&ctx, reinterpret_cast<const uint8_t*>(senha), strlen(senha)) == 0 &&
        mbedtls_md_hmac_update(&ctx, salt, saltLen) == 0 && mbedtls_md_hmac_update(&ctx, indice, 4) == 0 && mbedtls_md_hmac_finish(&ctx, u) == 0;
    if (ok) memcpy(out, u, 32);
    for (uint32_t i = 1; ok && i < iteracoes; ++i) {
        ok = mbedtls_md_hmac_reset(&ctx) == 0 && mbedtls_md_hmac_update(&ctx, u, 32) == 0 && mbedtls_md_hmac_finish(&ctx, proximo) == 0;
        if (ok) { memcpy(u, proximo, 32); for (size_t j = 0; j < 32; ++j) out[j] ^= u[j]; }
        if ((i & 63) == 0) vTaskDelay(1);
    }
    mbedtls_md_free(&ctx); return ok;
}
}
bool PgClient::falha(const String& erro) { _erro = erro; _tcp.stop(); return false; }
bool PgClient::receber(void* buf, size_t n) {
    auto* p = static_cast<uint8_t*>(buf);
    while (n) {
        if (millis() - _inicio > 10000) return falha("Tempo limite aguardando resposta PostgreSQL.");
        int disponivel = _tcp.available();
        if (disponivel > 0) {
            int lidos = _tcp.read(p, n < size_t(disponivel) ? n : size_t(disponivel));
            if (lidos > 0) { p += lidos; n -= lidos; }
        } else if (!_tcp.connected()) return falha("Conexao PostgreSQL encerrada pelo servidor.");
        vTaskDelay(1);
    }
    return true;
}
bool PgClient::enviar(const void* buf, size_t n) {
    auto* p = static_cast<const uint8_t*>(buf);
    while (n) {
        if (millis() - _inicio > 10000) return falha("Tempo limite enviando ao PostgreSQL.");
        int r = lwip_send(_tcp.fd(), p, n, MSG_DONTWAIT);
        if (r > 0) { p += r; n -= r; }
        else if (r == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) return falha("Falha de rede enviando ao PostgreSQL.");
        vTaskDelay(1);
    }
    return true;
}
bool PgClient::pacote(char tipo, const void* dados, size_t n) {
    uint8_t header[5]; header[0] = tipo; por32(header + 1, n + 4);
    return enviar(header, sizeof(header)) && enviar(dados, n);
}
bool PgClient::mensagem(char& tipo) {
    uint8_t header[5];
    if (!receber(header, sizeof(header))) return false;
    tipo = header[0]; uint32_t len = ler32(header + 1);
    if (len < 4 || len - 4 >= sizeof(_dados)) return falha("Mensagem PostgreSQL invalida ou maior que o limite de 2047 bytes.");
    _tamanho = len - 4;
    if (!receber(_dados, _tamanho)) return false;
    _dados[_tamanho] = 0;
    if (tipo == 'E') {
        String codigo, detalhe;
        for (size_t i = 0; i < _tamanho && _dados[i];) {
            char campo = _dados[i++]; size_t comeco = i;
            while (i < _tamanho && _dados[i]) ++i;
            String valor(reinterpret_cast<char*>(_dados + comeco), i - comeco);
            if (campo == 'C') codigo = valor;
            if (campo == 'M') detalhe = valor;
            ++i;
        }
        return falha("PostgreSQL SQLSTATE " + codigo + ": " + detalhe);
    }
    return true;
}
bool PgClient::conectar(const char* host, uint16_t porta, const char* banco, const char* usuario, const char* senha) {
    _erro = ""; _inicio = millis();
    if (!_tcp.connect(host, porta, 3000)) return falha("Conexao TCP indisponivel, errno=" + String(errno) + " (confira endereco, porta, firewall e DNS).");
    uint8_t startup[512]{}; size_t pos = 8;
    auto campo = [&](const char* texto) { size_t n = strlen(texto) + 1; if (pos + n >= sizeof(startup)) return false; memcpy(startup + pos, texto, n); pos += n; return true; };
    por32(startup + 4, 196608);
    if (!campo("user") || !campo(usuario) || !campo("database") || !campo(banco) ||
        !campo("client_encoding") || !campo("UTF8") || !campo("application_name") || !campo("Octopus_netscan")) return falha("Configuracao PostgreSQL excede tamanho permitido.");
    startup[pos++] = 0; por32(startup, pos);
    if (!enviar(startup, pos)) return false;
    String nonce, firstBare, assinaturaServidor;
    int scram = 0; bool autenticado = false;
    for (;;) {
        char tipo;
        if (!mensagem(tipo)) return false;
        if (tipo == 'Z') return autenticado && (scram == 0 || scram == 3) ? true : falha("Autenticacao PostgreSQL incompleta.");
        if (tipo != 'R') continue;
        if (_tamanho < 4) return falha("Resposta de autenticacao invalida.");
        uint32_t metodo = ler32(_dados);
        if (metodo == 0) { autenticado = true; continue; }
        if (metodo == 5 && _tamanho == 8) {
            uint8_t digest[16], conjunto[36]; String inicial = String(senha) + usuario;
            auto* md5 = mbedtls_md_info_from_type(MBEDTLS_MD_MD5);
            if (!md5 || mbedtls_md(md5, reinterpret_cast<const uint8_t*>(inicial.c_str()), inicial.length(), digest)) return falha("MD5 indisponivel.");
            String primeiro = hex(digest, 16); memcpy(conjunto, primeiro.c_str(), 32); memcpy(conjunto + 32, _dados + 4, 4);
            if (mbedtls_md(md5, conjunto, sizeof(conjunto), digest)) return falha("Falha de autenticacao MD5.");
            String resposta = "md5" + hex(digest, 16);
            if (!pacote('p', resposta.c_str(), resposta.length() + 1)) return false;
        } else if (metodo == 10 && scram == 0) {
            bool suportado = false;
            for (size_t i = 4; i < _tamanho && _dados[i];) {
                size_t inicio = i; while (i < _tamanho && _dados[i]) ++i;
                if (String(reinterpret_cast<char*>(_dados + inicio), i - inicio) == "SCRAM-SHA-256") suportado = true;
                ++i;
            }
            if (!suportado) return falha("Servidor nao oferece SCRAM-SHA-256 sem channel binding.");
            uint8_t aleatorio[18]; esp_fill_random(aleatorio, sizeof(aleatorio)); nonce = base64(aleatorio, sizeof(aleatorio));
            firstBare = "n=,r=" + nonce; String first = "n,," + firstBare;
            uint8_t msg[256]; constexpr size_t mecanismo = sizeof("SCRAM-SHA-256");
            memcpy(msg, "SCRAM-SHA-256", mecanismo); por32(msg + mecanismo, first.length());
            memcpy(msg + mecanismo + 4, first.c_str(), first.length());
            if (!pacote('p', msg, mecanismo + 4 + first.length())) return false;
            scram = 1;
        } else if (metodo == 11 && scram == 1) {
            String serverFirst(reinterpret_cast<char*>(_dados + 4), _tamanho - 4);
            String r = atributo(serverFirst, 'r'), s = atributo(serverFirst, 's'), iter = atributo(serverFirst, 'i');
            uint32_t vezes = iter.toInt();
            if (!r.startsWith(nonce) || r.length() <= nonce.length() || r.length() > 256 ||
                vezes < 4096 || vezes > 100000 || iter != String(vezes) || !atributo(serverFirst, 'm').isEmpty()) return falha("Desafio SCRAM invalido ou custo nao suportado (4096 a 100000).");
            uint8_t salt[128], salted[32], clientKey[32], storedKey[32], signature[32], serverKey[32], serverSig[32]; size_t saltLen = 0;
            if (mbedtls_base64_decode(salt, sizeof(salt), &saltLen, reinterpret_cast<const uint8_t*>(s.c_str()), s.length()) || !saltLen) return falha("Salt SCRAM invalido.");
            // Senhas ASCII evitam diferencas SASLprep entre cliente e PostgreSQL.
            for (size_t i = 0; senha[i]; ++i) if (uint8_t(senha[i]) < 32 || uint8_t(senha[i]) > 126) return falha("Use senha PostgreSQL ASCII neste cliente SCRAM.");
            if (!derivar(senha, salt, saltLen, vezes, salted)) return falha("Falha PBKDF2 SCRAM.");
            String finalSemProva = "c=biws,r=" + r;
            String auth = firstBare + "," + serverFirst + "," + finalSemProva;
            if (!hmac(salted, 32, "Client Key", clientKey) || mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), clientKey, 32, storedKey) ||
                !hmac(storedKey, 32, auth, signature) || !hmac(salted, 32, "Server Key", serverKey) || !hmac(serverKey, 32, auth, serverSig)) return falha("Falha criptografica SCRAM.");
            for (size_t i = 0; i < 32; ++i) clientKey[i] ^= signature[i];
            assinaturaServidor = base64(serverSig, 32);
            String final = finalSemProva + ",p=" + base64(clientKey, 32);
            if (!pacote('p', final.c_str(), final.length())) return false;
            scram = 2;
        } else if (metodo == 12 && scram == 2) {
            String final(reinterpret_cast<char*>(_dados + 4), _tamanho - 4);
            if (atributo(final, 'v') != assinaturaServidor) return falha("Assinatura do servidor SCRAM invalida.");
            scram = 3;
        } else return falha("Metodo de autenticacao nao suportado. Use SCRAM-SHA-256 ou MD5; senha em texto claro recusada.");
    }
}
bool PgClient::executar(const String& sql) {
    _inicio = millis(); bool completo = false;
    if (!pacote('Q', sql.c_str(), sql.length() + 1)) return false;
    for (;;) {
        char tipo; if (!mensagem(tipo)) return false;
        if (tipo == 'C') completo = true;
        if (tipo == 'Z') return completo ? true : falha("PostgreSQL nao confirmou o comando SQL.");
    }
}
