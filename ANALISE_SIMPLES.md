# Min-Max ou RI: qual heurística reloca menos?

O objetivo do BRP é retirar todos os blocos com o **menor número de relocações**. Por isso, a heurística melhor é a que reloca menos. As duas heurísticas rodaram nas mesmas 170 instâncias (17 arquivos com 10 instâncias cada). Os valores abaixo são o número de relocações de cada solução.

## Resultado geral

| | Min-Max | RI |
|---|---:|---:|
| Total de relocações | **3072** | 3605 |
| Média por instância | **18,1** | 21,2 |
| Instâncias em que relocou menos | **119** | 1 |
| Empates | 50 | 50 |

O RI fez 533 relocações a mais que o Min-Max, ou seja, 17,4% a mais.

## 1. Resultados por arquivo

Cada linha resume as 10 instâncias de um arquivo. As colunas "Min-Max vence", "Empate" e "RI vence" contam as instâncias.

| Arquivo | W | H | N | Média Min-Max | Média RI | Diferença média (RI − Min-Max) | Total Min-Max | Total RI | Min-Max vence | Empate | RI vence |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `brp_W3_H4_N8.json` | 3 | 4 | 8 | 4,2 | 4,4 | 0,2 | 42 | 44 | 2 | 8 | 0 |
| `brp_W4_H4_N10.json` | 4 | 4 | 10 | 4,4 | 4,6 | 0,2 | 44 | 46 | 2 | 8 | 0 |
| `brp_W4_H5_N12.json` | 4 | 5 | 12 | 7,4 | 7,4 | 0,0 | 74 | 74 | 0 | 10 | 0 |
| `brp_W5_H4_N15.json` | 5 | 4 | 15 | 7,5 | 8,6 | 1,1 | 75 | 86 | 6 | 4 | 0 |
| `brp_W5_H5_N18.json` | 5 | 5 | 18 | 10,4 | 12,0 | 1,6 | 104 | 120 | 8 | 1 | 1 |
| `brp_W7_H5_N16.json` | 7 | 5 | 16 | 7,0 | 7,1 | 0,1 | 70 | 71 | 1 | 9 | 0 |
| `brp_W7_H5_N23.json` | 7 | 5 | 23 | 11,7 | 13,4 | 1,7 | 117 | 134 | 7 | 3 | 0 |
| `brp_W7_H5_N31.json` | 7 | 5 | 31 | 24,3 | 27,5 | 3,2 | 243 | 275 | 10 | 0 | 0 |
| `brp_W7_H6_N18.json` | 7 | 6 | 18 | 7,8 | 8,3 | 0,5 | 78 | 83 | 4 | 6 | 0 |
| `brp_W7_H6_N28.json` | 7 | 6 | 28 | 18,1 | 20,4 | 2,3 | 181 | 204 | 9 | 1 | 0 |
| `brp_W7_H6_N37.json` | 7 | 6 | 37 | 31,4 | 35,6 | 4,2 | 314 | 356 | 10 | 0 | 0 |
| `brp_W7_H7_N22.json` | 7 | 7 | 22 | 10,6 | 12,6 | 2,0 | 106 | 126 | 10 | 0 | 0 |
| `brp_W7_H7_N32.json` | 7 | 7 | 32 | 22,2 | 27,9 | 5,7 | 222 | 279 | 10 | 0 | 0 |
| `brp_W7_H7_N43.json` | 7 | 7 | 43 | 41,0 | 49,3 | 8,3 | 410 | 493 | 10 | 0 | 0 |
| `brp_W7_H8_N24.json` | 7 | 8 | 24 | 13,9 | 16,5 | 2,6 | 139 | 165 | 10 | 0 | 0 |
| `brp_W7_H8_N37.json` | 7 | 8 | 37 | 31,3 | 38,9 | 7,6 | 313 | 389 | 10 | 0 | 0 |
| `brp_W7_H8_N49.json` | 7 | 8 | 49 | 54,0 | 66,0 | 12,0 | 540 | 660 | 10 | 0 | 0 |
| **Total** | | | | **18,1** | **21,2** | **3,1** | **3072** | **3605** | **119** | **50** | **1** |

## 2. Resultados por instância

Em cada tabela, "Diferença" é RI − Min-Max: um valor positivo indica que o RI relocou mais.

### `brp_W3_H4_N8.json` (W = 3, H = 4, N = 8)

| Instância (id) | Min-Max | RI | Diferença | Melhor |
|---:|---:|---:|---:|---|
| 1 | 7 | 7 | 0 | empate |
| 2 | 0 | 0 | 0 | empate |
| 3 | 3 | 4 | +1 | Min-Max |
| 4 | 4 | 4 | 0 | empate |
| 5 | 4 | 4 | 0 | empate |
| 6 | 6 | 6 | 0 | empate |
| 7 | 3 | 3 | 0 | empate |
| 8 | 6 | 6 | 0 | empate |
| 9 | 2 | 2 | 0 | empate |
| 10 | 7 | 8 | +1 | Min-Max |
| **Total** | **42** | **44** | **+2** | |

### `brp_W4_H4_N10.json` (W = 4, H = 4, N = 10)

| Instância (id) | Min-Max | RI | Diferença | Melhor |
|---:|---:|---:|---:|---|
| 1 | 8 | 9 | +1 | Min-Max |
| 2 | 5 | 6 | +1 | Min-Max |
| 3 | 1 | 1 | 0 | empate |
| 4 | 7 | 7 | 0 | empate |
| 5 | 2 | 2 | 0 | empate |
| 6 | 4 | 4 | 0 | empate |
| 7 | 6 | 6 | 0 | empate |
| 8 | 5 | 5 | 0 | empate |
| 9 | 3 | 3 | 0 | empate |
| 10 | 3 | 3 | 0 | empate |
| **Total** | **44** | **46** | **+2** | |

### `brp_W4_H5_N12.json` (W = 4, H = 5, N = 12)

| Instância (id) | Min-Max | RI | Diferença | Melhor |
|---:|---:|---:|---:|---|
| 1 | 6 | 6 | 0 | empate |
| 2 | 4 | 4 | 0 | empate |
| 3 | 8 | 8 | 0 | empate |
| 4 | 6 | 6 | 0 | empate |
| 5 | 6 | 6 | 0 | empate |
| 6 | 6 | 6 | 0 | empate |
| 7 | 10 | 10 | 0 | empate |
| 8 | 7 | 7 | 0 | empate |
| 9 | 11 | 11 | 0 | empate |
| 10 | 10 | 10 | 0 | empate |
| **Total** | **74** | **74** | **0** | |

### `brp_W5_H4_N15.json` (W = 5, H = 4, N = 15)

| Instância (id) | Min-Max | RI | Diferença | Melhor |
|---:|---:|---:|---:|---|
| 1 | 13 | 15 | +2 | Min-Max |
| 2 | 10 | 12 | +2 | Min-Max |
| 3 | 7 | 10 | +3 | Min-Max |
| 4 | 6 | 7 | +1 | Min-Max |
| 5 | 8 | 8 | 0 | empate |
| 6 | 7 | 9 | +2 | Min-Max |
| 7 | 7 | 7 | 0 | empate |
| 8 | 7 | 8 | +1 | Min-Max |
| 9 | 4 | 4 | 0 | empate |
| 10 | 6 | 6 | 0 | empate |
| **Total** | **75** | **86** | **+11** | |

### `brp_W5_H5_N18.json` (W = 5, H = 5, N = 18)

| Instância (id) | Min-Max | RI | Diferença | Melhor |
|---:|---:|---:|---:|---|
| 1 | 12 | 14 | +2 | Min-Max |
| 2 | 14 | 16 | +2 | Min-Max |
| 3 | 10 | 12 | +2 | Min-Max |
| 4 | 15 | 20 | +5 | Min-Max |
| 5 | 11 | 11 | 0 | empate |
| 6 | 8 | 7 | -1 | RI |
| 7 | 8 | 9 | +1 | Min-Max |
| 8 | 11 | 13 | +2 | Min-Max |
| 9 | 9 | 11 | +2 | Min-Max |
| 10 | 6 | 7 | +1 | Min-Max |
| **Total** | **104** | **120** | **+16** | |

### `brp_W7_H5_N16.json` (W = 7, H = 5, N = 16)

| Instância (id) | Min-Max | RI | Diferença | Melhor |
|---:|---:|---:|---:|---|
| 1 | 6 | 6 | 0 | empate |
| 2 | 8 | 8 | 0 | empate |
| 3 | 8 | 8 | 0 | empate |
| 4 | 8 | 8 | 0 | empate |
| 5 | 8 | 8 | 0 | empate |
| 6 | 6 | 7 | +1 | Min-Max |
| 7 | 8 | 8 | 0 | empate |
| 8 | 8 | 8 | 0 | empate |
| 9 | 6 | 6 | 0 | empate |
| 10 | 4 | 4 | 0 | empate |
| **Total** | **70** | **71** | **+1** | |

### `brp_W7_H5_N23.json` (W = 7, H = 5, N = 23)

| Instância (id) | Min-Max | RI | Diferença | Melhor |
|---:|---:|---:|---:|---|
| 1 | 13 | 17 | +4 | Min-Max |
| 2 | 10 | 11 | +1 | Min-Max |
| 3 | 15 | 19 | +4 | Min-Max |
| 4 | 13 | 15 | +2 | Min-Max |
| 5 | 10 | 11 | +1 | Min-Max |
| 6 | 14 | 17 | +3 | Min-Max |
| 7 | 13 | 13 | 0 | empate |
| 8 | 10 | 12 | +2 | Min-Max |
| 9 | 10 | 10 | 0 | empate |
| 10 | 9 | 9 | 0 | empate |
| **Total** | **117** | **134** | **+17** | |

### `brp_W7_H5_N31.json` (W = 7, H = 5, N = 31)

| Instância (id) | Min-Max | RI | Diferença | Melhor |
|---:|---:|---:|---:|---|
| 1 | 23 | 25 | +2 | Min-Max |
| 2 | 21 | 24 | +3 | Min-Max |
| 3 | 21 | 25 | +4 | Min-Max |
| 4 | 17 | 20 | +3 | Min-Max |
| 5 | 28 | 32 | +4 | Min-Max |
| 6 | 25 | 30 | +5 | Min-Max |
| 7 | 28 | 29 | +1 | Min-Max |
| 8 | 22 | 25 | +3 | Min-Max |
| 9 | 32 | 37 | +5 | Min-Max |
| 10 | 26 | 28 | +2 | Min-Max |
| **Total** | **243** | **275** | **+32** | |

### `brp_W7_H6_N18.json` (W = 7, H = 6, N = 18)

| Instância (id) | Min-Max | RI | Diferença | Melhor |
|---:|---:|---:|---:|---|
| 1 | 7 | 8 | +1 | Min-Max |
| 2 | 8 | 9 | +1 | Min-Max |
| 3 | 8 | 8 | 0 | empate |
| 4 | 10 | 10 | 0 | empate |
| 5 | 10 | 12 | +2 | Min-Max |
| 6 | 8 | 8 | 0 | empate |
| 7 | 7 | 7 | 0 | empate |
| 8 | 6 | 6 | 0 | empate |
| 9 | 8 | 9 | +1 | Min-Max |
| 10 | 6 | 6 | 0 | empate |
| **Total** | **78** | **83** | **+5** | |

### `brp_W7_H6_N28.json` (W = 7, H = 6, N = 28)

| Instância (id) | Min-Max | RI | Diferença | Melhor |
|---:|---:|---:|---:|---|
| 1 | 20 | 24 | +4 | Min-Max |
| 2 | 18 | 21 | +3 | Min-Max |
| 3 | 15 | 17 | +2 | Min-Max |
| 4 | 22 | 23 | +1 | Min-Max |
| 5 | 20 | 24 | +4 | Min-Max |
| 6 | 17 | 19 | +2 | Min-Max |
| 7 | 24 | 26 | +2 | Min-Max |
| 8 | 15 | 17 | +2 | Min-Max |
| 9 | 13 | 13 | 0 | empate |
| 10 | 17 | 20 | +3 | Min-Max |
| **Total** | **181** | **204** | **+23** | |

### `brp_W7_H6_N37.json` (W = 7, H = 6, N = 37)

| Instância (id) | Min-Max | RI | Diferença | Melhor |
|---:|---:|---:|---:|---|
| 1 | 32 | 37 | +5 | Min-Max |
| 2 | 43 | 55 | +12 | Min-Max |
| 3 | 34 | 39 | +5 | Min-Max |
| 4 | 30 | 31 | +1 | Min-Max |
| 5 | 35 | 37 | +2 | Min-Max |
| 6 | 31 | 37 | +6 | Min-Max |
| 7 | 21 | 23 | +2 | Min-Max |
| 8 | 31 | 36 | +5 | Min-Max |
| 9 | 32 | 34 | +2 | Min-Max |
| 10 | 25 | 27 | +2 | Min-Max |
| **Total** | **314** | **356** | **+42** | |

### `brp_W7_H7_N22.json` (W = 7, H = 7, N = 22)

| Instância (id) | Min-Max | RI | Diferença | Melhor |
|---:|---:|---:|---:|---|
| 1 | 7 | 8 | +1 | Min-Max |
| 2 | 10 | 13 | +3 | Min-Max |
| 3 | 14 | 20 | +6 | Min-Max |
| 4 | 12 | 14 | +2 | Min-Max |
| 5 | 11 | 12 | +1 | Min-Max |
| 6 | 13 | 15 | +2 | Min-Max |
| 7 | 7 | 8 | +1 | Min-Max |
| 8 | 11 | 12 | +1 | Min-Max |
| 9 | 11 | 12 | +1 | Min-Max |
| 10 | 10 | 12 | +2 | Min-Max |
| **Total** | **106** | **126** | **+20** | |

### `brp_W7_H7_N32.json` (W = 7, H = 7, N = 32)

| Instância (id) | Min-Max | RI | Diferença | Melhor |
|---:|---:|---:|---:|---|
| 1 | 21 | 22 | +1 | Min-Max |
| 2 | 23 | 29 | +6 | Min-Max |
| 3 | 20 | 26 | +6 | Min-Max |
| 4 | 18 | 23 | +5 | Min-Max |
| 5 | 16 | 18 | +2 | Min-Max |
| 6 | 26 | 29 | +3 | Min-Max |
| 7 | 21 | 25 | +4 | Min-Max |
| 8 | 21 | 29 | +8 | Min-Max |
| 9 | 24 | 32 | +8 | Min-Max |
| 10 | 32 | 46 | +14 | Min-Max |
| **Total** | **222** | **279** | **+57** | |

### `brp_W7_H7_N43.json` (W = 7, H = 7, N = 43)

| Instância (id) | Min-Max | RI | Diferença | Melhor |
|---:|---:|---:|---:|---|
| 1 | 37 | 41 | +4 | Min-Max |
| 2 | 36 | 43 | +7 | Min-Max |
| 3 | 41 | 52 | +11 | Min-Max |
| 4 | 43 | 46 | +3 | Min-Max |
| 5 | 37 | 45 | +8 | Min-Max |
| 6 | 38 | 45 | +7 | Min-Max |
| 7 | 39 | 51 | +12 | Min-Max |
| 8 | 48 | 67 | +19 | Min-Max |
| 9 | 48 | 56 | +8 | Min-Max |
| 10 | 43 | 47 | +4 | Min-Max |
| **Total** | **410** | **493** | **+83** | |

### `brp_W7_H8_N24.json` (W = 7, H = 8, N = 24)

| Instância (id) | Min-Max | RI | Diferença | Melhor |
|---:|---:|---:|---:|---|
| 1 | 13 | 17 | +4 | Min-Max |
| 2 | 14 | 16 | +2 | Min-Max |
| 3 | 11 | 13 | +2 | Min-Max |
| 4 | 11 | 12 | +1 | Min-Max |
| 5 | 14 | 17 | +3 | Min-Max |
| 6 | 16 | 19 | +3 | Min-Max |
| 7 | 15 | 17 | +2 | Min-Max |
| 8 | 17 | 21 | +4 | Min-Max |
| 9 | 14 | 17 | +3 | Min-Max |
| 10 | 14 | 16 | +2 | Min-Max |
| **Total** | **139** | **165** | **+26** | |

### `brp_W7_H8_N37.json` (W = 7, H = 8, N = 37)

| Instância (id) | Min-Max | RI | Diferença | Melhor |
|---:|---:|---:|---:|---|
| 1 | 39 | 43 | +4 | Min-Max |
| 2 | 27 | 37 | +10 | Min-Max |
| 3 | 44 | 57 | +13 | Min-Max |
| 4 | 24 | 28 | +4 | Min-Max |
| 5 | 24 | 38 | +14 | Min-Max |
| 6 | 34 | 40 | +6 | Min-Max |
| 7 | 30 | 42 | +12 | Min-Max |
| 8 | 38 | 40 | +2 | Min-Max |
| 9 | 22 | 29 | +7 | Min-Max |
| 10 | 31 | 35 | +4 | Min-Max |
| **Total** | **313** | **389** | **+76** | |

### `brp_W7_H8_N49.json` (W = 7, H = 8, N = 49)

| Instância (id) | Min-Max | RI | Diferença | Melhor |
|---:|---:|---:|---:|---|
| 1 | 63 | 78 | +15 | Min-Max |
| 2 | 66 | 76 | +10 | Min-Max |
| 3 | 55 | 57 | +2 | Min-Max |
| 4 | 51 | 63 | +12 | Min-Max |
| 5 | 49 | 55 | +6 | Min-Max |
| 6 | 57 | 85 | +28 | Min-Max |
| 7 | 42 | 53 | +11 | Min-Max |
| 8 | 43 | 56 | +13 | Min-Max |
| 9 | 56 | 66 | +10 | Min-Max |
| 10 | 58 | 71 | +13 | Min-Max |
| **Total** | **540** | **660** | **+120** | |

## 3. Conclusão

O Min-Max relocou menos em 119 das 170 instâncias, empatou em 50 e perdeu em 1. Teve média menor ou igual à do RI em todos os 17 arquivos. No total, o RI fez 17,4% mais relocações.

A diferença é pequena nos arquivos com pátio folgado (por exemplo, `brp_W7_H5_N16` e `brp_W4_H5_N12`), em que as duas regras costumam escolher a mesma pilha. Ela cresce com a lotação: nos arquivos com mais blocos para a mesma baia (`brp_W7_H7_N43` e `brp_W7_H8_N49`), o RI faz de 8 a 12 relocações a mais por instância, em média.

Como o objetivo do BRP é minimizar o número de relocações, **o Min-Max é a melhor das duas heurísticas** nas instâncias testadas.
