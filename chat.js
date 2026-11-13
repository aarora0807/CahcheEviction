// chat.js - "Ask CacheSim": a small assistant panel powered by Claude Haiku 5.5.
// Cost-saving settings: Claude Haiku 5.5, effort "low", thinking off, at most
// 500 output tokens per answer, and only the last 6 messages are sent.
// The API key is typed in by the user and kept only in this app's local
// storage on their PC. It is never written into the project folder.
"use strict";
(function () {
  const MODEL = "claude-haiku-5-5";
  const MAX_TOKENS = 500;          // short answers keep each reply cheap
  const HISTORY = 6;               // messages sent per request (older ones are dropped)
  const PRICE_IN = 0.10 / 1e6;     // USD per input token (Haiku 5.5)
  const PRICE_OUT = 0.50 / 1e6;    // USD per output token (Haiku 5.5)
  const KEY_STORE = "cachesim.apiKey";
  const UI_STORE = "cachesim.chatHidden";

  const $ = (s) => document.querySelector(s);
  const app = $("#app"), log = $("#chatLog"), input = $("#chatText");
  const state = { history: [], busy: false, inTok: 0, outTok: 0 };
  const D = window.SIM_DATA;

  const store = {
    get(k) { try { return localStorage.getItem(k) || ""; } catch (e) { return ""; } },
    set(k, v) { try { localStorage.setItem(k, v); return true; } catch (e) { return false; } },
    del(k) { try { localStorage.removeItem(k); } catch (e) { /* ignore */ } },
  };

  // ---------------------------------------------------------------- prompt
  // Built once from the C++ results; short on purpose (fewer input tokens).
  function projectFacts() {
    if (!D) return "";
    const pct = (x) => (100 * x).toFixed(1);
    const ws = [...new Set(D.results.map((r) => r.w))];
    const lines = ws.map((w) => {
      const rs = D.results.filter((r) => r.w === w && r.p !== "AI-K8");
      return `${w}: ` + rs.map((r) => `${r.p === "Belady-OPT" ? "best-possible" : r.p} ${pct(r.hr)}`).join(", ");
    });
    const g = (p) => (D.generalization || []).find((x) => x.p === p);
    const gen = g("AI") && g("LRU") ? `Unseen test workloads: AI ${pct(g("AI").test)}% vs LRU ${pct(g("LRU").test)}% (AI train ${pct(g("AI").train)}%).` : "";
    return [
      `Hit % with cache ${D.settings.capacity}, ${D.settings.requests} requests:`, ...lines, gen,
      `Learned AI weights: recency ${D.weights[0].toFixed(2)}, frequency ${D.weights[1].toFixed(2)}, age ${D.weights[2].toFixed(2)}, gap ${D.weights[3].toFixed(2)}.`,
    ].join("\n");
  }
  const FACTS = projectFacts();

  function systemPrompt() {
    const ctx = window.CacheSimApp ? window.CacheSimApp.context() : "";
    return `You are the help assistant inside "CacheSim Studio", a student's Object-Oriented Programming project: a C++17 cache eviction simulator with an AI eviction policy.
Write in simple, friendly English for a student. Keep answers short (under 120 words) unless the user asks for more. Use short bullet points for lists. Do not include internal or system XML tags in your response.

How the project works:
- A cache holds a fixed number of items. A hit means the item was already in the cache; a miss means it was not. When the cache is full, the eviction policy picks the item to throw out.
- Policies: FIFO (oldest out), LRU (least recently used out), LFU (least used out), AI (scores every item with 4 features - recency, frequency, age, gap - and evicts the highest score; the weights were learned by a genetic algorithm: 40 candidates, 60 generations, tournament selection, elitism, blend crossover, Gaussian mutation), AI-K8 (AI that checks only 8 random items, faster), Adaptive (follows whichever of FIFO/LRU/LFU is winning, using shadow caches), Belady-OPT (knows the future, the best possible; only for comparison).
- OOP design: Strategy (EvictionPolicy interface), Factory (PolicyFactory, WorkloadFactory), Template Method (WorkloadGenerator), Observer (CacheListener for metrics, tracer and the app export). RAII with unique_ptr, no raw new/delete, 43 unit tests.
- Speed: FIFO/LRU/LFU are O(1) per request; the AI is O(n) per eviction; AI-K8 is O(8).
- App pages: Home (results), Simulator (run your own experiments), Live Race (step-by-step animation), AI Lab (weight sliders and an in-app genetic algorithm), Workloads, How it works.

Results from the C++ simulator:
${FACTS}

Right now: ${ctx}`;
  }

  // ---------------------------------------------------------------- rendering
  function esc(s) { return String(s).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c])); }
  // Tiny, safe markdown: escape first, then **bold**, `code`, *italic*, bullet lists, paragraphs.
  function md(text) {
    const inline = (t) => esc(t).replace(/`([^`]+)`/g, "<code>$1</code>").replace(/\*\*([^*]+)\*\*/g, "<b>$1</b>").replace(/(^|[^*])\*([^*\n]+)\*/g, "$1<i>$2</i>");
    const out = []; let list = null;
    for (const raw of String(text).split("\n")) {
      const line = raw.trimEnd(), m = line.match(/^\s*(?:[-*•]|\d+\.)\s+(.*)$/);
      if (m) { if (!list) { list = []; } list.push(`<li>${inline(m[1])}</li>`); continue; }
      if (list) { out.push(`<ul>${list.join("")}</ul>`); list = null; }
      if (line.trim()) out.push(`<p>${inline(line)}</p>`);
    }
    if (list) out.push(`<ul>${list.join("")}</ul>`);
    return out.join("");
  }
  function bubble(cls, html) {
    const el = document.createElement("div");
    el.className = "msg " + cls; el.innerHTML = html;
    log.appendChild(el); log.scrollTop = log.scrollHeight;
    return el;
  }
  function updateUsage() {
    const total = state.inTok + state.outTok;
    $("#chatUsage").textContent = total ? `This session: ${total.toLocaleString()} tokens · about $${(state.inTok * PRICE_IN + state.outTok * PRICE_OUT).toFixed(4)}` : "";
  }
  function showWelcome() {
    log.innerHTML = "";
    const hasKey = !!store.get(KEY_STORE);
    bubble("bot welcome", hasKey
      ? "<p>Hi! I'm the CacheSim assistant. Ask me anything about caches, the policies, the AI, or the results on your screen.</p>"
      : `<p>Hi! I'm the CacheSim assistant, powered by <b>Claude Haiku 5.5</b>.</p><p>To chat, add your Anthropic API key:</p>
         <ol><li>Open <b>console.anthropic.com</b> and create an API key.</li><li>Click the <b>gear</b> above and paste it.</li><li>Click <b>Save key</b>.</li></ol>
         <p class="muted small" style="margin-top:8px">Everything else in CacheSim Studio works without a key.</p>`);
  }

  // ---------------------------------------------------------------- talking to Claude
  async function send(text) {
    text = String(text || "").trim();
    if (!text || state.busy) return;
    const key = store.get(KEY_STORE);
    if (!key) { openSettings(true); bubble("note", "Add your API key first (gear icon)."); return; }
    if (!window.AnthropicSDK || !window.AnthropicSDK.Anthropic) { bubble("error", "The chat library did not load. Check that web\\vendor\\anthropic-sdk.min.js exists."); return; }
    const Anthropic = window.AnthropicSDK.Anthropic;

    state.busy = true; $("#chatSend").disabled = true;
    bubble("user", esc(text));
    state.history.push({ role: "user", content: text });
    const out = bubble("bot", '<span class="typing"><i></i><i></i><i></i></span>');

    // Only the most recent messages are sent; the list must start with a user turn.
    let messages = state.history.slice(-HISTORY);
    while (messages.length && messages[0].role !== "user") messages = messages.slice(1);

    let answer = "";
    try {
      const client = new Anthropic({ apiKey: key, dangerouslyAllowBrowser: true, maxRetries: 1, timeout: 60000 });
      const stream = client.messages.stream({
        model: MODEL,
        max_tokens: MAX_TOKENS,
        thinking: { type: "disabled" },
        output_config: { effort: "low" },
        system: systemPrompt(),
        messages,
      });
      for await (const event of stream) {
        if (event.type === "content_block_delta" && event.delta.type === "text_delta") {
          answer += event.delta.text;
          out.innerHTML = md(answer);
          log.scrollTop = log.scrollHeight;
        }
      }
      const final = await stream.finalMessage();
      if (final.stop_reason === "refusal") answer = "Sorry, I can't help with that one.";
      if (!answer.trim()) answer = "(No answer was returned. Please try again.)";
      let note = "";
      if (final.stop_reason === "max_tokens") note = "Answer shortened to keep costs low. Type “continue” for more.";
      const u = final.usage || {};
      state.inTok += (u.input_tokens || 0) + (u.cache_read_input_tokens || 0) + (u.cache_creation_input_tokens || 0);
      state.outTok += u.output_tokens || 0;
      out.innerHTML = md(answer) + `<span class="meta">${note ? esc(note) + " · " : ""}${(u.input_tokens || 0) + (u.output_tokens || 0)} tokens</span>`;
      state.history.push({ role: "assistant", content: answer });
      updateUsage();
    } catch (err) {
      out.remove();
      state.history.pop(); // forget the unanswered question so turns keep alternating
      bubble("error", esc(explainError(err, Anthropic)));
    } finally {
      state.busy = false; $("#chatSend").disabled = false;
      log.scrollTop = log.scrollHeight;
    }
  }

  function explainError(err, Anthropic) {
    if (err instanceof Anthropic.AuthenticationError) return "That API key was not accepted. Click the gear and paste a valid key.";
    if (err instanceof Anthropic.PermissionDeniedError) return "This API key is not allowed to use Claude Haiku 5.5.";
    if (err instanceof Anthropic.RateLimitError) return "Too many requests for now. Please wait a minute and try again.";
    if (err instanceof Anthropic.BadRequestError) return "The request was rejected: " + (err.message || "bad request") + (/credit|billing/i.test(err.message || "") ? " Check your billing in the Anthropic console." : "");
    if (err instanceof Anthropic.APIConnectionError) return "Could not reach Anthropic. Check your internet connection.";
    if (err instanceof Anthropic.APIError) return `Anthropic returned an error (${err.status || "?"}). Please try again.`;
    return "Something went wrong: " + (err && err.message ? err.message : String(err));
  }

  // ---------------------------------------------------------------- settings & UI
  function openSettings(show) {
    const box = $("#chatSettings");
    box.hidden = show === undefined ? !box.hidden : !show;
    if (!box.hidden) { $("#apiKey").value = ""; $("#apiKey").placeholder = store.get(KEY_STORE) ? "A key is saved (paste a new one to replace it)" : "sk-ant-..."; $("#apiKey").focus(); }
  }
  $("#chatSettingsBtn").addEventListener("click", () => openSettings());
  $("#saveKey").addEventListener("click", () => {
    const k = $("#apiKey").value.trim();
    if (!k) { bubble("note", "Paste a key first."); return; }
    if (!store.set(KEY_STORE, k)) { bubble("error", "This app could not save the key (storage is blocked)."); return; }
    $("#apiKey").value = ""; openSettings(false); showWelcome(); bubble("note", "Key saved on this PC.");
  });
  $("#forgetKey").addEventListener("click", () => { store.del(KEY_STORE); $("#apiKey").value = ""; showWelcome(); bubble("note", "Key removed from this PC."); });

  function setHidden(h) { app.classList.toggle("chat-hidden", h); store.set(UI_STORE, h ? "1" : ""); }
  $("#chatClose").addEventListener("click", () => setHidden(true));
  $("#chatToggle").addEventListener("click", () => { setHidden(!app.classList.contains("chat-hidden")); if (!app.classList.contains("chat-hidden")) input.focus(); });

  $("#chatForm").addEventListener("submit", (e) => { e.preventDefault(); const t = input.value; input.value = ""; send(t); });
  input.addEventListener("keydown", (e) => { if (e.key === "Enter" && !e.shiftKey) { e.preventDefault(); $("#chatForm").requestSubmit(); } });

  const CHIPS = ["Why does LRU get 0% on loops?", "What did the AI learn?", "Explain what is on my screen", "Which design patterns are used?", "Explain Belady's algorithm simply"];
  $("#chatChips").innerHTML = CHIPS.map((c) => `<button type="button">${esc(c)}</button>`).join("");
  $("#chatChips").querySelectorAll("button").forEach((b) => b.addEventListener("click", () => send(b.textContent)));

  window.CacheSimChat = {
    ask(question) { setHidden(false); send(question); },
  };

  if (store.get(UI_STORE) === "1") app.classList.add("chat-hidden");
  showWelcome();
  updateUsage();
})();
