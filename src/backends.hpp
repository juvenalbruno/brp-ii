// backends.hpp
// Adaptadores entre a formulação neutra de brp2_model.hpp e os solvers de PLIM.
//
// Este arquivo NÃO contém restrições. Cada adaptador apenas copia para o solver as
// variáveis e as linhas já montadas por brp2::construirModelo(), manda otimizar e
// devolve o resultado numa estrutura comum (SolucaoMip).
//
// Cada adaptador é compilado só se a macro correspondente estiver definida (o CMake
// define as dos solvers que encontrar):
//   BRP2_COM_GUROBI   Gurobi  (API C++, gurobi_c++.h)
//   BRP2_COM_CPLEX    CPLEX   (biblioteca C, ilcplex/cplex.h)
//   BRP2_COM_HIGHS    HiGHS   (API C++, Highs.h) — solver aberto, sem licença
#pragma once

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

#include "brp2_model.hpp"

#ifdef BRP2_COM_GUROBI
#include "gurobi_c++.h"
#endif
#ifdef BRP2_COM_CPLEX
#include <ilcplex/cplex.h>
#endif
#ifdef BRP2_COM_HIGHS
#include "Highs.h"
#endif

namespace brp2 {

struct ParametrosMip {
    double limiteTempo = 3600.0;  // segundos de relógio
    int threads = 0;              // 0 = padrão do solver
    bool verboso = false;         // mostra o log do solver
    int presolve = -1;            // -1 = configuração padrão do solver, 0 = desligado, 1 = ligado
};

struct SolucaoMip {
    std::string solver;           // nome e versão do solver usado
    std::string status;           // optimal | time_limit | infeasible | other
    bool temSolucao = false;      // há solução viável em `valores`
    double objetivo = 0.0;
    bool temLimite = false;       // há limite inferior em `limiteInferior`
    double limiteInferior = 0.0;  // melhor limite inferior do solver (sem arredondar)
    std::vector<double> valores;  // valor de cada variável, na ordem de Modelo::nomeVar
};

// Erro que impede o uso do solver para qualquer instância (licença, ambiente).
struct ErroFatalSolver : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// ---------------------------------------------------------------------------
// HiGHS
// ---------------------------------------------------------------------------
#ifdef BRP2_COM_HIGHS
inline SolucaoMip otimizarComHighs(const Modelo& m, const ParametrosMip& p) {
    const int nVars = m.numVars();
    const int nLinhas = static_cast<int>(m.linhas.size());

    HighsLp lp;
    lp.num_col_ = nVars;
    lp.num_row_ = nLinhas;
    lp.sense_ = ObjSense::kMinimize;
    lp.col_cost_ = m.obj;
    lp.col_lower_ = m.lb;
    lp.col_upper_ = m.ub;
    lp.integrality_.assign(nVars, HighsVarType::kInteger);  // limites 0/1 => binárias
    lp.row_lower_.reserve(nLinhas);
    lp.row_upper_.reserve(nLinhas);
    lp.a_matrix_.format_ = MatrixFormat::kRowwise;
    lp.a_matrix_.start_.assign(1, 0);
    for (const Linha& ln : m.linhas) {
        lp.row_lower_.push_back(ln.sentido == '<' ? -kHighsInf : ln.rhs);
        lp.row_upper_.push_back(ln.sentido == '>' ? kHighsInf : ln.rhs);
        for (const Termo& t : ln.termos) {
            lp.a_matrix_.index_.push_back(t.var);
            lp.a_matrix_.value_.push_back(t.coef);
        }
        lp.a_matrix_.start_.push_back(static_cast<HighsInt>(lp.a_matrix_.index_.size()));
    }

    Highs highs;
    highs.setOptionValue("output_flag", p.verboso);
    highs.setOptionValue("time_limit", p.limiteTempo);
    if (p.threads > 0) highs.setOptionValue("threads", p.threads);
    if (p.presolve >= 0) highs.setOptionValue("presolve", p.presolve ? "on" : "off");
    if (highs.passModel(std::move(lp)) == HighsStatus::kError)
        throw std::runtime_error("HiGHS recusou o modelo");
    if (highs.run() == HighsStatus::kError) throw std::runtime_error("HiGHS falhou ao otimizar");

    SolucaoMip s;
    s.solver = std::string("highs ") + highsVersion();
    switch (highs.getModelStatus()) {
        case HighsModelStatus::kOptimal: s.status = "optimal"; break;
        case HighsModelStatus::kTimeLimit: s.status = "time_limit"; break;
        case HighsModelStatus::kInfeasible:
        // O objetivo é limitado inferiormente por 0: "ilimitado ou inviável" só pode ser inviável.
        case HighsModelStatus::kUnboundedOrInfeasible: s.status = "infeasible"; break;
        default: s.status = "other";
    }
    const HighsInfo& info = highs.getInfo();
    if (info.primal_solution_status == kSolutionStatusFeasible) {
        s.temSolucao = true;
        s.objetivo = info.objective_function_value;
        s.valores = highs.getSolution().col_value;
    }
    if ((s.status == "optimal" || s.status == "time_limit") && std::isfinite(info.mip_dual_bound)) {
        s.temLimite = true;
        s.limiteInferior = info.mip_dual_bound;
    }
    return s;
}
#endif  // BRP2_COM_HIGHS

// ---------------------------------------------------------------------------
// CPLEX (biblioteca C)
// ---------------------------------------------------------------------------
#ifdef BRP2_COM_CPLEX
inline SolucaoMip otimizarComCplex(const Modelo& m, const ParametrosMip& p) {
    int erro = 0;
    CPXENVptr env = CPXopenCPLEX(&erro);
    if (env == nullptr) {
        char msg[CPXMESSAGEBUFSIZE] = "";
        CPXgeterrorstring(nullptr, erro, msg);
        throw ErroFatalSolver(std::string("não foi possível abrir o CPLEX (código ") + std::to_string(erro) +
                              "): " + msg);
    }
    CPXLPptr lp = nullptr;
    // Libera o problema e o ambiente em qualquer saída (inclusive por exceção).
    struct Limpeza {
        CPXENVptr& env;
        CPXLPptr& lp;
        ~Limpeza() {
            if (lp) CPXfreeprob(env, &lp);
            if (env) CPXcloseCPLEX(&env);
        }
    } limpeza{env, lp};
    auto conferir = [&](int codigo, const char* onde) {
        if (codigo == 0) return;
        char msg[CPXMESSAGEBUFSIZE] = "";
        CPXgeterrorstring(env, codigo, msg);
        throw std::runtime_error(std::string("CPLEX, ") + onde + " (código " + std::to_string(codigo) + "): " + msg);
    };

    conferir(CPXsetintparam(env, CPXPARAM_ScreenOutput, p.verboso ? CPX_ON : CPX_OFF), "ScreenOutput");
    conferir(CPXsetdblparam(env, CPXPARAM_TimeLimit, p.limiteTempo), "TimeLimit");
    if (p.threads > 0) conferir(CPXsetintparam(env, CPXPARAM_Threads, p.threads), "Threads");
    if (p.presolve >= 0)
        conferir(CPXsetintparam(env, CPXPARAM_Preprocessing_Presolve, p.presolve ? CPX_ON : CPX_OFF), "Presolve");

    lp = CPXcreateprob(env, &erro, "brp2");
    if (lp == nullptr) conferir(erro ? erro : 1, "CPXcreateprob");
    conferir(CPXchgobjsen(env, lp, CPX_MIN), "CPXchgobjsen");

    const int nVars = m.numVars();
    const int nLinhas = static_cast<int>(m.linhas.size());
    std::vector<char> tipos(nVars, CPX_BINARY);
    conferir(CPXnewcols(env, lp, nVars, m.obj.data(), m.lb.data(), m.ub.data(), tipos.data(), nullptr),
             "CPXnewcols");

    std::vector<double> rhs;
    std::vector<char> sentido;
    std::vector<int> inicio, indice;
    std::vector<double> coef;
    rhs.reserve(nLinhas);
    sentido.reserve(nLinhas);
    inicio.reserve(nLinhas);
    for (const Linha& ln : m.linhas) {
        rhs.push_back(ln.rhs);
        sentido.push_back(ln.sentido == '<' ? 'L' : ln.sentido == '>' ? 'G' : 'E');
        inicio.push_back(static_cast<int>(indice.size()));
        for (const Termo& t : ln.termos) {
            indice.push_back(t.var);
            coef.push_back(t.coef);
        }
    }
    if (nLinhas > 0)
        conferir(CPXaddrows(env, lp, 0, nLinhas, static_cast<int>(indice.size()), rhs.data(), sentido.data(),
                            inicio.data(), indice.data(), coef.data(), nullptr, nullptr),
                 "CPXaddrows");

    conferir(CPXmipopt(env, lp), "CPXmipopt");

    SolucaoMip s;
    s.solver = std::string("cplex ") + CPXversion(env);
    switch (CPXgetstat(env, lp)) {
        case CPXMIP_OPTIMAL:
        // Ótimo dentro da tolerância de gap. O objetivo é inteiro e a tolerância padrão
        // é muito menor que 1, então a otimalidade está provada.
        case CPXMIP_OPTIMAL_TOL: s.status = "optimal"; break;
        case CPXMIP_TIME_LIM_FEAS:
        case CPXMIP_TIME_LIM_INFEAS: s.status = "time_limit"; break;
        case CPXMIP_INFEASIBLE:
        case CPXMIP_INForUNBD: s.status = "infeasible"; break;
        default: s.status = "other";
    }
    double valor = 0.0;
    std::vector<double> x(nVars, 0.0);
    if (CPXgetobjval(env, lp, &valor) == 0 && CPXgetx(env, lp, x.data(), 0, nVars - 1) == 0) {
        s.temSolucao = true;
        s.objetivo = valor;
        s.valores = std::move(x);
    }
    double limite = 0.0;
    if ((s.status == "optimal" || s.status == "time_limit") && CPXgetbestobjval(env, lp, &limite) == 0 &&
        std::isfinite(limite) && std::fabs(limite) < 1e50) {
        s.temLimite = true;
        s.limiteInferior = limite;
    }
    return s;
}
#endif  // BRP2_COM_CPLEX

// ---------------------------------------------------------------------------
// Gurobi
// ---------------------------------------------------------------------------
#ifdef BRP2_COM_GUROBI
inline SolucaoMip otimizarComGurobi(const Modelo& m, const ParametrosMip& p) {
    try {
        GRBEnv env(true);
        env.set(GRB_IntParam_OutputFlag, p.verboso ? 1 : 0);
        try {
            env.start();
        } catch (const GRBException& e) {
            throw ErroFatalSolver("não foi possível iniciar o Gurobi (código " + std::to_string(e.getErrorCode()) +
                                  "): " + e.getMessage());
        }
        GRBModel grb(env);

        std::vector<GRBVar> vars;
        vars.reserve(m.numVars());
        for (int v = 0; v < m.numVars(); ++v)
            vars.push_back(grb.addVar(m.lb[v], m.ub[v], m.obj[v], GRB_BINARY, m.nomeVar[v]));
        grb.update();
        grb.set(GRB_IntAttr_ModelSense, GRB_MINIMIZE);

        std::vector<double> coefs;
        std::vector<GRBVar> termos;
        for (const Linha& ln : m.linhas) {
            coefs.clear();
            termos.clear();
            for (const Termo& tm : ln.termos) {
                coefs.push_back(tm.coef);
                termos.push_back(vars[tm.var]);
            }
            GRBLinExpr expr;
            if (!termos.empty()) expr.addTerms(coefs.data(), termos.data(), static_cast<int>(termos.size()));
            char sentido = ln.sentido == '<' ? GRB_LESS_EQUAL : ln.sentido == '>' ? GRB_GREATER_EQUAL : GRB_EQUAL;
            grb.addConstr(expr, sentido, ln.rhs, ln.nome);
        }

        grb.set(GRB_DoubleParam_TimeLimit, p.limiteTempo);
        if (p.threads > 0) grb.set(GRB_IntParam_Threads, p.threads);
        // Gurobi: -1 = automático (ligado), 0 = desligado.
        if (p.presolve >= 0) grb.set(GRB_IntParam_Presolve, p.presolve ? -1 : 0);
        grb.optimize();

        SolucaoMip s;
        s.solver = "gurobi " + std::to_string(GRB_VERSION_MAJOR) + "." + std::to_string(GRB_VERSION_MINOR) + "." +
                   std::to_string(GRB_VERSION_TECHNICAL);
        int status = grb.get(GRB_IntAttr_Status);
        if (status == GRB_OPTIMAL) s.status = "optimal";
        else if (status == GRB_TIME_LIMIT) s.status = "time_limit";
        // O objetivo é limitado inferiormente por 0: INF_OR_UNBD só pode ser inviável.
        else if (status == GRB_INFEASIBLE || status == GRB_INF_OR_UNBD) s.status = "infeasible";
        else s.status = "other";

        if (grb.get(GRB_IntAttr_SolCount) > 0) {
            s.temSolucao = true;
            s.objetivo = grb.get(GRB_DoubleAttr_ObjVal);
            s.valores.resize(vars.size());
            for (std::size_t v = 0; v < vars.size(); ++v) s.valores[v] = vars[v].get(GRB_DoubleAttr_X);
        }
        if (s.status == "optimal" || s.status == "time_limit") {
            s.temLimite = true;
            s.limiteInferior = grb.get(GRB_DoubleAttr_ObjBound);
        }
        return s;
    } catch (const GRBException& e) {
        throw std::runtime_error("Gurobi (código " + std::to_string(e.getErrorCode()) + "): " + e.getMessage());
    }
}
#endif  // BRP2_COM_GUROBI

// ---------------------------------------------------------------------------
// Seleção do solver
// ---------------------------------------------------------------------------

// Solvers compilados neste executável, em ordem de preferência (o primeiro é o padrão).
inline std::vector<std::string> solversDisponiveis() {
    std::vector<std::string> nomes;
#ifdef BRP2_COM_GUROBI
    nomes.push_back("gurobi");
#endif
#ifdef BRP2_COM_CPLEX
    nomes.push_back("cplex");
#endif
#ifdef BRP2_COM_HIGHS
    nomes.push_back("highs");
#endif
    return nomes;
}

inline SolucaoMip otimizar(const Modelo& m, const ParametrosMip& p, const std::string& solver) {
#ifdef BRP2_COM_GUROBI
    if (solver == "gurobi") return otimizarComGurobi(m, p);
#endif
#ifdef BRP2_COM_CPLEX
    if (solver == "cplex") return otimizarComCplex(m, p);
#endif
#ifdef BRP2_COM_HIGHS
    if (solver == "highs") return otimizarComHighs(m, p);
#endif
    (void)m;
    (void)p;
    throw ErroFatalSolver("solver \"" + solver + "\" não foi compilado neste executável");
}

}  // namespace brp2
