// content.js — inyectado en claude.ai. Detecta envíos de mensaje y avisa al
// background (que reporta al ESP). Es una heurística: el conteo es aproximado.

let lastFire = 0;

function fire(reason) {
  const now = Date.now();
  if (now - lastFire < 700) return; // dedupe Enter+click del mismo envío
  lastFire = now;
  try {
    chrome.runtime.sendMessage({ type: "claude_send", reason });
  } catch (e) {
    /* SW dormido: reintenta en próximo evento */
  }
}

// 1) Enter (sin Shift) dentro del compositor (textarea o contenteditable)
document.addEventListener(
  "keydown",
  (e) => {
    if (e.key !== "Enter" || e.shiftKey || e.isComposing) return;
    const el = e.target;
    if (!el) return;
    const isComposer =
      el.tagName === "TEXTAREA" ||
      el.getAttribute("contenteditable") === "true" ||
      (el.closest && el.closest('[contenteditable="true"]'));
    if (isComposer) fire("enter");
  },
  true
);

// 2) Click en el botón de enviar (aria-label Send / Enviar)
document.addEventListener(
  "click",
  (e) => {
    const btn = e.target.closest && e.target.closest("button");
    if (!btn) return;
    const al = (btn.getAttribute("aria-label") || "").toLowerCase();
    if (al.includes("send") || al.includes("enviar")) fire("click");
  },
  true
);
