// ri.cpp — main 3: heurística Reshuffle Index (Murty et al., 2005) para o BRP restrito.
//
// Regra de destino para o bloco r a relocar:
//   RI(i) = número de blocos na pilha i com prioridade menor que r, isto é, quantos
//           blocos de i sairão antes de r e, portanto, serão bloqueados por ele.
//   Escolhe a pilha de menor RI. O desempate fica isolado em desempateRI().
#include "instance.hpp"

namespace {

struct CandidataRI {
    int pilha;   // índice da pilha (0-based)
    int ri;      // reshuffle index
    int altura;  // número de blocos na pilha
};

// CRITÉRIO DE DESEMPATE (troque aqui para experimentar outro).
// Chamado apenas quando a.ri == b.ri. Devolve true se `a` deve ser preferida a `b`.
// Atual: pilha de menor altura; persistindo o empate, menor índice.
bool desempateRI(const CandidataRI& a, const CandidataRI& b) {
    if (a.altura != b.altura) return a.altura < b.altura;
    return a.pilha < b.pilha;
}

int escolherRI(const brp::Baia& baia, const std::vector<int>& candidatas, int r, int /*N*/) {
    bool tem = false;
    CandidataRI melhor{};
    for (int i : candidatas) {
        CandidataRI c{i, 0, static_cast<int>(baia[i].size())};
        for (int b : baia[i])
            if (b < r) ++c.ri;
        if (!tem || c.ri < melhor.ri || (c.ri == melhor.ri && desempateRI(c, melhor))) {
            melhor = c;
            tem = true;
        }
    }
    return melhor.pilha;
}

}  // namespace

int main(int argc, char** argv) {
    return brp::mainHeuristica(argc, argv, "ri", escolherRI);
}
