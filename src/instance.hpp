// instance.hpp
// Código compartilhado pelos executáveis (solver, astar, min_max, ri) e pelos testes:
//   - leitor JSON mínimo (sem dependências externas);
//   - leitura e validação de instâncias;
//   - escrita do JSON de resultado;
//   - verificador de sequências de movimentos (regras do BRP restrito, suposição A1);
//   - laço construtivo comum às heurísticas;
//   - linha de comando comum.
//
// Convenções: no JSON e nos resultados as pilhas são numeradas de 1 a W. Dentro do
// código as pilhas são índices 0..W-1 e cada pilha é um vetor da base para o topo.
#pragma once

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace brp {

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// JSON mínimo
// ---------------------------------------------------------------------------

struct Json {
    enum Tipo { Nulo, Booleano, Numero, Texto, Lista, Objeto };
    Tipo tipo = Nulo;
    bool booleano = false;
    double numero = 0.0;
    std::string texto;
    std::vector<Json> lista;
    std::vector<std::pair<std::string, Json>> objeto;

    // Devolve o campo de um objeto, ou nullptr se não existir.
    const Json* campo(const std::string& chave) const {
        for (const auto& par : objeto)
            if (par.first == chave) return &par.second;
        return nullptr;
    }
};

class AnalisadorJson {
public:
    explicit AnalisadorJson(const std::string& s) : s_(s) {}

    Json analisar() {
        // Ignora o BOM de UTF-8, se houver.
        if (s_.size() >= 3 && s_.compare(0, 3, "\xEF\xBB\xBF") == 0) p_ = 3;
        Json v = valor();
        espacos();
        if (p_ != s_.size()) erro("conteúdo inesperado após o fim do documento");
        return v;
    }

private:
    const std::string& s_;
    std::size_t p_ = 0;

    [[noreturn]] void erro(const std::string& msg) const {
        throw std::runtime_error("JSON inválido (posição " + std::to_string(p_) + "): " + msg);
    }

    void espacos() {
        while (p_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[p_]))) ++p_;
    }

    bool consome(const char* literal) {
        std::size_t n = std::char_traits<char>::length(literal);
        if (s_.compare(p_, n, literal) == 0) { p_ += n; return true; }
        return false;
    }

    Json valor() {
        espacos();
        if (p_ >= s_.size()) erro("fim inesperado");
        Json v;
        char c = s_[p_];
        if (c == '{') {
            v.tipo = Json::Objeto;
            ++p_;
            espacos();
            if (p_ < s_.size() && s_[p_] == '}') { ++p_; return v; }
            while (true) {
                espacos();
                if (p_ >= s_.size() || s_[p_] != '"') erro("esperava o nome de um campo");
                std::string chave = cadeia();
                espacos();
                if (p_ >= s_.size() || s_[p_] != ':') erro("esperava ':'");
                ++p_;
                Json filho = valor();
                v.objeto.emplace_back(std::move(chave), std::move(filho));
                espacos();
                if (p_ < s_.size() && s_[p_] == ',') { ++p_; continue; }
                if (p_ < s_.size() && s_[p_] == '}') { ++p_; break; }
                erro("esperava ',' ou '}'");
            }
        } else if (c == '[') {
            v.tipo = Json::Lista;
            ++p_;
            espacos();
            if (p_ < s_.size() && s_[p_] == ']') { ++p_; return v; }
            while (true) {
                v.lista.push_back(valor());
                espacos();
                if (p_ < s_.size() && s_[p_] == ',') { ++p_; continue; }
                if (p_ < s_.size() && s_[p_] == ']') { ++p_; break; }
                erro("esperava ',' ou ']'");
            }
        } else if (c == '"') {
            v.tipo = Json::Texto;
            v.texto = cadeia();
        } else if (consome("true")) {
            v.tipo = Json::Booleano;
            v.booleano = true;
        } else if (consome("false")) {
            v.tipo = Json::Booleano;
            v.booleano = false;
        } else if (consome("null")) {
            v.tipo = Json::Nulo;
        } else {
            const char* inicio = s_.c_str() + p_;
            char* fim = nullptr;
            double d = std::strtod(inicio, &fim);
            if (fim == inicio) erro("valor não reconhecido");
            p_ += static_cast<std::size_t>(fim - inicio);
            v.tipo = Json::Numero;
            v.numero = d;
        }
        return v;
    }

    std::string cadeia() {
        ++p_;  // aspas de abertura
        std::string r;
        while (true) {
            if (p_ >= s_.size()) erro("texto sem aspas de fechamento");
            char c = s_[p_++];
            if (c == '"') break;
            if (c != '\\') { r += c; continue; }
            if (p_ >= s_.size()) erro("escape incompleto");
            char e = s_[p_++];
            switch (e) {
                case '"': r += '"'; break;
                case '\\': r += '\\'; break;
                case '/': r += '/'; break;
                case 'b': r += '\b'; break;
                case 'f': r += '\f'; break;
                case 'n': r += '\n'; break;
                case 'r': r += '\r'; break;
                case 't': r += '\t'; break;
                case 'u': {
                    if (p_ + 4 > s_.size()) erro("escape \\u incompleto");
                    unsigned cp = static_cast<unsigned>(std::stoul(s_.substr(p_, 4), nullptr, 16));
                    p_ += 4;
                    // Codifica em UTF-8 (plano básico; suficiente para nomes de instância).
                    if (cp < 0x80) {
                        r += static_cast<char>(cp);
                    } else if (cp < 0x800) {
                        r += static_cast<char>(0xC0 | (cp >> 6));
                        r += static_cast<char>(0x80 | (cp & 0x3F));
                    } else {
                        r += static_cast<char>(0xE0 | (cp >> 12));
                        r += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        r += static_cast<char>(0x80 | (cp & 0x3F));
                    }
                    break;
                }
                default: erro("escape desconhecido");
            }
        }
        return r;
    }
};

inline std::string lerArquivo(const fs::path& caminho) {
    std::ifstream f(caminho, std::ios::binary);
    if (!f) throw std::runtime_error("não foi possível abrir " + caminho.string());
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

inline Json lerJson(const fs::path& caminho) {
    std::string conteudo = lerArquivo(caminho);
    return AnalisadorJson(conteudo).analisar();
}

inline std::string escaparJson(const std::string& s) {
    std::string r;
    for (char c : s) {
        switch (c) {
            case '"': r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n"; break;
            case '\r': r += "\\r"; break;
            case '\t': r += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    r += buf;
                } else {
                    r += c;
                }
        }
    }
    return r;
}

// ---------------------------------------------------------------------------
// Instância
// ---------------------------------------------------------------------------

using Baia = std::vector<std::vector<int>>;  // baia[i] = pilha i, da base para o topo

struct Instancia {
    std::string nome;     // nome para exibição
    std::string chave;    // identificador único; nome do arquivo de resultado (sem .json)
    std::string arquivo;  // nome do arquivo de origem (ex.: brp_W3_H4_N8.json)
    int id = 0;           // "id" dentro do arquivo; 0 = arquivo com uma única instância
    int W = 0;            // número de pilhas
    int H = 0;            // altura máxima
    int N = 0;            // número de blocos
    Baia pilhas;
};

inline bool ehInteiro(const Json& j) {
    return j.tipo == Json::Numero && std::floor(j.numero) == j.numero && std::fabs(j.numero) < 1e9;
}

// Converte o campo "stacks" em pilhas e valida: nenhuma pilha acima de H e
// prioridades exatamente 1..N, sem repetição. `onde` identifica a instância nas
// mensagens de erro.
inline void preencherPilhas(Instancia& inst, const Json* pilhas, const std::string& onde) {
    auto falha = [&](const std::string& msg) { return std::runtime_error(onde + ": " + msg); };
    if (!pilhas || pilhas->tipo != Json::Lista) throw falha("campo \"stacks\" ausente ou não é uma lista");
    if (static_cast<int>(pilhas->lista.size()) > inst.W)
        throw falha("\"stacks\" tem " + std::to_string(pilhas->lista.size()) + " pilhas, mas W = " +
                    std::to_string(inst.W));
    inst.N = 0;
    inst.pilhas.assign(inst.W, {});
    for (std::size_t i = 0; i < pilhas->lista.size(); ++i) {
        const Json& p = pilhas->lista[i];
        if (p.tipo != Json::Lista) throw falha("pilha " + std::to_string(i + 1) + " não é uma lista");
        if (static_cast<int>(p.lista.size()) > inst.H)
            throw falha("pilha " + std::to_string(i + 1) + " tem " + std::to_string(p.lista.size()) +
                        " blocos, acima de H = " + std::to_string(inst.H));
        for (const Json& b : p.lista) {
            if (!ehInteiro(b)) throw falha("pilha " + std::to_string(i + 1) + " contém valor não inteiro");
            inst.pilhas[i].push_back(static_cast<int>(b.numero));
            ++inst.N;
        }
    }
    if (inst.N < 1) throw falha("a instância não tem blocos");

    std::vector<char> visto(inst.N + 1, 0);
    for (const auto& p : inst.pilhas) {
        for (int b : p) {
            if (b < 1 || b > inst.N)
                throw falha("prioridade " + std::to_string(b) + " fora do intervalo 1.." + std::to_string(inst.N));
            if (visto[b]) throw falha("prioridade " + std::to_string(b) + " repetida");
            visto[b] = 1;
        }
    }
}

// Lê e valida todas as instâncias de um arquivo. Dois esquemas são aceitos:
//
// (a) Arquivo com várias instâncias da mesma configuração (pasta instances/):
//       { "metadata": { "W": 3, "H": 4, "N": 8, "total_instancias": 10, "semente": 2374 },
//         "instancias": [ { "id": 1, "stacks": [[8,7,6],[2],[1,3,4,5]] }, ... ] }
//     W, H e N vêm de "metadata" e valem para todas. Cada "stacks" deve ter exatamente
//     W pilhas e N blocos. A chave de cada instância é <arquivo>_id<id com 2 dígitos>,
//     por exemplo brp_W3_H4_N8_id01.
//
// (b) Arquivo com uma única instância (usado nos testes):
//       { "name": "ex", "W": 3, "H": 4, "stacks": [[8,7,6],[2],[1,3,4,5]] }
//     "name" e "W" são opcionais. A chave é o nome do arquivo.
//
// Em ambos, cada pilha é listada da base para o topo e stacks[0] é a pilha 1.
inline std::vector<Instancia> lerInstancias(const fs::path& caminho) {
    const std::string origem = "instância " + caminho.string();
    auto falha = [&](const std::string& msg) { return std::runtime_error(origem + ": " + msg); };
    auto inteiroPositivo = [&](const Json& obj, const char* chave, const char* dono) {
        const Json* c = obj.campo(chave);
        if (!c || !ehInteiro(*c) || c->numero < 1)
            throw falha(std::string("campo \"") + chave + "\" de " + dono + " ausente ou inválido (inteiro >= 1)");
        return static_cast<int>(c->numero);
    };

    Json raiz;
    try {
        raiz = lerJson(caminho);
    } catch (const std::exception& e) {
        throw falha(e.what());
    }
    if (raiz.tipo != Json::Objeto) throw falha("a raiz do JSON deve ser um objeto");

    std::vector<Instancia> todas;
    const std::string base = caminho.stem().string();

    if (const Json* lista = raiz.campo("instancias")) {
        // ---- esquema (a): várias instâncias por arquivo ----
        if (lista->tipo != Json::Lista) throw falha("campo \"instancias\" não é uma lista");
        const Json* meta = raiz.campo("metadata");
        if (!meta || meta->tipo != Json::Objeto) throw falha("campo \"metadata\" ausente ou não é um objeto");
        const int W = inteiroPositivo(*meta, "W", "metadata");
        const int H = inteiroPositivo(*meta, "H", "metadata");
        const int N = inteiroPositivo(*meta, "N", "metadata");
        if (const Json* total = meta->campo("total_instancias"))
            if (ehInteiro(*total) && static_cast<std::size_t>(total->numero) != lista->lista.size())
                throw falha("\"total_instancias\" difere do tamanho da lista \"instancias\"");

        std::vector<int> ids;
        for (std::size_t e = 0; e < lista->lista.size(); ++e) {
            const Json& item = lista->lista[e];
            if (item.tipo != Json::Objeto) throw falha("elemento " + std::to_string(e + 1) + " de \"instancias\" não é um objeto");
            Instancia inst;
            inst.id = inteiroPositivo(item, "id", "instância");
            if (std::find(ids.begin(), ids.end(), inst.id) != ids.end())
                throw falha("id " + std::to_string(inst.id) + " repetido");
            ids.push_back(inst.id);
            char sufixo[32];
            std::snprintf(sufixo, sizeof sufixo, "_id%02d", inst.id);
            inst.chave = base + sufixo;
            inst.nome = inst.chave;
            inst.arquivo = caminho.filename().string();
            inst.W = W;
            inst.H = H;
            const std::string onde = origem + ", id " + std::to_string(inst.id);
            const Json* pilhas = item.campo("stacks");
            if (pilhas && pilhas->tipo == Json::Lista && static_cast<int>(pilhas->lista.size()) != W)
                throw std::runtime_error(onde + ": \"stacks\" tem " + std::to_string(pilhas->lista.size()) +
                                         " pilhas, mas metadata.W = " + std::to_string(W));
            preencherPilhas(inst, pilhas, onde);
            if (inst.N != N)
                throw std::runtime_error(onde + ": tem " + std::to_string(inst.N) + " blocos, mas metadata.N = " +
                                         std::to_string(N));
            todas.push_back(std::move(inst));
        }
        if (todas.empty()) throw falha("a lista \"instancias\" está vazia");
        return todas;
    }

    // ---- esquema (b): uma instância por arquivo ----
    Instancia inst;
    inst.chave = base;
    inst.nome = base;
    inst.arquivo = caminho.filename().string();
    if (const Json* nome = raiz.campo("name"); nome && nome->tipo == Json::Texto && !nome->texto.empty())
        inst.nome = nome->texto;
    inst.H = inteiroPositivo(raiz, "H", "instância");
    const Json* pilhas = raiz.campo("stacks");
    if (raiz.campo("W")) inst.W = inteiroPositivo(raiz, "W", "instância");
    else if (pilhas && pilhas->tipo == Json::Lista) inst.W = static_cast<int>(pilhas->lista.size());
    if (inst.W < 1) throw falha("a baia precisa de pelo menos uma pilha");
    preencherPilhas(inst, pilhas, origem);
    todas.push_back(std::move(inst));
    return todas;
}

// Lê uma instância específica: `id` > 0 escolhe pelo "id"; id = 0 exige que o
// arquivo tenha uma única instância.
inline Instancia lerInstanciaUnica(const fs::path& caminho, int id = 0) {
    std::vector<Instancia> todas = lerInstancias(caminho);
    if (id == 0) {
        if (todas.size() != 1)
            throw std::runtime_error(caminho.string() + " tem " + std::to_string(todas.size()) +
                                     " instâncias; informe o id");
        return todas[0];
    }
    for (const Instancia& i : todas)
        if (i.id == id) return i;
    throw std::runtime_error(caminho.string() + ": não há instância com id " + std::to_string(id));
}

// Lista os arquivos de instância: o próprio arquivo, ou todos os .json de uma pasta
// (em ordem). Cada arquivo pode conter várias instâncias (ver lerInstancias).
inline std::vector<fs::path> listarInstancias(const fs::path& entrada) {
    std::vector<fs::path> arquivos;
    if (fs::is_directory(entrada)) {
        for (const auto& e : fs::directory_iterator(entrada))
            if (e.is_regular_file() && e.path().extension() == ".json") arquivos.push_back(e.path());
        std::sort(arquivos.begin(), arquivos.end());
    } else if (fs::is_regular_file(entrada)) {
        arquivos.push_back(entrada);
    } else {
        throw std::runtime_error("entrada não encontrada: " + entrada.string());
    }
    return arquivos;
}

// ---------------------------------------------------------------------------
// Movimentos, verificação e resultado
// ---------------------------------------------------------------------------

struct Movimento {
    int bloco = 0;
    int origem = 0;   // pilha de origem, 1..W
    int destino = 0;  // pilha de destino, 1..W
    int periodo = 0;  // período (= bloco-alvo) em que a relocação acontece
};

// Executa a sequência sobre a instância e confere as regras do BRP restrito (A1):
//   - em cada período t o alvo é o bloco t;
//   - só se move o bloco do topo de uma pilha;
//   - só se movem blocos que estão acima do alvo (logo, na pilha do alvo);
//   - o destino é outra pilha, com altura menor que H;
//   - ao fim dos movimentos do período, o alvo está no topo e é retirado.
// Devolve true se a sequência é executável e esvazia a baia; senão, explica em `erro`.
inline bool verificarSequencia(const Instancia& inst, const std::vector<Movimento>& movs, std::string& erro) {
    Baia baia = inst.pilhas;
    std::vector<int> pilhaDe(inst.N + 1, -1);
    for (int i = 0; i < inst.W; ++i)
        for (int b : baia[i]) pilhaDe[b] = i;

    std::size_t m = 0;
    for (int t = 1; t <= inst.N; ++t) {
        int s = pilhaDe[t];
        while (m < movs.size() && movs[m].periodo == t) {
            const Movimento& mv = movs[m];
            std::string onde = "movimento " + std::to_string(m + 1) + " (bloco " + std::to_string(mv.bloco) +
                               ", período " + std::to_string(t) + "): ";
            if (mv.origem < 1 || mv.origem > inst.W || mv.destino < 1 || mv.destino > inst.W) {
                erro = onde + "pilha fora de 1..W";
                return false;
            }
            int o = mv.origem - 1, d = mv.destino - 1;
            if (o != s) {
                erro = onde + "a origem não é a pilha do alvo (viola A1)";
                return false;
            }
            if (baia[o].empty() || baia[o].back() != mv.bloco) {
                erro = onde + "o bloco não está no topo da pilha de origem";
                return false;
            }
            if (mv.bloco == t) {
                erro = onde + "o próprio alvo foi relocado (viola A1)";
                return false;
            }
            if (d == o) {
                erro = onde + "destino igual à origem";
                return false;
            }
            if (static_cast<int>(baia[d].size()) >= inst.H) {
                erro = onde + "pilha de destino cheia (viola H)";
                return false;
            }
            baia[o].pop_back();
            baia[d].push_back(mv.bloco);
            pilhaDe[mv.bloco] = d;
            ++m;
        }
        if (baia[s].empty() || baia[s].back() != t) {
            erro = "período " + std::to_string(t) + ": o alvo não está no topo na hora da retirada";
            return false;
        }
        baia[s].pop_back();
    }
    if (m != movs.size()) {
        erro = "movimento " + std::to_string(m + 1) + " tem período fora de ordem ou inexistente";
        return false;
    }
    return true;
}

struct Resultado {
    std::string instancia;         // chave da instância
    std::string arquivo;           // arquivo de origem
    int id = 0;                    // id dentro do arquivo (0 = arquivo de instância única)
    std::string metodo;
    int W = 0, H = 0, N = 0;
    bool temSolucao = true;        // false: nenhuma solução viável encontrada
    long long relocacoes = 0;
    double tempoCpu = 0.0;         // segundos de CPU do processo (soma das threads)
    double tempoParede = 0.0;      // segundos de relógio
    std::vector<Movimento> movimentos;

    // Campos gravados apenas pelos métodos exatos.
    //   doSolver: exato por PLIM (solver.cpp); grava solver, presolve, status, limite e "gap_mip".
    //   doBusca:  exato por busca (astar.cpp); grava status, limite e "gap" (sem solver/presolve).
    bool doSolver = false;
    bool doBusca = false;
    std::string solver;            // solver de PLIM usado, com a versão (ex.: "highs 1.16.0")
    std::string presolve;          // presolve usado: on | off | padrao
    std::string status;            // optimal | time_limit | infeasible | other (A*: também node_limit)
    bool otimoProvado = false;
    bool temLimite = false;
    double limiteInferior = 0.0;
    bool temGap = false;
    double gap = 0.0;

    // Campos extras específicos do método, gravados como estão: pares (chave, valor
    // JSON já formatado). Ex.: {"nos_expandidos", "14"}.
    std::vector<std::pair<std::string, std::string>> extras;
};

// Resultado já com a identificação da instância e do método.
inline Resultado resultadoBase(const Instancia& inst, const std::string& metodo) {
    Resultado r;
    r.instancia = inst.chave;
    r.arquivo = inst.arquivo;
    r.id = inst.id;
    r.metodo = metodo;
    r.W = inst.W;
    r.H = inst.H;
    r.N = inst.N;
    return r;
}

inline void gravarResultado(const Resultado& r, const fs::path& arquivo) {
    if (arquivo.has_parent_path()) fs::create_directories(arquivo.parent_path());
    std::ofstream f(arquivo);
    if (!f) throw std::runtime_error("não foi possível gravar " + arquivo.string());
    char buf[64];
    f << "{\n";
    f << "  \"instancia\": \"" << escaparJson(r.instancia) << "\",\n";
    f << "  \"arquivo\": \"" << escaparJson(r.arquivo) << "\",\n";
    if (r.id > 0) f << "  \"id\": " << r.id << ",\n";
    else f << "  \"id\": null,\n";
    f << "  \"metodo\": \"" << escaparJson(r.metodo) << "\",\n";
    f << "  \"W\": " << r.W << ",\n  \"H\": " << r.H << ",\n  \"N\": " << r.N << ",\n";
    // num_movimentos = relocações feitas (o mesmo valor de "relocacoes", que é o
    // objetivo). retiradas = N (não contam como movimento no BRP).
    // operacoes_totais = relocações + retiradas, isto é, tudo o que o guindaste fez.
    if (r.temSolucao) {
        f << "  \"relocacoes\": " << r.relocacoes << ",\n";
        f << "  \"num_movimentos\": " << r.relocacoes << ",\n";
        f << "  \"retiradas\": " << r.N << ",\n";
        f << "  \"operacoes_totais\": " << (r.relocacoes + r.N) << ",\n";
    } else {
        f << "  \"relocacoes\": null,\n";
        f << "  \"num_movimentos\": null,\n";
        f << "  \"retiradas\": null,\n";
        f << "  \"operacoes_totais\": null,\n";
    }
    std::snprintf(buf, sizeof buf, "%.9f", r.tempoCpu);
    f << "  \"tempo_cpu_s\": " << buf << ",\n";
    std::snprintf(buf, sizeof buf, "%.9f", r.tempoParede);
    f << "  \"tempo_parede_s\": " << buf << ",\n";
    if (r.doSolver) {
        f << "  \"solver\": \"" << escaparJson(r.solver) << "\",\n";
        f << "  \"presolve\": \"" << escaparJson(r.presolve) << "\",\n";
    }
    if (r.doSolver || r.doBusca) {
        f << "  \"status\": \"" << escaparJson(r.status) << "\",\n";
        f << "  \"otimo_provado\": " << (r.otimoProvado ? "true" : "false") << ",\n";
        if (r.temLimite) {
            std::snprintf(buf, sizeof buf, "%.6f", r.limiteInferior);
            f << "  \"limite_inferior\": " << buf << ",\n";
        } else {
            f << "  \"limite_inferior\": null,\n";
        }
        const char* chaveGap = r.doSolver ? "gap_mip" : "gap";
        if (r.temGap) {
            std::snprintf(buf, sizeof buf, "%.6f", r.gap);
            f << "  \"" << chaveGap << "\": " << buf << ",\n";
        } else {
            f << "  \"" << chaveGap << "\": null,\n";
        }
    }
    for (const auto& extra : r.extras) f << "  \"" << escaparJson(extra.first) << "\": " << extra.second << ",\n";
    // Sequência compacta: pares [origem, destino], pilhas a partir de 1; as retiradas
    // ficam implícitas (o alvo sai assim que chega ao topo).
    f << "  \"sequencia\": [";
    for (std::size_t i = 0; i < r.movimentos.size(); ++i)
        f << (i ? ", " : "") << "[" << r.movimentos[i].origem << ", " << r.movimentos[i].destino << "]";
    f << "],\n";
    f << "  \"movimentos\": [";
    for (std::size_t i = 0; i < r.movimentos.size(); ++i) {
        const Movimento& m = r.movimentos[i];
        f << (i ? ",\n    " : "\n    ") << "{\"bloco\": " << m.bloco << ", \"origem\": " << m.origem
          << ", \"destino\": " << m.destino << ", \"periodo\": " << m.periodo << "}";
    }
    f << (r.movimentos.empty() ? "]\n" : "\n  ]\n");
    f << "}\n";
}

// ---------------------------------------------------------------------------
// Tempo
// ---------------------------------------------------------------------------

// Tempo de CPU do processo, em segundos (soma de todas as threads).
inline double tempoCpuSeg() {
#if defined(CLOCK_PROCESS_CPUTIME_ID)
    timespec ts;
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) == 0)
        return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) * 1e-9;
#endif
    return static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
}

inline double tempoParedeSeg() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// ---------------------------------------------------------------------------
// Linha de comando comum
// ---------------------------------------------------------------------------

struct OpcoesCli {
    fs::path entrada;
    fs::path saida;
    double limiteTempo = -1.0;        // segundos; < 0 = padrão do executável
    int id = 0;                       // > 0: processa só a instância com esse "id" de cada arquivo
    bool restricao6Original = false;  // só o solver: usa a (6) do artigo original (demonstração)
    bool verboso = false;             // só o solver: mostra o log do solver de PLIM
    int threads = 0;                  // só o solver: 0 = padrão do solver de PLIM
    std::string solver;               // só o solver: gurobi | cplex | highs ("" = primeiro disponível)
    std::string presolve;             // só o solver: on | off | padrao ("" = padrão do executável)
    bool listarSolvers = false;       // só o solver: lista os solvers compilados e sai
};

inline fs::path pastaResultadosPadrao(const std::string& metodo) {
#ifdef BRP_RAIZ_PROJETO
    return fs::path(BRP_RAIZ_PROJETO) / "results" / metodo;
#else
    return fs::path("results") / metodo;
#endif
}

inline OpcoesCli lerArgumentos(int argc, char** argv, const std::string& metodo, bool ehSolver) {
    auto uso = [&]() {
        std::string u = std::string("uso: ") + argv[0] + " <instância.json | pasta> [--time-limit s] [--out pasta] [--id n]";
        if (ehSolver)
            u += " [--solver gurobi|cplex|highs] [--presolve on|off|padrao] [--threads n] [--verbose]"
                 " [--restricao6-original]"
                 "\n     " + std::string(argv[0]) + " --listar-solvers";
        return u;
    };
    OpcoesCli op;
    op.saida = pastaResultadosPadrao(metodo);
    bool temEntrada = false;
    for (int a = 1; a < argc; ++a) {
        std::string arg = argv[a];
        auto proximo = [&]() -> std::string {
            if (a + 1 >= argc) throw std::runtime_error("falta o valor de " + arg + "\n" + uso());
            return argv[++a];
        };
        if (arg == "--time-limit") {
            op.limiteTempo = std::stod(proximo());
        } else if (arg == "--out") {
            op.saida = proximo();
        } else if (arg == "--id") {
            op.id = std::stoi(proximo());
        } else if (ehSolver && arg == "--threads") {
            op.threads = std::stoi(proximo());
        } else if (ehSolver && arg == "--solver") {
            op.solver = proximo();
        } else if (ehSolver && arg == "--presolve") {
            op.presolve = proximo();
            if (op.presolve != "on" && op.presolve != "off" && op.presolve != "padrao")
                throw std::runtime_error("--presolve aceita on, off ou padrao\n" + uso());
        } else if (ehSolver && arg == "--listar-solvers") {
            op.listarSolvers = true;
        } else if (ehSolver && arg == "--verbose") {
            op.verboso = true;
        } else if (ehSolver && arg == "--restricao6-original") {
            op.restricao6Original = true;
        } else if (!arg.empty() && arg[0] == '-') {
            throw std::runtime_error("opção desconhecida: " + arg + "\n" + uso());
        } else if (!temEntrada) {
            op.entrada = arg;
            temEntrada = true;
        } else {
            throw std::runtime_error("argumento inesperado: " + arg + "\n" + uso());
        }
    }
    if (!temEntrada && !op.listarSolvers) throw std::runtime_error(uso());
    return op;
}

// ---------------------------------------------------------------------------
// Laço construtivo comum às heurísticas (restrito a A1, sem retrocesso)
// ---------------------------------------------------------------------------

// Para t = 1..N: enquanto o bloco t não estiver no topo, reloca o bloco r do topo da
// pilha dele para a pilha devolvida por `escolher`; depois retira t.
//
// `escolher(baia, candidatas, r, N)` recebe as pilhas candidatas (não cheias e
// diferentes da origem, em ordem crescente de índice) e devolve uma delas.
template <class Escolha>
std::vector<Movimento> heuristicaConstrutiva(const Instancia& inst, Escolha escolher) {
    Baia baia = inst.pilhas;
    std::vector<int> pilhaDe(inst.N + 1, -1);
    for (int i = 0; i < inst.W; ++i)
        for (int b : baia[i]) pilhaDe[b] = i;

    std::vector<Movimento> movs;
    std::vector<int> candidatas;
    for (int t = 1; t <= inst.N; ++t) {
        int s = pilhaDe[t];
        while (baia[s].back() != t) {
            int r = baia[s].back();
            candidatas.clear();
            for (int i = 0; i < inst.W; ++i)
                if (i != s && static_cast<int>(baia[i].size()) < inst.H) candidatas.push_back(i);
            if (candidatas.empty())
                throw std::runtime_error("sem pilha de destino para o bloco " + std::to_string(r) +
                                         " no período " + std::to_string(t) + " (todas cheias)");
            int d = escolher(baia, candidatas, r, inst.N);
            if (std::find(candidatas.begin(), candidatas.end(), d) == candidatas.end())
                throw std::logic_error("a regra de escolha devolveu uma pilha que não é candidata");
            baia[s].pop_back();
            baia[d].push_back(r);
            pilhaDe[r] = d;
            movs.push_back({r, s + 1, d + 1, t});
        }
        baia[s].pop_back();  // retirada do alvo
    }
    return movs;
}

// main comum das heurísticas: lê as instâncias, roda, confere a sequência e grava.
template <class Escolha>
int mainHeuristica(int argc, char** argv, const std::string& metodo, Escolha escolher) {
    OpcoesCli op;
    std::vector<fs::path> arquivos;
    try {
        op = lerArgumentos(argc, argv, metodo, false);
        arquivos = listarInstancias(op.entrada);
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 2;
    }
    // --time-limit é aceito por uniformidade, mas a heurística é construtiva e
    // termina em tempo polinomial; o limite não é usado.

    int falhas = 0;
    for (const fs::path& arq : arquivos) {
        std::vector<Instancia> instancias;
        try {
            instancias = lerInstancias(arq);
        } catch (const std::exception& e) {
            std::cerr << "ERRO: " << e.what() << "\n";
            ++falhas;
            continue;
        }
        for (const Instancia& inst : instancias) {
            if (op.id > 0 && inst.id != op.id) continue;
            try {
                double p0 = tempoParedeSeg(), c0 = tempoCpuSeg();
                std::vector<Movimento> movs = heuristicaConstrutiva(inst, escolher);
                double c1 = tempoCpuSeg(), p1 = tempoParedeSeg();

                std::string erro;
                if (!verificarSequencia(inst, movs, erro))
                    throw std::logic_error("sequência gerada não é executável: " + erro);

                Resultado r = resultadoBase(inst, metodo);
                r.relocacoes = static_cast<long long>(movs.size());
                r.tempoCpu = c1 - c0;
                r.tempoParede = p1 - p0;
                r.movimentos = std::move(movs);
                gravarResultado(r, op.saida / (inst.chave + ".json"));
                std::printf("%-28s %-8s relocações = %lld  cpu = %.6fs\n", inst.chave.c_str(), metodo.c_str(),
                            r.relocacoes, r.tempoCpu);
            } catch (const std::exception& e) {
                std::cerr << "ERRO em " << inst.chave << ": " << e.what() << "\n";
                ++falhas;
            }
        }
    }
    return falhas ? 1 : 0;
}

}  // namespace brp
