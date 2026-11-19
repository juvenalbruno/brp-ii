// regras.hpp — regras de destino compartilhadas entre executáveis.
//
// A regra Min-Max fica aqui porque é usada por dois executáveis: pela heurística
// min_max (min_max.cpp) e pelo A* (astar.cpp), que roda a Min-Max antes da busca para
// obter um limite superior e podar os nós com f(n) >= UB.
#pragma once

#include <algorithm>
#include <vector>

#include "instance.hpp"

namespace brp {

// min(i) = menor prioridade da pilha i; pilha vazia vale N+1.
inline int minimoDaPilha(const std::vector<int>& pilha, int N) {
    int m = N + 1;
    for (int b : pilha) m = std::min(m, b);
    return m;
}

// Regra Min-Max (Caserta, Schwarze e Voß) para o bloco r a relocar:
//   - se existe pilha com min(i) > r (r não bloqueará ninguém), escolhe a de MENOR min(i);
//   - senão (r vai bloquear de qualquer jeito), escolhe a de MAIOR min(i), para adiar
//     ao máximo a próxima relocação de r;
//   - empate: menor índice de pilha.
inline int escolherMinMax(const Baia& baia, const std::vector<int>& candidatas, int r, int N) {
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

}  // namespace brp
