// engine.js - the eviction policies again, in JavaScript, for the browser.
// They mirror the C++ classes line by line. On page load, app.js replays the
// traces exported by the C++ program and checks that every hit count matches.
"use strict";
(function (global) {

  class FIFO {
    constructor() { this.order = []; }
    get name() { return "FIFO"; }
    onInsert(k) { this.order.push(k); }
    onAccess() {}
    onRemove(k) { const i = this.order.indexOf(k); if (i >= 0) this.order.splice(i, 1); }
    selectVictim() { return this.order[0]; }
  }

  class LRU {
    constructor() { this.order = []; } // front = most recently used
    get name() { return "LRU"; }
    onInsert(k) { this.order.unshift(k); }
    onAccess(k) { const i = this.order.indexOf(k); if (i > 0) { this.order.splice(i, 1); this.order.unshift(k); } }
    onRemove(k) { const i = this.order.indexOf(k); if (i >= 0) this.order.splice(i, 1); }
    selectVictim() { return this.order[this.order.length - 1]; }
  }

  // Same rule as the C++ frequency buckets: lowest count first, and inside a
  // count the key that reached that count earliest (LRU tie-break).
  class LFU {
    constructor() { this.nodes = new Map(); this.tick = 0; }
    get name() { return "LFU"; }
    onInsert(k) { this.nodes.set(k, { f: 1, t: ++this.tick }); }
    onAccess(k) { const n = this.nodes.get(k); if (n) { n.f++; n.t = ++this.tick; } }
    onRemove(k) { this.nodes.delete(k); }
    selectVictim() {
      let best = null, bf = Infinity, bt = Infinity;
      for (const [k, n] of this.nodes) if (n.f < bf || (n.f === bf && n.t < bt)) { best = k; bf = n.f; bt = n.t; }
      return best;
    }
  }

  // Belady's optimal: evicts the key used farthest in the future (needs the trace).
  class Belady {
    constructor(trace) {
      this.next = new Array(trace.length); const seen = new Map();
      for (let i = trace.length - 1; i >= 0; i--) { this.next[i] = seen.has(trace[i]) ? seen.get(trace[i]) : Infinity; seen.set(trace[i], i); }
      this.trace = trace; this.cursor = 0; this.nextOf = new Map();
    }
    get name() { return "Belady-OPT"; }
    onInsert(k) { this.nextOf.set(k, this.next[this.cursor++]); }
    onAccess(k) { this.nextOf.set(k, this.next[this.cursor++]); }
    onRemove(k) { this.nextOf.delete(k); }
    selectVictim() {
      let best = null, bn = -1;
      for (const [k, n] of this.nextOf) if (n > bn || (n === bn && k > best)) { best = k; bn = n; }
      return best;
    }
  }

  // The learned policy: score = w . features, evict the highest score.
  class AI {
    constructor(w) { this.w = w.slice(); this.entries = []; this.index = new Map(); this.clock = 0; }
    get name() { return "AI"; }
    onInsert(k) { this.clock++; this.index.set(k, this.entries.length); this.entries.push({ k, ins: this.clock, last: this.clock, n: 1 }); }
    onAccess(k) { this.clock++; const i = this.index.get(k); if (i !== undefined) { const e = this.entries[i]; e.last = this.clock; e.n++; } }
    onRemove(k) {
      const pos = this.index.get(k); if (pos === undefined) return;
      const last = this.entries.length - 1;
      if (pos !== last) { this.entries[pos] = this.entries[last]; this.index.set(this.entries[pos].k, pos); }
      this.entries.pop(); this.index.delete(k);
    }
    raw(e) { const now = this.clock + 1, age = now - e.ins; return [now - e.last, Math.log1p(e.n), age, age / e.n]; }
    // Every resident's normalised features and score (shown in the UI).
    scores() {
      const raws = this.entries.map(e => this.raw(e)), mx = [0, 0, 0, 0];
      for (const r of raws) for (let j = 0; j < 4; j++) if (r[j] > mx[j]) mx[j] = r[j];
      return this.entries.map((e, i) => {
        const f = raws[i].map((x, j) => (mx[j] > 0 ? x / mx[j] : 0));
        return { k: e.k, f, s: this.w[0] * f[0] + this.w[1] * f[1] + this.w[2] * f[2] + this.w[3] * f[3] };
      });
    }
    // Same maths as scores(), without allocating: this runs thousands of times in the AI Lab.
    selectVictim() {
      const E = this.entries, now = this.clock + 1, w = this.w;
      let m0 = 0, m1 = 0, m2 = 0, m3 = 0;
      for (let i = 0; i < E.length; i++) {
        const e = E[i], age = now - e.ins, r0 = now - e.last, r1 = Math.log1p(e.n), r3 = age / e.n;
        if (r0 > m0) m0 = r0; if (r1 > m1) m1 = r1; if (age > m2) m2 = age; if (r3 > m3) m3 = r3;
      }
      let best = -Infinity, bk = E[0].k;
      for (let i = 0; i < E.length; i++) {
        const e = E[i], age = now - e.ins;
        const s = w[0] * (m0 > 0 ? (now - e.last) / m0 : 0) + w[1] * (m1 > 0 ? Math.log1p(e.n) / m1 : 0) +
                  w[2] * (m2 > 0 ? age / m2 : 0) + w[3] * (m3 > 0 ? (age / e.n) / m3 : 0);
        if (s > best) { best = s; bk = e.k; }
      }
      return bk;
    }
  }

  class Shadow {
    constructor(cap, policy) { this.cap = cap; this.p = policy; this.keys = new Set(); }
    request(k) {
      if (this.keys.has(k)) { this.p.onAccess(k); return true; }
      if (this.keys.size >= this.cap) { const v = this.p.selectVictim(); this.keys.delete(v); this.p.onRemove(v); }
      this.keys.add(k); this.p.onInsert(k); return false;
    }
  }

  // Runs FIFO, LRU and LFU side by side and follows the one with the best recent score.
  class Adaptive {
    constructor(cap, window = 1000) {
      this.window = window; this.leader = 1; this.switches = 0;
      const mk = () => [new FIFO(), new LRU(), new LFU()];
      const real = mk(), shadow = mk();
      this.experts = real.map((r, i) => ({ real: r, shadow: new Shadow(cap, shadow[i]), recent: [], hits: 0 }));
    }
    get name() { return "Adaptive"; }
    get leaderName() { return this.experts[this.leader].real.name; }
    observe(k) {
      for (const e of this.experts) {
        const h = e.shadow.request(k); e.recent.push(h); if (h) e.hits++;
        if (e.recent.length > this.window) { if (e.recent.shift()) e.hits--; }
      }
      let best = this.leader;
      this.experts.forEach((e, i) => { if (e.hits > this.experts[best].hits) best = i; });
      if (best !== this.leader) { this.leader = best; this.switches++; }
    }
    onInsert(k) { for (const e of this.experts) e.real.onInsert(k); this.observe(k); }
    onAccess(k) { for (const e of this.experts) e.real.onAccess(k); this.observe(k); }
    onRemove(k) { for (const e of this.experts) e.real.onRemove(k); }
    selectVictim() { return this.experts[this.leader].real.selectVictim(); }
  }

  function make(name, trace, cap, weights) {
    switch (name) {
      case "FIFO": return new FIFO();
      case "LRU": return new LRU();
      case "LFU": return new LFU();
      case "AI": return new AI(weights);
      case "Adaptive": return new Adaptive(cap);
      case "Belady-OPT": return new Belady(trace);
    }
    throw new Error("unknown policy " + name);
  }

  // Replays a trace. With record=true it also keeps every step for animation:
  // slot layout (a key keeps its slot until evicted), victim, AI scores.
  // opts.samples = N records the running hit ratio at N evenly spaced points.
  function simulate(name, trace, cap, weights, record, opts) {
    const p = make(name, trace, cap, weights), set = new Set();
    const slots = new Array(cap).fill(null), steps = record ? [] : null;
    const every = opts && opts.samples ? Math.max(1, Math.floor(trace.length / opts.samples)) : 0, curve = every ? [] : null;
    let hits = 0, evictions = 0;
    for (let i = 0; i < trace.length; i++) {
      const k = trace[i];
      if (every && i > 0 && i % every === 0) curve.push(hits / i);
      if (set.has(k)) {
        p.onAccess(k); hits++;
        if (record) steps.push({ k, hit: true, victim: null, slot: slots.indexOf(k), slots: slots.slice(), hits, leader: p.leaderName });
        continue;
      }
      let victim = null, ai = null, slot;
      if (set.size >= cap) {
        if (record && p instanceof AI) ai = p.scores();
        victim = p.selectVictim(); set.delete(victim); p.onRemove(victim); evictions++;
      }
      set.add(k); p.onInsert(k);
      if (record) {
        slot = victim === null ? slots.indexOf(null) : slots.indexOf(victim);
        slots[slot] = k;
        steps.push({ k, hit: false, victim, slot, slots: slots.slice(), hits, ai, leader: p.leaderName });
      }
    }
    if (curve) curve.push(trace.length ? hits / trace.length : 0);
    return { hits, misses: trace.length - hits, evictions, steps, curve };
  }

  // ---- request patterns (same ideas as the C++ workload classes) ----
  function rng32(seed) {
    let a = seed >>> 0;
    return function () { a = (a + 0x6d2b79f5) | 0; let t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; };
  }
  const WORKLOADS = {
    zipf: { title: "Popular keys (Zipf)", about: "A few keys are very popular, most are rare, like web traffic." },
    looping: { title: "Loop bigger than the cache", about: "The same loop again and again, 20% bigger than the cache." },
    scan: { title: "Hot set + one-time scans", about: "A small hot set, interrupted by long scans of keys used once." },
    phase: { title: "Hot set moves", about: "The popular keys change halfway through the run." },
    uniform: { title: "Uniform random", about: "Every key equally likely: no pattern to learn." },
    sequential: { title: "Sequential runs", about: "Short runs of keys in a row, like reading files." },
  };
  function generate(kind, n, cap, opt) {
    const o = Object.assign({ alpha: 0.9, universe: 1000, seed: 42 }, opt || {});
    const r = rng32(o.seed), out = new Array(n), U = Math.max(1, o.universe), hot = Math.max(1, Math.floor(cap * 0.8) + 1);
    if (kind === "zipf") {
      const cdf = new Float64Array(U); let sum = 0;
      for (let k = 0; k < U; k++) { sum += 1 / Math.pow(k + 1, o.alpha); cdf[k] = sum; }
      for (let i = 0; i < n; i++) { const x = r() * sum; let lo = 0, hi = U - 1; while (lo < hi) { const m = (lo + hi) >> 1; if (cdf[m] < x) lo = m + 1; else hi = m; } out[i] = lo; }
    } else if (kind === "looping") {
      const L = cap + Math.floor(cap / 5) + 1; for (let i = 0; i < n; i++) out[i] = i % L;
    } else if (kind === "scan") {
      const burst = 500, scan = cap * 2; let next = 1000000;
      for (let i = 0; i < n; i++) out[i] = (i % (burst + scan)) < burst ? Math.floor(r() * hot) : next++;
    } else if (kind === "phase") {
      for (let i = 0; i < n; i++) { const ph = Math.floor((i * 2) / n); out[i] = r() < 0.9 ? (ph * hot + Math.floor(r() * hot)) % U : Math.floor(r() * U); }
    } else if (kind === "sequential") {
      let start = 0; for (let i = 0; i < n; i++) { if (i % 16 === 0) start = Math.floor(r() * U); out[i] = (start + (i % 16)) % U; }
    } else {
      const half = Math.max(1, Math.floor(U / 2)); for (let i = 0; i < n; i++) out[i] = Math.floor(r() * half);
    }
    return out;
  }

  global.CacheEngine = { POLICIES: ["FIFO", "LRU", "LFU", "AI", "Adaptive", "Belady-OPT"], simulate, make, generate, WORKLOADS, rng32 };
})(typeof window !== "undefined" ? window : globalThis);
