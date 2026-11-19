#!/usr/bin/env python3
"""Comparação do GA (GA-A e GA-H, Gulić et al. 2022) com A* e Min-Max (aula 4).

Lê results/{astar,min_max,ga_a,ga_h} (e results/solver, se existir) e imprime, por
arquivo de instâncias, a tabela em Markdown do protocolo da disciplina:

  Z*      ótimo provado (A* ou solver com status optimal) ou, se não houver, o melhor
          valor conhecido entre todos os métodos (BKS). A coluna "ótimos" diz em
          quantas instâncias Z* é ótimo provado.
  gap     (Z - Z*) / Z*, média sobre as instâncias com Z* > 0 (com Z* = 0 o gap é
          indefinido; essas instâncias entram só na contagem de acertos).
  GA      média sobre as sementes (média de Z de cada instância) e melhor semente.
  tempo   CPU médio por instância (GA: média por semente; o orçamento é --time-limit).

Imprime também a convergência (PP2): tempo e geração médios até o melhor Z, e em
quantas instâncias cada variante melhora a Min-Max.

Uso: python3 analise/compara_ga.py [--results results] [--csv saida.csv]
"""
import argparse
import csv
import json
from collections import OrderedDict
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent


def ler(pasta):
    return {a.stem: json.loads(a.read_text()) for a in sorted(pasta.glob("*.json"))} if pasta.is_dir() else {}


def media(v):
    v = [x for x in v if x is not None]
    return sum(v) / len(v) if v else None


def gap(z, ref):
    return (z - ref) / ref if ref else None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--results", default=str(RAIZ / "results"))
    ap.add_argument("--csv", help="grava também uma linha por instância neste CSV")
    args = ap.parse_args()
    R = Path(args.results)
    astar, mm, gaa, gah, solver = (ler(R / m) for m in ("astar", "min_max", "ga_a", "ga_h", "solver"))
    if not (gaa and gah):
        raise SystemExit("faltam results/ga_a ou results/ga_h")

    linhas = []
    for k in sorted(gaa):
        if k not in gah or k not in mm:
            continue
        a, s = astar.get(k), solver.get(k)
        otimos = [r["relocacoes"] for r in (a, s) if r and r.get("status") == "optimal"]
        candidatos = [mm[k]["relocacoes"], gaa[k]["relocacoes"], gah[k]["relocacoes"]]
        candidatos += [r["relocacoes"] for r in (a, s) if r and r.get("relocacoes") is not None]
        zstar = otimos[0] if otimos else min(candidatos)
        l = OrderedDict(instancia=k, arquivo=gaa[k]["arquivo"], N=gaa[k]["N"], z_star=zstar,
                        otimo=bool(otimos), h_s0=gaa[k]["limite_inferior_h"],
                        astar=a["relocacoes"] if a else None, astar_status=a["status"] if a else None,
                        astar_cpu=a["tempo_cpu_s"] if a else None,
                        min_max=mm[k]["relocacoes"], min_max_cpu=mm[k]["tempo_cpu_s"])
        for nome, r in (("ga_a", gaa[k]), ("ga_h", gah[k])):
            l[nome + "_media"] = r["media_relocacoes"]
            l[nome + "_melhor"] = r["relocacoes"]
            l[nome + "_desvio"] = r["desvio_relocacoes"]
            l[nome + "_cpu"] = r["tempo_cpu_s"]
            l[nome + "_t_melhor"] = media(r["tempo_ate_melhor_s_por_semente"])
            l[nome + "_ger_melhor"] = media(r["geracao_do_melhor_por_semente"])
            l[nome + "_geracoes"] = media(r["geracoes_por_semente"])
        linhas.append(l)

    if args.csv:
        with open(args.csv, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(linhas[0].keys()))
            w.writeheader()
            w.writerows(linhas)

    grupos = OrderedDict()
    for l in sorted(linhas, key=lambda l: (l["arquivo"] != "", l["N"], l["arquivo"])):
        grupos.setdefault(l["arquivo"], []).append(l)
    grupos["TOTAL"] = linhas

    def pct(x):
        return "–" if x is None else "%.1f%%" % (100 * x)

    print("| Arquivo | ótimos | Z* médio | Min-Max | GA-A média | GA-A melhor | GA-H média | GA-H melhor"
          " | acertos Z* (MM / A / H) | CPU A* / GA (s) |")
    print("|---|---|---|---|---|---|---|---|---|---|")
    for nome, ls in grupos.items():
        n = len(ls)
        z = media([l["z_star"] for l in ls])

        def col(campo):
            return "%.1f (%s)" % (media([l[campo] for l in ls]),
                                  pct(media([gap(l[campo], l["z_star"]) for l in ls])))

        acertos = " / ".join(str(sum(1 for l in ls if l[c] == l["z_star"])) for c in ("min_max", "ga_a_melhor", "ga_h_melhor"))
        cpu = "%.2f / %.1f" % (media([l["astar_cpu"] for l in ls]) or 0.0, media([l["ga_a_cpu"] for l in ls]))
        rot = "**TOTAL**" if nome == "TOTAL" else "`%s`" % nome.replace(".json", "")
        print("| %s | %d/%d | %.1f | %s | %s | %s | %s | %s | %s | %s |"
              % (rot, sum(l["otimo"] for l in ls), n, z, col("min_max"), col("ga_a_media"), col("ga_a_melhor"),
                 col("ga_h_media"), col("ga_h_melhor"), acertos, cpu))

    print()
    print("Convergência (PP2), média sobre instâncias e sementes:")
    for v in ("ga_a", "ga_h"):
        melhora = sum(1 for l in linhas if l[v + "_melhor"] < l["min_max"])
        pior = sum(1 for l in linhas if l[v + "_media"] > l["min_max"])
        print("  %s: tempo até o melhor = %.2f s, geração do melhor = %.0f, gerações/execução = %.0f;"
              " melhor semente melhora a Min-Max em %d/%d; média pior que a Min-Max em %d/%d"
              % (v.upper().replace("_", "-"), media([l[v + "_t_melhor"] for l in linhas]),
                 media([l[v + "_ger_melhor"] for l in linhas]), media([l[v + "_geracoes"] for l in linhas]),
                 melhora, len(linhas), pior, len(linhas)))
    sem_otimo = [l for l in linhas if not l["otimo"]]
    if sem_otimo:
        print("  instâncias sem ótimo provado: %d; nelas, o GA-A acha o melhor conhecido em %d e fica abaixo da"
              " solução de parada do A* em %d"
              % (len(sem_otimo), sum(1 for l in sem_otimo if l["ga_a_melhor"] == l["z_star"]),
                 sum(1 for l in sem_otimo if l["astar"] is not None and l["ga_a_melhor"] < l["astar"])))


if __name__ == "__main__":
    main()
