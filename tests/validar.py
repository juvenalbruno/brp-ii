#!/usr/bin/env python3
"""Validação obrigatória do projeto BRP-II.

Passos:
  1. gera instâncias aleatórias pequenas (até 4x4) + a instância de referência, e
     acrescenta os arquivos reais de instances/ que cabem em 4x4 (INSTANCIAS_REAIS);
  2. calcula o ótimo restrito (A1) de cada uma por busca em largura (tests/bfs.cpp);
  3. roda as heurísticas, confere que as sequências são executáveis e que nenhuma
     fica abaixo do ótimo;
  4. roda o executável `solver` com CADA solver de PLIM compilado nele (gurobi, cplex,
     highs), confere as sequências e compara com a busca em largura; qualquer
     divergência encerra com erro;
  5. confere que o modelo com a restrição (6) ORIGINAL é inviável na referência.

O resumo diz com quais solvers a validação foi feita e quais adaptadores ficaram sem
teste. Se o executável `solver` não existir (nenhum solver encontrado), os passos 4 e
5 não são feitos e isso é dito claramente; nesse caso, se o pacote opcional `highspy`
estiver instalado, a formulação exportada em LP por tests/modelo_lp é conferida com
ele.

Uso: python3 tests/validar.py [--build build] [--n 40] [--seed 2026] [--time-limit 300]
"""
import argparse
import json
import random
import shutil
import subprocess
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
SAIDA = RAIZ / "tests" / "output"
OTIMO_REFERENCIA = 6
# Arquivos de instances/ pequenos o bastante (até 4x4) para entrar na comparação.
INSTANCIAS_REAIS = ["brp_W3_H4_N8.json", "brp_W4_H4_N10.json"]


def titulo(texto):
    print("\n" + "=" * 78 + "\n" + texto + "\n" + "=" * 78, flush=True)


def falhar(msg):
    print("\nFALHA NA VALIDAÇÃO: " + msg, flush=True)
    sys.exit(1)


def rodar(cmd, mostrar=True):
    """Roda um comando, mostra a saída e devolve o código de retorno."""
    p = subprocess.run([str(c) for c in cmd], capture_output=True, text=True)
    if mostrar:
        sys.stdout.write(p.stdout)
    sys.stdout.write(p.stderr)
    sys.stdout.flush()
    return p.returncode


def gerar_instancias(pasta, n, semente):
    """Instâncias aleatórias com W, H em 2..4 e N <= W*H-(H-1) (sempre viáveis)."""
    rng = random.Random(semente)
    pasta.mkdir(parents=True)
    shutil.copy(RAIZ / "tests" / "referencia.json", pasta / "referencia.json")
    for nome in INSTANCIAS_REAIS:
        if (RAIZ / "instances" / nome).exists():
            shutil.copy(RAIZ / "instances" / nome, pasta / nome)
    for idx in range(1, n + 1):
        w, h = rng.randint(2, 4), rng.randint(2, 4)
        n_max = w * h - (h - 1)
        nb = rng.randint(max(2, n_max // 2), n_max)
        prioridades = list(range(1, nb + 1))
        rng.shuffle(prioridades)
        pilhas = [[] for _ in range(w)]
        for p in prioridades:
            rng.choice([s for s in pilhas if len(s) < h]).append(p)
        nome = "aleat_%02d_%dx%d_n%d" % (idx, w, h, nb)
        with open(pasta / (nome + ".json"), "w") as f:
            json.dump({"name": nome, "W": w, "H": h, "stacks": pilhas}, f)
            f.write("\n")


def ler_resultados(pasta):
    return {a.stem: json.loads(a.read_text()) for a in sorted(pasta.glob("*.json"))}


def comparar_com_bfs(nome_metodo, resultados, bfs, exato):
    """exato=True: valor tem de ser igual ao da busca; False: tem de ser >=."""
    divergencias = 0
    for inst, ref in bfs.items():
        r = resultados.get(inst)
        if r is None:
            print("  %-28s %s não gravou resultado" % (inst, nome_metodo))
            divergencias += 1
            continue
        valor = r["relocacoes"]
        if exato:
            ok = valor == ref["relocacoes"] and r.get("status", "optimal") == "optimal"
        else:
            ok = valor is not None and valor >= ref["relocacoes"]
        if not ok:
            print("  DIVERGÊNCIA %-28s bfs = %s  %s = %s (status %s)"
                  % (inst, ref["relocacoes"], nome_metodo, valor, r.get("status", "-")))
            divergencias += 1
    return divergencias


def validar_com_highs(exe, pasta_inst, bfs, limite):
    """Confere a formulação exportada em LP com o HiGHS. Devolve nº de divergências."""
    import highspy

    def resolver(lp):
        h = highspy.Highs()
        h.setOptionValue("output_flag", False)
        h.setOptionValue("time_limit", float(limite))
        h.readModel(str(lp))
        h.run()
        status = h.modelStatusToString(h.getModelStatus())
        return h, status

    pasta_lp = SAIDA / "lp"
    pasta_res = SAIDA / "highs"
    pasta_lp.mkdir(parents=True)
    pasta_res.mkdir(parents=True)
    divergencias = 0
    for inst in sorted(bfs):
        # Cada resultado diz de que arquivo e de que id veio a instância.
        arq = pasta_inst / bfs[inst]["arquivo"]
        qual = ["--id", bfs[inst]["id"]] if bfs[inst]["id"] else []
        lp = pasta_lp / (inst + ".lp")
        if rodar([exe["modelo_lp"], "exportar", arq, lp] + qual, mostrar=False) != 0:
            falhar("modelo_lp não conseguiu exportar " + inst)
        h, status = resolver(lp)
        if status != "Optimal":
            print("  DIVERGÊNCIA %-28s HiGHS terminou com status %s" % (inst, status))
            divergencias += 1
            continue
        valor = round(h.getInfo().objective_function_value)
        nomes = h.getLp().col_names_
        valores = h.getSolution().col_value
        sol = pasta_lp / (inst + ".sol")
        with open(sol, "w") as f:
            for nome, v in zip(nomes, valores):
                if v > 0.5:
                    f.write("%s 1\n" % nome)
        if rodar([exe["modelo_lp"], "importar", arq, sol, pasta_res / (inst + ".json"), "--metodo", "highs"]
                 + qual) != 0:
            print("  DIVERGÊNCIA %-28s solução do modelo não é executável" % inst)
            divergencias += 1
            continue
        movs = len(json.loads((pasta_res / (inst + ".json")).read_text())["movimentos"])
        ok = valor == bfs[inst]["relocacoes"] == movs
        print("  %-28s bfs = %2d  modelo(HiGHS) = %2d  movimentos = %2d  %s"
              % (inst, bfs[inst]["relocacoes"], valor, movs, "ok" if ok else "DIVERGÊNCIA"), flush=True)
        divergencias += 0 if ok else 1

    # Restrição (6) original na instância de referência: tem de ser inviável.
    lp = pasta_lp / "referencia_restricao6_original.lp"
    rodar([exe["modelo_lp"], "exportar", pasta_inst / "referencia.json", lp, "--restricao6-original"], mostrar=False)
    _, status = resolver(lp)
    print("  referência com a (6) ORIGINAL (sem o fator H): status HiGHS = %s" % status)
    if status != "Infeasible":
        print("  DIVERGÊNCIA: esperava Infeasible")
        divergencias += 1
    return divergencias


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--build", default=str(RAIZ / "build"), help="pasta com os executáveis")
    ap.add_argument("--n", type=int, default=40, help="número de instâncias aleatórias (mínimo 30)")
    ap.add_argument("--seed", type=int, default=2026)
    ap.add_argument("--time-limit", type=float, default=300.0, help="limite por instância no método exato")
    args = ap.parse_args()
    if args.n < 30:
        falhar("use pelo menos 30 instâncias aleatórias")

    build = Path(args.build)
    exe = {n: build / n for n in ("bfs", "min_max", "ri", "solver", "modelo_lp", "verificar_resultados")}
    for n in ("bfs", "min_max", "ri", "modelo_lp", "verificar_resultados"):
        if not exe[n].exists():
            falhar("executável %s não encontrado; compile antes (cmake --build build)" % exe[n])

    if SAIDA.exists():
        shutil.rmtree(SAIDA)
    pasta_inst = SAIDA / "instancias"

    titulo("1. Instâncias: referência + %d aleatórias de até 4x4 (semente %d) + reais pequenas"
           % (args.n, args.seed))
    gerar_instancias(pasta_inst, args.n, args.seed)
    print("gravadas em", pasta_inst)
    print("arquivos reais incluídos:", ", ".join(n for n in INSTANCIAS_REAIS if (pasta_inst / n).exists()) or "nenhum")

    titulo("2. Busca em largura (ótimo restrito, A1)")
    if rodar([exe["bfs"], pasta_inst, "--out", SAIDA / "bfs"]) != 0:
        falhar("a busca em largura falhou")
    bfs = ler_resultados(SAIDA / "bfs")
    if any(r["relocacoes"] is None for r in bfs.values()):
        falhar("a busca em largura encontrou instância inviável")
    if bfs["referencia"]["relocacoes"] != OTIMO_REFERENCIA:
        falhar("ótimo da referência pela busca = %s, esperado %d" % (bfs["referencia"]["relocacoes"], OTIMO_REFERENCIA))
    print("referência: ótimo restrito = %d (esperado %d) ok" % (bfs["referencia"]["relocacoes"], OTIMO_REFERENCIA))

    titulo("3. Heurísticas Min-Max e RI")
    for metodo in ("min_max", "ri"):
        if rodar([exe[metodo], pasta_inst, "--out", SAIDA / metodo], mostrar=False) != 0:
            falhar("a heurística %s falhou" % metodo)
    if rodar([exe["verificar_resultados"], pasta_inst, SAIDA / "bfs", SAIDA / "min_max", SAIDA / "ri"]) != 0:
        falhar("há sequência não executável (bfs/heurísticas)")
    print("%-28s %4s %8s %4s" % ("instância", "bfs", "min_max", "ri"))
    heur = {m: ler_resultados(SAIDA / m) for m in ("min_max", "ri")}
    for inst in bfs:
        print("%-28s %4d %8d %4d" % (inst, bfs[inst]["relocacoes"], heur["min_max"][inst]["relocacoes"],
                                     heur["ri"][inst]["relocacoes"]))
    for metodo in ("min_max", "ri"):
        if comparar_com_bfs(metodo, heur[metodo], bfs, exato=False):
            falhar("heurística %s abaixo do ótimo: a busca ou a heurística está errada" % metodo)
        iguais = sum(1 for i in bfs if heur[metodo][i]["relocacoes"] == bfs[i]["relocacoes"])
        print("%s: nenhuma abaixo do ótimo; atinge o ótimo em %d de %d" % (metodo, iguais, len(bfs)))

    # Solvers de PLIM compilados no executável `solver` (gurobi, cplex, highs).
    solvers = []
    if exe["solver"].exists():
        p = subprocess.run([str(exe["solver"]), "--listar-solvers"], capture_output=True, text=True)
        solvers = p.stdout.split()

    validados = []
    highs_lp_validado = False
    for nome in solvers:
        titulo("4. Solver (%s) x busca em largura" % nome)
        saida = SAIDA / ("solver_" + nome)
        if rodar([exe["solver"], pasta_inst, "--solver", nome, "--out", saida,
                  "--time-limit", args.time_limit]) != 0:
            falhar("o solver (%s) falhou em alguma instância" % nome)
        if rodar([exe["verificar_resultados"], pasta_inst, saida]) != 0:
            falhar("há sequência do solver (%s) não executável" % nome)
        res = ler_resultados(saida)
        if comparar_com_bfs("solver/" + nome, res, bfs, exato=True):
            falhar("solver (%s) e busca em largura divergem" % nome)
        print("solver (%s) = busca em largura nas %d instâncias (referência = %d)"
              % (res["referencia"]["solver"], len(bfs), res["referencia"]["relocacoes"]))

        titulo("5. Restrição (6) original (sem o fator H) na referência, solver %s" % nome)
        rodar([exe["solver"], pasta_inst / "referencia.json", "--solver", nome,
               "--out", SAIDA / ("solver_%s_restricao6_original" % nome), "--restricao6-original"])
        orig = ler_resultados(SAIDA / ("solver_%s_restricao6_original" % nome))["referencia"]
        if orig["status"] != "infeasible":
            falhar("esperava status infeasible com a (6) original, veio %s" % orig["status"])
        print("modelo original inviável na referência, como esperado")
        validados.append(nome)

    if not solvers:
        titulo("4. Solver: NÃO DISPONÍVEL")
        print("O executável `solver` não existe em %s: nenhum solver de PLIM foi encontrado na compilação." % build)
        try:
            import highspy  # noqa: F401
        except ImportError:
            print("O pacote opcional highspy também não está instalado: a formulação NÃO foi validada.")
        else:
            titulo("4b. Formulação de brp2_model.hpp exportada em LP e resolvida com HiGHS (highspy)")
            if validar_com_highs(exe, pasta_inst, bfs, args.time_limit):
                falhar("a formulação (via HiGHS) e a busca em largura divergem")
            if rodar([exe["verificar_resultados"], pasta_inst, SAIDA / "highs"]) != 0:
                falhar("há sequência do modelo (via HiGHS) não executável")
            highs_lp_validado = True

    titulo("RESUMO")
    print("busca em largura: referência = %d; %d instâncias no total (%d aleatórias)"
          % (OTIMO_REFERENCIA, len(bfs), args.n))
    print("heurísticas: sequências executáveis e nunca abaixo do ótimo")
    for nome in validados:
        print("solver com %s: igual à busca em largura em todas as instâncias; (6) original inviável na referência"
              % nome)
    nao_testados = [n for n in ("gurobi", "cplex", "highs") if n not in validados]
    if validados:
        if nao_testados:
            print("ATENÇÃO: adaptadores NÃO compilados nem executados nesta máquina: %s." % ", ".join(nao_testados))
            print("A formulação é a mesma para todos; o que falta testar neles é só a ligação de backends.hpp.")
        print("VALIDAÇÃO OK com: %s" % ", ".join(validados))
    elif highs_lp_validado:
        print("formulação (LP + highspy): igual à busca em largura em todas as instâncias; (6) original inviável")
        print("ATENÇÃO: o executável `solver` não foi compilado; solver.cpp e backends.hpp NÃO foram testados.")
        print("VALIDAÇÃO PARCIAL.")
    else:
        print("ATENÇÃO: nenhum solver de PLIM disponível. O MODELO EXATO NÃO FOI VALIDADO.")
        print("VALIDAÇÃO PARCIAL: apenas busca em largura e heurísticas.")


if __name__ == "__main__":
    main()
