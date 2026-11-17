// brp2_model.hpp
// ÚNICO arquivo com toda a formulação BRP-II corrigida (BRP restrito, suposição A1).
//
// A formulação é montada numa estrutura neutra (variáveis binárias + linhas
// lineares), independente de solver. Com a macro BRP2_COM_GUROBI definida, a função
// carregarNoGurobi() copia essa estrutura para um GRBModel. A estrutura neutra existe
// para que exatamente as mesmas restrições possam ser exportadas em formato LP e
// conferidas com outro solver (ver tests/modelo_lp.cpp).
//
// -----------------------------------------------------------------------------
// Modelo
// -----------------------------------------------------------------------------
// Índices: pilhas i,k em 1..W; níveis j,l em 1..H (1 = base); blocos n em 1..N;
//          períodos t em 1..N. No período t o bloco t é retirado, depois de
//          relocados os blocos que estão acima dele.
//
// Variáveis binárias:
//   b[i][j][n][t]       = 1 se o bloco n está em (i,j) no início do período t
//   x[i][j][k][l][n][t] = 1 se o bloco n é relocado de (i,j) para (k,l) no período t
//   y[i][j][n][t]       = 1 se o bloco n é retirado de (i,j) no período t
// Parâmetro: v[n][t] = 1 se n < t (já retirado), 0 caso contrário.
//
// Objetivo: minimizar a soma de todos os x.
//
// Restrições do enunciado:
//   (1)  Σ_i Σ_j b[i][j][n][t] + v[n][t] = 1                          ∀ n, t
//   (2)  Σ_n b[i][j][n][t] <= 1                                       ∀ i, j, t
//   (3)  Σ_n b[i][j][n][t] >= Σ_n b[i][j+1][n][t]                     ∀ i, j<H, t
//   (4)  b[i][j][n][t] = b[i][j][n][t-1] + Σ_k Σ_l x[k][l][i][j][n][t-1]
//                        - Σ_k Σ_l x[i][j][k][l][n][t-1] - y[i][j][n][t-1]   ∀ t>=2
//   (5)  v[n][t] = Σ_i Σ_j Σ_{t'<t} y[i][j][n][t']                    ∀ n, t
//   (6c) H * (1 - Σ_n x[i][j][k][l][n][t])
//            >= Σ_n Σ_{j'>j} Σ_{l'>l} x[i][j'][k][l'][n][t]           ∀ i, k, j, l, t
//   (7)  M * (1 - Σ_j b[i][j][t][t])
//            >= Σ_{i'≠i} Σ_j Σ_k Σ_l Σ_n x[i'][j][k][l][n][t]         ∀ i, t
//   (8)  x[i][j][i][l][n][t] = 0                                      ∀ i, j, l, n, t
//
// Restrições de ligação acrescentadas (necessárias para o modelo ser correto):
//   (L1) Σ_k Σ_l x[i][j][k][l][n][t] <= b[i][j][n][t]
//        só se reloca um bloco a partir da posição em que ele está (e, portanto,
//        cada bloco é relocado no máximo uma vez por período).
//   (L2) y[i][j][t][t] <= b[i][j][t][t]
//        só se retira o bloco da posição em que ele está.
//   (L3) Σ_i Σ_j y[i][j][t][t] = 1                                    ∀ t
//        exatamente o bloco t é retirado no período t. É a (5) depois do corte F2.
//   (L4) Σ_k Σ_l Σ_n x[i][j][k][l][n][t] <= Σ_{j'<j} b[i][j'][t][t]   ∀ i, j, t
//        suposição A1: só sai de (i,j) um bloco que está ACIMA do alvo t. A (7) só
//        proíbe sair de outras pilhas; sem a (L4) um bloco abaixo do alvo poderia
//        ser relocado.
//   (L5) Σ_k Σ_l x[i][j][k][l][n][t] >= b[i][j][n][t] + Σ_{j'<j} b[i][j'][t][t] - 1
//        todo bloco acima do alvo tem de ser relocado antes da retirada.
//   (L6) b[.][.][n][1] fixado pela configuração inicial da instância.
//
// Cortes de fixação (pré-processamento). Uma variável "fixada em 0" simplesmente NÃO
// É CRIADA, o que equivale à fixação e reduz o modelo:
//   (F1) b[i][j][n][t] = 0 se n < t                (bloco já retirado; vem da (1))
//   (F2) y[i][j][n][t] = 0 se n ≠ t                (bloco n só é retirado no período n)
//   (F3) x[...][n][t] = 0 se n <= t                (retirado, ou é o próprio alvo)
//        e x[...][n][N] = 0                         (no último período resta só o bloco N)
//   (F4) x de/para posição impossível: origem no nível 1 (não há alvo abaixo),
//        origem/destino onde o bloco n não pode estar (nada sai de posição vazia).
//   (F5) níveis inalcançáveis: no período t restam N-t+1 blocos, logo b exige
//        j <= N-t+1 e o destino de x exige l <= N-t (o alvo está em outra pilha).
//   (F6) blocos parados: seja m(n) a menor prioridade abaixo de n na pilha inicial.
//        Se m(n) < n, o bloco n fica na posição inicial até o período m(n) e só então
//        pode ser relocado pela primeira vez; se não existe tal m(n), n nunca é
//        relocado. Enquanto parado, b é fixado em 1 na posição inicial (e 0 no resto)
//        e x é 0. No período m(n) a origem de x é a posição inicial.
//   (F7) alvo parado: se o bloco t nunca é relocado, sua posição (i_t, j_t) no
//        período t é conhecida; então x só sai de (i_t, j) com j > j_t.
//   (8)  x com k = i não é criada.
//
// -----------------------------------------------------------------------------
// CORREÇÃO DA RESTRIÇÃO (6)
// -----------------------------------------------------------------------------
// No artigo original de Caserta, Schwarze e Voß (EJOR 219(1)) o lado esquerdo da
// restrição (6) é "1 - Σ_n x[i][j][k][l][n][t]", SEM o fator H. A intenção é impor a
// ordem LIFO entre um par de pilhas: se um bloco vai de (i,j) para (k,l), nenhum
// bloco que estava acima dele em i (j' > j) pode terminar acima dele em k (l' > l).
// Mas, sem o fator H, quando Σ_n x[i][j][k][l][n][t] = 0 o lado esquerdo vale 1 e a
// restrição limita a UMA a quantidade de relocações da pilha i para a pilha k com
// j' > j e l' > l no período. Isso proíbe mover dois ou mais blocos da mesma pilha de
// origem para a mesma pilha de destino no mesmo período e torna inviáveis instâncias
// que são viáveis (por exemplo W=3, H=4, pilhas [[8,7,6],[2],[1,3,4,5]]).
// Com o fator H o lado esquerdo vale H quando não há relocação de (i,j) para (k,l),
// e a restrição fica inativa (no máximo H-1 blocos saem de uma pilha num período).
// O erro e a correção estão em Eskandari & Azari (2015, Scientia Iranica 22(6)) e em
// Zehendner, Caserta, Feillet, Schwarze & Voß (2015, EJOR 245(2)).
#pragma once

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

#include "instance.hpp"

#ifdef BRP2_COM_GUROBI
#include "gurobi_c++.h"
#endif

namespace brp2 {

struct Opcoes {
    // true = usa a (6) do artigo original (sem o fator H). Serve só para demonstrar
    // que a versão original é inviável em instâncias viáveis. NÃO use para resolver.
    bool restricao6Original = false;
};

struct Termo {
    int var;
    double coef;
};

struct Linha {
    std::vector<Termo> termos;
    char sentido = '<';  // '<' (<=), '>' (>=) ou '=' — os mesmos caracteres do Gurobi
    double rhs = 0.0;
    std::string nome;
};

// Variável x criada (as demais estão fixadas em 0). Índices internos 0-based para
// pilha e nível; bloco e período 1-based.
struct VarX {
    int id;
    int i, j, k, l, n, t;
};

struct Modelo {
    int W = 0, H = 0, N = 0;

    // Variáveis (todas binárias). lb = ub = 1 marca variável fixada em 1.
    std::vector<std::string> nomeVar;
    std::vector<double> lb, ub, obj;

    std::vector<Linha> linhas;
    std::vector<VarX> xs;

    // Estatísticas do pré-processamento.
    long long varsSemPreProcessamento = 0;  // total de b, x, y da formulação completa
    long long varsFixadasEmUm = 0;

    int numVars() const { return static_cast<int>(nomeVar.size()); }
};

inline Modelo construirModelo(const brp::Instancia& inst, const Opcoes& op = {}) {
    const int W = inst.W, H = inst.H, N = inst.N;
    Modelo m;
    m.W = W;
    m.H = H;
    m.N = N;
    m.varsSemPreProcessamento = 2LL * W * H * N * N + 1LL * W * H * W * H * N * N;

    auto rot = [](int v) { return std::to_string(v); };
    auto novaVar = [&](std::string nome, double lb, double custo) {
        m.nomeVar.push_back(std::move(nome));
        m.lb.push_back(lb);
        m.ub.push_back(1.0);
        m.obj.push_back(custo);
        if (lb > 0.5) ++m.varsFixadasEmUm;
        return m.numVars() - 1;
    };
    auto fixa = [&](int var) { return m.lb[var] == m.ub[var]; };

    // Emite uma linha, descartando as trivialmente satisfeitas (sem termos, ou só com
    // variáveis fixadas). Linhas violadas são mantidas: o modelo fica inviável.
    auto emitir = [&](Linha&& ln) {
        bool todasFixas = true;
        double valor = 0.0;
        for (const Termo& tm : ln.termos) {
            if (!fixa(tm.var)) { todasFixas = false; break; }
            valor += tm.coef * m.lb[tm.var];
        }
        if (todasFixas) {
            bool ok = ln.sentido == '<' ? valor <= ln.rhs + 1e-9
                    : ln.sentido == '>' ? valor >= ln.rhs - 1e-9
                                        : std::abs(valor - ln.rhs) <= 1e-9;
            if (ok) return;
        }
        m.linhas.push_back(std::move(ln));
    };

    // ---------------- Pré-processamento: posição inicial e (F6) ----------------
    // pi0/pj0 = posição inicial do bloco n; primeiroMov[n] = m(n), primeiro período em
    // que n pode ser relocado (N+1 se nunca).
    std::vector<int> pi0(N + 1), pj0(N + 1), primeiroMov(N + 1, N + 1);
    for (int i = 0; i < W; ++i) {
        int menorAbaixo = N + 1;
        for (int j = 0; j < static_cast<int>(inst.pilhas[i].size()); ++j) {
            int n = inst.pilhas[i][j];
            pi0[n] = i;
            pj0[n] = j;
            if (menorAbaixo < n) primeiroMov[n] = menorAbaixo;
            menorAbaixo = std::min(menorAbaixo, n);
        }
    }
    // O bloco n está parado (posição conhecida) no início do período t?
    auto parado = [&](int n, int t) { return t <= std::min(n, primeiroMov[n]); };

    // ---------------- Variáveis b ----------------
    // idB[(i,j,n,t)] = índice da variável, ou -1 se fixada em 0 (não criada).
    std::vector<int> idB(static_cast<std::size_t>(W) * H * (N + 1) * (N + 1), -1);
    auto posB = [&](int i, int j, int n, int t) {
        return ((static_cast<std::size_t>(i) * H + j) * (N + 1) + n) * (N + 1) + t;
    };
    auto B = [&](int i, int j, int n, int t) { return idB[posB(i, j, n, t)]; };
    auto nomeB = [&](int i, int j, int n, int t) {
        return "b_" + rot(i + 1) + "_" + rot(j + 1) + "_" + rot(n) + "_" + rot(t);
    };

    for (int t = 1; t <= N; ++t) {
        for (int n = t; n <= N; ++n) {  // (F1): n < t não é criada
            if (parado(n, t)) {
                // (L6) no período 1 e (F6) nos seguintes: posição conhecida, b = 1.
                idB[posB(pi0[n], pj0[n], n, t)] = novaVar(nomeB(pi0[n], pj0[n], n, t), 1.0, 0.0);
            } else {
                int jMax = std::min(H - 1, N - t);  // (F5): nível <= N-t+1
                for (int i = 0; i < W; ++i)
                    for (int j = 0; j <= jMax; ++j) idB[posB(i, j, n, t)] = novaVar(nomeB(i, j, n, t), 0.0, 0.0);
            }
        }
    }

    // ---------------- Variáveis y ----------------
    // (F2): só existe y[i][j][t][t]; e só onde o bloco t pode estar no período t.
    std::vector<int> idY(static_cast<std::size_t>(W) * H * (N + 1), -1);
    auto posY = [&](int i, int j, int t) { return (static_cast<std::size_t>(i) * H + j) * (N + 1) + t; };
    for (int t = 1; t <= N; ++t)
        for (int i = 0; i < W; ++i)
            for (int j = 0; j < H; ++j)
                if (B(i, j, t, t) >= 0)
                    idY[posY(i, j, t)] =
                        novaVar("y_" + rot(i + 1) + "_" + rot(j + 1) + "_" + rot(t) + "_" + rot(t), 0.0, 0.0);

    // ---------------- Variáveis x ----------------
    std::vector<std::vector<int>> xsDoPeriodo(N + 1);  // índices em m.xs
    for (int t = 1; t <= N - 1; ++t) {                 // (F3): nada é relocado no período N
        bool alvoParado = primeiroMov[t] > t;          // (F7): o alvo t nunca foi relocado
        for (int n = t + 1; n <= N; ++n) {             // (F3): n <= t não é criada
            if (t < primeiroMov[n]) continue;          // (F6): n ainda não pode ser relocado
            for (int i = 0; i < W; ++i) {
                for (int j = 1; j < H; ++j) {          // (F4): origem no nível 1 é impossível
                    if (B(i, j, n, t) < 0) continue;   // (F4)/(F6): n não pode estar em (i,j)
                    if (alvoParado && (i != pi0[t] || j <= pj0[t])) continue;  // (F7)
                    for (int k = 0; k < W; ++k) {
                        if (k == i) continue;          // (8): não reloca para a própria pilha
                        int lMax = std::min(H - 1, N - t - 1);  // (F5): nível de destino <= N-t
                        for (int l = 0; l <= lMax; ++l) {
                            if (B(k, l, n, t + 1) < 0) continue;  // (F4): destino impossível
                            int id = novaVar("x_" + rot(i + 1) + "_" + rot(j + 1) + "_" + rot(k + 1) + "_" +
                                                 rot(l + 1) + "_" + rot(n) + "_" + rot(t),
                                             0.0, 1.0);  // objetivo: soma de todos os x
                            xsDoPeriodo[t].push_back(static_cast<int>(m.xs.size()));
                            m.xs.push_back({id, i, j, k, l, n, t});
                        }
                    }
                }
            }
        }
    }

    // ---------------- (1) cada bloco ainda na baia ocupa exatamente uma posição ----------------
    // Para n < t, v[n][t] = 1 e todos os b são 0 (F1). Para n >= t, v = 0 e Σ b = 1.
    for (int t = 1; t <= N; ++t) {
        for (int n = t; n <= N; ++n) {
            Linha ln;
            ln.sentido = '=';
            ln.rhs = 1.0;
            ln.nome = "R1_n" + rot(n) + "_t" + rot(t);
            for (int i = 0; i < W; ++i)
                for (int j = 0; j < H; ++j)
                    if (B(i, j, n, t) >= 0) ln.termos.push_back({B(i, j, n, t), 1.0});
            emitir(std::move(ln));
        }
    }

    // ---------------- (2) no máximo um bloco por posição ----------------
    for (int t = 1; t <= N; ++t)
        for (int i = 0; i < W; ++i)
            for (int j = 0; j < H; ++j) {
                Linha ln;
                ln.sentido = '<';
                ln.rhs = 1.0;
                ln.nome = "R2_i" + rot(i + 1) + "_j" + rot(j + 1) + "_t" + rot(t);
                for (int n = t; n <= N; ++n)
                    if (B(i, j, n, t) >= 0) ln.termos.push_back({B(i, j, n, t), 1.0});
                if (ln.termos.size() >= 2) emitir(std::move(ln));
            }

    // ---------------- (3) sem buracos: (i,j+1) ocupada só se (i,j) ocupada ----------------
    for (int t = 1; t <= N; ++t)
        for (int i = 0; i < W; ++i)
            for (int j = 0; j + 1 < H; ++j) {
                Linha ln;
                ln.sentido = '>';
                ln.rhs = 0.0;
                ln.nome = "R3_i" + rot(i + 1) + "_j" + rot(j + 1) + "_t" + rot(t);
                bool temCima = false;
                for (int n = t; n <= N; ++n) {
                    if (B(i, j, n, t) >= 0) ln.termos.push_back({B(i, j, n, t), 1.0});
                    if (B(i, j + 1, n, t) >= 0) {
                        ln.termos.push_back({B(i, j + 1, n, t), -1.0});
                        temCima = true;
                    }
                }
                if (temCima) emitir(std::move(ln));
            }

    // ---------------- (4) conservação: configuração de t a partir da de t-1 ----------------
    // b[i][j][n][t] - b[i][j][n][t-1] - Σ entradas x(t-1) + Σ saídas x(t-1) + y[i][j][n][t-1] = 0.
    // Para n = t-1 (retirado em t-1) b[.][.][n][t] = 0 e não há x, então a linha vira
    // y[i][j][t-1][t-1] = b[i][j][t-1][t-1].
    for (int t = 2; t <= N; ++t) {
        std::vector<Linha> porPos(static_cast<std::size_t>(W) * H * (N + 1));
        auto pos = [&](int i, int j, int n) { return (static_cast<std::size_t>(i) * H + j) * (N + 1) + n; };
        for (int n = t - 1; n <= N; ++n)
            for (int i = 0; i < W; ++i)
                for (int j = 0; j < H; ++j) {
                    Linha& ln = porPos[pos(i, j, n)];
                    if (n >= t && B(i, j, n, t) >= 0) ln.termos.push_back({B(i, j, n, t), 1.0});
                    if (B(i, j, n, t - 1) >= 0) ln.termos.push_back({B(i, j, n, t - 1), -1.0});
                    if (n == t - 1 && idY[posY(i, j, t - 1)] >= 0)
                        ln.termos.push_back({idY[posY(i, j, t - 1)], 1.0});
                }
        for (int ix : xsDoPeriodo[t - 1]) {
            const VarX& x = m.xs[ix];
            porPos[pos(x.k, x.l, x.n)].termos.push_back({x.id, -1.0});  // entrada em (k,l)
            porPos[pos(x.i, x.j, x.n)].termos.push_back({x.id, 1.0});   // saída de (i,j)
        }
        for (int n = t - 1; n <= N; ++n)
            for (int i = 0; i < W; ++i)
                for (int j = 0; j < H; ++j) {
                    Linha& ln = porPos[pos(i, j, n)];
                    if (ln.termos.empty()) continue;
                    ln.sentido = '=';
                    ln.rhs = 0.0;
                    ln.nome = "R4_i" + rot(i + 1) + "_j" + rot(j + 1) + "_n" + rot(n) + "_t" + rot(t);
                    emitir(std::move(ln));
                }
    }

    // ---------------- (5)/(L3) exatamente o bloco t é retirado no período t ----------------
    // Com (F2), a (5) "v[n][t] = Σ_{t'<t} y[.][.][n][t']" reduz-se a Σ_ij y[i][j][n][n] = 1
    // para t > n; a (L3) estende a exigência ao último bloco (n = N).
    for (int t = 1; t <= N; ++t) {
        Linha ln;
        ln.sentido = '=';
        ln.rhs = 1.0;
        ln.nome = "R5_t" + rot(t);
        for (int i = 0; i < W; ++i)
            for (int j = 0; j < H; ++j)
                if (idY[posY(i, j, t)] >= 0) ln.termos.push_back({idY[posY(i, j, t)], 1.0});
        emitir(std::move(ln));
    }

    // ---------------- (L2) só se retira o bloco de onde ele está ----------------
    for (int t = 1; t <= N; ++t)
        for (int i = 0; i < W; ++i)
            for (int j = 0; j < H; ++j) {
                if (idY[posY(i, j, t)] < 0) continue;
                Linha ln;
                ln.sentido = '<';
                ln.rhs = 0.0;
                ln.nome = "L2_i" + rot(i + 1) + "_j" + rot(j + 1) + "_t" + rot(t);
                ln.termos.push_back({idY[posY(i, j, t)], 1.0});
                ln.termos.push_back({B(i, j, t, t), -1.0});
                emitir(std::move(ln));
            }

    // ---------------- restrições por período que envolvem x ----------------
    const double fator6 = op.restricao6Original ? 1.0 : static_cast<double>(H);
    const double M = static_cast<double>(std::max(1, H - 1));  // máx. de relocações por período

    for (int t = 1; t <= N - 1; ++t) {
        const std::vector<int>& xt = xsDoPeriodo[t];

        // ---- (6c) ordem LIFO entre um par de pilhas, com o fator H (ver cabeçalho) ----
        // fator * Σ_n x[i][j][k][l][n][t] + Σ_n Σ_{j'>j} Σ_{l'>l} x[i][j'][k][l'][n][t] <= fator
        // Na versão corrigida (fator = H) a linha é redundante quando o primeiro
        // somatório não tem variáveis (no máximo H-1 blocos saem de uma pilha) e por
        // isso é omitida. Na versão original (fator = 1) ela NÃO é redundante e é
        // emitida sempre que o segundo somatório tem variáveis.
        {
            std::vector<std::vector<int>> dePara(static_cast<std::size_t>(W) * W * H * H);
            auto pos = [&](int i, int k, int j, int l) {
                return ((static_cast<std::size_t>(i) * W + k) * H + j) * H + l;
            };
            for (int ix : xt) {
                const VarX& x = m.xs[ix];
                dePara[pos(x.i, x.k, x.j, x.l)].push_back(x.id);
            }
            for (int i = 0; i < W; ++i)
                for (int k = 0; k < W; ++k) {
                    if (k == i) continue;
                    for (int j = 0; j < H; ++j)
                        for (int l = 0; l < H; ++l) {
                            Linha ln;
                            ln.sentido = '<';
                            ln.rhs = fator6;
                            ln.nome = "R6c_i" + rot(i + 1) + "_k" + rot(k + 1) + "_j" + rot(j + 1) + "_l" +
                                      rot(l + 1) + "_t" + rot(t);
                            const std::vector<int>& primeiro = dePara[pos(i, k, j, l)];
                            for (int id : primeiro) ln.termos.push_back({id, fator6});
                            bool temSegundo = false;
                            for (int j2 = j + 1; j2 < H; ++j2)
                                for (int l2 = l + 1; l2 < H; ++l2)
                                    for (int id : dePara[pos(i, k, j2, l2)]) {
                                        ln.termos.push_back({id, 1.0});
                                        temSegundo = true;
                                    }
                            if (!temSegundo) continue;
                            if (primeiro.empty() && !op.restricao6Original) continue;
                            emitir(std::move(ln));
                        }
                }
        }

        // ---- (7) só saem blocos da pilha em que está o alvo t ----
        // M * Σ_j b[i][j][t][t] + Σ_{origem ≠ i} x <= M
        for (int i = 0; i < W; ++i) {
            Linha ln;
            ln.sentido = '<';
            ln.rhs = M;
            ln.nome = "R7_i" + rot(i + 1) + "_t" + rot(t);
            for (int j = 0; j < H; ++j)
                if (B(i, j, t, t) >= 0) ln.termos.push_back({B(i, j, t, t), M});
            bool temX = false;
            for (int ix : xt) {
                const VarX& x = m.xs[ix];
                if (x.i != i) {
                    ln.termos.push_back({x.id, 1.0});
                    temX = true;
                }
            }
            if (temX) emitir(std::move(ln));
        }

        // ---- (L1) só se reloca o bloco a partir de onde ele está ----
        // Σ_k Σ_l x[i][j][k][l][n][t] - b[i][j][n][t] <= 0
        // ---- (L4) A1: só sai de (i,j) bloco que está acima do alvo ----
        // Σ_k Σ_l Σ_n x[i][j][k][l][n][t] - Σ_{j'<j} b[i][j'][t][t] <= 0
        // ---- (L5) todo bloco acima do alvo é relocado ----
        // Σ_k Σ_l x[i][j][k][l][n][t] - b[i][j][n][t] - Σ_{j'<j} b[i][j'][t][t] >= -1
        {
            std::vector<std::vector<int>> saiDe(static_cast<std::size_t>(W) * H * (N + 1));
            auto pos = [&](int i, int j, int n) { return (static_cast<std::size_t>(i) * H + j) * (N + 1) + n; };
            for (int ix : xt) {
                const VarX& x = m.xs[ix];
                saiDe[pos(x.i, x.j, x.n)].push_back(x.id);
            }
            for (int i = 0; i < W; ++i)
                for (int j = 1; j < H; ++j) {
                    std::vector<int> alvoAbaixo;  // b[i][j'][t][t], j' < j
                    for (int j2 = 0; j2 < j; ++j2)
                        if (B(i, j2, t, t) >= 0) alvoAbaixo.push_back(B(i, j2, t, t));

                    Linha l4;
                    l4.sentido = '<';
                    l4.rhs = 0.0;
                    l4.nome = "L4_i" + rot(i + 1) + "_j" + rot(j + 1) + "_t" + rot(t);

                    for (int n = t + 1; n <= N; ++n) {
                        int bn = B(i, j, n, t);
                        const std::vector<int>& saidas = saiDe[pos(i, j, n)];
                        std::string sufixo = "_i" + rot(i + 1) + "_j" + rot(j + 1) + "_n" + rot(n) + "_t" + rot(t);

                        if (!saidas.empty()) {
                            Linha l1;
                            l1.sentido = '<';
                            l1.rhs = 0.0;
                            l1.nome = "L1" + sufixo;
                            for (int id : saidas) {
                                l1.termos.push_back({id, 1.0});
                                l4.termos.push_back({id, 1.0});
                            }
                            l1.termos.push_back({bn, -1.0});
                            emitir(std::move(l1));
                        }

                        if (bn >= 0 && !alvoAbaixo.empty()) {
                            Linha l5;
                            l5.sentido = '>';
                            l5.rhs = -1.0;
                            l5.nome = "L5" + sufixo;
                            for (int id : saidas) l5.termos.push_back({id, 1.0});
                            l5.termos.push_back({bn, -1.0});
                            for (int id : alvoAbaixo) l5.termos.push_back({id, -1.0});
                            emitir(std::move(l5));
                        }
                    }

                    if (!l4.termos.empty()) {
                        for (int id : alvoAbaixo) l4.termos.push_back({id, -1.0});
                        emitir(std::move(l4));
                    }
                }
        }
    }

    return m;
}

// Converte os valores das variáveis (na ordem de Modelo::nomeVar) na sequência de
// movimentos. Dentro de um período, os blocos saem da pilha do alvo de cima para
// baixo, isto é, em ordem decrescente do nível de origem.
inline std::vector<brp::Movimento> extrairMovimentos(const Modelo& m, const std::vector<double>& valores) {
    if (static_cast<int>(valores.size()) != m.numVars())
        throw std::logic_error("extrairMovimentos: número de valores diferente do número de variáveis");
    std::vector<const VarX*> ativos;
    for (const VarX& x : m.xs)
        if (valores[x.id] > 0.5) ativos.push_back(&x);
    std::stable_sort(ativos.begin(), ativos.end(), [](const VarX* a, const VarX* b) {
        if (a->t != b->t) return a->t < b->t;
        return a->j > b->j;
    });
    std::vector<brp::Movimento> movs;
    for (const VarX* x : ativos) movs.push_back({x->n, x->i + 1, x->k + 1, x->t});
    return movs;
}

#ifdef BRP2_COM_GUROBI
// Copia a formulação para um GRBModel (minimização). Devolve as variáveis do Gurobi
// na mesma ordem de Modelo::nomeVar.
inline std::vector<GRBVar> carregarNoGurobi(const Modelo& m, GRBModel& grb) {
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
    grb.update();
    return vars;
}
#endif  // BRP2_COM_GUROBI

}  // namespace brp2
