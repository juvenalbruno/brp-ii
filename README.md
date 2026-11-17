# BRP-II — Blocks Relocation Problem restrito (suposição A1)

Três métodos em C++17 para o BRP restrito, sem chegada de blocos:

| Executável | Método | Arquivo |
|---|---|---|
| `solver` | exato, formulação BRP-II corrigida, Gurobi | `src/solver.cpp` + `src/brp2_model.hpp` |
| `min_max` | heurística Min-Max (Caserta et al.) | `src/min_max.cpp` |
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
  solver.cpp       main do exato (Gurobi)
  min_max.cpp      main da heurística Min-Max
  ri.cpp           main da heurística RI
instances/         instâncias (JSON)
results/{solver,min_max,ri}/   um JSON por instância
hv/hv.py           hipervolume e gaps
tests/             busca em largura, exportação LP, verificador e validar.py
```

## Compilação

Requisitos: CMake >= 3.16 e um compilador C++17. O Gurobi é necessário **apenas**
para o alvo `solver`.

```sh
export GUROBI_HOME=/caminho/do/gurobi    # ex.: /Library/gurobi1103/macos_universal2
cmake -S . -B build                      # ou: cmake -S . -B build -DGUROBI_HOME=...
cmake --build build -j
```

`GUROBI_HOME` deve conter `include/gurobi_c++.h` e `lib/` (com `libgurobi_c++` e
`libgurobiNNN`). Se não estiver definido ou estiver incompleto, o CMake avisa e pula
o alvo `solver`; `min_max`, `ri` e os testes compilam normalmente. Os executáveis
ficam em `build/`.

## Uso

```sh
./build/<exe> <instância.json | pasta> [--time-limit s] [--out pasta]
```

- Passando uma pasta, todos os `.json` dela são processados.
- `--out`: pasta de saída. Padrão: `results/<método>/` na raiz do projeto.
- `--time-limit`: limite em segundos por instância para o `solver` (padrão 3600). As
  heurísticas aceitam a opção por uniformidade, mas não a usam.
- Só no `solver`: `--threads n`, `--verbose` (log do Gurobi) e
  `--restricao6-original` (usa a restrição (6) do artigo original, só para
  demonstrar que ela é inviável; não use para resolver).

```sh
./build/min_max instances/
./build/ri      instances/
./build/solver  instances/ --time-limit 600
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
  - `status`: `optimal`, `time_limit`, `infeasible` ou `other`;
  - `otimo_provado`: `true` só com `status = optimal`;
  - `limite_inferior`: melhor limite inferior (arredondado para cima, pois o
    objetivo é inteiro);
  - `gap_mip`: gap relativo do MIP.

  Com `status = time_limit`, `relocacoes` é só a melhor solução encontrada (ou
  `null` se não houver nenhuma) e **não** é tratada como ótimo, nem aqui nem no HV.

Antes de gravar, todo executável confere que a própria sequência é executável. Para
reconferir os arquivos já gravados:

```sh
./build/verificar_resultados instances results/min_max results/ri results/solver
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

A formulação é montada numa estrutura neutra e copiada para o Gurobi por
`carregarNoGurobi()`. Isso permite exportar exatamente as mesmas restrições em
formato LP (`tests/modelo_lp`) e conferi-las com outro solver.

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
4. roda o `solver`, confere as sequências e compara com a busca em largura,
   encerrando com erro se algum valor divergir;
5. confere que o modelo com a restrição (6) original é inviável na referência.

Sem o executável `solver` (Gurobi não encontrado), os passos 4 e 5 não são feitos
com o Gurobi e o script diz isso no resumo. Nesse caso, se o pacote opcional
`highspy` estiver instalado (`pip install highspy`), a formulação é exportada em LP
e resolvida com o HiGHS, com as mesmas comparações. Isso valida as restrições de
`brp2_model.hpp`, mas não `solver.cpp` nem `carregarNoGurobi()`.

Ferramentas individuais:

```sh
./build/bfs <instância.json | pasta> [--out pasta]
./build/verificar_resultados <arquivo ou pasta de instâncias> <pasta_de_resultados> [...]
./build/modelo_lp exportar <instância.json> <saída.lp> [--id n] [--restricao6-original]
```

### Estado da validação neste repositório

Na máquina em que o projeto foi criado **o Gurobi não estava instalado** (havia só
um arquivo de licença). Portanto:

- busca em largura, heurísticas e verificador: executados e aprovados;
- formulação de `brp2_model.hpp`: conferida com HiGHS contra a busca em largura em
  361 instâncias (340 aleatórias, 20 reais de até 4×4 e a referência), sem
  divergência; referência = 6; versão original da (6) inviável na referência;
- heurísticas nas 170 instâncias de `instances/`: 340 sequências gravadas e
  reconferidas pelo verificador;
- `solver.cpp` e `carregarNoGurobi()`: **não compilados contra o Gurobi nem
  executados**. Rode `python3 tests/validar.py` depois de compilar com o Gurobi.

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
