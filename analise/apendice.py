import json, glob, os, sys
S = sys.argv[1]
def res(p): return {os.path.basename(f)[:-5]: json.load(open(f)) for f in glob.glob(p + "/*.json")}
mm, ri, A, B = res("results/min_max"), res("results/ri"), res("results/solver"), res(S + "/solver_antes")
abaixo = 0
print("| Arquivo | id | Min-Max | RI | Solver | Status do solver | Limite inferior | Melhor conhecido | Ótimo conhecido | t Min-Max (µs) | t RI (µs) |")
print("|---|---:|---:|---:|---:|---|---:|---:|:---:|---:|---:|")
for arq in sorted(glob.glob("instances/brp_*.json")):
    d = json.load(open(arq)); c = os.path.basename(arq)[:-5]
    for inst in d["instancias"]:
        k = "%s_id%02d" % (c, inst["id"])
        bloq = sum(1 for p in inst["stacks"] for j, b in enumerate(p) if j and min(p[:j]) < b)
        runs = [r for r in (A.get(k), B.get(k)) if r]
        prov = any(r["status"] == "optimal" for r in runs)
        ubs = [r["relocacoes"] for r in runs if r.get("relocacoes") is not None]
        lbs = [r["limite_inferior"] for r in runs if r.get("limite_inferior") is not None]
        a, b = mm[k]["relocacoes"], ri[k]["relocacoes"]
        lb = int(max([bloq] + lbs)); bks = min([a, b] + ubs)
        if min(a, b) < lb: abaixo += 1
        st = "ótimo provado" if prov else ("limite de tempo" if ubs else "sem solução no limite")
        print("| %s | %d | %d | %d | %s | %s | %d | %d | %s | %.0f | %.0f |" % (c, inst["id"], a, b,
              str(min(ubs)) if ubs else "–", st, lb, bks, "sim" if lb == bks else "não",
              1e6 * mm[k]["tempo_cpu_s"], 1e6 * ri[k]["tempo_cpu_s"]))
print("ABAIXO_DO_LB", abaixo, file=sys.stderr)
