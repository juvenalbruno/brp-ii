#!/usr/bin/env python3
"""Hipervolume (HV) dos métodos do BRP-II a partir dos JSONs de results/.

Cada método gera, por instância, um ponto com os objetivos de OBJETIVOS (todos de
minimização). Cada objetivo é normalizado em [0,1] entre o ponto ideal e o ponto de
referência, e o HV do método é a área (volume) dominada entre o ponto e a referência:

    f'_m = (f_m - ideal_m) / (ref_m - ideal_m)        HV = Π_m (1 - f'_m)

Modos (por instância):
  hv_max  não exige o exato. ref_m = FATOR_REFERENCIA x pior valor observado entre os
          métodos disponíveis; ideal_m = melhor valor observado.
  hv_min  exige resultado do exato com status ótimo. O ideal em OBJETIVO_DO_EXATO
          (relocações) é o valor ótimo do exato; nos demais objetivos o ideal é o
          melhor observado; a referência segue a regra do hv_max.
  auto    (padrão) hv_min se a instância tem exato ótimo, senão hv_max.

Quando há exato ótimo, calcula também:
  gap de HV         = (HV_exato - HV_heurística) / HV_exato
  gap de relocações = (heurística - ótimo) / ótimo

Saídas em hv/output/:
  instancias/<instância>.csv   uma linha por método
  hv_por_instancia.csv         todas as instâncias juntas
  resumo_por_classe.csv        médias por classe WxH e por método (+ linha GERAL)
  resumo_por_arquivo.csv       médias por arquivo de instâncias (W, H, N) e por método
  resumo.txt                   o mesmo resumo em texto

Uso: python3 hv/hv.py [--results results] [--output hv/output]
                      [--modo auto|hv_max|hv_min] [--metodos solver,min_max,ri]
Só biblioteca padrão.
"""
import argparse
import csv
import json
import sys
from pathlib import Path

# ----------------------------------------------------------------------------
# CONFIGURAÇÃO — edite aqui
# ----------------------------------------------------------------------------
# Lista de objetivos (todos de minimização): chave do campo no JSON de resultado.
# Para rodar só com relocações, deixe apenas "relocacoes" na lista.
OBJETIVOS = [
    "relocacoes",
    "tempo_cpu_s",
]
OBJETIVO_DO_EXATO = "relocacoes"  # objetivo cujo ideal vem do exato no modo hv_min
FATOR_REFERENCIA = 1.1            # referência = fator x pior valor observado
METODO_EXATO = "solver"           # nome da subpasta de results/ com o método exato
STATUS_OTIMO = "optimal"
# ----------------------------------------------------------------------------

RAIZ = Path(__file__).resolve().parent.parent


def carregar(pasta_results, metodos):
    """Devolve {instância: {método: registro}} lendo results/<método>/<instância>.json."""
    dados = {}
    for pasta in sorted(p for p in pasta_results.iterdir() if p.is_dir()):
        metodo = pasta.name
        if metodos and metodo not in metodos and metodo != METODO_EXATO:
            continue
        for arq in sorted(pasta.glob("*.json")):
            try:
                reg = json.loads(arq.read_text())
            except (OSError, ValueError) as e:
                print("aviso: ignorando %s (%s)" % (arq, e), file=sys.stderr)
                continue
            dados.setdefault(arq.stem, {})[metodo] = reg
    return dados


def tem_ponto(reg):
    """O registro tem valor numérico em todos os objetivos?"""
    return all(isinstance(reg.get(o), (int, float)) and not isinstance(reg.get(o), bool) for o in OBJETIVOS)


def normalizar(valor, ideal, ref):
    if ref - ideal <= 0:  # todos os métodos empatados em 0 neste objetivo
        return 0.0
    return min(1.0, max(0.0, (valor - ideal) / (ref - ideal)))


def avaliar_instancia(inst, regs, modo_pedido, metodos):
    """Devolve a lista de linhas (dicionários) da instância, uma por método."""
    exato = regs.get(METODO_EXATO)
    exato_otimo = (exato is not None and exato.get("status") == STATUS_OTIMO
                   and isinstance(exato.get("relocacoes"), int))
    otimo = exato["relocacoes"] if exato_otimo else None

    # Métodos que entram como pontos (o exato pode ter sido filtrado por --metodos;
    # mesmo assim o valor ótimo dele continua servindo de ideal no hv_min).
    pontos = {m: r for m, r in regs.items() if (not metodos or m in metodos) and tem_ponto(r)}
    sem_ponto = [m for m, r in regs.items() if (not metodos or m in metodos) and not tem_ponto(r)]

    modo = modo_pedido
    if modo == "auto":
        modo = "hv_min" if exato_otimo else "hv_max"
    if modo == "hv_min" and not exato_otimo:
        print("aviso: %s sem exato ótimo; hv_min indisponível, instância pulada" % inst, file=sys.stderr)
        return []
    if not pontos:
        return []

    ideal, ref = {}, {}
    for o in OBJETIVOS:
        valores = [r[o] for r in pontos.values()]
        ideal[o] = min(valores)
        ref[o] = FATOR_REFERENCIA * max(valores)
    if modo == "hv_min" and OBJETIVO_DO_EXATO in OBJETIVOS:
        ideal[OBJETIVO_DO_EXATO] = otimo

    hv = {}
    norm = {}
    for m, r in pontos.items():
        norm[m] = {o: normalizar(r[o], ideal[o], ref[o]) for o in OBJETIVOS}
        valor = 1.0
        for o in OBJETIVOS:
            valor *= 1.0 - norm[m][o]
        hv[m] = valor

    um = next(iter(regs.values()))
    base = {"instancia": inst, "arquivo": um.get("arquivo", ""), "id": um.get("id") or "",
            "classe": "%dx%d" % (um["W"], um["H"]), "W": um["W"], "H": um["H"], "N": um["N"],
            "modo": modo, "otimo": otimo if otimo is not None else ""}
    linhas = []
    for m in sorted(pontos):
        r = pontos[m]
        linha = dict(base, metodo=m, status=r.get("status", ""))
        for o in OBJETIVOS:
            linha[o] = r[o]
            linha["norm_" + o] = norm[m][o]
            linha["ideal_" + o] = ideal[o]
            linha["ref_" + o] = ref[o]
        linha["hv"] = hv[m]
        linha["gap_hv"] = ""
        linha["gap_relocacoes"] = ""
        if exato_otimo:
            if METODO_EXATO in hv and hv[METODO_EXATO] > 0:
                linha["gap_hv"] = (hv[METODO_EXATO] - hv[m]) / hv[METODO_EXATO]
            rel = r.get("relocacoes")
            if isinstance(rel, int):
                if otimo > 0:
                    linha["gap_relocacoes"] = (rel - otimo) / otimo
                elif rel == 0:
                    linha["gap_relocacoes"] = 0.0  # ótimo 0 e heurística 0; senão indefinido
        linhas.append(linha)
    for m in sorted(sem_ponto):
        print("aviso: %s/%s sem solução (status %s); fora do HV" % (m, inst, regs[m].get("status", "-")),
              file=sys.stderr)
    return linhas


def media(valores):
    valores = [v for v in valores if v != ""]
    return sum(valores) / len(valores) if valores else ""


def fmt(v):
    if v == "":
        return ""
    if isinstance(v, float):
        return "%.6g" % v
    return str(v)


def gravar_csv(arquivo, colunas, linhas):
    with open(arquivo, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(colunas)
        for l in linhas:
            w.writerow([fmt(l.get(c, "")) for c in colunas])


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--results", default=str(RAIZ / "results"), help="pasta com results/<método>/*.json")
    ap.add_argument("--output", default=str(RAIZ / "hv" / "output"), help="pasta de saída")
    ap.add_argument("--modo", choices=["auto", "hv_max", "hv_min"], default="auto",
                    help="auto escolhe por instância; hv_max/hv_min forçam o modo")
    ap.add_argument("--metodos", default="", help="lista separada por vírgula (padrão: todas as subpastas)")
    args = ap.parse_args()

    metodos = [m for m in args.metodos.split(",") if m]
    pasta_results = Path(args.results)
    if not pasta_results.is_dir():
        sys.exit("pasta de resultados não encontrada: %s" % pasta_results)
    dados = carregar(pasta_results, metodos)
    if not dados:
        sys.exit("nenhum resultado encontrado em %s" % pasta_results)

    saida = Path(args.output)
    (saida / "instancias").mkdir(parents=True, exist_ok=True)

    colunas = ["instancia", "arquivo", "id", "classe", "W", "H", "N", "modo", "metodo", "status"]
    colunas += OBJETIVOS
    colunas += ["norm_" + o for o in OBJETIVOS] + ["ideal_" + o for o in OBJETIVOS] + ["ref_" + o for o in OBJETIVOS]
    colunas += ["otimo", "hv", "gap_hv", "gap_relocacoes"]

    todas = []
    for inst in sorted(dados):
        linhas = avaliar_instancia(inst, dados[inst], args.modo, metodos)
        if not linhas:
            continue
        gravar_csv(saida / "instancias" / (inst + ".csv"), colunas, linhas)
        todas.extend(linhas)
    if not todas:
        sys.exit("nenhuma instância pôde ser avaliada no modo %s" % args.modo)
    gravar_csv(saida / "hv_por_instancia.csv", colunas, todas)

    # Resumos: média por método dentro de cada grupo.
    def resumir(rotulo, grupos):
        cols = [rotulo, "metodo", "instancias", "hv_max", "hv_min"]
        cols += ["media_" + o for o in OBJETIVOS] + ["media_hv", "media_gap_hv", "media_gap_relocacoes"]
        saida_resumo = []
        for nome, linhas in grupos:
            for m in sorted({l["metodo"] for l in linhas}):
                do_metodo = [l for l in linhas if l["metodo"] == m]
                r = {rotulo: nome, "metodo": m, "instancias": len(do_metodo),
                     "hv_max": sum(1 for l in do_metodo if l["modo"] == "hv_max"),
                     "hv_min": sum(1 for l in do_metodo if l["modo"] == "hv_min"),
                     "media_hv": media([l["hv"] for l in do_metodo]),
                     "media_gap_hv": media([l["gap_hv"] for l in do_metodo]),
                     "media_gap_relocacoes": media([l["gap_relocacoes"] for l in do_metodo])}
                for o in OBJETIVOS:
                    r["media_" + o] = media([l[o] for l in do_metodo])
                saida_resumo.append(r)
        return cols, saida_resumo

    # Por classe WxH (+ GERAL).
    classes = sorted({(l["W"], l["H"]) for l in todas})
    grupos = [("%dx%d" % c, [l for l in todas if (l["W"], l["H"]) == c]) for c in classes]
    grupos.append(("GERAL", todas))
    col_resumo, resumo = resumir("classe", grupos)
    gravar_csv(saida / "resumo_por_classe.csv", col_resumo, resumo)

    # Por arquivo de instâncias, isto é, por configuração (W, H, N).
    arquivos = sorted({(l["W"], l["H"], l["N"], l["arquivo"]) for l in todas})
    col_arq, resumo_arq = resumir("arquivo", [(a[3] or "%dx%d_N%d" % a[:3],
                                               [l for l in todas if (l["W"], l["H"], l["N"], l["arquivo"]) == a])
                                              for a in arquivos])
    gravar_csv(saida / "resumo_por_arquivo.csv", col_arq, resumo_arq)

    larguras = [max(len(c), max(len(fmt(r.get(c, ""))) for r in resumo)) for c in col_resumo]
    texto = ["Hipervolume — objetivos: %s | modo pedido: %s | referência = %.2f x pior valor"
             % (", ".join(OBJETIVOS), args.modo, FATOR_REFERENCIA),
             "Colunas hv_max/hv_min = nº de instâncias avaliadas em cada modo. Gaps só onde há exato ótimo.",
             "",
             "  ".join(c.ljust(w) for c, w in zip(col_resumo, larguras))]
    for r in resumo:
        texto.append("  ".join(fmt(r.get(c, "")).ljust(w) for c, w in zip(col_resumo, larguras)))
    (saida / "resumo.txt").write_text("\n".join(texto) + "\n")
    print("\n".join(texto))
    print("\n%d instâncias avaliadas; arquivos em %s" % (len({l["instancia"] for l in todas}), saida))


if __name__ == "__main__":
    main()
