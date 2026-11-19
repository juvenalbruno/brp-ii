# BRP-II — Blocks Relocation Problem restrito (suposição A1)

Quatro métodos em C++17 para o BRP restrito, sem chegada de blocos:

| Executável | Método | Arquivo |
|---|---|---|
| `solver` | exato, formulação BRP-II corrigida, resolvida com Gurobi, CPLEX ou HiGHS | `src/solver.cpp` + `src/brp2_model.hpp` + `src/backends.hpp` |
| `astar` | exato, busca informada A* (slides de IC0024, módulo 2) | `src/astar.cpp` |
| `min_max` | heurística Min-Max (Caserta et al.) | `src/min_max.cpp` + `src/regras.hpp` |
| `ri` | heurística Reshuffle Index (Murty et al.) | `src/ri.cpp` |

Problema: baia com `W` pilhas de altura máxima `H` e `N` blocos com prioridades
distintas `1..N`. Os blocos saem na ordem `1, 2, ..., N`; só o topo de cada pilha é
acessível; pela suposição A1 só podem ser relocados os blocos que estão acima do
bloco-alvo atual. Minimiza-se o número de relocações.

## Estrutura

```
CMakeLists.txt
src/
  brp2_model.hpp   toda a formulação BRP-II corrigida (único lugar com restrições)
  instance.hpp     JSON, instância, resultado, verificador de sequências, laço das heurísticas
  backends.hpp     ligação da formulação com Gurobi, CPLEX e HiGHS (sem restrições)
  solver.cpp       main do exato por PLIM
  astar.cpp        main do exato por busca A*
  regras.hpp       regra Min-Max (usada pela heurística e como limite superior do A*)
  min_max.cpp      main da heurística Min-Max
  ri.cpp           main da heurística RI
instances/         instâncias (JSON)
results/{solver,astar,min_max,ri}/   um JSON por instância
hv/hv.py           hipervolume e gaps
tests/             busca em largura, exportação LP, verificador e validar.py
```

## Compilação

Requisitos: CMake >= 3.16 e um compilador C++17. As heurísticas, o A* e os testes
não dependem de nenhum solver. O alvo `solver` é compilado com **todos os solvers de PLIM
que o CMake encontrar**:

| Solver | Como o CMake encontra | Licença |
|---|---|---|
| HiGHS | `find_package(highs)`; no macOS: `brew install highs` | aberto, sem licença |
| CPLEX | `CPLEX_HOME` (variável de ambiente ou `-DCPLEX_HOME=...`) | acadêmica completa (a Community Edition limita a 1 000 variáveis) |
| Gurobi | `GUROBI_HOME` (variável de ambiente ou `-DGUROBI_HOME=...`) | acadêmica |

```sh
brew install highs                                   # HiGHS
export CPLEX_HOME=/Applications/CPLEX_Studio2211/cplex   # opcional
export GUROBI_HOME=/Library/gurobi1300/macos_universal2  # opcional
cmake -S . -B build
cmake --build build -j
```

- `CPLEX_HOME` é a pasta `cplex` do CPLEX Optimization Studio (a que contém
  `include/ilcplex/cplex.h` e `lib/<arquitetura>/static_pic/libcplex.a`); a raiz do
  Studio também é aceita.
- `GUROBI_HOME` deve conter `include/gurobi_c++.h` e `lib/`.
- Se o HiGHS estiver fora do caminho padrão, use `-DCMAKE_PREFIX_PATH=/caminho`.

O CMake informa quais solvers entraram (`Alvo solver com: ...`). Se nenhum for
encontrado, avisa e pula o alvo `solver`. Os executáveis ficam em `build/`.

No Windows, com o g++ do MSYS2 (MinGW-w64) e o CMake/Ninja que vêm com o Visual
Studio Build Tools (os executáveis ficam como `build/<nome>.exe`, e a pasta
`C:\msys64\mingw64\bin` precisa estar no `PATH` para achar as DLLs do g++):

```sh
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=g++
cmake --build build -j
```

## Uso

```sh
./build/<exe> <instância.json | pasta> [--time-limit s] [--out pasta] [--id n]
```

- Passando uma pasta, todos os `.json` dela são processados.
- `--id n`: processa só a instância de `id` n de cada arquivo.
- `--out`: pasta de saída. Padrão: `results/<método>/` na raiz do projeto.
- `--time-limit`: limite em segundos por instância para o `solver` e o `astar`
  (padrão 3600). As heurísticas aceitam a opção por uniformidade, mas não a usam.
- Só no `astar`:
  - `--sem-ub`: desliga a poda pelo limite superior da Min-Max (A* puro);
  - `--max-nos n`: número máximo de estados distintos guardados (padrão 5 000 000,
    cerca de 1 GB). Ao atingir esse limite ou o de tempo, o A* para com
    `status = node_limit` ou `time_limit`.
- Só no `solver`:
  - `--solver gurobi|cplex|highs`: escolhe o solver de PLIM. Sem a opção, usa o
    primeiro disponível nessa ordem. `./build/solver --listar-solvers` mostra os
    que foram compilados;
  - `--presolve on|off|padrao`: liga, desliga ou deixa na configuração padrão o
    pré-processamento do solver de PLIM. Sem a opção, o HiGHS roda com o presolve
    **desligado** e Gurobi/CPLEX com a configuração padrão deles (ver abaixo);
  - `--threads n` e `--verbose` (log do solver de PLIM);
  - `--restricao6-original`: usa a restrição (6) do artigo original, só para
    demonstrar que ela é inviável; não use para resolver.

```sh
./build/min_max instances/
./build/ri      instances/
./build/astar   instances/ --time-limit 600
./build/solver  instances/ --time-limit 600 --solver highs
```

## Formato de instância

As instâncias de `instances/` (descritas em `instances/LEIAME.md`) trazem **10
instâncias por arquivo**, todas com a mesma configuração. O nome do arquivo é
`brp_W{W}_H{H}_N{N}.json`:

```json
{
  "metadata": { "W": 3, "H": 4, "N": 8, "total_instancias": 10, "semente": 2374 },
  "instancias": [
    { "id": 1, "stacks": [[8,7,6],[2],[1,3,4,5]] }
  ]
}
```

- `metadata.W`, `metadata.H`, `metadata.N` valem para todas as instâncias do arquivo.
- `stacks`: exatamente `W` listas, cada pilha **da base para o topo**; `stacks[0]` é
  a pilha 1 e `[]` é uma pilha vazia.
- Cada instância é identificada pela chave `<arquivo>_id<id com 2 dígitos>`, por
  exemplo `brp_W3_H4_N8_id01`. Essa chave é o nome do arquivo de resultado.

Passar um arquivo a um executável processa as 10 instâncias dele; passar a pasta
processa os 17 arquivos (170 instâncias).

Validação na leitura (`lerInstancias()` em `src/instance.hpp`): `stacks` com
exatamente `W` pilhas, nenhuma pilha acima de `H`, prioridades exatamente `1..N` sem
repetição, número de blocos igual a `metadata.N`, `id` sem repetição e
`total_instancias` igual ao tamanho da lista. Um arquivo inválido gera erro e não é
resolvido.

O leitor também aceita o esquema de instância única usado nos testes
(`tests/referencia.json`); nesse caso a chave é o nome do arquivo:

```json
{ "name": "ex", "W": 3, "H": 4, "stacks": [[8,7,6],[2],[1,3,4,5]] }
```

## Formato de resultado

Um JSON por instância em `results/<método>/<chave>.json`, por exemplo
`results/min_max/brp_W3_H4_N8_id01.json`:

```json
{
  "instancia": "brp_W3_H4_N8_id01",
  "arquivo": "brp_W3_H4_N8.json",
  "id": 1,
  "metodo": "min_max",
  "W": 3, "H": 4, "N": 8,
  "relocacoes": 7,
  "num_movimentos": 7,
  "retiradas": 8,
  "operacoes_totais": 15,
  "tempo_cpu_s": 0.000003000,
  "tempo_parede_s": 0.000004084,
  "sequencia": [[3, 1], [3, 2], [3, 1]],
  "movimentos": [
    {"bloco": 7, "origem": 3, "destino": 1, "periodo": 1}
  ]
}
```

- `arquivo` e `id`: identificação pedida no `LEIAME.md` para reportar resultados
  (`id` é `null` em arquivos de instância única).
- `relocacoes`: valor do objetivo. `num_movimentos`: número de movimentos feitos,
  igual a `relocacoes` (as retiradas não contam como movimento). `retiradas`: sempre
  `N`. `operacoes_totais`: relocações + retiradas.
- `sequencia`: pares `[origem, destino]` na ordem de execução, com pilhas a partir de
  1 e retiradas implícitas, no formato recomendado pelo `LEIAME.md`.
- `movimentos`: os mesmos movimentos com o bloco relocado e o `periodo` (o
  bloco-alvo no momento da relocação).
- `tempo_cpu_s`: tempo de CPU do processo (soma das threads; no `solver` inclui a
  montagem do modelo). `tempo_parede_s`: tempo de relógio.
- Só no `solver`:
  - `solver`: solver de PLIM usado, com a versão (ex.: `"highs 1.16.0"`);
  - `presolve`: configuração de presolve usada (`on`, `off` ou `padrao`);
  - `status`: `optimal`, `time_limit`, `infeasible` ou `other`;
  - `otimo_provado`: `true` só com `status = optimal`;
  - `limite_inferior`: melhor limite inferior (arredondado para cima, pois o
    objetivo é inteiro);
  - `gap_mip`: gap relativo, calculado da mesma forma para todos os solvers:
    `(relocacoes - limite_inferior) / relocacoes`.

  Com `status = time_limit`, `relocacoes` é só a melhor solução encontrada (ou
  `null` se não houver nenhuma) e **não** é tratada como ótimo, nem aqui nem no HV.
- Só no `astar`:
  - `status`, `otimo_provado` e `limite_inferior`, com o mesmo significado do
    `solver` (`status` pode ser `optimal`, `time_limit`, `node_limit` ou
    `infeasible`), e `gap` (mesma fórmula do `gap_mip`). Sem ótimo provado,
    `relocacoes` é a solução da Min-Max (o UB) e `limite_inferior` é o menor `f` da
    fila;
  - `usa_ub` e `limite_superior_inicial` (relocações da Min-Max, ou `null` com
    `--sem-ub`);
  - `h_raiz`: `h` do estado inicial;
  - `nos_gerados` (inclui a raiz), `nos_expandidos` (inclui o nó objetivo),
    `podados_repetidos`, `podados_ub` e `nos_atualizados` (estados reencontrados por
    um caminho mais barato antes de serem expandidos).

Antes de gravar, todo executável confere que a própria sequência é executável. Para
reconferir os arquivos já gravados:

```sh
./build/verificar_resultados instances results/min_max results/ri results/solver results/astar
```

## Formulação (`src/brp2_model.hpp`)

O cabeçalho do arquivo lista as variáveis `b`, `x`, `y`, as restrições (1) a (8), as
restrições de ligação (L1) a (L6) e os cortes de fixação (F1) a (F7), e cada bloco
de código é comentado com o número da restrição.

Correção da restrição (6): no artigo original o lado esquerdo é `1 - Σ_n x`, o que
limita a uma relocação por par de pilhas por período e torna inviáveis instâncias
viáveis. Aqui usa-se `H * (1 - Σ_n x)` (restrição 6c), conforme Eskandari & Azari
(2015) e Zehendner et al. (2015).

Variáveis impossíveis não são criadas (equivale a fixá-las em 0). Tamanho do modelo
na instância de id 1 de alguns arquivos:

| Arquivo | Variáveis | Sem pré-processamento | Restrições | Não-zeros |
|---|---|---|---|---|
| `brp_W3_H4_N8` | 809 | 10 752 | 748 | 6 521 |
| `brp_W5_H5_N18` | 14 854 | 218 700 | 6 043 | 173 410 |
| `brp_W7_H5_N31` | 86 824 | 1 244 495 | 22 684 | 1 212 477 |
| `brp_W7_H8_N49` | 1 140 693 | 7 798 448 | 150 315 | 24 692 743 |

O modelo cresce rápido: nas instâncias maiores do conjunto B é provável que o
`solver` pare no limite de tempo sem provar otimalidade (`status = time_limit`).

Presolve do HiGHS: com o presolve ligado (padrão do HiGHS), nas instâncias maiores
do conjunto B o HiGHS gastava o limite de tempo inteiro no pré-processamento, sem
resolver nem a relaxação linear (0 iterações de LP, 0 nós) e, portanto, sem
solução. Exemplo, `brp_W7_H8_N24` id 5 com limite de 120 s: presolve ligado, sem
solução após 132 s, todos no presolve; presolve desligado, ótimo provado (14) em
19 s, com um único nó. Por isso o `solver` desliga o presolve do HiGHS por padrão.

A formulação é montada numa estrutura neutra (variáveis e linhas), e
`src/backends.hpp` apenas a copia para o solver escolhido. Por isso Gurobi, CPLEX e
HiGHS resolvem exatamente o mesmo modelo, e `tests/modelo_lp` pode exportá-lo em
formato LP.

## Heurísticas

Laço comum (`heuristicaConstrutiva()` em `instance.hpp`): para `t = 1..N`, enquanto
o bloco `t` não está no topo, reloca o bloco `r` do topo da pilha dele para uma
pilha não cheia e diferente da origem; depois retira `t`. Sem retrocesso.

- **Min-Max**: `min(i)` = menor prioridade da pilha `i` (vazia vale `N+1`). Se
  existe pilha com `min(i) > r`, escolhe a de menor `min(i)`; senão, a de maior
  `min(i)`. Empate: menor índice.
- **RI**: `RI(i)` = número de blocos da pilha `i` com prioridade menor que `r`.
  Escolhe o menor RI. Empate: menor altura e depois menor índice. O desempate está
  isolado na função `desempateRI()` de `src/ri.cpp`.

## A* (`src/astar.cpp`)

Implementação do A* descrito nos slides do módulo 2 de IC0024 (aula "Algoritmo A*"),
adaptada ao BRP restrito (A1) deste repositório.

- **Estados**: cada nó é uma "fotografia do pátio". Como em `tests/bfs.cpp`, sob A1 a
  retirada do alvo é forçada assim que ele chega ao topo, então ela é feita na hora,
  com custo 0, e o estado guardado já está sem os blocos retirados. Cada aresta é
  uma relocação (custo 1): o bloco do topo da pilha do alvo vai para outra pilha.
- **f(n) = g(n) + h(n)**: `g` = relocações feitas; `h` = número de blocos "mal
  posicionados" (bloqueadores), isto é, blocos acima de algum bloco de número menor
  na mesma pilha. Cada um terá de sair pelo menos uma vez, logo `h` é admissível.
  Também é consistente (uma relocação muda `h` em no máximo 1), por isso um estado
  expandido nunca precisa ser reaberto.
- **Fila de prioridade** por `f`; empate em `f`: maior `g` (critério do slide 24);
  persistindo o empate, ordem de inserção. O teste de objetivo é feito ao retirar o
  nó da fila, o que garante a otimalidade.
- **Podas** (slide 21): (1) estados repetidos: um estado já alcançado com `g` menor
  ou igual é descartado; se for alcançado com `g` menor antes de ser expandido, o nó
  é atualizado; (2) infactibilidade: não há movimento para pilha cheia (`H`);
  (3) limite superior: a Min-Max roda antes da busca e todo nó com `f >= UB` é
  descartado (o exercício proposto no slide 35). Se a fila esvaziar sem achar
  solução melhor, a solução da Min-Max é ótima. `--sem-ub` desliga essa poda.

Conferência com o exercício do slide 24 (`tests/referencia.json`, pilhas
`876 | 2 | 1345`, `H = 4`): com `--sem-ub`, o A* encontra 6 relocações, gera 17 nós
e expande 14, exatamente como no slide e pelo mesmo caminho (`5→P1`, `4→P2`,
`3→P2`, `3→P3`, `4→P3`, `4→P2`). Com o UB da Min-Max (que também dá 6), expande 12.
O `validar.py` confere esses números.

Resultados em `results/astar/` (`./build/astar instances --time-limit 600`, limite
padrão de 5 000 000 de nós, Ryzen 5 5600G). Em todas as instâncias sem ótimo, o A*
parou pelo limite de nós em 10 a 15 s, nunca pelo de tempo. "Ótimos novos" são
instâncias em que o A* provou o ótimo e o `solver` não:

| Arquivo | Ótimos A* | Ótimos `solver` | Ótimos novos | CPU total do A* (s) |
|---|---|---|---|---|
| W3–W5 e `W7_H5_N16` (6 arquivos) | 60/60 | 60/60 | 0 | 0,0 |
| `brp_W7_H5_N23` | 10/10 | 10/10 | 0 | 0,1 |
| `brp_W7_H5_N31` | 10/10 | 9/10 | 1 | 0,2 |
| `brp_W7_H6_N18` | 10/10 | 10/10 | 0 | 0,0 |
| `brp_W7_H6_N28` | 10/10 | 9/10 | 1 | 2,4 |
| `brp_W7_H6_N37` | 9/10 | 3/10 | 6 | 20,6 |
| `brp_W7_H7_N22` | 10/10 | 10/10 | 0 | 0,0 |
| `brp_W7_H7_N32` | 10/10 | 2/10 | 8 | 9,0 |
| `brp_W7_H7_N43` | 2/10 | 0/10 | 2 | 97,1 |
| `brp_W7_H8_N24` | 10/10 | 9/10 | 1 | 0,0 |
| `brp_W7_H8_N37` | 6/10 | 0/10 | 6 | 48,9 |
| `brp_W7_H8_N49` | 3/10 | 0/10 | 3 | 98,6 |
| **Total** | **150/170** | **122/170** | **28** | **276,9** |

Nas 122 instâncias em que os dois provaram o ótimo, os valores são iguais. Nas
demais, o ótimo do A* nunca fica abaixo do limite inferior do `solver` nem acima
da solução dele. Com os 150 ótimos, o gap médio de relocações é de 3,9% para a
Min-Max e de 16,9% para a RI (`hv/output/resumo.txt`, linha GERAL).

## Hipervolume (`hv/hv.py`)

Python 3, só biblioteca padrão.

```sh
python3 hv/hv.py                      # lê results/, grava hv/output/
python3 hv/hv.py --modo hv_max        # força o modo (auto | hv_max | hv_min)
python3 hv/hv.py --metodos min_max,ri # restringe os métodos comparados
python3 hv/hv.py --results outra/pasta --output outra/saida
```

Objetivos (minimização): `f1 = relocacoes`, `f2 = tempo_cpu_s`. A lista `OBJETIVOS`
no topo do script é configurável; para usar só relocações, deixe só `"relocacoes"`.

Por instância, cada método é um ponto. Cada objetivo é normalizado por
`f' = (f - ideal) / (ref - ideal)` e `HV = Π (1 - f')`.

- **hv_max** (não exige o exato): `ref` = 1,1 × pior valor observado entre os
  métodos disponíveis; `ideal` = melhor valor observado.
- **hv_min** (exige exato com `status = optimal`): o ideal em relocações é o ótimo
  do exato; a referência segue a regra do hv_max.

Métodos exatos (`METODOS_EXATOS` no topo do script): `solver` e `astar`, nessa
ordem de preferência. O ótimo de uma instância vem do primeiro deles com
`status = optimal` (coluna `fonte_otimo`), e é contra o HV desse método que se
calcula o `gap_hv`. Se os dois provarem ótimos diferentes, o script para com erro.
- **auto** (padrão): hv_min nas instâncias com exato ótimo, hv_max nas demais.

Com exato ótimo, calcula também `gap_hv = (HV_exato - HV_heur) / HV_exato` e
`gap_relocacoes = (heur - ótimo) / ótimo` (se o ótimo é 0, o gap é 0 quando a
heurística também faz 0 e fica vazio caso contrário).

Saídas em `hv/output/`: `instancias/<chave>.csv`, `hv_por_instancia.csv` (com as
colunas `arquivo` e `id`), `resumo_por_classe.csv` (médias por classe `W×H` e geral),
`resumo_por_arquivo.csv` (médias por arquivo, isto é, por configuração `W, H, N`) e
`resumo.txt`.

Observações:

- Quando o exato ótimo está entre os pontos, ele já é o melhor valor observado em
  relocações; nesse caso hv_min e hv_max dão o mesmo número. Os modos só diferem
  quando o exato é excluído dos pontos com `--metodos min_max,ri`.
- Com o tempo de CPU como objetivo, o exato costuma ser o pior em `f2` e fica com
  HV perto de 0,09; as heurísticas ficam com HV maior e o `gap_hv` sai negativo.
  Isso é consequência da definição, não um erro do script.
- Se todos os métodos empatam em 0 num objetivo, a normalização desse objetivo
  vale 0.

## Validação (`tests/`)

```sh
python3 tests/validar.py              # opções: --n 40 --seed 2026 --time-limit 300
```

O script:

1. gera a instância de referência e 40 instâncias aleatórias de até 4×4, e inclui os
   arquivos reais que cabem em 4×4 (`brp_W3_H4_N8.json` e `brp_W4_H4_N10.json`),
   num total de 61 instâncias;
2. calcula o ótimo restrito por busca em largura (`tests/bfs.cpp`) e confere que a
   referência `[[8,7,6],[2],[1,3,4,5]]` dá 6;
3. roda `min_max` e `ri`, relê os resultados gravados e confere que toda sequência é
   executável (respeita `H`, só move topos, só move blocos acima do alvo) e que
   nenhuma fica abaixo do ótimo;
4. roda o `astar` com e sem o UB da Min-Max (`--sem-ub`), confere as sequências e
   exige que o valor seja **igual** ao da busca em largura, com `status = optimal`,
   em todas as instâncias; na referência, exige também os 17 nós gerados e 14
   expandidos do slide 24 (A* sem UB);
5. roda o `solver` com cada solver de PLIM compilado (Gurobi, CPLEX, HiGHS), confere
   as sequências e compara com a busca em largura, encerrando com erro se algum
   valor divergir;
6. confere que o modelo com a restrição (6) original é inviável na referência.

O resumo final diz com quais solvers a validação foi feita e quais adaptadores
ficaram sem teste. Sem o executável `solver`, os passos 5 e 6 não são feitos e o
script diz isso; nesse caso, se o pacote opcional `highspy` estiver instalado, a
formulação exportada em LP é conferida com ele.

Ferramentas individuais:

```sh
./build/bfs <instância.json | pasta> [--out pasta]
./build/verificar_resultados <arquivo ou pasta de instâncias> <pasta_de_resultados> [...]
./build/modelo_lp exportar <instância.json> <saída.lp> [--id n] [--restricao6-original]
```

### Estado da validação neste repositório

Na máquina em que o projeto foi desenvolvido há apenas o HiGHS (o Gurobi está sem
licença e o CPLEX não está instalado). Portanto:

- busca em largura, heurísticas e verificador: executados e aprovados;
- `astar` (com e sem UB): igual à busca em largura nas 61 instâncias do
  `validar.py` e em mais 663 aleatórias (`--n 200` com as sementes 1, 2 e 3);
  referência = 6, com os 17 nós gerados e 14 expandidos do slide 24. Testado no
  Windows (g++ 15 do MSYS2), onde não há solver de PLIM, então os passos 5 e 6 não
  rodaram lá;
- `solver` com **HiGHS**: igual à busca em largura nas 61 instâncias do
  `validar.py` (40 aleatórias, 20 reais de até 4×4 e a referência); referência = 6;
  versão original da (6) inviável na referência;
- adaptadores do **CPLEX** e do **Gurobi** em `src/backends.hpp`: escritos, mas
  **não compilados contra as bibliotecas reais nem executados**. A formulação é a
  mesma; o que falta testar neles é só a ligação. Depois de instalar um deles,
  recompile e rode `python3 tests/validar.py`, que testa todos os solvers
  compilados.

## Referências

- Caserta, M., Schwarze, S., Voß, S. (2012). A mathematical formulation and
  complexity considerations for the blocks relocation problem. *European Journal of
  Operational Research*, 219(1), 96–104.
- Zehendner, E., Caserta, M., Feillet, D., Schwarze, S., Voß, S. (2015). An improved
  mathematical formulation for the blocks relocation problem. *European Journal of
  Operational Research*, 245(2), 415–422.
- Eskandari, H., Azari, E. (2015). Notes on mathematical formulation and complexity
  considerations for blocks relocation problem. *Scientia Iranica*, 22(6),
  2722–2728.
- Murty, K. G., Wan, Y.-W., Liu, J., Tseng, M. M., Leung, E., Lai, K.-K., Chiu,
  H. W. C. (2005). Hongkong International Terminals gains elastic capacity using a
  data-intensive decision-support system. *Interfaces*, 35(1), 61–75.
