// min_max.cpp — main 2: heurística Min-Max (Caserta, Schwarze e Voß) para o BRP restrito.
//
// Regra de destino para o bloco r a relocar:
//   min(i) = menor prioridade na pilha i (pilha vazia vale N+1).
//   - Se existe pilha com min(i) > r (r não bloqueará ninguém), escolhe a de MENOR min(i).
//   - Senão (r vai bloquear de qualquer jeito), escolhe a de MAIOR min(i), para adiar
//     ao máximo a próxima relocação de r.
//   - Empate: menor índice de pilha.
#include "instance.hpp"

namespace {

int minimoDaPilha(const std::vector<int>& pilha, int N) {
    int m = N + 1;
    for (int b : pilha) m = std::min(m, b);
    return m;
}

int escolherMinMax(const brp::Baia& baia, const std::vector<int>& candidatas, int r, int N) {
    int melhorAcima = -1, minAcima = 0;    // pilhas com min(i) > r: queremos o menor min
    int melhorAbaixo = -1, maxAbaixo = 0;  // pilhas com min(i) < r: queremos o maior min
    for (int i : candidatas) {             // índices crescentes => empate fica com o menor
        int m = minimoDaPilha(baia[i], N);
        if (m > r) {
            if (melhorAcima < 0 || m < minAcima) { melhorAcima = i; minAcima = m; }
        } else {
            if (melhorAbaixo < 0 || m > maxAbaixo) { melhorAbaixo = i; maxAbaixo = m; }
        }
    }
    return melhorAcima >= 0 ? melhorAcima : melhorAbaixo;
}

}  // namespace

int main(int argc, char** argv) {
    return brp::mainHeuristica(argc, argv, "min_max", escolherMinMax);
}
