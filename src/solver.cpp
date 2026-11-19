// solver.cpp — main 1: método exato para o BRP restrito.
//
// Este arquivo NÃO contém restrições: toda a formulação está em brp2_model.hpp e a
// ligação com cada solver (Gurobi, CPLEX, HiGHS) está em backends.hpp. Aqui só se lê
// a instância, chama-se a construção do modelo, otimiza-se e grava-se o resultado.
//
// O solver é escolhido com --solver gurobi|cplex|highs entre os que foram compilados
// (ver --listar-solvers). Sem a opção, usa o primeiro disponível nessa ordem.
#include "backends.hpp"
#include "brp2_model.hpp"
#include "instance.hpp"

namespace {

const double LIMITE_TEMPO_PADRAO = 3600.0;  // segundos

// Presolve efetivo: "on", "off" ou "padrao" (configuração padrão do solver).
// Sem --presolve, o HiGHS roda com o presolve DESLIGADO: nas instâncias maiores o
// presolve dele consumia o limite de tempo inteiro, sem chegar a resolver a relaxação
// linear (0 iterações de LP, 0 nós). Gurobi e CPLEX ficam com a configuração padrão.
std::string presolveEfetivo(const std::string& pedido, const std::string& solver) {
    if (!pedido.empty()) return pedido;
    return solver == "highs" ? "off" : "padrao";
}

brp::Resultado resolver(const brp::Instancia& inst, const brp::OpcoesCli& op, const std::string& solver) {
    brp::Resultado r = brp::resultadoBase(inst, "solver");
    r.doSolver = true;
    r.temSolucao = false;

    double p0 = brp::tempoParedeSeg(), c0 = brp::tempoCpuSeg();

    brp2::Opcoes opModelo;
    opModelo.restricao6Original = op.restricao6Original;
    brp2::Modelo modelo = brp2::construirModelo(inst, opModelo);

    brp2::ParametrosMip param;
    param.limiteTempo = op.limiteTempo > 0 ? op.limiteTempo : LIMITE_TEMPO_PADRAO;
    param.threads = op.threads;
    param.verboso = op.verboso;
    const std::string presolve = presolveEfetivo(op.presolve, solver);
    param.presolve = presolve == "off" ? 0 : presolve == "on" ? 1 : -1;
    brp2::SolucaoMip sol = brp2::otimizar(modelo, param, solver);

    r.tempoCpu = brp::tempoCpuSeg() - c0;
    r.tempoParede = brp::tempoParedeSeg() - p0;

    r.solver = sol.solver;
    r.presolve = presolve;
    r.status = sol.status;
    // Só é ótimo se o solver provou. No limite de tempo, o valor é apenas a melhor
    // solução encontrada (limite superior).
    r.otimoProvado = (sol.status == "optimal");

    if (sol.temSolucao) {
        r.temSolucao = true;
        r.relocacoes = std::llround(sol.objetivo);
        r.movimentos = brp2::extrairMovimentos(modelo, sol.valores);
        if (static_cast<long long>(r.movimentos.size()) != r.relocacoes)
            throw std::logic_error("número de movimentos extraídos difere do valor do objetivo");
        std::string erro;
        if (!brp::verificarSequencia(inst, r.movimentos, erro))
            throw std::logic_error("a solução do modelo não é executável: " + erro);
    }
    if (sol.temLimite) {
        // O objetivo é inteiro: o limite inferior pode ser arredondado para cima.
        r.limiteInferior = std::max(0.0, std::ceil(sol.limiteInferior - 1e-6));
        r.temLimite = true;
    }
    if (sol.temSolucao && sol.temLimite) {
        // Gap relativo calculado da mesma forma para todos os solvers:
        // (melhor solução - limite inferior) / melhor solução.
        double limite = std::min(r.limiteInferior, static_cast<double>(r.relocacoes));
        r.gap = r.relocacoes > 0 ? (static_cast<double>(r.relocacoes) - limite) / static_cast<double>(r.relocacoes)
                                 : 0.0;
        r.temGap = true;
    }
    return r;
}

}  // namespace

int main(int argc, char** argv) {
    brp::OpcoesCli op;
    std::vector<brp::fs::path> arquivos;
    const std::vector<std::string> disponiveis = brp2::solversDisponiveis();
    std::string solver;
    try {
        op = brp::lerArgumentos(argc, argv, "solver", true);
        if (op.listarSolvers) {
            for (const std::string& s : disponiveis) std::cout << s << "\n";
            return 0;
        }
        solver = op.solver.empty() ? disponiveis.front() : op.solver;
        if (std::find(disponiveis.begin(), disponiveis.end(), solver) == disponiveis.end()) {
            std::string lista;
            for (const std::string& s : disponiveis) lista += (lista.empty() ? "" : ", ") + s;
            throw std::runtime_error("solver \"" + solver + "\" não disponível; compilados: " + lista);
        }
        arquivos = brp::listarInstancias(op.entrada);
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
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
                brp::Resultado r = resolver(inst, op, solver);
                brp::gravarResultado(r, op.saida / (inst.chave + ".json"));
                if (r.temSolucao)
                    std::printf("%-28s %-8s relocações = %lld  status = %s  limite = %.0f  cpu = %.3fs\n",
                                inst.chave.c_str(), solver.c_str(), r.relocacoes, r.status.c_str(),
                                r.limiteInferior, r.tempoCpu);
                else
                    std::printf("%-28s %-8s sem solução  status = %s  cpu = %.3fs\n", inst.chave.c_str(),
                                solver.c_str(), r.status.c_str(), r.tempoCpu);
                std::fflush(stdout);
            } catch (const brp2::ErroFatalSolver& e) {
                // Licença ou ambiente do solver: não adianta tentar as outras instâncias.
                std::cerr << "ERRO FATAL: " << e.what() << "\n";
                return 3;
            } catch (const std::exception& e) {
                std::cerr << "ERRO em " << inst.chave << ": " << e.what() << "\n";
                ++falhas;
            }
        }
    }
    return falhas ? 1 : 0;
}
