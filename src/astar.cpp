// astar.cpp — main 4: método exato por busca informada A* para o BRP restrito (A1),
// como descrito nos slides do módulo 2 de IC0024 (aula "Algoritmo A*").
//
// Espaço de estados: cada nó é uma "fotografia do pátio". Sob A1, quando o alvo está
// no topo nada pode ser relocado, então a retirada é forçada e tem custo 0; os estados
// são guardados já com todas as retiradas possíveis feitas (como em tests/bfs.cpp).
// Cada aresta é uma relocação (custo 1): o bloco do topo da pilha do alvo vai para
// outra pilha não cheia.
//
// Função de avaliação: f(n) = g(n) + h(n)
//   g(n) = número de relocações (reshuffles) já feitas até o estado n;
//   h(n) = número de blocos "mal posicionados" (bloqueadores): blocos que estão acima
//          de algum bloco de número menor na mesma pilha. Cada um terá de ser movido
//          pelo menos uma vez, logo h nunca superestima o custo real (admissível).
//          h também é consistente: uma relocação tira o bloco movido da contagem da
//          origem (ele estava acima do alvo) e o acrescenta no máximo uma vez no
//          destino; as retiradas não mudam a contagem. Logo h(n) <= 1 + h(n').
//
// Fila de prioridade ordenada por f; empate em f: maior g (critério do slide 24);
// persistindo o empate: ordem de inserção (FIFO). O objetivo é testado quando o nó é
// retirado da fila (expandido), o que garante a otimalidade com h admissível.
//
// Critérios de poda (slide 21):
//   1. estados repetidos: se a configuração já foi alcançada com g menor ou igual, o
//      novo nó é descartado (se chegar com g menor, o nó existente é atualizado);
//   2. infactibilidade: não se gera movimento para pilha cheia (altura H);
//   3. limite superior: antes da busca roda-se a heurística Min-Max; seu número de
//      relocações é o UB e todo nó gerado com f(n) >= UB é descartado. Se a fila
//      esvaziar sem achar solução melhor, a solução da Min-Max é ótima. Desligue com
//      --sem-ub para rodar o A* puro.
//
// Limites: --time-limit s (padrão 3600) e --max-nos n (padrão 5 000 000 de estados
// distintos guardados, cerca de 1 GB). Ao atingir um limite, grava o limite inferior
// (menor f da fila) e, se houver UB, a solução da Min-Max como melhor solução conhecida,
// com status time_limit ou node_limit, sem declarar otimalidade.
//
// Uso: ./astar <instância.json | pasta> [--time-limit s] [--out pasta] [--id n]
//                                       [--sem-ub] [--max-nos n]
#include <cstdint>
#include <limits>
#include <queue>
#include <unordered_set>

#include "instance.hpp"
#include "regras.hpp"

namespace {

const double LIMITE_TEMPO_PADRAO = 3600.0;      // segundos
const std::size_t MAX_NOS_PADRAO = 5000000;     // estados distintos guardados
const int SEM_UB = std::numeric_limits<int>::max();

struct OpcoesAstar {
    bool usarUB = true;
    std::size_t maxNos = MAX_NOS_PADRAO;
};

// ---------------------------------------------------------------------------
// Estado
// ---------------------------------------------------------------------------

// Codificação compacta da baia: as prioridades de cada pilha (da base para o topo) como
// bytes, com '\0' separando as pilhas (prioridades começam em 1).
std::string codificar(const brp::Baia& baia) {
    std::string s;
    for (const auto& p : baia) {
        for (int b : p) s += static_cast<char>(b);
        s += '\0';
    }
    return s;
}

brp::Baia decodificar(const std::string& s, int W) {
    brp::Baia baia(W);
    int i = 0;
    for (char c : s) {
        if (c == '\0') ++i;
        else baia[i].push_back(static_cast<unsigned char>(c));
    }
    return baia;
}

// Retira o alvo enquanto ele estiver no topo de alguma pilha (custo 0). Devolve o novo alvo.
int retirarEnquantoPossivel(brp::Baia& baia, int t, int N) {
    bool retirou = true;
    while (retirou && t <= N) {
        retirou = false;
        for (auto& p : baia)
            if (!p.empty() && p.back() == t) {
                p.pop_back();
                ++t;
                retirou = true;
                break;
            }
    }
    return t;
}

// h(n): número de bloqueadores, isto é, blocos acima de algum bloco de número menor.
int heuristica(const brp::Baia& baia) {
    int h = 0;
    for (const auto& p : baia) {
        int menor = std::numeric_limits<int>::max();
        for (int b : p) {
            if (b > menor) ++h;
            else menor = b;
        }
    }
    return h;
}

// ---------------------------------------------------------------------------
// Busca
// ---------------------------------------------------------------------------

struct No {
    std::string estado;  // baia codificada (já com as retiradas forçadas)
    int pai;             // índice do nó pai; -1 na raiz
    int g;               // relocações desde a raiz pelo melhor caminho conhecido
    int alvo;            // próximo bloco a retirar (N+1 = baia vazia)
    std::uint8_t bloco, origem, destino, periodo;  // relocação que gerou o nó (pilhas 1..W)
};

struct EntradaFila {
    int f;
    int g;
    std::uint64_t ordem;  // ordem de inserção (desempate final: FIFO)
    int idx;
};

// std::priority_queue devolve o "maior"; aqui o maior é o de menor f, depois maior g,
// depois o inserido primeiro.
struct PiorQue {
    bool operator()(const EntradaFila& a, const EntradaFila& b) const {
        if (a.f != b.f) return a.f > b.f;
        if (a.g != b.g) return a.g < b.g;
        return a.ordem > b.ordem;
    }
};

// Conjunto de estados já gerados, guardado como índices em `nos` (a string fica uma vez só).
struct HashIdx {
    const std::vector<No>* nos;
    std::size_t operator()(int i) const { return std::hash<std::string>{}((*nos)[i].estado); }
};
struct IgualIdx {
    const std::vector<No>* nos;
    bool operator()(int a, int b) const { return (*nos)[a].estado == (*nos)[b].estado; }
};

struct Estatisticas {
    long long gerados = 0;          // nós criados (inclui a raiz)
    long long expandidos = 0;       // nós retirados da fila e expandidos (inclui o objetivo)
    long long podadosRepetidos = 0; // filhos descartados por estado repetido
    long long podadosUB = 0;        // filhos descartados por f >= UB
    long long atualizados = 0;      // estados reencontrados com g menor (nó atualizado)
    int hRaiz = 0;
    int ub = SEM_UB;                // UB inicial da Min-Max (SEM_UB = não usado)
};

struct SaidaAstar {
    std::string status;             // optimal | time_limit | node_limit | infeasible
    bool temSolucao = false;
    std::vector<brp::Movimento> movimentos;
    int limiteInferior = 0;
    Estatisticas est;
};

SaidaAstar resolverAstar(const brp::Instancia& inst, const OpcoesAstar& op, double limiteTempo) {
    if (inst.N > 255) throw std::runtime_error("astar: N > 255 não é suportado");
    if (inst.W > 255) throw std::runtime_error("astar: W > 255 não é suportado");
    SaidaAstar s;
    const double p0 = brp::tempoParedeSeg();

    // Limite superior pela heurística Min-Max (poda 3 do slide 21).
    std::vector<brp::Movimento> movsUB;
    if (op.usarUB) {
        try {
            movsUB = brp::heuristicaConstrutiva(inst, brp::escolherMinMax);
            s.est.ub = static_cast<int>(movsUB.size());
        } catch (const std::runtime_error&) {
            s.est.ub = SEM_UB;  // a Min-Max travou (todas as pilhas cheias): busca sem UB
        }
    }
    const int UB = s.est.ub;
    auto usarSolucaoUB = [&]() {
        s.temSolucao = true;
        s.movimentos = movsUB;
    };

    std::vector<No> nos;
    nos.reserve(1024);
    std::unordered_set<int, HashIdx, IgualIdx> vistos(1024, HashIdx{&nos}, IgualIdx{&nos});
    std::priority_queue<EntradaFila, std::vector<EntradaFila>, PiorQue> fila;
    std::uint64_t ordem = 0;

    brp::Baia inicial = inst.pilhas;
    const int t0 = retirarEnquantoPossivel(inicial, 1, inst.N);
    s.est.hRaiz = heuristica(inicial);
    nos.push_back({codificar(inicial), -1, 0, t0, 0, 0, 0, 0});
    vistos.insert(0);
    s.est.gerados = 1;

    if (s.est.hRaiz >= UB) {
        // LB = h(raiz) >= UB: a solução da Min-Max já é ótima.
        s.status = "optimal";
        s.limiteInferior = UB;
        usarSolucaoUB();
        return s;
    }
    fila.push({s.est.hRaiz, 0, ordem++, 0});

    auto reconstruir = [&](int idx) {
        s.movimentos.clear();
        for (int v = idx; nos[v].pai >= 0; v = nos[v].pai)
            s.movimentos.push_back({nos[v].bloco, nos[v].origem, nos[v].destino, nos[v].periodo});
        std::reverse(s.movimentos.begin(), s.movimentos.end());
    };
    // Menor f entre as entradas válidas da fila (limite inferior global ao parar).
    auto menorFValido = [&]() {
        while (!fila.empty() && fila.top().g != nos[fila.top().idx].g) fila.pop();
        return fila.empty() ? UB : std::min(fila.top().f, UB);
    };

    std::string parada;  // "" enquanto não atingir um limite
    while (!fila.empty()) {
        if ((s.est.expandidos & 255) == 0 && brp::tempoParedeSeg() - p0 > limiteTempo) {
            parada = "time_limit";
            break;
        }
        if (nos.size() > op.maxNos) {
            parada = "node_limit";
            break;
        }
        const EntradaFila e = fila.top();
        fila.pop();
        if (e.g != nos[e.idx].g) continue;  // entrada antiga: o nó foi atualizado com g menor

        ++s.est.expandidos;
        const int idx = e.idx;
        const int g = nos[idx].g;
        const int t = nos[idx].alvo;
        if (t > inst.N) {  // baia vazia: objetivo
            s.status = "optimal";
            s.temSolucao = true;
            s.limiteInferior = g;
            reconstruir(idx);
            return s;
        }

        brp::Baia baia = decodificar(nos[idx].estado, inst.W);
        int origem = -1;
        for (int i = 0; i < inst.W && origem < 0; ++i)
            if (std::find(baia[i].begin(), baia[i].end(), t) != baia[i].end()) origem = i;
        const int r = baia[origem].back();  // bloqueador no topo da pilha do alvo (A1)

        for (int d = 0; d < inst.W; ++d) {
            // Poda 2 (infactibilidade): destino igual à origem ou pilha cheia.
            if (d == origem || static_cast<int>(baia[d].size()) >= inst.H) continue;
            brp::Baia nova = baia;
            nova[origem].pop_back();
            nova[d].push_back(r);
            const int t2 = retirarEnquantoPossivel(nova, t, inst.N);
            const int g2 = g + 1;
            const int f2 = g2 + heuristica(nova);
            // Poda 3 (limite superior): f(n) >= UB não pode melhorar a Min-Max.
            if (f2 >= UB) {
                ++s.est.podadosUB;
                continue;
            }
            const int novo = static_cast<int>(nos.size());
            nos.push_back({codificar(nova), idx, g2, t2, static_cast<std::uint8_t>(r),
                           static_cast<std::uint8_t>(origem + 1), static_cast<std::uint8_t>(d + 1),
                           static_cast<std::uint8_t>(t)});
            auto [it, inseriu] = vistos.insert(novo);
            if (!inseriu) {
                // Poda 1 (estados repetidos).
                const int j = *it;
                nos.pop_back();
                if (nos[j].g <= g2) {
                    ++s.est.podadosRepetidos;
                    continue;
                }
                // Caminho mais barato até um estado ainda na fila: atualiza o nó.
                nos[j].pai = idx;
                nos[j].g = g2;
                nos[j].bloco = static_cast<std::uint8_t>(r);
                nos[j].origem = static_cast<std::uint8_t>(origem + 1);
                nos[j].destino = static_cast<std::uint8_t>(d + 1);
                nos[j].periodo = static_cast<std::uint8_t>(t);
                ++s.est.atualizados;
                fila.push({f2, g2, ordem++, j});
                continue;
            }
            ++s.est.gerados;
            fila.push({f2, g2, ordem++, novo});
        }
    }

    if (parada.empty()) {
        // Fila vazia sem solução com f < UB.
        if (UB != SEM_UB) {
            s.status = "optimal";
            s.limiteInferior = UB;
            usarSolucaoUB();
        } else {
            s.status = "infeasible";
        }
        return s;
    }
    const int lb = menorFValido();
    if (lb == SEM_UB) {
        // Só restavam entradas antigas na fila: a busca na verdade terminou sem solução.
        s.status = "infeasible";
        return s;
    }
    s.status = parada;
    s.limiteInferior = lb;
    if (UB != SEM_UB) usarSolucaoUB();
    return s;
}

std::string inteiroOuNulo(long long v, bool tem) { return tem ? std::to_string(v) : "null"; }

brp::Resultado resolver(const brp::Instancia& inst, const OpcoesAstar& op, double limiteTempo) {
    brp::Resultado r = brp::resultadoBase(inst, "astar");
    r.doBusca = true;

    const double p0 = brp::tempoParedeSeg(), c0 = brp::tempoCpuSeg();
    SaidaAstar s = resolverAstar(inst, op, limiteTempo);
    r.tempoCpu = brp::tempoCpuSeg() - c0;
    r.tempoParede = brp::tempoParedeSeg() - p0;

    r.status = s.status;
    r.otimoProvado = (s.status == "optimal");
    r.temSolucao = s.temSolucao;
    if (s.temSolucao) {
        r.relocacoes = static_cast<long long>(s.movimentos.size());
        r.movimentos = std::move(s.movimentos);
        std::string erro;
        if (!brp::verificarSequencia(inst, r.movimentos, erro))
            throw std::logic_error("a solução do A* não é executável: " + erro);
    }
    if (s.status != "infeasible") {
        r.temLimite = true;
        r.limiteInferior = s.limiteInferior;
    }
    if (r.temSolucao && r.temLimite) {
        double limite = std::min(r.limiteInferior, static_cast<double>(r.relocacoes));
        r.gap = r.relocacoes > 0 ? (static_cast<double>(r.relocacoes) - limite) / static_cast<double>(r.relocacoes)
                                 : 0.0;
        r.temGap = true;
    }
    if (r.otimoProvado && r.relocacoes != static_cast<long long>(r.limiteInferior))
        throw std::logic_error("A* declarou ótimo, mas a solução difere do limite inferior");

    const Estatisticas& e = s.est;
    r.extras = {
        {"usa_ub", op.usarUB ? "true" : "false"},
        {"limite_superior_inicial", inteiroOuNulo(e.ub, e.ub != SEM_UB)},
        {"h_raiz", std::to_string(e.hRaiz)},
        {"nos_gerados", std::to_string(e.gerados)},
        {"nos_expandidos", std::to_string(e.expandidos)},
        {"podados_repetidos", std::to_string(e.podadosRepetidos)},
        {"podados_ub", std::to_string(e.podadosUB)},
        {"nos_atualizados", std::to_string(e.atualizados)},
    };
    return r;
}

}  // namespace

int main(int argc, char** argv) {
    // Opções próprias do A* são retiradas antes da linha de comando comum.
    OpcoesAstar opA;
    std::vector<char*> resto{argv[0]};
    brp::OpcoesCli op;
    std::vector<brp::fs::path> arquivos;
    try {
        for (int a = 1; a < argc; ++a) {
            std::string arg = argv[a];
            if (arg == "--sem-ub") {
                opA.usarUB = false;
            } else if (arg == "--max-nos") {
                if (a + 1 >= argc) throw std::runtime_error("falta o valor de --max-nos");
                opA.maxNos = std::stoull(argv[++a]);
            } else {
                resto.push_back(argv[a]);
            }
        }
        op = brp::lerArgumentos(static_cast<int>(resto.size()), resto.data(), "astar", false);
        arquivos = brp::listarInstancias(op.entrada);
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n(opções do A*: [--sem-ub] [--max-nos n])\n";
        return 2;
    }
    const double limiteTempo = op.limiteTempo > 0 ? op.limiteTempo : LIMITE_TEMPO_PADRAO;

    int falhas = 0;
    for (const brp::fs::path& arq : arquivos) {
        std::vector<brp::Instancia> instancias;
        try {
            instancias = brp::lerInstancias(arq);
        } catch (const std::exception& e) {
            std::cerr << "ERRO: " << e.what() << "\n";
            ++falhas;
            continue;
        }
        for (const brp::Instancia& inst : instancias) {
            if (op.id > 0 && inst.id != op.id) continue;
            try {
                brp::Resultado r = resolver(inst, opA, limiteTempo);
                brp::gravarResultado(r, op.saida / (inst.chave + ".json"));
                std::string expandidos, gerados;
                for (const auto& x : r.extras) {
                    if (x.first == "nos_expandidos") expandidos = x.second;
                    if (x.first == "nos_gerados") gerados = x.second;
                }
                if (r.temSolucao)
                    std::printf("%-28s astar    relocações = %lld  status = %s  limite = %.0f  nós = %s/%s  cpu = %.3fs\n",
                                inst.chave.c_str(), r.relocacoes, r.status.c_str(), r.limiteInferior,
                                expandidos.c_str(), gerados.c_str(), r.tempoCpu);
                else
                    std::printf("%-28s astar    sem solução  status = %s  nós = %s/%s  cpu = %.3fs\n",
                                inst.chave.c_str(), r.status.c_str(), expandidos.c_str(), gerados.c_str(),
                                r.tempoCpu);
                std::fflush(stdout);
            } catch (const std::exception& e) {
                std::cerr << "ERRO em " << inst.chave << ": " << e.what() << "\n";
                ++falhas;
            }
        }
    }
    return falhas ? 1 : 0;
}
