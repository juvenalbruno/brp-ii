// modelo_lp.cpp — exporta a formulação de brp2_model.hpp em formato LP e importa uma
// solução de volta. Não depende do Gurobi: permite conferir a formulação com qualquer
// solver que leia LP (HiGHS, CBC, SCIP, gurobi_cl ...).
//
// Este arquivo só serializa; nenhuma restrição é definida aqui.
//
// Uso:
//   ./modelo_lp exportar <instância.json> <saída.lp> [--id n] [--restricao6-original]
//   ./modelo_lp importar <instância.json> <solução.txt> <resultado.json> [--id n] [--metodo nome]
// --id escolhe a instância quando o arquivo tem várias.
// O arquivo de solução tem uma linha "nome_da_variável valor" por variável (variáveis
// ausentes valem 0). Na importação a sequência é extraída com a mesma função usada
// pelo solver.cpp, conferida e gravada no formato padrão de resultado.
#include <unordered_map>

#include "../src/brp2_model.hpp"
#include "../src/instance.hpp"

namespace {

void escreverLp(const brp2::Modelo& m, const brp::fs::path& arquivo) {
    std::ofstream f(arquivo);
    if (!f) throw std::runtime_error("não foi possível gravar " + arquivo.string());
    auto termos = [&](const std::vector<brp2::Termo>& ts) {
        int naLinha = 0;
        for (const brp2::Termo& t : ts) {
            char buf[64];
            std::snprintf(buf, sizeof buf, " %c %.12g ", t.coef < 0 ? '-' : '+', std::fabs(t.coef));
            f << buf << m.nomeVar[t.var];
            if (++naLinha % 6 == 0) f << "\n   ";
        }
    };

    f << "\\ BRP-II corrigido: W=" << m.W << " H=" << m.H << " N=" << m.N << "\n";
    f << "Minimize\n obj:";
    std::vector<brp2::Termo> objetivo;
    for (int v = 0; v < m.numVars(); ++v)
        if (m.obj[v] != 0.0) objetivo.push_back({v, m.obj[v]});
    if (objetivo.empty()) f << " 0 " << m.nomeVar[0];
    else termos(objetivo);
    f << "\nSubject To\n";
    for (const brp2::Linha& ln : m.linhas) {
        f << " " << ln.nome << ":";
        if (ln.termos.empty()) f << " 0 " << m.nomeVar[0];
        else termos(ln.termos);
        char buf[64];
        std::snprintf(buf, sizeof buf, " %s %.12g\n", ln.sentido == '<' ? "<=" : ln.sentido == '>' ? ">=" : "=",
                      ln.rhs);
        f << buf;
    }
    f << "Bounds\n";
    for (int v = 0; v < m.numVars(); ++v)
        if (m.lb[v] == m.ub[v]) f << " " << m.nomeVar[v] << " = " << m.lb[v] << "\n";
    f << "Binaries\n";
    for (int v = 0; v < m.numVars(); ++v) f << " " << m.nomeVar[v] << "\n";
    f << "End\n";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        // Separa as opções dos argumentos posicionais.
        std::vector<std::string> pos;
        int id = 0;
        std::string metodo = "lp";
        brp2::Opcoes op;
        for (int a = 1; a < argc; ++a) {
            std::string arg = argv[a];
            if (arg == "--id" && a + 1 < argc) id = std::stoi(argv[++a]);
            else if (arg == "--metodo" && a + 1 < argc) metodo = argv[++a];
            else if (arg == "--restricao6-original") op.restricao6Original = true;
            else pos.push_back(arg);
        }

        if (pos.size() == 3 && pos[0] == "exportar") {
            brp::Instancia inst = brp::lerInstanciaUnica(pos[1], id);
            double c0 = brp::tempoCpuSeg();
            brp2::Modelo m = brp2::construirModelo(inst, op);
            double c1 = brp::tempoCpuSeg();
            escreverLp(m, pos[2]);
            long long naoZeros = 0;
            for (const brp2::Linha& ln : m.linhas) naoZeros += static_cast<long long>(ln.termos.size());
            std::printf("%s: %d variáveis (%lld sem pré-processamento, %lld fixadas em 1), %zu restrições, "
                        "%lld não-zeros, montagem %.2fs\n",
                        inst.chave.c_str(), m.numVars(), m.varsSemPreProcessamento, m.varsFixadasEmUm,
                        m.linhas.size(), naoZeros, c1 - c0);
            return 0;
        }
        if (pos.size() == 4 && pos[0] == "importar") {
            brp::Instancia inst = brp::lerInstanciaUnica(pos[1], id);
            brp2::Modelo m = brp2::construirModelo(inst);
            std::unordered_map<std::string, int> indice;
            for (int v = 0; v < m.numVars(); ++v) indice[m.nomeVar[v]] = v;

            std::vector<double> valores(m.numVars(), 0.0);
            std::ifstream f(pos[2]);
            if (!f) throw std::runtime_error("não foi possível abrir " + pos[2]);
            std::string nome;
            double valor;
            while (f >> nome >> valor) {
                auto it = indice.find(nome);
                if (it == indice.end()) throw std::runtime_error("variável desconhecida na solução: " + nome);
                valores[it->second] = valor;
            }

            brp::Resultado r = brp::resultadoBase(inst, metodo);
            r.movimentos = brp2::extrairMovimentos(m, valores);
            r.relocacoes = static_cast<long long>(r.movimentos.size());
            std::string erro;
            if (!brp::verificarSequencia(inst, r.movimentos, erro))
                throw std::logic_error("a solução do modelo não é executável: " + erro);
            brp::gravarResultado(r, pos[3]);
            return 0;
        }
        std::cerr << "uso: modelo_lp exportar <instância.json> <saída.lp> [--id n] [--restricao6-original]\n"
                     "     modelo_lp importar <instância.json> <solução.txt> <resultado.json> [--id n] [--metodo nome]\n";
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "ERRO: " << e.what() << "\n";
        return 1;
    }
}
