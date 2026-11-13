// app.js - the dashboard: overview charts, live race, AI lab, workload patterns.
"use strict";
(function () {
  const D = window.SIM_DATA, E = window.CacheEngine;
  const $ = (s) => document.querySelector(s);
  const $$ = (s) => Array.from(document.querySelectorAll(s));

  if (!D || !E) {
    document.querySelector("main").innerHTML =
      '<div class="card missing"><h2>No simulation data yet</h2><p class="muted" style="margin-top:8px">' +
      "Double-click <code>run_windows.bat</code>, or run <code>build\\cache_sim.exe --export-web</code>, then refresh this page.</p></div>";
    $("#check").textContent = "No data";
    return;
  }

  // ------------------------------------------------------------ shared bits
  const COLORS = { FIFO: "#9AA5B1", LRU: "#4EA1E0", LFU: "#9B7BD1", AI: "#FF6B47", "AI-K8": "#F2A541", Adaptive: "#2EC4A6", "Belady-OPT": "#E8EDF2" };
  const RULE = {
    FIFO: "throws out the oldest item", LRU: "throws out the least recently used", LFU: "throws out the least used",
    AI: "learned rule (genetic algorithm)", "AI-K8": "AI that checks only 8 random items",
    Adaptive: "copies whichever of FIFO, LRU, LFU is winning", "Belady-OPT": "knows the future: the best possible",
  };
  const LABEL = (p) => (p === "Belady-OPT" ? "Best possible" : p);
  const pct = (x, d = 1) => (100 * x).toFixed(d) + "%";
  const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));

  if (window.Chart) {
    Chart.defaults.color = "#a9b6ca";
    Chart.defaults.borderColor = "rgba(255,255,255,0.07)";
    Chart.defaults.font.family = '"Segoe UI", system-ui, -apple-system, Roboto, Arial, sans-serif';
    Chart.defaults.plugins.legend.labels.usePointStyle = true;
    Chart.defaults.plugins.legend.labels.boxWidth = 8;
    Chart.defaults.maintainAspectRatio = false;
    Chart.defaults.plugins.tooltip.backgroundColor = "rgba(12,18,34,0.95)";
    Chart.defaults.plugins.tooltip.borderColor = "rgba(255,255,255,0.12)";
    Chart.defaults.plugins.tooltip.borderWidth = 1;
  }
  function chart(id, config) {
    const el = document.getElementById(id);
    if (!window.Chart || !el) return null;
    return new Chart(el, config);
  }

  const workloads = [...new Set(D.results.map((r) => r.w))];
  const policies = [...new Set(D.results.map((r) => r.p))];
  const R = (w, p) => D.results.find((r) => r.w === w && r.p === p);

  // ------------------------------------------------------------ tabs
  const inited = {};
  const current = { tab: "overview" };
  function showTab(id, scroll = true) {
    $$("nav button").forEach((b) => b.classList.toggle("active", b.dataset.tab === id));
    $$(".tab").forEach((t) => t.classList.toggle("active", t.id === id));
    if (!inited[id]) { inited[id] = true; ({ overview: initOverview, sim: initSim, race: initRace, lab: initLab, patterns: initPatterns, how: () => {} })[id](); }
    current.tab = id;
    try { localStorage.setItem("cachesim.tab", id); } catch (e) { /* storage may be blocked */ }
    if (scroll) $("#main").scrollTo({ top: 0 });
  }
  $$("nav button").forEach((b) => b.addEventListener("click", () => showTab(b.dataset.tab)));
  $$("[data-goto]").forEach((b) => b.addEventListener("click", () => showTab(b.dataset.goto)));

  // ------------------------------------------------------------ engine check
  function engineCheck() {
    let ok = 0, total = 0;
    for (const t of D.traces) for (const p of E.POLICIES) {
      if (t.hits[p] === undefined) continue;
      total++;
      if (E.simulate(p, t.keys, t.capacity, D.weights, false).hits === t.hits[p]) ok++;
    }
    const el = $("#check");
    el.classList.remove("pending");
    el.classList.add(ok === total ? "ok" : "fail");
    el.textContent = ok === total ? `✓ Browser engine matches C++ (${ok}/${total})` : `⚠ Engine mismatch (${ok}/${total})`;
  }

  // ------------------------------------------------------------ overview
  function initOverview() {
    const loop = workloads.find((w) => w.startsWith("looping"));
    const g = (p) => (D.generalization || []).find((x) => x.p === p);
    const kpis = [];
    if (loop) kpis.push({ cls: "hot", v: pct(R(loop, "AI").hr), t: `AI on loops. LRU, LFU and FIFO get ${pct(R(loop, "LRU").hr, 0)}` });
    if (g("AI") && g("LRU")) kpis.push({ cls: "good", v: "+" + (100 * (g("AI").test - g("LRU").test)).toFixed(1) + " pts", t: "AI vs LRU on new, unseen workloads" });
    if (loop) kpis.push({ cls: "", v: (100 * (R(loop, "Belady-OPT").hr - R(loop, "AI").hr)).toFixed(2) + " pts", t: "gap between AI and the best possible on loops" });
    kpis.push({ cls: "", v: `${policies.length} × ${workloads.length}`, t: "policies × workloads, same requests for all" });
    $("#kpis").innerHTML = kpis.map((k) => `<div class="kpi ${k.cls}"><b>${k.v}</b><span>${esc(k.t)}</span></div>`).join("");

    chart("chartHits", {
      type: "bar",
      data: {
        labels: workloads,
        datasets: policies.map((p) => ({
          label: LABEL(p), data: workloads.map((w) => +(100 * R(w, p).hr).toFixed(2)),
          backgroundColor: COLORS[p] + (p === "Belady-OPT" ? "55" : "dd"), borderColor: COLORS[p], borderWidth: p === "Belady-OPT" ? 1 : 0,
          borderRadius: 4, hidden: p === "AI-K8",
        })),
      },
      options: {
        scales: { y: { beginAtZero: true, max: 100, title: { display: true, text: "hit ratio (%)" } }, x: { grid: { display: false } } },
        plugins: { tooltip: { callbacks: { label: (c) => ` ${c.dataset.label}: ${c.parsed.y.toFixed(1)}%` } } },
      },
    });

    if (D.training) {
      const lru = g("LRU");
      chart("chartTrain", {
        type: "line",
        data: {
          labels: D.training.gen,
          datasets: [
            { label: "Best rule", data: D.training.best.map((x) => 100 * x), borderColor: COLORS.AI, backgroundColor: COLORS.AI + "22", fill: false, tension: 0.25, pointRadius: 0, borderWidth: 3 },
            { label: "Average rule", data: D.training.mean.map((x) => 100 * x), borderColor: "#4EA1E0", pointRadius: 0, tension: 0.25, borderWidth: 2 },
            { label: "Worst rule", data: D.training.worst.map((x) => 100 * x), borderColor: "#6b778c", borderDash: [5, 4], pointRadius: 0, borderWidth: 1.5 },
          ].concat(lru ? [{ label: "LRU", data: D.training.gen.map(() => 100 * lru.train), borderColor: "#9AA5B1", borderDash: [2, 3], pointRadius: 0, borderWidth: 1.5 }] : []),
        },
        options: { interaction: { mode: "index", intersect: false }, scales: { x: { title: { display: true, text: "generation" }, ticks: { maxTicksLimit: 7 } }, y: { title: { display: true, text: "average hit ratio (%)" } } } },
      });
    }

    if (D.generalization && D.generalization.length) {
      const gp = D.generalization.map((x) => x.p);
      chart("chartGen", {
        type: "bar",
        data: {
          labels: gp.map(LABEL),
          datasets: [
            { label: "Training workloads", data: D.generalization.map((x) => +(100 * x.train).toFixed(2)), backgroundColor: gp.map((p) => COLORS[p] + "ee"), borderRadius: 4 },
            { label: "New workloads", data: D.generalization.map((x) => +(100 * x.test).toFixed(2)), backgroundColor: gp.map((p) => COLORS[p] + "66"), borderColor: gp.map((p) => COLORS[p]), borderWidth: 1, borderRadius: 4 },
          ],
        },
        options: { scales: { y: { beginAtZero: true, title: { display: true, text: "average hit ratio (%)" } }, x: { grid: { display: false } } } },
      });
    }

    if (D.benchmark && D.benchmark.length) {
      const caps = [...new Set(D.benchmark.map((b) => b.cap))].sort((a, b) => a - b);
      const bp = [...new Set(D.benchmark.map((b) => b.p))];
      chart("chartSpeed", {
        type: "line",
        data: {
          labels: caps.map((c) => `${c} items`),
          datasets: bp.map((p) => ({
            label: LABEL(p), data: caps.map((c) => D.benchmark.find((b) => b.p === p && b.cap === c).rps / 1e6),
            borderColor: COLORS[p], backgroundColor: COLORS[p], borderWidth: p.startsWith("AI") ? 3 : 1.6, tension: 0.2, pointRadius: 4,
          })),
        },
        options: { scales: { y: { type: "logarithmic", title: { display: true, text: "million requests / second" } } }, plugins: { legend: { position: "bottom" } } },
      });
    }

    const names = D.featureNames, meaning = ["time since last use", "how popular", "time in the cache", "time between uses"];
    $("#weights").innerHTML = D.weights.map((w, i) => {
      const left = w < 0 ? 50 + 50 * w : 50, width = Math.abs(w) * 50;
      return `<div class="wrow"><div><b>${names[i]}</b><div class="muted small">${meaning[i]}</div></div>
        <div class="wbar"><i style="left:${left}%;width:${width}%;background:${w > 0 ? COLORS.AI : COLORS.LRU}"></i></div>
        <div class="wval">${w > 0 ? "+" : ""}${w.toFixed(2)}</div></div>`;
    }).join("") + `<ul class="wexplain"><li><b>Negative recency</b>: throws out the <i>most</i> recently used item (MRU). Perfect for loops and one-time scans.</li>
      <li><b>Negative frequency</b>: protects popular items, like LFU.</li><li><b>Positive gap</b>: throws out items whose uses are far apart.</li>
      <li>Nobody programmed these rules. The genetic algorithm found them.</li></ul>`;

    const online = policies.filter((p) => p !== "Belady-OPT");
    let html = `<table class="heat"><thead><tr><th>Workload</th>${policies.map((p) => `<th><span class="dot" style="--c:${COLORS[p]}"></span> ${LABEL(p)}</th>`).join("")}</tr></thead><tbody>`;
    for (const w of workloads) {
      const best = Math.max(...online.map((p) => R(w, p).hr));
      html += `<tr><td><b>${esc(w)}</b></td>` + policies.map((p) => {
        const hr = R(w, p).hr, a = best > 0 ? Math.min(hr / Math.max(best, 1e-9), 1) : 0;
        const isBest = p !== "Belady-OPT" && hr >= best - 1e-12 && best > 0;
        const bg = p === "Belady-OPT" ? "rgba(255,255,255,0.06)" : `rgba(46,196,166,${(0.06 + 0.42 * a * a).toFixed(3)})`;
        return `<td class="${isBest ? "best" : ""}" style="background:${bg}" title="${esc(LABEL(p))}: ${R(w, p).hits.toLocaleString()} hits">${(100 * hr).toFixed(1)}${isBest ? " ★" : ""}</td>`;
      }).join("") + "</tr>";
    }
    $("#resultsTable").innerHTML = html + "</tbody></table>";
    $("#tableNote").textContent = `Hit ratio (%) with a cache of ${D.settings.capacity} items and ${D.settings.requests.toLocaleString()} requests per workload. ★ marks the best real policy.`;
  }


  // ------------------------------------------------------------ simulator
  const sim = { runs: [], active: -1, bar: null, curve: null };
  function initSim() {
    const sel = $("#simWorkload");
    sel.innerHTML = Object.entries(E.WORKLOADS).map(([k, w]) => `<option value="${k}">${esc(w.title)}</option>`).join("") + '<option value="custom">Your own request list</option>';
    const about = () => {
      const k = sel.value;
      $("#simAbout").textContent = k === "custom" ? "Type any numbers. Every policy sees exactly the same requests." : E.WORKLOADS[k].about;
      $("#simAlphaRow").hidden = k !== "zipf";
      $("#simCustomRow").hidden = k !== "custom";
      $("#simN").disabled = k === "custom";
    };
    sel.addEventListener("change", about); about();
    $("#simAlpha").addEventListener("input", () => { $("#simAlphaVal").textContent = (+$("#simAlpha").value).toFixed(2); });
    $("#simPolicies").innerHTML = E.POLICIES.map((p) => `<label style="--c:${COLORS[p]}"><input type="checkbox" data-p="${p}" checked><span class="dot"></span>${LABEL(p)}</label>`).join("");
    $("#simRun").addEventListener("click", runSim);
    $("#simCsv").addEventListener("click", exportSimCsv);
    $("#simExplain").addEventListener("click", () => {
      const r = sim.runs[sim.active];
      if (!r) { $("#simStatus").textContent = "Run a simulation first."; return; }
      if (window.CacheSimChat) window.CacheSimChat.ask("Explain my last simulation in simple words: who won and why?");
    });
    sim.bar = chart("chartSimBar", { type: "bar", data: { labels: [], datasets: [{ data: [], backgroundColor: [], borderRadius: 6 }] },
      options: { indexAxis: "y", plugins: { legend: { display: false }, tooltip: { callbacks: { label: (c) => ` ${c.parsed.x.toFixed(2)}% hits` } } },
        scales: { x: { beginAtZero: true, max: 100, title: { display: true, text: "hit ratio (%)" } }, y: { grid: { display: false } } } } });
    sim.curve = chart("chartSimCurve", { type: "line", data: { labels: [], datasets: [] },
      options: { animation: false, interaction: { mode: "index", intersect: false }, elements: { point: { radius: 0 } },
        scales: { x: { title: { display: true, text: "requests processed" }, ticks: { maxTicksLimit: 6 } }, y: { beginAtZero: true, max: 100, title: { display: true, text: "hit ratio so far (%)" } } },
        plugins: { legend: { position: "bottom" } } } });
    renderSimSummary(null);
  }

  async function runSim() {
    const btn = $("#simRun"), kind = $("#simWorkload").value;
    const cap = Math.max(1, Math.min(500, Math.floor(+$("#simCap").value || 1)));
    const universe = Math.max(10, Math.min(100000, Math.floor(+$("#simUniverse").value || 1000)));
    const seed = Math.max(0, Math.floor(+$("#simSeed").value || 0));
    const policies = $$("#simPolicies input").filter((c) => c.checked).map((c) => c.dataset.p);
    if (!policies.length) { $("#simStatus").textContent = "Pick at least one policy."; return; }
    let trace;
    if (kind === "custom") {
      trace = $("#simCustom").value.trim().split(/[\s,]+/).filter(Boolean).map(Number);
      if (!trace.length || trace.some((x) => !Number.isInteger(x) || x < 0)) { $("#simStatus").textContent = "Your request list must be whole numbers separated by spaces."; return; }
    } else {
      trace = E.generate(kind, +$("#simN").value, cap, { alpha: +$("#simAlpha").value, universe, seed });
    }
    const weights = $("#simAiRule").value === "lab" ? lab.w.slice() : D.weights.slice();
    $("#simCap").value = cap; $("#simUniverse").value = universe;
    btn.disabled = true;
    const results = [];
    for (let i = 0; i < policies.length; i++) {
      const p = policies[i];
      $("#simStatus").textContent = `Running ${LABEL(p)} (${i + 1} of ${policies.length})...`;
      $("#simProgress").style.width = (100 * i) / policies.length + "%";
      await new Promise((res) => setTimeout(res, 20));
      const t0 = performance.now();
      const r = E.simulate(p, trace, cap, weights, false, { samples: 100 });
      results.push({ p, hits: r.hits, misses: r.misses, evictions: r.evictions, curve: r.curve, ms: performance.now() - t0 });
    }
    $("#simProgress").style.width = "100%";
    const run = {
      id: sim.runs.length + 1, kind, title: kind === "custom" ? "Your own list" : E.WORKLOADS[kind].title,
      cap, n: trace.length, universe, seed, alpha: +$("#simAlpha").value, aiRule: $("#simAiRule").value, weights, results,
    };
    sim.runs.push(run);
    showRun(sim.runs.length - 1);
    $("#simStatus").textContent = `Done: ${trace.length.toLocaleString()} requests through ${policies.length} policies.`;
    btn.disabled = false;
  }

  function showRun(i) {
    sim.active = i;
    const run = sim.runs[i], rs = run.results.slice().sort((a, b) => b.hits - a.hits), n = run.n;
    renderSimSummary(run);
    if (sim.bar) {
      sim.bar.data.labels = rs.map((r) => LABEL(r.p));
      sim.bar.data.datasets[0].data = rs.map((r) => (100 * r.hits) / n);
      sim.bar.data.datasets[0].backgroundColor = rs.map((r) => COLORS[r.p] + (r.p === "Belady-OPT" ? "66" : "dd"));
      sim.bar.update();
    }
    if (sim.curve) {
      const pts = run.results[0].curve.length;
      sim.curve.data.labels = Array.from({ length: pts }, (_, j) => Math.round(((j + 1) * n) / pts).toLocaleString());
      sim.curve.data.datasets = run.results.map((r) => ({ label: LABEL(r.p), data: r.curve.map((x) => 100 * x), borderColor: COLORS[r.p], borderWidth: r.p === "AI" ? 3 : 1.8, tension: 0.2, borderDash: r.p === "Belady-OPT" ? [5, 4] : [] }));
      sim.curve.update();
    }
    $("#simTable").innerHTML = `<table class="plain"><thead><tr><th>Policy</th><th>Hits</th><th>Misses</th><th>Evictions</th><th>Hit ratio</th><th>Time</th></tr></thead><tbody>` +
      rs.map((r) => `<tr><td><span class="dot" style="--c:${COLORS[r.p]}"></span> ${LABEL(r.p)}</td><td>${r.hits.toLocaleString()}</td><td>${r.misses.toLocaleString()}</td><td>${r.evictions.toLocaleString()}</td><td><b>${pct(r.hits / n, 2)}</b></td><td>${r.ms.toFixed(0)} ms</td></tr>`).join("") +
      `</tbody></table><p class="muted small" style="margin-top:8px">${esc(run.title)} &middot; cache ${run.cap} &middot; ${n.toLocaleString()} requests &middot; ${run.universe.toLocaleString()} keys &middot; seed ${run.seed}${run.kind === "zipf" ? " &middot; skew " + run.alpha.toFixed(2) : ""} &middot; AI rule: ${run.aiRule === "lab" ? "yours" : "learned by GA"}</p>`;
    $("#simHistory").innerHTML = sim.runs.map((r, j) => {
      const best = r.results.filter((x) => x.p !== "Belady-OPT").sort((a, b) => b.hits - a.hits)[0];
      return `<button class="history-item ${j === i ? "active" : ""}" data-run="${j}"><div><b>#${r.id} ${esc(r.title)}</b> <span>cache ${r.cap} &middot; ${r.n.toLocaleString()} requests</span></div><span>${best ? "winner " + LABEL(best.p) + " " + pct(best.hits / r.n) : ""}</span></button>`;
    }).reverse().join("");
    $$("#simHistory .history-item").forEach((b) => b.addEventListener("click", () => showRun(+b.dataset.run)));
  }

  function renderSimSummary(run) {
    if (!run) { $("#simSummary").innerHTML = ["Winner", "AI vs LRU", "Gap to best possible", "Requests"].map((t) => `<div class="kpi"><b>&ndash;</b><span>${t}</span></div>`).join(""); return; }
    const n = run.n, get = (p) => run.results.find((r) => r.p === p);
    const online = run.results.filter((r) => r.p !== "Belady-OPT").sort((a, b) => b.hits - a.hits);
    const best = online[0], opt = get("Belady-OPT"), ai = get("AI"), lru = get("LRU");
    const cards = [
      { cls: "hot", v: best ? LABEL(best.p) : "&ndash;", t: best ? `winner with ${pct(best.hits / n)} hits` : "no real policy selected" },
      { cls: "good", v: ai && lru ? ((ai.hits - lru.hits) / n * 100 >= 0 ? "+" : "") + ((100 * (ai.hits - lru.hits)) / n).toFixed(1) + " pts" : "&ndash;", t: "AI compared with LRU" },
      { cls: "", v: opt && best ? ((100 * (opt.hits - best.hits)) / n).toFixed(1) + " pts" : "&ndash;", t: "winner's gap to the best possible" },
      { cls: "", v: n.toLocaleString(), t: `requests, cache of ${run.cap} items` },
    ];
    $("#simSummary").innerHTML = cards.map((k) => `<div class="kpi ${k.cls}"><b>${k.v}</b><span>${k.t}</span></div>`).join("");
  }

  function exportSimCsv() {
    const run = sim.runs[sim.active];
    if (!run) { $("#simStatus").textContent = "Run a simulation first."; return; }
    const rows = [["workload", "capacity", "requests", "policy", "hits", "misses", "evictions", "hit_ratio"]].concat(
      run.results.map((r) => [run.title, run.cap, run.n, r.p, r.hits, r.misses, r.evictions, (r.hits / run.n).toFixed(6)]));
    const blob = new Blob([rows.map((r) => r.map((x) => `"${String(x).replace(/"/g, '""')}"`).join(",")).join("\r\n")], { type: "text/csv" });
    const a = document.createElement("a");
    a.href = URL.createObjectURL(blob); a.download = `cachesim_run${run.id}.csv`; a.click();
    setTimeout(() => URL.revokeObjectURL(a.href), 2000);
  }

  // What the assistant should know about the screen right now (kept short to save tokens).
  window.CacheSimApp = {
    context() {
      const names = { overview: "Home (results)", sim: "Simulator", race: "Live Race", lab: "AI Lab", patterns: "Workloads", how: "How it works" };
      const parts = [`The user is on the ${names[current.tab] || current.tab} page.`];
      const run = sim.runs[sim.active];
      if (run) parts.push(`Latest simulator run: ${run.title}, cache ${run.cap}, ${run.n} requests, ${run.universe} keys` + (run.kind === "zipf" ? `, skew ${run.alpha}` : "") + `. Hit %: ` +
        run.results.map((r) => `${LABEL(r.p)} ${(100 * r.hits / run.n).toFixed(1)}`).join(", ") + ".");
      if (current.tab === "race" && race.seq.length) {
        const c = race.cur;
        parts.push(`Live Race: cache ${race.cap}, requests [${race.seq.slice(0, 40).join(" ")}${race.seq.length > 40 ? " ..." : ""}], at step ${c + 1}. Hits so far: ` +
          E.POLICIES.map((p) => `${LABEL(p)} ${c < 0 ? 0 : race.runs[p].steps[c].hits}`).join(", ") + ".");
      }
      if (current.tab === "lab") parts.push(`AI Lab sliders: recency ${lab.w[0]}, frequency ${lab.w[1]}, age ${lab.w[2]}, gap ${lab.w[3]}; average hit ratio ${$("#labAvg").textContent}.`);
      return parts.join(" ");
    },
    showTab,
  };

  // ------------------------------------------------------------ live race
  const race = { seq: [], cap: 3, runs: {}, cur: -1, timer: null, enabled: new Set(E.POLICIES) };
  let rng = mulberry32(7);

  function initRace() {
    const sel = $("#scenario");
    sel.innerHTML = D.scenarios.map((s) => `<option value="${s.id}">${esc(s.title)}</option>`).join("") + '<option value="custom">Your own requests</option>';
    sel.addEventListener("change", () => { if (sel.value !== "custom") loadScenario(sel.value); });
    $("#policyToggles").innerHTML = E.POLICIES.map((p) => `<label style="--c:${COLORS[p]}"><input type="checkbox" data-p="${p}" checked><span class="dot"></span>${LABEL(p)}</label>`).join("");
    $$("#policyToggles input").forEach((cb) => cb.addEventListener("change", () => {
      cb.checked ? race.enabled.add(cb.dataset.p) : race.enabled.delete(cb.dataset.p);
      renderRace();
    }));
    $("#cap").addEventListener("input", () => { $("#capVal").textContent = $("#cap").value; });
    $("#cap").addEventListener("change", () => applyCustom());
    $("#applySeq").addEventListener("click", () => applyCustom());
    $("#seq").addEventListener("keydown", (e) => { if (e.key === "Enter" && !e.shiftKey) { e.preventDefault(); applyCustom(); } });
    $$("[data-gen]").forEach((b) => b.addEventListener("click", () => { $("#seq").value = makePattern(b.dataset.gen, +$("#cap").value).join(" "); applyCustom(); }));
    $("#btnPlay").addEventListener("click", togglePlay);
    $("#btnStep").addEventListener("click", () => { stop(); step(1); });
    $("#btnBack").addEventListener("click", () => { stop(); step(-1); });
    $("#btnReset").addEventListener("click", () => { stop(); race.cur = -1; renderRace(); });
    $("#speed").addEventListener("input", () => { if (race.timer) { stop(); play(); } });
    document.addEventListener("keydown", (e) => {
      if (!$("#race").classList.contains("active") || e.target.matches("textarea, input, select")) return;
      if (e.key === " ") { e.preventDefault(); togglePlay(); }
      if (e.key === "ArrowRight") { stop(); step(1); }
      if (e.key === "ArrowLeft") { stop(); step(-1); }
    });
    loadScenario(D.scenarios[0].id);
  }

  function loadScenario(id) {
    const s = D.scenarios.find((x) => x.id === id);
    $("#scenario").value = id;
    $("#scenarioDesc").textContent = s.description;
    $("#cap").value = s.capacity; $("#capVal").textContent = s.capacity;
    $("#seq").value = s.requests.join(" ");
    setRace(s.requests.slice(), s.capacity);
  }

  function applyCustom() {
    const tokens = $("#seq").value.trim().split(/[\s,]+/).filter(Boolean);
    const seq = tokens.map(Number);
    if (!seq.length || seq.some((x) => !Number.isInteger(x) || x < 0 || x > 99999)) {
      $("#seqError").textContent = "Please type whole numbers between 0 and 99999, separated by spaces."; return;
    }
    if (seq.length > 200) { $("#seqError").textContent = "Please use 200 requests or fewer."; return; }
    $("#seqError").textContent = "";
    $("#scenario").value = "custom";
    $("#scenarioDesc").textContent = "Your own request list. Every policy sees exactly the same requests.";
    setRace(seq, +$("#cap").value);
  }

  function setRace(seq, cap) {
    stop();
    race.seq = seq; race.cap = cap; race.cur = -1; race.runs = {};
    for (const p of E.POLICIES) race.runs[p] = E.simulate(p, seq, cap, D.weights, true);
    renderRace();
  }

  function makePattern(kind, cap) {
    const out = [];
    if (kind === "loop") { for (let r = 0; r < 5; r++) for (let k = 1; k <= cap + 1; k++) out.push(k); }
    else if (kind === "scan") {
      const hot = Array.from({ length: Math.max(1, cap - 1) }, (_, i) => i + 1);
      for (let r = 0; r < 3; r++) out.push(...hot);
      for (let k = 0; k < cap + 2; k++) out.push(100 + k);
      for (let r = 0; r < 2; r++) out.push(...hot);
    } else if (kind === "phase") {
      const n = Math.max(1, cap - 1);
      for (let r = 0; r < 4; r++) for (let k = 1; k <= n; k++) out.push(k);
      for (let r = 0; r < 4; r++) for (let k = 1; k <= n; k++) out.push(10 + k);
    } else if (kind === "zipf") {
      const N = 15, cdf = []; let sum = 0;
      for (let k = 1; k <= N; k++) { sum += 1 / k; cdf.push(sum); }
      for (let i = 0; i < 40; i++) { const u = rng() * sum; out.push(cdf.findIndex((c) => c >= u) + 1); }
    } else {
      for (let i = 0; i < 30; i++) out.push(1 + Math.floor(rng() * cap * 2));
    }
    return out;
  }

  function step(d) {
    race.cur = Math.max(-1, Math.min(race.seq.length - 1, race.cur + d));
    renderRace();
    if (race.cur >= race.seq.length - 1) stop();
  }
  function play() {
    if (race.cur >= race.seq.length - 1) race.cur = -1;
    const delay = 1250 - 110 * +$("#speed").value;
    race.timer = setInterval(() => step(1), delay);
    $("#btnPlay").innerHTML = "&#10074;&#10074; Pause";
    step(1);
  }
  function stop() { clearInterval(race.timer); race.timer = null; $("#btnPlay").innerHTML = "&#9654; Play"; }
  function togglePlay() { race.timer ? stop() : play(); }

  function renderRace() {
    const n = race.seq.length, cur = race.cur;
    $("#progressBar").style.width = (n ? (100 * (cur + 1)) / n : 0) + "%";
    $("#stepLabel").textContent = cur < 0 ? `Ready · ${n} requests` : `Request ${cur + 1} of ${n}`;
    $("#ribbon").innerHTML = race.seq.map((k, i) => `<span class="chip ${i < cur ? "done" : ""} ${i === cur ? "now" : ""}" data-i="${i}">${k}</span>`).join("");
    $$("#ribbon .chip").forEach((c) => c.addEventListener("click", () => { stop(); race.cur = +c.dataset.i; renderRace(); }));

    const enabled = E.POLICIES.filter((p) => race.enabled.has(p));
    const hitsAt = (p) => (cur < 0 ? 0 : race.runs[p].steps[cur].hits);
    const ranked = enabled.slice().sort((a, b) => hitsAt(b) - hitsAt(a));
    const maxHits = Math.max(1, ...enabled.map((p) => race.runs[p].hits));
    $("#standings").innerHTML = ranked.map((p) => `<div class="srow" style="--c:${COLORS[p]}"><span><span class="dot"></span> ${LABEL(p)}</span>
      <div class="bar"><i style="width:${(100 * hitsAt(p)) / maxHits}%"></i></div><span class="num">${hitsAt(p)} hits</span></div>`).join("");

    $("#lanes").innerHTML = enabled.map((p) => laneHtml(p)).join("");
    renderAiExplain();
  }

  function laneHtml(p) {
    const run = race.runs[p], cur = race.cur, s = cur >= 0 ? run.steps[cur] : null;
    const slots = s ? s.slots : new Array(race.cap).fill(null);
    const hits = s ? s.hits : 0, seen = cur + 1;
    const slotHtml = slots.map((k, i) => {
      let cls = "slot" + (k !== null ? " full" : "");
      if (s && i === s.slot) cls += s.hit ? " hit" : " new";
      const gone = s && !s.hit && s.victim !== null && i === s.slot ? `<span class="gone">−${s.victim}</span>` : "";
      return `<div class="${cls}">${k === null ? "" : k}${gone}</div>`;
    }).join("");
    let status = '<span class="muted">waiting for the first request</span>';
    if (s) status = s.hit ? `<span class="status hit">HIT ${s.k}</span>` :
      `<span class="status miss">MISS ${s.k}</span><span class="muted">${s.victim !== null ? "threw out <b>" + s.victim + "</b>" : "free slot used"}</span>`;
    const hist = run.steps.map((st, i) => `<i class="${i <= cur ? (st.hit ? "h" : "m") : ""}${i === cur ? " cur" : ""}"></i>`).join("");
    const extra = p === "Adaptive" && s ? ` · following ${s.leader}` : "";
    const flash = s ? (s.hit ? "flash-hit" : "flash-miss") : "";
    return `<div class="lane ${flash}" style="--c:${COLORS[p]}">
      <div class="lane-head"><span class="dot"></span><b>${LABEL(p)}</b><span class="rule">${RULE[p]}${extra}</span>
      <span class="score">${hits}/${seen} ${seen ? "· " + pct(hits / seen, 0) : ""}</span></div>
      <div class="slots">${slotHtml}</div>
      <div class="lane-foot">${status}</div>
      <div class="history" title="green = hit, red = miss">${hist}</div></div>`;
  }

  function renderAiExplain() {
    const box = $("#aiExplain");
    if (!race.enabled.has("AI")) { box.style.display = "none"; return; }
    box.style.display = "";
    const s = race.cur >= 0 ? race.runs.AI.steps[race.cur] : null;
    let html = '<h2>Why did the AI choose that item?</h2>';
    if (!s || !s.ai) {
      html += `<p class="muted">${!s ? "Press Play. When the AI's cache is full, this panel shows every item's score and which one it threw out." :
        s.hit ? "This request was a hit, so the AI did not need to throw anything out." : "There was still a free slot, so nothing was thrown out."}</p>`;
      box.innerHTML = html; return;
    }
    const rows = s.ai.slice().sort((a, b) => b.s - a.s);
    html += `<p class="muted">Request <b>${s.k}</b> missed and the cache was full. The AI scored every item with its learned weights and threw out <b>${s.victim}</b>, the highest score.</p>`;
    html += "<table><thead><tr><th>Item</th><th>Recency</th><th>Frequency</th><th>Age</th><th>Gap</th><th>Score</th><th></th></tr></thead><tbody>";
    for (const r of rows) {
      const bars = r.f.map((f) => `<td><div class="fbar"><i style="width:${Math.max(2, 70 * f)}px"></i><span>${f.toFixed(2)}</span></div></td>`).join("");
      html += `<tr class="${r.k === s.victim ? "victim" : ""}"><td><b>${r.k}</b></td>${bars}<td><b>${r.s.toFixed(3)}</b></td><td>${r.k === s.victim ? '<span class="tag">thrown out</span>' : ""}</td></tr>`;
    }
    html += `</tbody></table><p class="muted small" style="margin-top:8px">Weights: ${D.featureNames.map((f, i) => `${f} ${D.weights[i] >= 0 ? "+" : ""}${D.weights[i].toFixed(2)}`).join(" · ")}</p>`;
    box.innerHTML = html;
  }

  // ------------------------------------------------------------ AI lab
  const lab = { w: D.weights.slice(), chart: null, best: null, running: false };
  const LAB_FEATURES = [
    ["Recency", "Time since the item was last used"], ["Frequency", "How many times it was used"],
    ["Age", "How long it has been in the cache"], ["Gap", "Average time between its uses"],
  ];
  const PRESETS = { "Learned by GA": D.weights, "Like LRU": [1, 0, 0, 0], "Like FIFO": [0, 0, 1, 0], "Like LFU": [0.02, -1, 0, 0] };

  function initLab() {
    $("#sliders").innerHTML = LAB_FEATURES.map(([n, d], i) => `<div class="slider-row"><b>${n}</b><span class="val" id="wv${i}"></span><p>${d}</p>
      <input type="range" min="-1" max="1" step="0.01" id="w${i}" aria-label="${n} weight"></div>`).join("");
    LAB_FEATURES.forEach((_, i) => $("#w" + i).addEventListener("input", (e) => { lab.w[i] = +e.target.value; syncSliders(); scheduleLab(); }));
    $("#presets").innerHTML = Object.keys(PRESETS).map((k) => `<button data-preset="${esc(k)}">${esc(k)}</button>`).join("") + '<button data-preset="Random">Random</button>';
    $$("#presets button").forEach((b) => b.addEventListener("click", () => {
      lab.w = b.dataset.preset === "Random" ? [0, 1, 2, 3].map(() => +(rng() * 2 - 1).toFixed(2)) : PRESETS[b.dataset.preset].slice();
      syncSliders(); updateLab();
    }));
    const names = D.traces.map((t) => t.name).concat(["Average"]);
    const base = (p) => { const v = D.traces.map((t) => (100 * t.hits[p]) / t.keys.length); return v.concat([v.reduce((a, b) => a + b, 0) / v.length]); };
    lab.chart = chart("chartLab", {
      type: "bar",
      data: {
        labels: names,
        datasets: [
          { label: "Your rule", data: names.map(() => 0), backgroundColor: "#FF6B47", borderRadius: 4 },
          { label: "AI (learned by GA)", data: base("AI"), backgroundColor: "#FF6B4755", borderColor: "#FF6B47", borderWidth: 1, borderRadius: 4 },
          { label: "LRU", data: base("LRU"), backgroundColor: COLORS.LRU + "cc", borderRadius: 4 },
          { label: "LFU", data: base("LFU"), backgroundColor: COLORS.LFU + "cc", borderRadius: 4 },
          { label: "Best possible", data: base("Belady-OPT"), backgroundColor: "#E8EDF233", borderColor: "#E8EDF2", borderWidth: 1, borderRadius: 4 },
        ],
      },
      options: { animation: { duration: 250 }, scales: { y: { beginAtZero: true, max: 100, title: { display: true, text: "hit ratio (%)" } }, x: { grid: { display: false } } },
        plugins: { tooltip: { callbacks: { label: (c) => ` ${c.dataset.label}: ${c.parsed.y.toFixed(1)}%` } } } },
    });
    lab.base = { AI: base("AI"), LRU: base("LRU"), LFU: base("LFU"), OPT: base("Belady-OPT") };
    lab.evo = chart("chartEvo", {
      type: "line",
      data: { labels: [], datasets: [
        { label: "Best rule", data: [], borderColor: COLORS.AI, borderWidth: 3, pointRadius: 3, tension: 0.25 },
        { label: "Average rule", data: [], borderColor: COLORS.LRU, borderWidth: 2, pointRadius: 2, tension: 0.25 },
      ] },
      options: { animation: { duration: 200 }, scales: { x: { title: { display: true, text: "generation" } }, y: { suggestedMin: 30, suggestedMax: 60, title: { display: true, text: "average hit ratio (%)" } } } },
    });
    $("#btnEvolve").addEventListener("click", evolve);
    $("#btnApplyBest").addEventListener("click", () => { if (lab.best) { lab.w = lab.best.slice(); syncSliders(); updateLab(); } });
    syncSliders(); updateLab();
  }

  function syncSliders() { lab.w.forEach((v, i) => { $("#w" + i).value = v; $("#wv" + i).textContent = (v >= 0 ? "+" : "") + (+v).toFixed(2); }); }
  let labTimer = null;
  function scheduleLab() { clearTimeout(labTimer); labTimer = setTimeout(updateLab, 40); }
  function evalWeights(w, limit) {
    return D.traces.map((t) => {
      const keys = limit ? t.keys.slice(0, limit) : t.keys;
      return E.simulate("AI", keys, t.capacity, w, false).hits / keys.length;
    });
  }
  function updateLab() {
    const v = evalWeights(lab.w).map((x) => 100 * x), avg = v.reduce((a, b) => a + b, 0) / v.length;
    if (lab.chart) { lab.chart.data.datasets[0].data = v.concat([avg]); lab.chart.update(); }
    $("#labAvg").textContent = avg.toFixed(1) + "%";
    const last = (a) => a[a.length - 1].toFixed(1) + "%";
    $("#labCompare").innerHTML = `<div>AI learned by GA <b>${last(lab.base.AI)}</b></div><div>LRU <b>${last(lab.base.LRU)}</b></div>
      <div>LFU <b>${last(lab.base.LFU)}</b></div><div>Best possible <b>${last(lab.base.OPT)}</b></div>`;
  }

  // A small genetic algorithm, the same recipe as the C++ PolicyTrainer.
  async function evolve() {
    if (lab.running) return;
    lab.running = true; $("#btnEvolve").disabled = true; $("#btnApplyBest").disabled = true;
    const r = mulberry32(2024), POP = 16, GENS = 12, LIMIT = 4000;
    const gauss = () => Math.sqrt(-2 * Math.log(1 - r())) * Math.cos(2 * Math.PI * r());
    const clamp = (x) => Math.max(-1, Math.min(1, x));
    const fit = (w) => { const v = evalWeights(w, LIMIT); return v.reduce((a, b) => a + b, 0) / v.length; };
    let pop = [[1, 0, 0, 0], [0, 0, 1, 0], [0.02, -1, 0, 0]];
    while (pop.length < POP) pop.push([0, 0, 0, 0].map(() => r() * 2 - 1));
    lab.evo.data.labels = []; lab.evo.data.datasets.forEach((d) => (d.data = []));
    let best = null, bestFit = -1;
    for (let g = 0; g < GENS; g++) {
      const scores = [];
      for (let i = 0; i < pop.length; i++) {
        scores.push(fit(pop[i]));
        $("#evoStatus").textContent = `Generation ${g + 1} of ${GENS} · testing rule ${i + 1} of ${POP} on 6 workloads…`;
        await new Promise((res) => setTimeout(res, 0));
      }
      const order = scores.map((s, i) => i).sort((a, b) => scores[b] - scores[a]);
      if (scores[order[0]] > bestFit) { bestFit = scores[order[0]]; best = pop[order[0]].slice(); }
      lab.evo.data.labels.push(g);
      lab.evo.data.datasets[0].data.push(100 * scores[order[0]]);
      lab.evo.data.datasets[1].data.push((100 * scores.reduce((a, b) => a + b, 0)) / scores.length);
      lab.evo.update();
      const sigma = 0.3 * Math.pow(0.02 / 0.3, g / (GENS - 1));
      const pick = () => { let b = Math.floor(r() * POP); for (let k = 1; k < 3; k++) { const c = Math.floor(r() * POP); if (scores[c] > scores[b]) b = c; } return pop[b]; };
      const next = [pop[order[0]].slice(), pop[order[1]].slice()];
      while (next.length < POP) {
        const a = pick(), b = pick();
        const child = r() < 0.9 ? a.map((x, i) => clamp(x + (r() * 1.5 - 0.25) * (b[i] - x))) : a.slice();
        next.push(child.map((x) => (r() < 0.1 ? clamp(x + sigma * gauss()) : x)));
      }
      pop = next;
    }
    lab.best = best.map((x) => +x.toFixed(2));
    $("#evoStatus").innerHTML = `Done. Best rule: ${D.featureNames.map((f, i) => `${f} <b>${lab.best[i] >= 0 ? "+" : ""}${lab.best[i].toFixed(2)}</b>`).join(", ")} · score ${(100 * bestFit).toFixed(1)}% on the first ${LIMIT.toLocaleString()} requests.`;
    lab.running = false; $("#btnEvolve").disabled = false; $("#btnApplyBest").disabled = false;
  }

  // ------------------------------------------------------------ workload patterns
  const ABOUT = {
    sequential: "Short runs of keys in a row, starting at random places, like reading files. Almost no reuse, so every policy struggles.",
    uniform: "Every key is equally likely. There is no pattern to learn, so all real policies score about the same.",
    zipf: "A few keys are very popular and most are rare, like web traffic. Counting uses (LFU) works well.",
    looping: "The same loop again and again, a bit bigger than the cache. LRU always throws out the next key needed, so it gets 0%.",
    scan: "A small hot set, interrupted by long one-time scans. LRU lets the scan push out the hot data.",
    phase: "The popular keys change halfway through. LFU is slow to forget the old ones.",
  };
  function initPatterns() {
    const grid = $("#patternGrid");
    grid.innerHTML = D.traces.map((t, i) => {
      const key = Object.keys(ABOUT).find((k) => t.name.startsWith(k)) || "";
      const w = workloads.find((x) => x === t.name);
      let win = "";
      if (w) {
        const online = policies.filter((p) => p !== "Belady-OPT");
        const best = online.reduce((a, b) => (R(w, b).hr > R(w, a).hr ? b : a));
        win = `<div class="winner"><span class="pill" style="--c:${COLORS[best]}">Winner: ${LABEL(best)} ${pct(R(w, best).hr)}</span>
          <span class="pill" style="--c:${COLORS.AI}">AI ${pct(R(w, "AI").hr)}</span><span class="pill" style="--c:${COLORS.LRU}">LRU ${pct(R(w, "LRU").hr)}</span></div>`;
      }
      return `<div class="card"><h2>${esc(t.name)}</h2><p class="muted small">${ABOUT[key] || ""}</p><canvas id="pat${i}" width="760" height="300"></canvas>${win}</div>`;
    }).join("");
    D.traces.forEach((t, i) => drawPattern($("#pat" + i), t.keys.slice(0, 2000), i));
  }
  function drawPattern(canvas, keys, i) {
    const ctx = canvas.getContext("2d"), W = canvas.width, H = canvas.height;
    const uniq = [...new Set(keys)].sort((a, b) => a - b), rank = new Map(uniq.map((k, j) => [k, j]));
    const palette = ["#4EA1E0", "#9B7BD1", "#FF6B47", "#F2A541", "#2EC4A6", "#E8EDF2"];
    ctx.fillStyle = palette[i % palette.length];
    keys.forEach((k, x) => {
      const y = uniq.length > 1 ? rank.get(k) / (uniq.length - 1) : 0.5;
      ctx.globalAlpha = 0.75;
      ctx.fillRect((x / keys.length) * W, H - 8 - y * (H - 16), 2.2, 2.2);
    });
  }

  // ------------------------------------------------------------ helpers
  function mulberry32(a) {
    return function () { a |= 0; a = (a + 0x6d2b79f5) | 0; let t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; };
  }

  // ------------------------------------------------------------ start
  // Deep links: index.html#race, #race/6 (jump to request 6), #lab, #patterns, #how
  let start = "overview", jump = null;
  try { start = localStorage.getItem("cachesim.tab") || "overview"; } catch (e) { /* ignore */ }
  if (location.hash.length > 1) { const [t, n] = location.hash.slice(1).split("/"); start = t; jump = n ? +n : null; }
  showTab($("#" + start) ? start : "overview", false);
  // The hash matches a section id, so the browser would jump past the header: undo that.
  if ("scrollRestoration" in history) history.scrollRestoration = "manual";
  window.scrollTo(0, 0); $("#main").scrollTo(0, 0);
  requestAnimationFrame(() => { window.scrollTo(0, 0); $("#main").scrollTo(0, 0); });
  if (start === "race" && jump) { race.cur = Math.min(race.seq.length - 1, jump - 1); renderRace(); }
  setTimeout(engineCheck, 150);
})();
