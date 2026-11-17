// solver.cpp — main 1: método exato (Gurobi) para o BRP restrito.
//
// Este arquivo NÃO contém restrições: toda a formulação está em brp2_model.hpp.
// Aqui só se lê a instância, chama-se a construção do modelo, otimiza-se e grava-se
// o resultado.
#define BRP2_COM_GUROBI
#include "brp2_model.hpp"
#include "instance.hpp"

namespace {

const double LIMITE_TEMPO_PADRAO = 3600.0;  // segundos

brp::Resultado resolver(const brp::Instancia& inst, const brp::OpcoesCli& op, GRBEnv& env) {
    brp::Resultado r = brp::resultadoBase(inst, "solver");
    r.doSolver = true;
    r.temSolucao = false;

    double p0 = brp::tempoParedeSeg(), c0 = brp::tempoCpuSeg();

    brp2::Opcoes opModelo;
    opModelo.restricao6Original = op.restricao6Original;
    brp2::Modelo modelo = brp2::construirModelo(inst, opModelo);

    GRBModel grb(env);
    std::vector<GRBVar> vars = brp2::carregarNoGurobi(modelo, grb);
    grb.set(GRB_DoubleParam_TimeLimit, op.limiteTempo > 0 ? op.limiteTempo : LIMITE_TEMPO_PADRAO);
    if (op.threads > 0) grb.set(GRB_IntParam_Threads, op.threads);
    grb.optimize();

    int status = grb.get(GRB_IntAttr_Status);
    int numSolucoes = grb.get(GRB_IntAttr_SolCount);
    if (status == GRB_OPTIMAL) {
        r.status = "optimal";
    } else if (status == GRB_TIME_LIMIT) {
        r.status = "time_limit";
    } else if (status == GRB_INFEASIBLE || status == GRB_INF_OR_UNBD) {
        // O objetivo é limitado inferiormente por 0, então INF_OR_UNBD só pode ser inviável.
        r.status = "infeasible";
    } else {
        r.status = "other";
    }
    // Só é ótimo se o Gurobi provou. No limite de tempo, o valor é apenas a melhor
    // solução encontrada (limite superior).
    r.otimoProvado = (status == GRB_OPTIMAL);

    if (status == GRB_OPTIMAL || status == GRB_TIME_LIMIT) {
        // O objetivo é inteiro: o limite inferior pode ser arredondado para cima.
        r.limiteInferior = std::ceil(grb.get(GRB_DoubleAttr_ObjBound) - 1e-6);
        r.temLimite = true;
    }
    if (numSolucoes > 0) {
        std::vector<double> valores(vars.size());
        for (std::size_t v = 0; v < vars.size(); ++v) valores[v] = vars[v].get(GRB_DoubleAttr_X);
        r.movimentos = brp2::extrairMovimentos(modelo, valores);
        r.relocacoes = std::llround(grb.get(GRB_DoubleAttr_ObjVal));
        r.temSolucao = true;
        r.gap = grb.get(GRB_DoubleAttr_MIPGap);
        r.temGap = true;
    }

    r.tempoCpu = brp::tempoCpuSeg() - c0;
    r.tempoParede = brp::tempoParedeSeg() - p0;

    if (r.temSolucao) {
        if (static_cast<long long>(r.movimentos.size()) != r.relocacoes)
            throw std::logic_error("número de movimentos extraídos difere do valor do objetivo");
        std::string erro;
        if (!brp::verificarSequencia(inst, r.movimentos, erro))
            throw std::logic_error("a solução do modelo não é executável: " + erro);
    }
    return r;
}

}  // namespace

int main(int argc, char** argv) {
    brp::OpcoesCli op;
    std::vector<brp::fs::path> arquivos;
    try {
        op = brp::lerArgumentos(argc, argv, "solver", true);
        arquivos = brp::listarInstancias(op.entrada);
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 2;
    }

    int falhas = 0;
    try {
        GRBEnv env(true);
        env.set(GRB_IntParam_OutputFlag, op.verboso ? 1 : 0);
        env.start();

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
                try {
                    brp::Resultado r = resolver(inst, op, env);
                    brp::gravarResultado(r, op.saida / (inst.chave + ".json"));
                    if (r.temSolucao)
                        std::printf("%-28s solver   relocações = %lld  status = %s  limite = %.0f  cpu = %.3fs\n",
                                    inst.chave.c_str(), r.relocacoes, r.status.c_str(), r.limiteInferior, r.tempoCpu);
                    else
                        std::printf("%-28s solver   sem solução  status = %s  cpu = %.3fs\n", inst.chave.c_str(),
                                    r.status.c_str(), r.tempoCpu);
                    std::fflush(stdout);
                } catch (const GRBException&) {
                    throw;  // erro do Gurobi (licença etc.): não adianta continuar
                } catch (const std::exception& e) {
                    std::cerr << "ERRO em " << inst.chave << ": " << e.what() << "\n";
                    ++falhas;
                }
            }
        }
    } catch (const GRBException& e) {
        std::cerr << "Erro do Gurobi (código " << e.getErrorCode() << "): " << e.getMessage() << "\n";
        return 3;
    }
    return falhas ? 1 : 0;
}
