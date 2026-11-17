// bfs.cpp — verificador por busca exaustiva: ótimo do BRP restrito (A1) por busca em
// largura. Serve de gabarito para instâncias pequenas (até cerca de 4x4).
//
// Sob A1, quando o alvo está no topo nada pode ser relocado, então a retirada é
// forçada; por isso os estados são guardados já com todas as retiradas possíveis
// feitas. Cada aresta é uma relocação (custo 1): o bloco do topo da pilha do alvo vai
// para outra pilha não cheia. A primeira vez que a busca em largura alcança a baia
// vazia dá o número mínimo de relocações.
//
// Uso: ./bfs <instância.json | pasta> [--out pasta] [--max-estados n]
// Grava um JSON por instância no mesmo formato dos outros métodos (método "bfs").
#include <unordered_map>

#include "../src/instance.hpp"

namespace {

std::string codificar(const brp::Baia& baia) {
    std::string s;
    for (const auto& p : baia) {
        for (int b : p) s += static_cast<char>(b);
        s += '\0';  // separador (prioridades começam em 1)
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

// Retira o alvo enquanto ele estiver no topo de alguma pilha. Devolve o novo alvo.
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

struct No {
    std::string estado;
    int alvo;
    int pai;
    brp::Movimento mov;
};

// Devolve true se a instância é viável; preenche a sequência ótima.
bool buscaEmLargura(const brp::Instancia& inst, std::size_t maxEstados, std::vector<brp::Movimento>& movs) {
    if (inst.N > 255) throw std::runtime_error("bfs: N > 255 não é suportado");
    std::vector<No> nos;
    std::unordered_map<std::string, int> visto;

    brp::Baia inicial = inst.pilhas;
    int t0 = retirarEnquantoPossivel(inicial, 1, inst.N);
    nos.push_back({codificar(inicial), t0, -1, {}});
    visto.emplace(nos[0].estado, 0);

    auto reconstruir = [&](int idx) {
        movs.clear();
        for (int v = idx; nos[v].pai >= 0; v = nos[v].pai) movs.push_back(nos[v].mov);
        std::reverse(movs.begin(), movs.end());
    };
    if (t0 > inst.N) { reconstruir(0); return true; }

    for (std::size_t cabeca = 0; cabeca < nos.size(); ++cabeca) {
        brp::Baia baia = decodificar(nos[cabeca].estado, inst.W);
        int t = nos[cabeca].alvo;
        int s = -1;
        for (int i = 0; i < inst.W && s < 0; ++i)
            if (std::find(baia[i].begin(), baia[i].end(), t) != baia[i].end()) s = i;
        int r = baia[s].back();  // bloco acima do alvo, no topo
        for (int d = 0; d < inst.W; ++d) {
            if (d == s || static_cast<int>(baia[d].size()) >= inst.H) continue;
            brp::Baia nova = baia;
            nova[s].pop_back();
            nova[d].push_back(r);
            int t2 = retirarEnquantoPossivel(nova, t, inst.N);
            std::string cod = codificar(nova);
            if (visto.count(cod)) continue;
            int idx = static_cast<int>(nos.size());
            visto.emplace(cod, idx);
            nos.push_back({std::move(cod), t2, static_cast<int>(cabeca), {r, s + 1, d + 1, t}});
            if (t2 > inst.N) { reconstruir(idx); return true; }
            if (nos.size() > maxEstados)
                throw std::runtime_error("bfs: limite de estados excedido (" + std::to_string(maxEstados) + ")");
        }
    }
    return false;  // nenhum caminho esvazia a baia
}

}  // namespace

int main(int argc, char** argv) {
    brp::fs::path entrada, saida = brp::fs::path("tests") / "output" / "bfs";
    std::size_t maxEstados = 30000000;
    bool temEntrada = false;
    for (int a = 1; a < argc; ++a) {
        std::string arg = argv[a];
        if (arg == "--out" && a + 1 < argc) saida = argv[++a];
        else if (arg == "--max-estados" && a + 1 < argc) maxEstados = std::stoull(argv[++a]);
        else if (!temEntrada && arg[0] != '-') { entrada = arg; temEntrada = true; }
        else { std::cerr << "uso: bfs <instância.json | pasta> [--out pasta] [--max-estados n]\n"; return 2; }
    }
    if (!temEntrada) { std::cerr << "uso: bfs <instância.json | pasta> [--out pasta] [--max-estados n]\n"; return 2; }

    int falhas = 0;
    try {
        for (const brp::fs::path& arq : brp::listarInstancias(entrada)) {
            for (const brp::Instancia& inst : brp::lerInstancias(arq)) {
                try {
                    brp::Resultado r = brp::resultadoBase(inst, "bfs");
                    double p0 = brp::tempoParedeSeg(), c0 = brp::tempoCpuSeg();
                    r.temSolucao = buscaEmLargura(inst, maxEstados, r.movimentos);
                    r.tempoCpu = brp::tempoCpuSeg() - c0;
                    r.tempoParede = brp::tempoParedeSeg() - p0;
                    r.relocacoes = static_cast<long long>(r.movimentos.size());
                    if (r.temSolucao) {
                        std::string erro;
                        if (!brp::verificarSequencia(inst, r.movimentos, erro))
                            throw std::logic_error("sequência da busca não é executável: " + erro);
                        std::printf("%-28s bfs      ótimo restrito = %lld  cpu = %.3fs\n", inst.chave.c_str(),
                                    r.relocacoes, r.tempoCpu);
                    } else {
                        std::printf("%-28s bfs      inviável\n", inst.chave.c_str());
                    }
                    std::fflush(stdout);
                    brp::gravarResultado(r, saida / (inst.chave + ".json"));
                } catch (const std::exception& e) {
                    std::cerr << "ERRO em " << inst.chave << ": " << e.what() << "\n";
                    ++falhas;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 2;
    }
    return falhas ? 1 : 0;
}
