// min_max.cpp — main 2: heurística Min-Max (Caserta, Schwarze e Voß) para o BRP restrito.
//
// Regra de destino para o bloco r a relocar:
//   min(i) = menor prioridade na pilha i (pilha vazia vale N+1).
//   - Se existe pilha com min(i) > r (r não bloqueará ninguém), escolhe a de MENOR min(i).
//   - Senão (r vai bloquear de qualquer jeito), escolhe a de MAIOR min(i), para adiar
//     ao máximo a próxima relocação de r.
//   - Empate: menor índice de pilha.
//
// A regra está em regras.hpp (brp::escolherMinMax) porque o A* também a usa, como
// limite superior.
#include "instance.hpp"
#include "regras.hpp"

int main(int argc, char** argv) {
    return brp::mainHeuristica(argc, argv, "min_max", brp::escolherMinMax);
}
