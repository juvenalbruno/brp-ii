# Min-Max e Reshuffle Index no BRP restrito: relatório dos experimentos

## Resumo

Este relatório compara duas heurísticas construtivas para o *Blocks Relocation Problem*
(BRP) restrito: Min-Max (Caserta, Schwarze e Voß, 2012) e Reshuffle Index, ou RI (Murty
et al., 2005). Os testes usam as 170 instâncias fornecidas para a disciplina.

- O Min-Max fez menos relocações que o RI em 119 instâncias, empatou em 50 e perdeu em 1.
  O teste de Wilcoxon pareado rejeita a igualdade (p ≈ 3 × 10⁻²¹).
- O ótimo é conhecido em 126 instâncias. Nelas, o Min-Max ficou em média **2,5%** acima
  do ótimo e o atingiu em 101 casos. O RI ficou **13,0%** acima e atingiu o ótimo em 47.
- A vantagem do Min-Max cresce com a ocupação do pátio. Com cerca de 50% de ocupação, o
  RI faz em média 1,3 relocação a mais; com 100%, faz 6,9 a mais.
- As duas heurísticas levam microssegundos por instância. O tempo não as diferencia.

---

## 1. Heurísticas usadas

### 1.1 O problema

São N blocos, numerados de 1 a N, distribuídos em W pilhas de altura máxima H. Os blocos
saem em ordem: primeiro o 1, depois o 2, e assim por diante. Um bloco só sai quando está
no topo da sua pilha. Na variante **restrita** (suposição A1 do artigo de Caserta et al.),
só podem ser relocados os blocos que estão acima do alvo atual, na pilha do alvo. O
objetivo é retirar todos os blocos com o menor número de relocações.

### 1.2 As duas heurísticas

| Heurística | Origem | Executável | Código |
|---|---|---|---|
| Min-Max | Caserta, Schwarze e Voß (2012), Eq. (11); no artigo, *Heuristic_Rule* | `min_max` | `src/min_max.cpp` |
| Reshuffle Index (RI) | Murty et al. (2005) | `ri` | `src/ri.cpp` |

As duas usam o mesmo procedimento construtivo, que é o Algoritmo 1 do artigo:

1. Para cada alvo t = 1, …, N:
   1. enquanto t não estiver no topo da sua pilha s, tome o bloco r do topo de s e mova-o
      para uma pilha d, escolhida **pela regra da heurística** entre as outras pilhas com
      menos de H blocos;
   2. retire t.

Só a escolha da pilha de destino d muda de uma heurística para a outra. Toda diferença
nos resultados vem, portanto, da regra de escolha. O procedimento comum está em
`heuristicaConstrutiva`, em `src/instance.hpp`.

### 1.3 Referências usadas na comparação

Estas não são heurísticas. Servem para medir a distância de cada heurística ao ótimo:

- **Método exato:** o modelo BRP-II do artigo, resolvido com o HiGHS (Seção 3.3).
- **Limite inferior:** o maior entre o limite do solver e a contagem de blocos que
  bloqueiam outros (Seção 3.4).

---

## 2. Finalidade e funcionamento de cada heurística

### 2.1 Finalidade comum

As duas heurísticas existem para dar, quase de imediato, uma solução viável de boa
qualidade. Caserta et al. (2012) apresentam a sua como uma "regra prática" para quando a
resposta precisa ser dada em tempo real, como na operação de um pátio de contêineres. A
solução heurística também serve de limite superior para o método exato.

As duas são determinísticas: a mesma instância sempre produz a mesma solução. A cada
relocação, examinam as W pilhas, com custo O(W·H).

### 2.2 Min-Max

Para cada pilha i, seja **min(i)** o menor número da pilha, isto é, o próximo bloco de i a
sair. Uma pilha vazia vale min(i) = N + 1. Para o bloco r que vai ser relocado:

- **Se alguma pilha candidata tem min(i) > r**, colocar r ali não bloqueia ninguém. Entre
  essas pilhas, escolhe a de **menor** min(i).
- **Se nenhuma tem**, r vai bloquear alguém de qualquer jeito. Escolhe a pilha de **maior**
  min(i).
- **Empate:** a pilha de menor índice.

No primeiro caso, a regra usa a pilha "mais justa" e preserva as pilhas de min(i) grande,
incluindo as vazias, para blocos futuros que precisem delas. No segundo caso, coloca r
sobre a pilha cujo bloco mais urgente sai por último. Assim, a próxima relocação de r fica
o mais tarde possível.

### 2.3 Reshuffle Index (RI)

Para cada pilha i, **RI(i)** é o número de blocos de i com número menor que r, ou seja,
quantos blocos r bloquearia se fosse colocado ali. A regra escolhe a pilha de **menor**
RI(i). No empate, escolhe a pilha mais baixa e, persistindo o empate, a de menor índice.

O critério de desempate é uma escolha desta implementação, isolada na função
`desempateRI` em `src/ri.cpp`. A finalidade da regra é minimizar o dano imediato: o número
de blocos sobre os quais r é colocado.

### 2.4 Onde as regras divergem

As pilhas estão escritas da base para o topo.

| Situação | Pilhas candidatas | RI escolhe | Min-Max escolhe |
|---|---|---|---|
| Várias pilhas sem bloqueio (r = 3) | P1 = [9]; P2 = [4, 5] | **P1**: as duas têm RI = 0 e P1 é mais baixa | **P2**: entre as pilhas com min(i) > 3, a de menor min(i) é P2, com min 4 |
| Bloqueio inevitável (r = 7) | P1 = [2, 8, 9]; P2 = [5, 6] | **P1**: bloqueia só o bloco 2 (RI = 1, contra 2 em P2) | **P2**: maior min(i), 5 contra 2 |

- **Primeira situação:** o RI põe o 3 sobre o 9 e ocupa uma pilha que depois poderia
  receber, por exemplo, o bloco 8 sem bloqueio. O Min-Max mantém essa pilha livre.
- **Segunda situação:** em qualquer das pilhas, o bloco 7 será relocado de novo
  exatamente uma vez, quando sair o primeiro bloco abaixo dele. Bloquear um bloco ou dois
  custa o mesmo: uma relocação. Em P1 essa relocação acontece quando sai o bloco 2; em P2,
  só quando sai o 5. O Min-Max adia a relocação. O RI minimiza uma contagem que não muda o
  custo imediato.

---

## 3. Como os testes foram realizados

### 3.1 Instâncias

Foram usadas as 170 instâncias fornecidas, sem alteração: 17 arquivos com 10 instâncias
cada.

- **Conjunto A (50 instâncias):** `brp_W3_H4_N8`, `brp_W4_H4_N10`, `brp_W4_H5_N12`,
  `brp_W5_H4_N15` e `brp_W5_H5_N18`.
- **Conjunto B (120 instâncias):** W = 7 e H de 5 a 8. Para cada H há três valores de N,
  que correspondem a cerca de 50%, 75% e 100% da capacidade viável W·H − (H − 1).

| H | ~50% | ~75% | ~100% |
|---|---|---|---|
| 5 | N = 16 | N = 23 | N = 31 |
| 6 | N = 18 | N = 28 | N = 37 |
| 7 | N = 22 | N = 32 | N = 43 |
| 8 | N = 24 | N = 37 | N = 49 |

### 3.2 Implementação e ambiente

- **Código:** C++17, compilado com Apple clang 21 e CMake em modo Release.
- **Máquina:** Apple M1, 8 GB de RAM, macOS.
- **Execuções:** cada heurística rodou uma vez por instância, porque é determinística.
- **Tempo medido:** o tempo de CPU do procedimento construtivo. A leitura do JSON, a
  verificação e a gravação do resultado ficam de fora.
- **Registro:** cada solução foi gravada em JSON com a sequência completa de movimentos, em
  `results/min_max/` e `results/ri/`.

### 3.3 Solução de referência (método exato)

- **Modelo:** é o BRP-II de Caserta et al. (2012), com uma correção. Na restrição que
  impõe a ordem LIFO (Eq. 8 do artigo), o lado esquerdo ganha o fator H, conforme Eskandari
  e Azari (2015) e Zehendner et al. (2015). Sem a correção, a restrição proíbe mover dois
  blocos da mesma pilha para a mesma pilha de destino no mesmo período. Com isso, a
  instância da Fig. 2 do artigo fica inviável. O modelo também tem restrições de ligação
  entre as variáveis e fixações de variáveis impossíveis.
- **Solver:** HiGHS 1.16.0. O Gurobi e o CPLEX não estavam disponíveis.
- **Rodada 1:** as 170 instâncias, com limite de 120 s por instância e o presolve padrão do
  HiGHS (ligado).
- **Rodada 2:** só as 59 instâncias sem ótimo provado na rodada 1, com o presolve
  **desligado**. Na rodada 1, o presolve consumia todo o limite de tempo nas instâncias
  maiores, sem chegar a resolver a relaxação linear. As 59 rodaram primeiro com 120 s. As
  53 que continuaram sem prova seguiram para uma passada de 600 s, interrompida depois de
  15 instâncias: 2 de `W7_H5_N31`, 3 de `W7_H6_N28`, 9 de `W7_H6_N37` e 1 de `W7_H7_N32`.
- **Referência adotada:** para cada instância, a melhor solução e o melhor limite inferior
  encontrados em qualquer das rodadas. Juntar as rodadas é legítimo porque toda solução foi
  verificada e todo limite do solver é válido, qualquer que seja a configuração.

O artigo usou 86.400 s (um dia) por instância. Os limites aqui são bem menores, o que
explica parte das instâncias grandes sem ótimo conhecido.

### 3.4 Limite inferior

O limite inferior de cada instância é o maior entre:

- o **limite do solver**, isto é, o melhor limite dual do HiGHS arredondado para cima,
  pois o objetivo é inteiro;
- o **limite de bloqueio**, isto é, o número de blocos que têm, abaixo deles na mesma
  pilha, algum bloco de número menor. Cada um deles precisa ser relocado ao menos uma vez.

### 3.5 Métricas

**Relocações.** O número de relocações da solução.

**Gap em relação ao ótimo.** Usa a definição do artigo, γ = (z_H − z\*) / z\*, em que z_H
é o valor da heurística. Como z\* (o ótimo) nem sempre é conhecido, o gap é dado como um
intervalo:

- **γ inferior:** z\* = melhor solução conhecida, o menor valor entre solver, Min-Max e
  RI. É uma estimativa otimista.
- **γ superior:** z\* = limite inferior. É uma estimativa pessimista.

O gap verdadeiro está dentro desse intervalo. Quando o limite inferior coincide com a
melhor solução conhecida, o ótimo é conhecido e o gap é exato. Quando a melhor solução
conhecida é a da própria heurística, o γ inferior dela é zero por construção. Nas tabelas,
esses casos têm asterisco (\*).

**Comparação direta.** Vitórias, empates e derrotas por instância, e o teste de Wilcoxon
pareado bilateral sobre as diferenças RI − Min-Max. O teste usa aproximação normal,
descarta as diferenças nulas e dá postos médios aos empates. O nível de significância é 5%.

**Tempo.** Reportado, mas não usado como critério, como no artigo. Uma métrica que combine
relocações e tempo, como o hipervolume, não foi usada nesta comparação. Com tempos de
microssegundos, ela refletiria mais o ruído do cronômetro do que a qualidade das soluções.

### 3.6 Verificação

- **Soluções:** cada solução gravada foi reexecutada por um verificador independente
  (`verificar_resultados`). Ele confere que cada relocação sai da pilha do alvo, de cima
  do alvo, respeita a altura H, que os blocos saem na ordem e que o pátio termina vazio.
  Foram 471 soluções conferidas (170 do Min-Max, 170 do RI e 131 do solver), sem nenhuma
  falha.
- **Limites:** nenhuma heurística ficou abaixo do limite inferior em nenhuma instância.
- **Modelo exato:** foi validado contra uma busca em largura exaustiva em 61 instâncias.
  São 40 aleatórias de até 4×4 (semente 2026), as 20 de `brp_W3_H4_N8` e `brp_W4_H4_N10` e
  a instância da Fig. 2 do artigo. O ótimo foi igual em todas, e a instância da Fig. 2 dá
  6 relocações, como no artigo. Com a restrição original, sem a correção, essa instância
  fica inviável.

---

## 4. Resultados

### 4.1 Solução de referência

| Arquivo | Ótimo provado pelo solver | Ótimo conhecido | Só solução viável | Sem solução do solver | Tempo médio das provas (s) | Tempo máximo das provas (s) |
|---|---:|---:|---:|---:|---:|---:|
| W3_H4_N8 | 10 | 10 | 0 | 0 | 0,07 | 0,2 |
| W4_H4_N10 | 10 | 10 | 0 | 0 | 0,18 | 1,3 |
| W4_H5_N12 | 10 | 10 | 0 | 0 | 2,7 | 14,9 |
| W5_H4_N15 | 10 | 10 | 0 | 0 | 1,4 | 4,6 |
| W5_H5_N18 | 10 | 10 | 0 | 0 | 13,3 | 24,0 |
| W7_H5_N16 | 10 | 10 | 0 | 0 | 15,9 | 42,3 |
| W7_H5_N23 | 10 | 10 | 0 | 0 | 24,9 | 61,3 |
| W7_H5_N31 | 9 | 9 | 0 | 1 | 114,0 | 539,1 |
| W7_H6_N18 | 10 | 10 | 0 | 0 | 29,5 | 81,7 |
| W7_H6_N28 | 9 | 9 | 1 | 0 | 119,9 | 368,3 |
| W7_H6_N37 | 3 | 4 | 4 | 3 | 272,1 | 399,0 |
| W7_H7_N22 | 10 | 10 | 0 | 0 | 53,7 | 115,6 |
| W7_H7_N32 | 2 | 4 | 6 | 2 | 80,8 | 103,1 |
| W7_H7_N43 | 0 | 0 | 0 | 10 | – | – |
| W7_H8_N24 | 9 | 9 | 1 | 0 | 65,4 | 116,4 |
| W7_H8_N37 | 0 | 1 | 1 | 9 | – | – |
| W7_H8_N49 | 0 | 0 | 0 | 10 | – | – |
| **Total** | **122** | **126** | **13** | **35** | | |

- **Provas:** 111 das provas vieram da rodada 1 e 11 da rodada 2.
- **Ótimos conhecidos sem prova do solver:** em 4 instâncias (`W7_H6_N37` id 4,
  `W7_H7_N32` ids 1 e 5, `W7_H8_N37` id 9), o ótimo é conhecido porque o valor do Min-Max
  coincide com o limite inferior.
- **Sem solução do solver:** as 35 instâncias estão no conjunto B, quase todas nas classes
  com ~100% de ocupação. Nelas, as heurísticas dão as únicas soluções disponíveis.
- **Tempos:** são o tempo de relógio da rodada que provou o ótimo. Vêm de configurações
  diferentes (Seção 3.3) e servem só como ordem de grandeza.

### 4.2 Relocações por classe

As colunas de relocações dão a média das 10 instâncias da classe. V/E/D conta as
instâncias em que o Min-Max foi melhor, empatou ou foi pior que o RI.

| Arquivo | Limite inferior | Melhor conhecido | Min-Max | RI | RI − Min-Max (média) | RI − Min-Max (máximo) | V/E/D |
|---|---:|---:|---:|---:|---:|---:|:---:|
| W3_H4_N8 | 4,1 | 4,1 | 4,2 | 4,4 | 0,2 | 1 | 2/8/0 |
| W4_H4_N10 | 4,4 | 4,4 | 4,4 | 4,6 | 0,2 | 1 | 2/8/0 |
| W4_H5_N12 | 7,1 | 7,1 | 7,4 | 7,4 | 0,0 | 0 | 0/10/0 |
| W5_H4_N15 | 7,1 | 7,1 | 7,5 | 8,6 | 1,1 | 3 | 6/4/0 |
| W5_H5_N18 | 9,8 | 9,8 | 10,4 | 12,0 | 1,6 | 5 | 8/1/1 |
| W7_H5_N16 | 7,0 | 7,0 | 7,0 | 7,1 | 0,1 | 1 | 1/9/0 |
| W7_H5_N23 | 11,6 | 11,6 | 11,7 | 13,4 | 1,7 | 4 | 7/3/0 |
| W7_H5_N31 | 22,2 | 22,6 | 24,3 | 27,5 | 3,2 | 5 | 10/0/0 |
| W7_H6_N18 | 7,8 | 7,8 | 7,8 | 8,3 | 0,5 | 2 | 4/6/0 |
| W7_H6_N28 | 17,3 | 17,4 | 18,1 | 20,4 | 2,3 | 4 | 9/1/0 |
| W7_H6_N37 | 27,9 | 31,0 | 31,4 | 35,6 | 4,2 | 12 | 10/0/0 |
| W7_H7_N22 | 10,5 | 10,5 | 10,6 | 12,6 | 2,0 | 6 | 10/0/0 |
| W7_H7_N32 | 19,9 | 22,1 | 22,2 | 27,9 | 5,7 | 14 | 10/0/0 |
| W7_H7_N43 | 32,8 | 41,0 | 41,0 | 49,3 | 8,3 | 19 | 10/0/0 |
| W7_H8_N24 | 13,5 | 13,8 | 13,9 | 16,5 | 2,6 | 4 | 10/0/0 |
| W7_H8_N37 | 24,2 | 31,3 | 31,3 | 38,9 | 7,6 | 14 | 10/0/0 |
| W7_H8_N49 | 31,3 | 54,0 | 54,0 | 66,0 | 12,0 | 28 | 10/0/0 |
| **Total** | **15,2** | **17,8** | **18,1** | **21,2** | **3,1** | **28** | **119/50/1** |

### 4.3 Distância ao ótimo por classe

Os gaps são médias por classe. Um único valor indica que o ótimo é conhecido em todas as
instâncias da classe. As duas últimas colunas contam, entre as instâncias com ótimo
conhecido, quantas vezes cada heurística o atingiu.

| Arquivo | γ Min-Max | γ RI | Ótimo conhecido | Min-Max atingiu | RI atingiu |
|---|---:|---:|---:|---:|---:|
| W3_H4_N8 | 2,0% | 6,8% | 10 | 9 | 7 |
| W4_H4_N10 | 0,0% | 3,2% | 10 | 10 | 8 |
| W4_H5_N12 | 3,5% | 3,5% | 10 | 8 | 8 |
| W5_H4_N15 | 4,4% | 18,6% | 10 | 7 | 3 |
| W5_H5_N18 | 6,9% | 21,9% | 10 | 5 | 2 |
| W7_H5_N16 | 0,0% | 1,7% | 10 | 10 | 9 |
| W7_H5_N23 | 0,7% | 14,3% | 10 | 9 | 3 |
| W7_H5_N31 | 7,9 – 9,3% | 22,4 – 24,0% | 9 | 3 | 0 |
| W7_H6_N18 | 0,0% | 5,9% | 10 | 10 | 6 |
| W7_H6_N28 | 3,7 – 4,2% | 16,7 – 17,2% | 9 | 6 | 1 |
| W7_H6_N37 | 1,5 – 12,3% | 14,3 – 27,1% | 4 | 2 | 0 |
| W7_H7_N22 | 1,0% | 19,2% | 10 | 9 | 0 |
| W7_H7_N32 | 0,4 – 10,9% | 25,1 – 38,6% | 4 | 4 | 0 |
| W7_H7_N43 | 0,0\* – 27,0% | 20,0 – 53,5% | 0 | – | – |
| W7_H8_N24 | 0,8 – 2,9% | 19,4 – 22,0% | 9 | 8 | 0 |
| W7_H8_N37 | 0,0\* – 28,9% | 25,9 – 60,5% | 1 | 1 | 0 |
| W7_H8_N49 | 0,0\* – 72,6% | 22,4 – 110,3% | 0 | – | – |
| **Total** | **1,9 – 11,0%** | **15,4 – 26,4%** | **126** | **101** | **47** |

\* Em todas as instâncias dessas classes, a melhor solução conhecida é a do próprio
Min-Max. O 0,0% não indica que o Min-Max seja ótimo, só que nenhum método achou solução
melhor.

### 4.4 Resultados agregados

| Grupo | Instâncias | Min-Max | RI | RI − Min-Max | V/E/D | γ Min-Max | γ RI | γ Min-Max (ótimo conhecido) | γ RI (ótimo conhecido) | Ótimo conhecido | Min-Max atingiu | RI atingiu |
|---|---:|---:|---:|---:|:---:|---:|---:|---:|---:|---:|---:|---:|
| Conjunto A | 50 | 6,8 | 7,4 | 0,6 | 18/31/1 | 3,4% | 10,8% | 3,4% | 10,8% | 50 | 39 | 28 |
| Conjunto B | 120 | 22,8 | 27,0 | 4,2 | 101/19/0 | 1,3 – 14,2% | 17,3 – 32,9% | 2,0% | 14,5% | 76 | 62 | 19 |
| B, ~50% | 40 | 9,8 | 11,1 | 1,3 | 25/15/0 | 0,4 – 1,0% | 11,5 – 12,2% | 0,5% | 11,2% | 39 | 37 | 15 |
| B, ~75% | 40 | 20,8 | 25,2 | 4,3 | 36/4/0 | 1,2 – 11,2% | 20,5 – 32,7% | 1,7% | 17,0% | 24 | 20 | 4 |
| B, ~100% | 40 | 37,7 | 44,6 | 6,9 | 40/0/0 | 2,4 – 30,3% | 19,8 – 53,7% | 7,0% | 19,9% | 13 | 5 | 0 |
| **Total** | **170** | **18,1** | **21,2** | **3,1** | **119/50/1** | **1,9 – 11,0%** | **15,4 – 26,4%** | **2,5%** | **13,0%** | **126** | **101** | **47** |

As colunas "γ (ótimo conhecido)" usam só as instâncias com ótimo conhecido. Nelas o gap é
exato, mas a amostra tende a reunir as instâncias mais fáceis de cada grupo.

### 4.5 Teste de Wilcoxon (diferenças RI − Min-Max)

| Grupo | Diferenças não nulas | Soma dos postos (RI pior) | Soma dos postos (RI melhor) | z | p |
|---|---:|---:|---:|---:|---:|
| Conjunto A | 19 | 185 | 5 | 3,62 | 2,9 × 10⁻⁴ |
| Conjunto B | 101 | 5.151 | 0 | 8,72 | 2,7 × 10⁻¹⁸ |
| Total | 120 | 7.247 | 13 | 9,47 | 2,7 × 10⁻²¹ |

Nos três grupos, p < 0,05. A diferença entre as heurísticas é estatisticamente
significativa, a favor do Min-Max.

### 4.6 Tempo das heurísticas

| | Min-Max | RI |
|---|---:|---:|
| Média por classe | de 5 a 12 µs | de 3 a 9 µs |
| Média geral | 7,9 µs | 4,9 µs |
| Máximo | 48 µs | 13 µs |

Os valores por instância estão no Apêndice B. Diferenças de poucos microssegundos estão
no nível do ruído de medição e não devem ser interpretadas.

### 4.7 Casos individuais

- **Única instância em que o RI venceu:** `brp_W5_H5_N18` id 6. O Min-Max fez 8
  relocações; o RI fez 7, que é o ótimo.
- **Maiores diferenças:**
  - `brp_W7_H8_N49` id 6: Min-Max 57 e RI 85, ou seja, 28 a mais (+49%);
  - `brp_W7_H7_N43` id 8: Min-Max 48 e RI 67 (+19).
- **Maiores distâncias do Min-Max ao ótimo:**
  - `brp_W7_H6_N28` id 7: Min-Max 24 e ótimo 20;
  - `brp_W7_H5_N31` id 7: Min-Max 28 e ótimo 24.
- **Heurística contra o solver:**
  - em 7 instâncias, o solver parou no limite com uma solução pior que a do Min-Max (por
    exemplo, `brp_W7_H6_N37` id 1: solver 38, Min-Max 32);
  - em 3 instâncias, a solução do solver, sem prova de otimalidade, foi melhor que a do
    Min-Max;
  - no total, o valor do Min-Max é a melhor solução conhecida em 142 das 170 instâncias
    (103 das 120 do conjunto B), contra 47 do RI.

---

## 5. Análise dos resultados

### 5.1 O Min-Max é melhor que o RI

O Min-Max nunca foi pior que o RI, exceto em uma instância, e foi melhor em 70% delas
(119 de 170). O teste de Wilcoxon confirma a diferença em todos os grupos. Esta conclusão
não depende de conhecer o ótimo: ela vem da comparação direta, instância a instância.

### 5.2 Distância ao ótimo

O ótimo é conhecido em 126 instâncias:

- **Min-Max:** 2,5% acima do ótimo, em média, e acerta o ótimo em 80% dos casos (101 de
  126);
- **RI:** 13,0% acima do ótimo e acerta em 37% (47 de 126).

Nas classes em que o ótimo não é conhecido, o intervalo do gap fica largo: até 72,6% para
o Min-Max em `W7_H8_N49`. A largura vem do limite inferior fraco, e não necessariamente de
uma heurística ruim. Em `W7_H8_N49`, o limite do solver nunca superou o limite de
bloqueio. A comparação relativa continua válida nessas classes: o RI faz de 20% a 26% mais
relocações que o Min-Max.

### 5.3 Efeito do tamanho e da ocupação

- **Empates nas instâncias pequenas:** no conjunto A, as heurísticas empatam em 31 de 50
  instâncias. Com poucas pilhas e poucos blocos, há poucas opções de destino, e as duas
  regras costumam escolher a mesma pilha.
- **Ocupação no conjunto B:** a diferença cresce com a ocupação. A média de RI − Min-Max
  sobe de 1,3 (~50%) para 4,3 (~75%) e 6,9 relocações (~100%). Os empates caem de 15 para
  4 e depois para 0. Com 100% de ocupação, o Min-Max vence nas 40 instâncias.

### 5.4 Por que o Min-Max decide melhor

O que segue é uma interpretação dos resultados, apoiada nas situações da Seção 2.4.

Quando um bloco r é colocado sobre blocos de número menor, ele será relocado de novo
exatamente uma vez, quando sair o primeiro deles. Esse é o min(i) da pilha no momento da
colocação. O número de blocos bloqueados não muda esse custo; só o momento da próxima
relocação depende da escolha.

- **Bloqueio inevitável:** o RI minimiza a contagem de blocos bloqueados, que é
  irrelevante para o custo imediato. O Min-Max adia ao máximo a próxima relocação de r.
  Quanto mais tarde ela acontece, menos blocos restam no pátio e mais espaço há para
  acomodar r sem novo bloqueio.
- **Várias pilhas livres de bloqueio:** todas têm RI = 0, e o RI desempata pela altura,
  sem olhar o min(i). Pode então "gastar" uma pilha valiosa, de min(i) grande. O Min-Max
  usa a pilha mais justa e preserva as valiosas.

Com o pátio cheio, as duas situações ficam frequentes, porque há poucas pilhas livres de
bloqueio. Por isso a diferença aumenta com a ocupação.

### 5.5 Comparação com o artigo

Caserta et al. (2012) compararam a mesma heurística Min-Max com a de Kim e Hong (2006), e
não com o RI. Usaram instâncias de 3×3 a 4×6 e um dia de solver por instância. O gap médio
foi 1,91% para o Min-Max e 13,5% para Kim e Hong. Aqui, nas instâncias com ótimo conhecido,
o Min-Max ficou com 2,5% e o RI com 13,0%. As instâncias e a heurística de comparação são
diferentes, então os números não são diretamente comparáveis. Mesmo assim, a ordem de
grandeza do gap do Min-Max é a mesma, e ele também é bem melhor que a regra concorrente.

O artigo não conseguiu provar o ótimo de nenhuma instância 4×7 ou maior em um dia de
processamento. Aqui também não houve prova nas classes maiores, com limites de 120 s a
600 s.

### 5.6 Limitações

- **Configurações do solver:** a solução de referência vem de duas rodadas com
  configurações diferentes (presolve ligado e desligado) e limites diferentes (120 s e
  600 s). Isso não afeta os valores nem os limites, que são válidos em qualquer caso. Os
  tempos do solver, porém, não são comparáveis entre si.
- **Limites de tempo curtos:** são muito menores que o do artigo. Com mais tempo de
  solver, mais instâncias do conjunto B teriam ótimo conhecido, e os intervalos de gap
  ficariam mais estreitos.
- **Limite inferior fraco nas instâncias grandes:** em `W7_H7_N43`, `W7_H8_N37` e
  `W7_H8_N49`, a qualidade absoluta das heurísticas fica pouco determinada.
- **Desempate do RI:** o resultado do RI depende do critério de desempate escolhido (pilha
  mais baixa, depois menor índice). Outro critério pode mudar os números do RI, embora não
  altere a regra principal.
- **Uma execução por instância:** como as heurísticas são determinísticas, não há
  variabilidade entre execuções a reportar.

### 5.7 Conclusão

Nas 170 instâncias, o Min-Max foi claramente superior ao RI. Fez menos relocações em 70%
delas, empatou nas demais com uma única exceção, e ficou em média a 2,5% do ótimo onde o
ótimo é conhecido, contra 13,0% do RI. A vantagem cresce com a ocupação do pátio, que é o
cenário mais difícil e mais próximo da operação real. Com tempos de microssegundos para as
duas, o Min-Max é a melhor escolha entre as duas como regra rápida de decisão. O RI serve
como referência de comparação.

Passos que melhorariam a análise:

- rodar o solver por mais tempo nas classes de 75% e 100% de ocupação;
- usar limites inferiores combinatórios mais fortes;
- testar a sensibilidade do RI ao critério de desempate.

---

## Referências

- Caserta, M., Schwarze, S., Voß, S. (2012). A mathematical formulation and complexity
  considerations for the blocks relocation problem. *European Journal of Operational
  Research*, 219(1), 96–104.
- Eskandari, H., Azari, E. (2015). Notes on mathematical formulation and complexity
  considerations for blocks relocation problem. *Scientia Iranica*, 22(6), 2722–2728.
- Kim, K. H., Hong, G. P. (2006). A heuristic rule for relocating blocks. *Computers and
  Operations Research*, 33, 940–954.
- Murty, K. G., Wan, Y.-W., Liu, J., Tseng, M. M., Leung, E., Lai, K.-K., Chiu, H. W. C.
  (2005). Hongkong International Terminals gains elastic capacity using a data-intensive
  decision-support system. *Interfaces*, 35(1), 61–75.
- Zehendner, E., Caserta, M., Feillet, D., Schwarze, S., Voß, S. (2015). An improved
  mathematical formulation for the blocks relocation problem. *European Journal of
  Operational Research*, 245(2), 415–422.

---

## Apêndice A: como reproduzir

```bash
cmake -S . -B build
cmake --build build -j

./build/min_max instances/              # grava results/min_max/
./build/ri      instances/              # grava results/ri/
./build/solver  instances/ --time-limit 120 --presolve on   # rodada 1
./build/solver  instances/<arquivo>.json --id <n> --time-limit 120   # rodada 2 (presolve off é o padrão no HiGHS)

./build/verificar_resultados instances results/min_max results/ri results/solver
```

---

## Apêndice B: resultados por instância

Colunas:

- **Solver:** a melhor solução encontrada pelo solver em qualquer das rodadas.
- **Status do solver:** "ótimo provado"; "limite de tempo", quando parou com solução mas
  sem prova; ou "sem solução no limite".
- **Melhor conhecido:** o menor valor entre solver, Min-Max e RI.
- **Ótimo conhecido:** "sim" quando o limite inferior coincide com o melhor conhecido.
- **t:** o tempo de CPU da heurística.

| Arquivo | id | Min-Max | RI | Solver | Status do solver | Limite inferior | Melhor conhecido | Ótimo conhecido | t Min-Max (µs) | t RI (µs) |
|---|---:|---:|---:|---:|---|---:|---:|:---:|---:|---:|
| brp_W3_H4_N8 | 1 | 7 | 7 | 7 | ótimo provado | 7 | 7 | sim | 8 | 5 |
| brp_W3_H4_N8 | 2 | 0 | 0 | 0 | ótimo provado | 0 | 0 | sim | 5 | 2 |
| brp_W3_H4_N8 | 3 | 3 | 4 | 3 | ótimo provado | 3 | 3 | sim | 4 | 4 |
| brp_W3_H4_N8 | 4 | 4 | 4 | 4 | ótimo provado | 4 | 4 | sim | 7 | 4 |
| brp_W3_H4_N8 | 5 | 4 | 4 | 4 | ótimo provado | 4 | 4 | sim | 5 | 3 |
| brp_W3_H4_N8 | 6 | 6 | 6 | 6 | ótimo provado | 6 | 6 | sim | 4 | 4 |
| brp_W3_H4_N8 | 7 | 3 | 3 | 3 | ótimo provado | 3 | 3 | sim | 5 | 3 |
| brp_W3_H4_N8 | 8 | 6 | 6 | 5 | ótimo provado | 5 | 5 | sim | 24 | 4 |
| brp_W3_H4_N8 | 9 | 2 | 2 | 2 | ótimo provado | 2 | 2 | sim | 5 | 3 |
| brp_W3_H4_N8 | 10 | 7 | 8 | 7 | ótimo provado | 7 | 7 | sim | 6 | 4 |
| brp_W4_H4_N10 | 1 | 8 | 9 | 8 | ótimo provado | 8 | 8 | sim | 5 | 5 |
| brp_W4_H4_N10 | 2 | 5 | 6 | 5 | ótimo provado | 5 | 5 | sim | 7 | 5 |
| brp_W4_H4_N10 | 3 | 1 | 1 | 1 | ótimo provado | 1 | 1 | sim | 5 | 6 |
| brp_W4_H4_N10 | 4 | 7 | 7 | 7 | ótimo provado | 7 | 7 | sim | 4 | 4 |
| brp_W4_H4_N10 | 5 | 2 | 2 | 2 | ótimo provado | 2 | 2 | sim | 3 | 1 |
| brp_W4_H4_N10 | 6 | 4 | 4 | 4 | ótimo provado | 4 | 4 | sim | 5 | 3 |
| brp_W4_H4_N10 | 7 | 6 | 6 | 6 | ótimo provado | 6 | 6 | sim | 10 | 10 |
| brp_W4_H4_N10 | 8 | 5 | 5 | 5 | ótimo provado | 5 | 5 | sim | 5 | 4 |
| brp_W4_H4_N10 | 9 | 3 | 3 | 3 | ótimo provado | 3 | 3 | sim | 48 | 3 |
| brp_W4_H4_N10 | 10 | 3 | 3 | 3 | ótimo provado | 3 | 3 | sim | 4 | 4 |
| brp_W4_H5_N12 | 1 | 6 | 6 | 6 | ótimo provado | 6 | 6 | sim | 7 | 4 |
| brp_W4_H5_N12 | 2 | 4 | 4 | 4 | ótimo provado | 4 | 4 | sim | 4 | 8 |
| brp_W4_H5_N12 | 3 | 8 | 8 | 8 | ótimo provado | 8 | 8 | sim | 6 | 12 |
| brp_W4_H5_N12 | 4 | 6 | 6 | 6 | ótimo provado | 6 | 6 | sim | 5 | 3 |
| brp_W4_H5_N12 | 5 | 6 | 6 | 6 | ótimo provado | 6 | 6 | sim | 5 | 7 |
| brp_W4_H5_N12 | 6 | 6 | 6 | 6 | ótimo provado | 6 | 6 | sim | 4 | 8 |
| brp_W4_H5_N12 | 7 | 10 | 10 | 8 | ótimo provado | 8 | 8 | sim | 6 | 4 |
| brp_W4_H5_N12 | 8 | 7 | 7 | 7 | ótimo provado | 7 | 7 | sim | 5 | 3 |
| brp_W4_H5_N12 | 9 | 11 | 11 | 10 | ótimo provado | 10 | 10 | sim | 11 | 7 |
| brp_W4_H5_N12 | 10 | 10 | 10 | 10 | ótimo provado | 10 | 10 | sim | 9 | 9 |
| brp_W5_H4_N15 | 1 | 13 | 15 | 11 | ótimo provado | 11 | 11 | sim | 8 | 5 |
| brp_W5_H4_N15 | 2 | 10 | 12 | 9 | ótimo provado | 9 | 9 | sim | 7 | 3 |
| brp_W5_H4_N15 | 3 | 7 | 10 | 7 | ótimo provado | 7 | 7 | sim | 6 | 3 |
| brp_W5_H4_N15 | 4 | 6 | 7 | 6 | ótimo provado | 6 | 6 | sim | 6 | 2 |
| brp_W5_H4_N15 | 5 | 8 | 8 | 7 | ótimo provado | 7 | 7 | sim | 6 | 4 |
| brp_W5_H4_N15 | 6 | 7 | 9 | 7 | ótimo provado | 7 | 7 | sim | 7 | 4 |
| brp_W5_H4_N15 | 7 | 7 | 7 | 7 | ótimo provado | 7 | 7 | sim | 5 | 2 |
| brp_W5_H4_N15 | 8 | 7 | 8 | 7 | ótimo provado | 7 | 7 | sim | 6 | 3 |
| brp_W5_H4_N15 | 9 | 4 | 4 | 4 | ótimo provado | 4 | 4 | sim | 6 | 2 |
| brp_W5_H4_N15 | 10 | 6 | 6 | 6 | ótimo provado | 6 | 6 | sim | 4 | 2 |
| brp_W5_H5_N18 | 1 | 12 | 14 | 10 | ótimo provado | 10 | 10 | sim | 7 | 11 |
| brp_W5_H5_N18 | 2 | 14 | 16 | 13 | ótimo provado | 13 | 13 | sim | 8 | 4 |
| brp_W5_H5_N18 | 3 | 10 | 12 | 10 | ótimo provado | 10 | 10 | sim | 6 | 3 |
| brp_W5_H5_N18 | 4 | 15 | 20 | 14 | ótimo provado | 14 | 14 | sim | 11 | 6 |
| brp_W5_H5_N18 | 5 | 11 | 11 | 11 | ótimo provado | 11 | 11 | sim | 6 | 3 |
| brp_W5_H5_N18 | 6 | 8 | 7 | 7 | ótimo provado | 7 | 7 | sim | 5 | 3 |
| brp_W5_H5_N18 | 7 | 8 | 9 | 8 | ótimo provado | 8 | 8 | sim | 4 | 3 |
| brp_W5_H5_N18 | 8 | 11 | 13 | 11 | ótimo provado | 11 | 11 | sim | 10 | 4 |
| brp_W5_H5_N18 | 9 | 9 | 11 | 9 | ótimo provado | 9 | 9 | sim | 6 | 2 |
| brp_W5_H5_N18 | 10 | 6 | 7 | 5 | ótimo provado | 5 | 5 | sim | 4 | 2 |
| brp_W7_H5_N16 | 1 | 6 | 6 | 6 | ótimo provado | 6 | 6 | sim | 7 | 2 |
| brp_W7_H5_N16 | 2 | 8 | 8 | 8 | ótimo provado | 8 | 8 | sim | 8 | 5 |
| brp_W7_H5_N16 | 3 | 8 | 8 | 8 | ótimo provado | 8 | 8 | sim | 13 | 3 |
| brp_W7_H5_N16 | 4 | 8 | 8 | 8 | ótimo provado | 8 | 8 | sim | 7 | 3 |
| brp_W7_H5_N16 | 5 | 8 | 8 | 8 | ótimo provado | 8 | 8 | sim | 7 | 3 |
| brp_W7_H5_N16 | 6 | 6 | 7 | 6 | ótimo provado | 6 | 6 | sim | 6 | 3 |
| brp_W7_H5_N16 | 7 | 8 | 8 | 8 | ótimo provado | 8 | 8 | sim | 5 | 4 |
| brp_W7_H5_N16 | 8 | 8 | 8 | 8 | ótimo provado | 8 | 8 | sim | 6 | 8 |
| brp_W7_H5_N16 | 9 | 6 | 6 | 6 | ótimo provado | 6 | 6 | sim | 4 | 2 |
| brp_W7_H5_N16 | 10 | 4 | 4 | 4 | ótimo provado | 4 | 4 | sim | 4 | 3 |
| brp_W7_H5_N23 | 1 | 13 | 17 | 13 | ótimo provado | 13 | 13 | sim | 8 | 9 |
| brp_W7_H5_N23 | 2 | 10 | 11 | 10 | ótimo provado | 10 | 10 | sim | 7 | 3 |
| brp_W7_H5_N23 | 3 | 15 | 19 | 14 | ótimo provado | 14 | 14 | sim | 8 | 5 |
| brp_W7_H5_N23 | 4 | 13 | 15 | 13 | ótimo provado | 13 | 13 | sim | 7 | 3 |
| brp_W7_H5_N23 | 5 | 10 | 11 | 10 | ótimo provado | 10 | 10 | sim | 6 | 9 |
| brp_W7_H5_N23 | 6 | 14 | 17 | 14 | ótimo provado | 14 | 14 | sim | 7 | 9 |
| brp_W7_H5_N23 | 7 | 13 | 13 | 13 | ótimo provado | 13 | 13 | sim | 8 | 3 |
| brp_W7_H5_N23 | 8 | 10 | 12 | 10 | ótimo provado | 10 | 10 | sim | 8 | 3 |
| brp_W7_H5_N23 | 9 | 10 | 10 | 10 | ótimo provado | 10 | 10 | sim | 8 | 4 |
| brp_W7_H5_N23 | 10 | 9 | 9 | 9 | ótimo provado | 9 | 9 | sim | 7 | 2 |
| brp_W7_H5_N31 | 1 | 23 | 25 | 20 | ótimo provado | 20 | 20 | sim | 9 | 5 |
| brp_W7_H5_N31 | 2 | 21 | 24 | 20 | ótimo provado | 20 | 20 | sim | 10 | 5 |
| brp_W7_H5_N31 | 3 | 21 | 25 | 18 | ótimo provado | 18 | 18 | sim | 9 | 5 |
| brp_W7_H5_N31 | 4 | 17 | 20 | 17 | ótimo provado | 17 | 17 | sim | 9 | 4 |
| brp_W7_H5_N31 | 5 | 28 | 32 | 25 | ótimo provado | 25 | 25 | sim | 10 | 7 |
| brp_W7_H5_N31 | 6 | 25 | 30 | 22 | ótimo provado | 22 | 22 | sim | 11 | 5 |
| brp_W7_H5_N31 | 7 | 28 | 29 | 24 | ótimo provado | 24 | 24 | sim | 10 | 5 |
| brp_W7_H5_N31 | 8 | 22 | 25 | 22 | ótimo provado | 22 | 22 | sim | 9 | 4 |
| brp_W7_H5_N31 | 9 | 32 | 37 | – | sem solução no limite | 28 | 32 | não | 18 | 7 |
| brp_W7_H5_N31 | 10 | 26 | 28 | 26 | ótimo provado | 26 | 26 | sim | 11 | 5 |
| brp_W7_H6_N18 | 1 | 7 | 8 | 7 | ótimo provado | 7 | 7 | sim | 6 | 4 |
| brp_W7_H6_N18 | 2 | 8 | 9 | 8 | ótimo provado | 8 | 8 | sim | 5 | 3 |
| brp_W7_H6_N18 | 3 | 8 | 8 | 8 | ótimo provado | 8 | 8 | sim | 5 | 3 |
| brp_W7_H6_N18 | 4 | 10 | 10 | 10 | ótimo provado | 10 | 10 | sim | 6 | 3 |
| brp_W7_H6_N18 | 5 | 10 | 12 | 10 | ótimo provado | 10 | 10 | sim | 5 | 5 |
| brp_W7_H6_N18 | 6 | 8 | 8 | 8 | ótimo provado | 8 | 8 | sim | 5 | 3 |
| brp_W7_H6_N18 | 7 | 7 | 7 | 7 | ótimo provado | 7 | 7 | sim | 5 | 3 |
| brp_W7_H6_N18 | 8 | 6 | 6 | 6 | ótimo provado | 6 | 6 | sim | 4 | 3 |
| brp_W7_H6_N18 | 9 | 8 | 9 | 8 | ótimo provado | 8 | 8 | sim | 5 | 2 |
| brp_W7_H6_N18 | 10 | 6 | 6 | 6 | ótimo provado | 6 | 6 | sim | 5 | 3 |
| brp_W7_H6_N28 | 1 | 20 | 24 | 20 | ótimo provado | 20 | 20 | sim | 8 | 4 |
| brp_W7_H6_N28 | 2 | 18 | 21 | 18 | ótimo provado | 18 | 18 | sim | 8 | 4 |
| brp_W7_H6_N28 | 3 | 15 | 17 | 15 | ótimo provado | 15 | 15 | sim | 6 | 4 |
| brp_W7_H6_N28 | 4 | 22 | 23 | 21 | limite de tempo | 20 | 21 | não | 7 | 4 |
| brp_W7_H6_N28 | 5 | 20 | 24 | 20 | ótimo provado | 20 | 20 | sim | 8 | 4 |
| brp_W7_H6_N28 | 6 | 17 | 19 | 16 | ótimo provado | 16 | 16 | sim | 7 | 3 |
| brp_W7_H6_N28 | 7 | 24 | 26 | 20 | ótimo provado | 20 | 20 | sim | 8 | 6 |
| brp_W7_H6_N28 | 8 | 15 | 17 | 15 | ótimo provado | 15 | 15 | sim | 7 | 3 |
| brp_W7_H6_N28 | 9 | 13 | 13 | 13 | ótimo provado | 13 | 13 | sim | 8 | 2 |
| brp_W7_H6_N28 | 10 | 17 | 20 | 16 | ótimo provado | 16 | 16 | sim | 8 | 4 |
| brp_W7_H6_N37 | 1 | 32 | 37 | 38 | limite de tempo | 27 | 32 | não | 10 | 6 |
| brp_W7_H6_N37 | 2 | 43 | 55 | – | sem solução no limite | 33 | 43 | não | 12 | 7 |
| brp_W7_H6_N37 | 3 | 34 | 39 | 42 | limite de tempo | 27 | 34 | não | 11 | 7 |
| brp_W7_H6_N37 | 4 | 30 | 31 | – | sem solução no limite | 30 | 30 | sim | 10 | 6 |
| brp_W7_H6_N37 | 5 | 35 | 37 | 39 | limite de tempo | 34 | 35 | não | 12 | 6 |
| brp_W7_H6_N37 | 6 | 31 | 37 | – | sem solução no limite | 26 | 31 | não | 10 | 7 |
| brp_W7_H6_N37 | 7 | 21 | 23 | 20 | ótimo provado | 20 | 20 | sim | 12 | 5 |
| brp_W7_H6_N37 | 8 | 31 | 36 | 29 | ótimo provado | 29 | 29 | sim | 9 | 5 |
| brp_W7_H6_N37 | 9 | 32 | 34 | 31 | limite de tempo | 28 | 31 | não | 10 | 6 |
| brp_W7_H6_N37 | 10 | 25 | 27 | 25 | ótimo provado | 25 | 25 | sim | 8 | 4 |
| brp_W7_H7_N22 | 1 | 7 | 8 | 7 | ótimo provado | 7 | 7 | sim | 6 | 2 |
| brp_W7_H7_N22 | 2 | 10 | 13 | 10 | ótimo provado | 10 | 10 | sim | 6 | 2 |
| brp_W7_H7_N22 | 3 | 14 | 20 | 14 | ótimo provado | 14 | 14 | sim | 8 | 3 |
| brp_W7_H7_N22 | 4 | 12 | 14 | 12 | ótimo provado | 12 | 12 | sim | 6 | 3 |
| brp_W7_H7_N22 | 5 | 11 | 12 | 11 | ótimo provado | 11 | 11 | sim | 6 | 3 |
| brp_W7_H7_N22 | 6 | 13 | 15 | 13 | ótimo provado | 13 | 13 | sim | 5 | 3 |
| brp_W7_H7_N22 | 7 | 7 | 8 | 7 | ótimo provado | 7 | 7 | sim | 4 | 3 |
| brp_W7_H7_N22 | 8 | 11 | 12 | 10 | ótimo provado | 10 | 10 | sim | 5 | 4 |
| brp_W7_H7_N22 | 9 | 11 | 12 | 11 | ótimo provado | 11 | 11 | sim | 5 | 3 |
| brp_W7_H7_N22 | 10 | 10 | 12 | 10 | ótimo provado | 10 | 10 | sim | 5 | 4 |
| brp_W7_H7_N32 | 1 | 21 | 22 | 22 | limite de tempo | 21 | 21 | sim | 7 | 4 |
| brp_W7_H7_N32 | 2 | 23 | 29 | – | sem solução no limite | 21 | 23 | não | 8 | 5 |
| brp_W7_H7_N32 | 3 | 20 | 26 | 20 | ótimo provado | 20 | 20 | sim | 7 | 6 |
| brp_W7_H7_N32 | 4 | 18 | 23 | 18 | ótimo provado | 18 | 18 | sim | 7 | 5 |
| brp_W7_H7_N32 | 5 | 16 | 18 | 18 | limite de tempo | 16 | 16 | sim | 7 | 4 |
| brp_W7_H7_N32 | 6 | 26 | 29 | 25 | limite de tempo | 20 | 25 | não | 8 | 5 |
| brp_W7_H7_N32 | 7 | 21 | 25 | 22 | limite de tempo | 18 | 21 | não | 8 | 5 |
| brp_W7_H7_N32 | 8 | 21 | 29 | 21 | limite de tempo | 20 | 21 | não | 7 | 5 |
| brp_W7_H7_N32 | 9 | 24 | 32 | 24 | limite de tempo | 20 | 24 | não | 7 | 5 |
| brp_W7_H7_N32 | 10 | 32 | 46 | – | sem solução no limite | 25 | 32 | não | 9 | 9 |
| brp_W7_H7_N43 | 1 | 37 | 41 | – | sem solução no limite | 35 | 37 | não | 10 | 7 |
| brp_W7_H7_N43 | 2 | 36 | 43 | – | sem solução no limite | 33 | 36 | não | 11 | 8 |
| brp_W7_H7_N43 | 3 | 41 | 52 | – | sem solução no limite | 30 | 41 | não | 14 | 9 |
| brp_W7_H7_N43 | 4 | 43 | 46 | – | sem solução no limite | 35 | 43 | não | 12 | 7 |
| brp_W7_H7_N43 | 5 | 37 | 45 | – | sem solução no limite | 33 | 37 | não | 9 | 7 |
| brp_W7_H7_N43 | 6 | 38 | 45 | – | sem solução no limite | 36 | 38 | não | 16 | 8 |
| brp_W7_H7_N43 | 7 | 39 | 51 | – | sem solução no limite | 35 | 39 | não | 10 | 7 |
| brp_W7_H7_N43 | 8 | 48 | 67 | – | sem solução no limite | 27 | 48 | não | 12 | 9 |
| brp_W7_H7_N43 | 9 | 48 | 56 | – | sem solução no limite | 29 | 48 | não | 11 | 8 |
| brp_W7_H7_N43 | 10 | 43 | 47 | – | sem solução no limite | 35 | 43 | não | 11 | 6 |
| brp_W7_H8_N24 | 1 | 13 | 17 | 13 | ótimo provado | 13 | 13 | sim | 6 | 4 |
| brp_W7_H8_N24 | 2 | 14 | 16 | 14 | ótimo provado | 14 | 14 | sim | 6 | 4 |
| brp_W7_H8_N24 | 3 | 11 | 13 | 11 | ótimo provado | 11 | 11 | sim | 5 | 3 |
| brp_W7_H8_N24 | 4 | 11 | 12 | 11 | ótimo provado | 11 | 11 | sim | 6 | 5 |
| brp_W7_H8_N24 | 5 | 14 | 17 | 14 | ótimo provado | 14 | 14 | sim | 6 | 4 |
| brp_W7_H8_N24 | 6 | 16 | 19 | 16 | ótimo provado | 16 | 16 | sim | 6 | 4 |
| brp_W7_H8_N24 | 7 | 15 | 17 | 15 | ótimo provado | 15 | 15 | sim | 6 | 4 |
| brp_W7_H8_N24 | 8 | 17 | 21 | 17 | limite de tempo | 14 | 17 | não | 5 | 4 |
| brp_W7_H8_N24 | 9 | 14 | 17 | 14 | ótimo provado | 14 | 14 | sim | 6 | 4 |
| brp_W7_H8_N24 | 10 | 14 | 16 | 13 | ótimo provado | 13 | 13 | sim | 9 | 3 |
| brp_W7_H8_N37 | 1 | 39 | 43 | – | sem solução no limite | 23 | 39 | não | 10 | 6 |
| brp_W7_H8_N37 | 2 | 27 | 37 | – | sem solução no limite | 22 | 27 | não | 8 | 7 |
| brp_W7_H8_N37 | 3 | 44 | 57 | – | sem solução no limite | 26 | 44 | não | 11 | 8 |
| brp_W7_H8_N37 | 4 | 24 | 28 | – | sem solução no limite | 23 | 24 | não | 8 | 5 |
| brp_W7_H8_N37 | 5 | 24 | 38 | 27 | limite de tempo | 23 | 24 | não | 7 | 6 |
| brp_W7_H8_N37 | 6 | 34 | 40 | – | sem solução no limite | 23 | 34 | não | 8 | 7 |
| brp_W7_H8_N37 | 7 | 30 | 42 | – | sem solução no limite | 27 | 30 | não | 10 | 6 |
| brp_W7_H8_N37 | 8 | 38 | 40 | – | sem solução no limite | 28 | 38 | não | 9 | 5 |
| brp_W7_H8_N37 | 9 | 22 | 29 | – | sem solução no limite | 22 | 22 | sim | 7 | 6 |
| brp_W7_H8_N37 | 10 | 31 | 35 | – | sem solução no limite | 25 | 31 | não | 8 | 5 |
| brp_W7_H8_N49 | 1 | 63 | 78 | – | sem solução no limite | 35 | 63 | não | 12 | 9 |
| brp_W7_H8_N49 | 2 | 66 | 76 | – | sem solução no limite | 33 | 66 | não | 14 | 10 |
| brp_W7_H8_N49 | 3 | 55 | 57 | – | sem solução no limite | 31 | 55 | não | 12 | 7 |
| brp_W7_H8_N49 | 4 | 51 | 63 | – | sem solução no limite | 33 | 51 | não | 10 | 8 |
| brp_W7_H8_N49 | 5 | 49 | 55 | – | sem solução no limite | 30 | 49 | não | 11 | 7 |
| brp_W7_H8_N49 | 6 | 57 | 85 | – | sem solução no limite | 36 | 57 | não | 12 | 13 |
| brp_W7_H8_N49 | 7 | 42 | 53 | – | sem solução no limite | 27 | 42 | não | 10 | 9 |
| brp_W7_H8_N49 | 8 | 43 | 56 | – | sem solução no limite | 30 | 43 | não | 10 | 9 |
| brp_W7_H8_N49 | 9 | 56 | 66 | – | sem solução no limite | 29 | 56 | não | 11 | 8 |
| brp_W7_H8_N49 | 10 | 58 | 71 | – | sem solução no limite | 29 | 58 | não | 13 | 9 |
