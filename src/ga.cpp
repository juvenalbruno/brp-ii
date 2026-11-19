// ga.cpp — main 5: algoritmo genético de Gulić, Maglić, Krljan e Maglić (2022),
// "Solving the Container Relocation Problem by Using a Metaheuristic Genetic
// Algorithm", Applied Sciences 12(15), 7397, para o BRP restrito (A1).
//
// Especificação seguida: a apresentação da equipe (IC0024, aula 3), que descreve o
// método do artigo e as adaptações ao protocolo da disciplina.
//
// Cromossomo (codificação indireta): 10N genes, g_k ∈ {1..W} = pilha de destino do
// k-ésimo bloqueador relocado. O gene diz PARA ONDE, nunca QUAL bloco.
//
// Decodificação: para t = 1..N, enquanto o alvo t não está no topo, o bloqueador r do
// topo da pilha s do alvo vai para g_k se g_k ∈ D = {i ≠ s : altura(i) < H}; senão o
// gene é ignorado (sem custo) e lê-se o próximo. Retiradas não custam. Como
// W·H − N ≥ H − 1 garante D ≠ ∅, todo cromossomo vira uma solução factível, sem
// penalidade e sem reparo. Salvaguarda da equipe: se os 10N genes acabarem, os
// bloqueadores restantes seguem a Min-Max. Fitness = número de relocações (minimizar).
//
// Ciclo (parâmetros da Tabela 1 do artigo): população 100; avaliação; seleção por
// torneio determinístico (k = 3, não informado no artigo) com elitismo, 50% da geração
// seguem para reprodução; crossover de 1 ponto com corte sorteado em [2, n−1]; mutação
// uniforme com p_m = 0,05 por gene; nova geração = elite + filhos + novos aleatórios.
// As proporções de elite e de novos aleatórios não são dadas no artigo: aqui são 10% e
// 10% (opções --elite e --imigrantes); os demais 80% são filhos.
//
// Adaptações ao protocolo da disciplina:
//   - parada por tempo: cada execução roda até --time-limit segundos de CPU (padrão 10,
//     o mesmo orçamento proposto para A* e Min-Max) em vez de 300 gerações fixas
//     (--geracoes n reproduz o critério do artigo); a geração alcançada é registrada;
//     a execução também para se o melhor Z atingir o limite inferior h(S0) (número de
//     bloqueadores do estado inicial), pois aí ele é ótimo (--sem-parada-lb desliga);
//   - ablação do warm-start: --variante A (GA-A, população inicial 100% aleatória,
//     como no artigo) ou --variante H (GA-H, injeta o cromossomo da Min-Max). Com
//     elitismo, Z(GA-H) ≤ Z(Min-Max) por construção;
//   - várias sementes: --sementes n roda as sementes s0..s0+n−1 (--semente-inicial s0,
//     padrão 0; padrão n = 10). O JSON grava a MELHOR solução entre as sementes (com a
//     sequência), e em campos extras o Z de cada semente, média, desvio, gerações e o
//     tempo até o melhor. tempo_cpu_s e tempo_parede_s são a MÉDIA por semente.
//
// Modo de conferência: --cromossomo g1,g2,... apenas decodifica esse cromossomo (sem
// GA) e grava o resultado com o método "ga_cromossomo"; usado por tests/validar.py
// para reproduzir a execução à mão da apresentação (Pai 1 = 8, Pai 2 = 7, Filho = 6).
//
// Uso: ./ga <instância.json | pasta> [--variante A|H] [--time-limit s] [--sementes n]
//          [--semente-inicial s] [--geracoes n] [--populacao n] [--mutacao p]
//          [--elite n] [--imigrantes n] [--torneio k] [--sem-parada-lb]
//          [--cromossomo g1,g2,...] [--out pasta] [--id n]
#include <cstdint>
#include <random>
#include <sstream>

#include "instance.hpp"
#include "regras.hpp"

namespace {

struct ParametrosGa {
    char variante = 'A';            // 'A' = população aleatória; 'H' = warm-start com a Min-Max
    double limiteTempo = 10.0;      // segundos de CPU por semente
    int sementes = 10;
    std::uint64_t sementeInicial = 0;
    long long maxGeracoes = 0;      // 0 = sem limite (parada só por tempo ou LB)
    int populacao = 100;
    double mutacao = 0.05;          // probabilidade por gene
    double selecionados = 0.5;      // fração da geração que segue para reprodução
    int elite = 10;
    int imigrantes = 10;
    int torneio = 3;
    bool pararNoLB = true;
    std::vector<int> cromossomo;    // não vazio: só decodifica (modo de conferência)
};

using Cromossomo = std::vector<std::uint8_t>;  // genes 1..W

// ---------------------------------------------------------------------------
// Decodificação
// ---------------------------------------------------------------------------

struct Decodificacao {
    int relocacoes = 0;
    int genesLidos = 0;
    int genesIgnorados = 0;
    int pelaSalvaguarda = 0;  // relocações decididas pela Min-Max (genes esgotados)
};

// Decodifica o cromossomo. Se `movs` não for nulo, grava a sequência de relocações.
Decodificacao decodificar(const brp::Instancia& inst, const Cromossomo& c, std::vector<brp::Movimento>* movs) {
    Decodificacao d;
    brp::Baia baia = inst.pilhas;
    std::vector<int> pilhaDe(inst.N + 1, -1);
    for (int i = 0; i < inst.W; ++i)
        for (int b : baia[i]) pilhaDe[b] = i;
    std::vector<int> candidatas;
    const int G = static_cast<int>(c.size());
    int k = 0;
    for (int t = 1; t <= inst.N; ++t) {
        const int s = pilhaDe[t];
        while (baia[s].back() != t) {
            const int r = baia[s].back();
            int dest = -1;
            while (k < G) {
                const int g = c[k++] - 1;
                if (g != s && g >= 0 && g < inst.W && static_cast<int>(baia[g].size()) < inst.H) {
                    dest = g;
                    break;
                }
                ++d.genesIgnorados;
            }
            if (dest < 0) {  // salvaguarda: genes esgotados, decide pela Min-Max
                candidatas.clear();
                for (int i = 0; i < inst.W; ++i)
                    if (i != s && static_cast<int>(baia[i].size()) < inst.H) candidatas.push_back(i);
                if (candidatas.empty())
                    throw std::runtime_error("sem pilha de destino para o bloco " + std::to_string(r) +
                                             " (todas cheias): instância inviável sob A1");
                dest = brp::escolherMinMax(baia, candidatas, r, inst.N);
                ++d.pelaSalvaguarda;
            }
            baia[s].pop_back();
            baia[dest].push_back(r);
            pilhaDe[r] = dest;
            ++d.relocacoes;
            if (movs) movs->push_back({r, s + 1, dest + 1, t});
        }
        baia[s].pop_back();  // retirada do alvo (sem custo)
    }
    d.genesLidos = k;
    return d;
}

// h(S0): número de bloqueadores (blocos acima de algum de número menor) — limite inferior.
int bloqueadores(const brp::Baia& baia) {
    int h = 0;
    for (const auto& p : baia) {
        int menor = 1 << 30;
        for (int b : p) {
            if (b > menor) ++h;
            else menor = b;
        }
    }
    return h;
}

// ---------------------------------------------------------------------------
// Algoritmo genético
// ---------------------------------------------------------------------------

struct Rng {
    std::mt19937_64 g;
    explicit Rng(std::uint64_t s) : g(s) {}
    // Inteiro uniforme em [0, n). mt19937_64 é padronizado, então a sequência é a mesma
    // em qualquer compilador (ao contrário de std::uniform_int_distribution).
    int inteiro(int n) { return static_cast<int>(g() % static_cast<std::uint64_t>(n)); }
    double real() { return static_cast<double>(g() >> 11) * (1.0 / 9007199254740992.0); }
};

struct Individuo {
    Cromossomo genes;
    int z = 0;
};

struct ExecucaoGa {
    std::uint64_t semente = 0;
    int melhorZ = 0;
    Cromossomo melhor;
    long long geracoes = 0;          // gerações completas após a população inicial
    long long avaliacoes = 0;        // decodificações
    long long geracaoDoMelhor = 0;
    double tempoAteMelhor = 0.0;     // s de CPU até achar o melhor Z
    double tempoCpu = 0.0;
    double tempoParede = 0.0;
    std::string parada;              // tempo | geracoes | lb
};

Cromossomo aleatorio(int G, int W, Rng& rng) {
    Cromossomo c(G);
    for (auto& g : c) g = static_cast<std::uint8_t>(1 + rng.inteiro(W));
    return c;
}

ExecucaoGa executarGa(const brp::Instancia& inst, const ParametrosGa& p, std::uint64_t semente, int lb,
                      const std::vector<brp::Movimento>& movsMinMax) {
    ExecucaoGa e;
    e.semente = semente;
    const double c0 = brp::tempoCpuSeg(), p0 = brp::tempoParedeSeg();
    Rng rng(semente);
    const int G = 10 * inst.N;
    const int P = p.populacao;
    const int nElite = std::min(p.elite, P);
    const int nImigrantes = std::min(p.imigrantes, P - nElite);
    const int nFilhos = P - nElite - nImigrantes;
    const int nPais = std::max(2, static_cast<int>(p.selecionados * P + 0.5));

    std::vector<Individuo> pop(P);
    for (auto& ind : pop) ind.genes = aleatorio(G, inst.W, rng);
    if (p.variante == 'H') {
        // Cromossomo da Min-Max: o destino de cada relocação dela, na ordem. Todos esses
        // genes são válidos, então a decodificação reproduz exatamente a Min-Max; os
        // genes seguintes (aleatórios) nunca são lidos.
        for (std::size_t k = 0; k < movsMinMax.size() && k < static_cast<std::size_t>(G); ++k)
            pop[0].genes[k] = static_cast<std::uint8_t>(movsMinMax[k].destino);
    }
    auto avaliar = [&](Individuo& ind) {
        ind.z = decodificar(inst, ind.genes, nullptr).relocacoes;
        ++e.avaliacoes;
    };
    for (auto& ind : pop) avaliar(ind);
    auto ordenar = [&]() {
        std::stable_sort(pop.begin(), pop.end(), [](const Individuo& a, const Individuo& b) { return a.z < b.z; });
    };
    ordenar();
    e.melhorZ = pop[0].z;
    e.melhor = pop[0].genes;
    e.tempoAteMelhor = brp::tempoCpuSeg() - c0;

    std::vector<int> pais(nPais);
    std::vector<Individuo> nova;
    nova.reserve(P);
    for (;;) {
        if (p.pararNoLB && e.melhorZ <= lb) { e.parada = "lb"; break; }
        if (p.maxGeracoes > 0 && e.geracoes >= p.maxGeracoes) { e.parada = "geracoes"; break; }
        if (brp::tempoCpuSeg() - c0 >= p.limiteTempo) { e.parada = "tempo"; break; }

        // Seleção: torneio determinístico (vence o de menor Z entre k sorteados); a
        // população está ordenada, então o menor índice sorteado é o vencedor.
        for (int& pai : pais) {
            int v = rng.inteiro(P);
            for (int j = 1; j < p.torneio; ++j) v = std::min(v, rng.inteiro(P));
            pai = v;
        }
        nova.clear();
        for (int i = 0; i < nElite; ++i) nova.push_back(pop[i]);  // elitismo
        // Filhos: crossover de 1 ponto (corte c em [2, G−1]: genes 1..c de um pai e
        // c+1..G do outro) gera os dois filhos complementares; depois, mutação uniforme.
        while (static_cast<int>(nova.size()) < nElite + nFilhos) {
            const Cromossomo& a = pop[pais[rng.inteiro(nPais)]].genes;
            const Cromossomo& b = pop[pais[rng.inteiro(nPais)]].genes;
            const int corte = 2 + rng.inteiro(G - 2);  // 2..G−1
            for (int filho = 0; filho < 2 && static_cast<int>(nova.size()) < nElite + nFilhos; ++filho) {
                const Cromossomo& x = filho == 0 ? a : b;
                const Cromossomo& y = filho == 0 ? b : a;
                Individuo ind;
                ind.genes.assign(x.begin(), x.begin() + corte);
                ind.genes.insert(ind.genes.end(), y.begin() + corte, y.end());
                for (auto& g : ind.genes)
                    if (rng.real() < p.mutacao) g = static_cast<std::uint8_t>(1 + rng.inteiro(inst.W));
                avaliar(ind);
                nova.push_back(std::move(ind));
            }
        }
        while (static_cast<int>(nova.size()) < P) {  // novos aleatórios
            Individuo ind;
            ind.genes = aleatorio(G, inst.W, rng);
            avaliar(ind);
            nova.push_back(std::move(ind));
        }
        pop.swap(nova);
        ordenar();
        ++e.geracoes;
        if (pop[0].z < e.melhorZ) {
            e.melhorZ = pop[0].z;
            e.melhor = pop[0].genes;
            e.geracaoDoMelhor = e.geracoes;
            e.tempoAteMelhor = brp::tempoCpuSeg() - c0;
        }
    }
    e.tempoCpu = brp::tempoCpuSeg() - c0;
    e.tempoParede = brp::tempoParedeSeg() - p0;
    return e;
}

// ---------------------------------------------------------------------------
// Saída
// ---------------------------------------------------------------------------

template <class T, class F>
std::string listaJson(const std::vector<T>& v, F formatar) {
    std::string s = "[";
    for (std::size_t i = 0; i < v.size(); ++i) s += (i ? ", " : "") + formatar(v[i]);
    return s + "]";
}

std::string numero(double x, const char* fmt = "%.6f") {
    char buf[64];
    std::snprintf(buf, sizeof buf, fmt, x);
    return buf;
}

brp::Resultado resolverGa(const brp::Instancia& inst, const ParametrosGa& p, const std::string& metodo) {
    brp::Resultado r = brp::resultadoBase(inst, metodo);
    const int lb = bloqueadores(inst.pilhas);
    std::vector<brp::Movimento> movsMinMax = brp::heuristicaConstrutiva(inst, brp::escolherMinMax);

    std::vector<ExecucaoGa> execs;
    for (int i = 0; i < p.sementes; ++i)
        execs.push_back(executarGa(inst, p, p.sementeInicial + static_cast<std::uint64_t>(i), lb, movsMinMax));

    std::size_t melhor = 0;
    double soma = 0.0, somaCpu = 0.0, somaParede = 0.0;
    for (std::size_t i = 0; i < execs.size(); ++i) {
        if (execs[i].melhorZ < execs[melhor].melhorZ) melhor = i;
        soma += execs[i].melhorZ;
        somaCpu += execs[i].tempoCpu;
        somaParede += execs[i].tempoParede;
    }
    const double n = static_cast<double>(execs.size());
    const double media = soma / n;
    double var = 0.0;
    for (const auto& e : execs) var += (e.melhorZ - media) * (e.melhorZ - media);
    const double desvio = execs.size() > 1 ? std::sqrt(var / (n - 1.0)) : 0.0;
    int pior = 0;
    for (const auto& e : execs) pior = std::max(pior, e.melhorZ);

    Decodificacao d = decodificar(inst, execs[melhor].melhor, &r.movimentos);
    r.relocacoes = d.relocacoes;
    if (r.relocacoes != execs[melhor].melhorZ) throw std::logic_error("Z redecodificado difere do Z do GA");
    r.tempoCpu = somaCpu / n;
    r.tempoParede = somaParede / n;

    auto inteiro = [](long long v) { return std::to_string(v); };
    auto texto = [](const std::string& s) { return "\"" + brp::escaparJson(s) + "\""; };
    std::vector<int> zs;
    std::vector<long long> geracoes, geracaoDoMelhor, avaliacoes;
    std::vector<std::uint64_t> sementes;
    std::vector<double> tempoAteMelhor, tempoCpu;
    std::vector<std::string> paradas;
    for (const auto& e : execs) {
        zs.push_back(e.melhorZ);
        sementes.push_back(e.semente);
        geracoes.push_back(e.geracoes);
        geracaoDoMelhor.push_back(e.geracaoDoMelhor);
        avaliacoes.push_back(e.avaliacoes);
        tempoAteMelhor.push_back(e.tempoAteMelhor);
        tempoCpu.push_back(e.tempoCpu);
        paradas.push_back(e.parada);
    }
    r.extras = {
        {"variante", p.variante == 'H' ? "\"GA-H\"" : "\"GA-A\""},
        {"limite_inferior_h", std::to_string(lb)},
        {"relocacoes_min_max", std::to_string(movsMinMax.size())},
        {"media_relocacoes", numero(media)},
        {"desvio_relocacoes", numero(desvio)},
        {"pior_relocacoes", std::to_string(pior)},
        {"semente_da_melhor", std::to_string(execs[melhor].semente)},
        {"sementes", listaJson(sementes, [](std::uint64_t v) { return std::to_string(v); })},
        {"relocacoes_por_semente", listaJson(zs, [](int v) { return std::to_string(v); })},
        {"geracoes_por_semente", listaJson(geracoes, inteiro)},
        {"geracao_do_melhor_por_semente", listaJson(geracaoDoMelhor, inteiro)},
        {"avaliacoes_por_semente", listaJson(avaliacoes, inteiro)},
        {"tempo_ate_melhor_s_por_semente", listaJson(tempoAteMelhor, [](double v) { return numero(v); })},
        {"tempo_cpu_s_por_semente", listaJson(tempoCpu, [](double v) { return numero(v); })},
        {"parada_por_semente", listaJson(paradas, texto)},
        {"limite_tempo_s", numero(p.limiteTempo, "%.3f")},
        {"max_geracoes", p.maxGeracoes > 0 ? std::to_string(p.maxGeracoes) : "null"},
        {"populacao", std::to_string(p.populacao)},
        {"genes", std::to_string(10 * inst.N)},
        {"mutacao", numero(p.mutacao, "%.4f")},
        {"selecionados", numero(p.selecionados, "%.2f")},
        {"elite", std::to_string(p.elite)},
        {"imigrantes", std::to_string(p.imigrantes)},
        {"torneio", std::to_string(p.torneio)},
        {"relocacoes_pela_salvaguarda", std::to_string(d.pelaSalvaguarda)},
    };
    return r;
}

brp::Resultado decodificarCromossomo(const brp::Instancia& inst, const ParametrosGa& p) {
    brp::Resultado r = brp::resultadoBase(inst, "ga_cromossomo");
    Cromossomo c;
    for (int g : p.cromossomo) {
        if (g < 1 || g > inst.W) throw std::runtime_error("gene " + std::to_string(g) + " fora de 1..W");
        c.push_back(static_cast<std::uint8_t>(g));
    }
    const double c0 = brp::tempoCpuSeg(), p0 = brp::tempoParedeSeg();
    Decodificacao d = decodificar(inst, c, &r.movimentos);
    r.tempoCpu = brp::tempoCpuSeg() - c0;
    r.tempoParede = brp::tempoParedeSeg() - p0;
    r.relocacoes = d.relocacoes;
    r.extras = {
        {"cromossomo", listaJson(p.cromossomo, [](int v) { return std::to_string(v); })},
        {"genes_lidos", std::to_string(d.genesLidos)},
        {"genes_ignorados", std::to_string(d.genesIgnorados)},
        {"relocacoes_pela_salvaguarda", std::to_string(d.pelaSalvaguarda)},
    };
    return r;
}

std::vector<int> lerCromossomo(const std::string& s) {
    std::vector<int> v;
    std::stringstream ss(s);
    std::string parte;
    while (std::getline(ss, parte, ',')) {
        if (parte.empty()) continue;
        v.push_back(std::stoi(parte));
    }
    if (v.empty()) throw std::runtime_error("--cromossomo vazio");
    return v;
}

}  // namespace

int main(int argc, char** argv) {
    ParametrosGa p;
    std::vector<char*> resto{argv[0]};
    brp::OpcoesCli op;
    std::vector<brp::fs::path> arquivos;
    std::string metodo;
    try {
        for (int a = 1; a < argc; ++a) {
            std::string arg = argv[a];
            auto valor = [&]() -> std::string {
                if (a + 1 >= argc) throw std::runtime_error("falta o valor de " + arg);
                return argv[++a];
            };
            if (arg == "--variante") {
                std::string v = valor();
                if (v != "A" && v != "H" && v != "a" && v != "h") throw std::runtime_error("--variante aceita A ou H");
                p.variante = static_cast<char>(std::toupper(static_cast<unsigned char>(v[0])));
            } else if (arg == "--sementes") {
                p.sementes = std::stoi(valor());
            } else if (arg == "--semente-inicial") {
                p.sementeInicial = std::stoull(valor());
            } else if (arg == "--geracoes") {
                p.maxGeracoes = std::stoll(valor());
            } else if (arg == "--populacao") {
                p.populacao = std::stoi(valor());
            } else if (arg == "--mutacao") {
                p.mutacao = std::stod(valor());
            } else if (arg == "--elite") {
                p.elite = std::stoi(valor());
            } else if (arg == "--imigrantes") {
                p.imigrantes = std::stoi(valor());
            } else if (arg == "--torneio") {
                p.torneio = std::stoi(valor());
            } else if (arg == "--sem-parada-lb") {
                p.pararNoLB = false;
            } else if (arg == "--cromossomo") {
                p.cromossomo = lerCromossomo(valor());
            } else {
                resto.push_back(argv[a]);
            }
        }
        if (p.sementes < 1 || p.populacao < 4 || p.torneio < 1 || p.elite < 0 || p.imigrantes < 0 ||
            p.elite + p.imigrantes >= p.populacao || p.mutacao < 0.0 || p.mutacao > 1.0)
            throw std::runtime_error("parâmetros do GA inválidos");
        metodo = !p.cromossomo.empty() ? "ga_cromossomo" : (p.variante == 'H' ? "ga_h" : "ga_a");
        op = brp::lerArgumentos(static_cast<int>(resto.size()), resto.data(), metodo, false);
        if (op.limiteTempo > 0) p.limiteTempo = op.limiteTempo;
        arquivos = brp::listarInstancias(op.entrada);
    } catch (const std::exception& e) {
        std::cerr << e.what()
                  << "\n(opções do GA: [--variante A|H] [--sementes n] [--semente-inicial s] [--geracoes n]"
                     " [--populacao n] [--mutacao p] [--elite n] [--imigrantes n] [--torneio k]"
                     " [--sem-parada-lb] [--cromossomo g1,g2,...])\n";
        return 2;
    }

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
                brp::Resultado r = p.cromossomo.empty() ? resolverGa(inst, p, metodo) : decodificarCromossomo(inst, p);
                std::string erro;
                if (!brp::verificarSequencia(inst, r.movimentos, erro))
                    throw std::logic_error("sequência do GA não é executável: " + erro);
                brp::gravarResultado(r, op.saida / (inst.chave + ".json"));
                std::string media, lb;
                for (const auto& x : r.extras) {
                    if (x.first == "media_relocacoes") media = x.second;
                    if (x.first == "limite_inferior_h") lb = x.second;
                }
                if (p.cromossomo.empty())
                    std::printf("%-28s %-6s melhor = %lld  média = %s  h(S0) = %s  cpu/semente = %.3fs\n",
                                inst.chave.c_str(), metodo.c_str(), r.relocacoes, media.c_str(), lb.c_str(),
                                r.tempoCpu);
                else
                    std::printf("%-28s cromossomo  Z = %lld\n", inst.chave.c_str(), r.relocacoes);
                std::fflush(stdout);
            } catch (const std::exception& e) {
                std::cerr << "ERRO em " << inst.chave << ": " << e.what() << "\n";
                ++falhas;
            }
        }
    }
    return falhas ? 1 : 0;
}
