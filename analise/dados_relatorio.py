import json, glob, os, sys, math, statistics as st
from collections import defaultdict
S = sys.argv[1]
def res(p): return {os.path.basename(f)[:-5]: json.load(open(f)) for f in glob.glob(p + "/*.json")}
mm, ri, A, B = res("results/min_max"), res("results/ri"), res("results/solver"), res(S + "/solver_antes")
L = []
for arq in sorted(glob.glob("instances/brp_*.json")):
    d = json.load(open(arq)); c = os.path.basename(arq)[:-5]; md = d["metadata"]
    for inst in d["instancias"]:
        k = "%s_id%02d" % (c, inst["id"])
        bloq = sum(1 for p in inst["stacks"] for j, b in enumerate(p) if j and min(p[:j]) < b)
        runs = [r for r in (A.get(k), B.get(k)) if r]
        prov = [r for r in runs if r["status"] == "optimal"]
        ubs = [r["relocacoes"] for r in runs if r.get("relocacoes") is not None]
        lbs = [r["limite_inferior"] for r in runs if r.get("limite_inferior") is not None]
        a, b = mm[k]["relocacoes"], ri[k]["relocacoes"]
        lb = max([bloq] + lbs); bks = min([a, b] + ubs)
        L.append(dict(k=k, c=c, W=md["W"], H=md["H"], N=md["N"], mm=a, ri=b, bloq=bloq, lbs=max(lbs, default=None),
                      lb=lb, bks=bks, ub=min(ubs, default=None), prov=bool(prov),
                      tprov=min((r["tempo_parede_s"] for r in prov), default=None),
                      presprov=(prov[0].get("presolve", "on (padrão)") if prov else None),
                      tmm=mm[k]["tempo_cpu_s"], tri=ri[k]["tempo_cpu_s"], temsol=bool(ubs)))
g = lambda z, r: (z - r) / r if r else 0.0
por = defaultdict(list)
for l in L: por[l["c"]].append(l)
def linha(c, ls):
    n = len(ls); m = lambda f: sum(f(l) for l in ls) / n
    w = sum(l["mm"] < l["ri"] for l in ls); t = sum(l["mm"] == l["ri"] for l in ls)
    conh = [l for l in ls if l["lb"] == l["bks"]]
    tp = [l["tprov"] for l in ls if l["prov"]]
    return dict(c=c, n=n, mm=m(lambda l: l["mm"]), ri=m(lambda l: l["ri"]), bks=m(lambda l: l["bks"]), lb=m(lambda l: l["lb"]),
        bloq=m(lambda l: l["bloq"]), w=w, t=t, lo=n - w - t,
        gmb=100*m(lambda l: g(l["mm"], l["bks"])), grb=100*m(lambda l: g(l["ri"], l["bks"])),
        gml=100*m(lambda l: g(l["mm"], l["lb"])), grl=100*m(lambda l: g(l["ri"], l["lb"])),
        conh=len(conh), prov=sum(l["prov"] for l in ls), semsol=sum(not l["temsol"] for l in ls),
        sol_so_viavel=sum(l["temsol"] and not l["prov"] for l in ls),
        mm_otimo=sum(l["mm"] == l["bks"] for l in conh), ri_otimo=sum(l["ri"] == l["bks"] for l in conh),
        mm_bks=sum(l["mm"] == l["bks"] for l in ls), ri_bks=sum(l["ri"] == l["bks"] for l in ls),
        solver_melhor=sum(l["ub"] is not None and l["ub"] < l["mm"] for l in ls),
        tp_med=(st.mean(tp) if tp else None), tp_max=(max(tp) if tp else None),
        tmm=1e6*m(lambda l: l["tmm"]), tri=1e6*m(lambda l: l["tri"]),
        dmax=max(l["ri"] - l["mm"] for l in ls), dmed=m(lambda l: l["ri"] - l["mm"]),
        gmo=100*st.mean([g(l["mm"], l["bks"]) for l in conh]) if conh else None,
        gro=100*st.mean([g(l["ri"], l["bks"]) for l in conh]) if conh else None)
out = [linha(c, ls) for c, ls in por.items()]
grupos = {"A": [l for l in L if l["W"] < 7], "B": [l for l in L if l["W"] == 7], "TOTAL": L}
for H in (5, 6, 7, 8): pass
ocup = defaultdict(list)
for l in L:
    if l["W"] == 7:
        Ns = sorted({x["N"] for x in L if x["W"] == 7 and x["H"] == l["H"]})
        ocup[["~50%", "~75%", "~100%"][Ns.index(l["N"])]].append(l)
for nome, ls in list(grupos.items()) + [("B " + k, v) for k, v in ocup.items()]:
    out.append(linha(nome, ls))
for o in out: print(json.dumps(o, ensure_ascii=False))
# Wilcoxon
def wilcoxon(ls):
    d = [l["ri"] - l["mm"] for l in ls if l["ri"] != l["mm"]]
    ab = sorted((abs(x), i) for i, x in enumerate(d)); rk = [0]*len(d); i = 0
    while i < len(ab):
        j = i
        while j < len(ab) and ab[j][0] == ab[i][0]: j += 1
        for q in range(i, j): rk[ab[q][1]] = (i + j + 1) / 2
        i = j
    n = len(d); wp = sum(r for r, x in zip(rk, d) if x > 0); wn = sum(r for r, x in zip(rk, d) if x < 0)
    z = (wp - n*(n+1)/4) / math.sqrt(n*(n+1)*(2*n+1)/24)
    return n, wp, wn, z, math.erfc(abs(z)/math.sqrt(2))
for nome, ls in grupos.items(): print("wilcoxon", nome, wilcoxon(ls))
print("RI melhor:", [(l["k"], l["mm"], l["ri"], l["bks"], l["lb"]) for l in L if l["ri"] < l["mm"]])
print("maior diferença:", sorted(((l["ri"]-l["mm"], l["k"], l["mm"], l["ri"]) for l in L), reverse=True)[:3])
print("MM pior que BKS (top):", sorted(((l["mm"]-l["bks"], l["k"], l["mm"], l["bks"], l["prov"]) for l in L), reverse=True)[:5])
print("solver incumbente pior que MM:", sum(1 for l in L if l["ub"] is not None and not l["prov"] and l["ub"] > l["mm"]),
      "| solver incumbente < MM sem prova:", sum(1 for l in L if l["ub"] is not None and not l["prov"] and l["ub"] < l["mm"]))
print("conhecido só por LB de bloqueio/heurística (sem prova do solver):", [l["k"] for l in L if l["lb"] == l["bks"] and not l["prov"]])
print("presolve das provas:", {p: sum(1 for l in L if l["presprov"] == p) for p in {l["presprov"] for l in L if l["prov"]}})
print("tempo heurísticas máx (µs): mm %.1f ri %.1f" % (1e6*max(l["tmm"] for l in L), 1e6*max(l["tri"] for l in L)))
print("LB solver > bloqueio:", sum(1 for l in L if l["lbs"] is not None and l["lbs"] > l["bloq"]), "| LB solver ausente:", sum(1 for l in L if l["lbs"] is None))
