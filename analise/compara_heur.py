import json, glob, os, sys, math
from collections import defaultdict
S = sys.argv[1]
def res(pasta):
    return {os.path.basename(f)[:-5]: json.load(open(f)) for f in glob.glob(pasta + "/*.json")}
mm, ri = res("results/min_max"), res("results/ri")
sol_a, sol_b = res("results/solver"), res(S + "/solver_antes")
linhas = []
for arq in sorted(glob.glob("instances/brp_*.json")):
    d = json.load(open(arq)); classe = os.path.basename(arq)[:-5]
    for inst in d["instancias"]:
        k = "%s_id%02d" % (classe, inst["id"])
        bloq = sum(1 for p in inst["stacks"] for j, b in enumerate(p) if j and min(p[:j]) < b)
        lbs, ubs = [bloq], []
        for s in (sol_a, sol_b):
            r = s.get(k)
            if not r: continue
            if r.get("limite_inferior") is not None: lbs.append(r["limite_inferior"])
            if r.get("relocacoes") is not None: ubs.append(r["relocacoes"])
        a, b = mm[k]["relocacoes"], ri[k]["relocacoes"]
        lb = max(lbs); bks = min([a, b] + ubs)
        linhas.append(dict(classe=classe, mm=a, ri=b, bloq=bloq, lb=lb, bks=bks, lbsolver=max(lbs[1:], default=None)))
def g(z, ref): return (z - ref) / ref if ref else 0.0
por = defaultdict(list)
for l in linhas: por[l["classe"]].append(l)
print("%-15s %5s %5s  %-8s %8s %8s %8s %8s %6s" % ("classe", "MM", "RI", "MM/=/RI", "MM~BKS", "RI~BKS", "MM~LB", "RI~LB", "prov"))
for c, ls in list(por.items()) + [("TOTAL", linhas)]:
    n = len(ls); m = lambda f: sum(f(l) for l in ls) / n
    w = sum(l["mm"] < l["ri"] for l in ls); t = sum(l["mm"] == l["ri"] for l in ls)
    print("%-15s %5.1f %5.1f  %2d/%2d/%2d %7.1f%% %7.1f%% %7.1f%% %7.1f%% %3d/%d" % (
        c, m(lambda l: l["mm"]), m(lambda l: l["ri"]), w, t, n - w - t,
        100 * m(lambda l: g(l["mm"], l["bks"])), 100 * m(lambda l: g(l["ri"], l["bks"])),
        100 * m(lambda l: g(l["mm"], l["lb"])), 100 * m(lambda l: g(l["ri"], l["lb"])),
        sum(l["lb"] == l["bks"] for l in ls), n))
# Wilcoxon pareado (aprox. normal), diferenças RI - MM
d = [l["ri"] - l["mm"] for l in linhas if l["ri"] != l["mm"]]
ab = sorted((abs(x), i) for i, x in enumerate(d)); ranks = [0] * len(d); i = 0
while i < len(ab):
    j = i
    while j < len(ab) and ab[j][0] == ab[i][0]: j += 1
    for k in range(i, j): ranks[ab[k][1]] = (i + j + 1) / 2
    i = j
n = len(d); wp = sum(r for r, x in zip(ranks, d) if x > 0)
mu = n * (n + 1) / 4; sd = math.sqrt(n * (n + 1) * (2 * n + 1) / 24)
z = (wp - mu) / sd; p = math.erfc(abs(z) / math.sqrt(2))
print("\nWilcoxon (n=%d diferentes): z=%.2f p=%.2g" % (n, z, p))
print("LB do solver acima do LB de bloqueio em %d de %d" % (sum(1 for l in linhas if l["lbsolver"] is not None and l["lbsolver"] > l["bloq"]), len(linhas)))
print("BKS vindo de heurística (solver pior ou sem solução): %d" % sum(1 for l in linhas if min(l["mm"], l["ri"]) == l["bks"]))
