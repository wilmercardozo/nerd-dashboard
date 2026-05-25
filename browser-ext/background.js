// background.js — service worker MV3.
// - mantiene el contador local (ventana 5h) en chrome.storage.local
// - reporta cada envío al ESP (POST /api/claude/msg)
// - cola de reintentos (pending) si el ESP no responde
//
// El fetch al ESP por http:// se hace AQUÍ (no en el content script) porque
// claude.ai es https y bloquearía el contenido mixto; el service worker del
// extension no está sujeto a esa política.

const WIN_MS = 5 * 3600 * 1000;

async function getState() {
  const d = await chrome.storage.local.get(["espIp", "events", "pending"]);
  return { espIp: d.espIp || "", events: d.events || [], pending: d.pending || 0 };
}
function purge(events) {
  const cutoff = Date.now() - WIN_MS;
  return events.filter((t) => t >= cutoff);
}
async function updateBadge() {
  const { events } = await getState();
  const n = purge(events).length;
  chrome.action.setBadgeText({ text: n ? String(n) : "" });
  chrome.action.setBadgeBackgroundColor({ color: "#3b82f6" });
}

async function postOne(ip) {
  const r = await fetch(`http://${ip}/api/claude/msg`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ source: "web", ts: Date.now() }),
  });
  if (!r.ok) throw new Error("HTTP " + r.status);
  return true;
}

async function flush(ip) {
  let { pending } = await getState();
  while (pending > 0) {
    try {
      await postOne(ip);
      pending--;
    } catch (e) {
      break;
    }
  }
  await chrome.storage.local.set({ pending });
}

async function handleSend() {
  const s = await getState();
  const events = purge(s.events);
  events.push(Date.now());
  await chrome.storage.local.set({ events });
  updateBadge();

  if (!s.espIp) {
    await chrome.storage.local.set({ pending: s.pending + 1 });
    return;
  }
  try {
    await postOne(s.espIp);
    await flush(s.espIp);
  } catch (e) {
    await chrome.storage.local.set({ pending: s.pending + 1 });
  }
}

async function testConn(ip) {
  try {
    const r = await fetch(`http://${ip}/api/status`);
    const j = await r.json();
    return { ok: true, fw: j.fw, ip: j.ip };
  } catch (e) {
    return { ok: false, error: String(e) };
  }
}

chrome.runtime.onMessage.addListener((msg, sender, sendResponse) => {
  if (msg.type === "claude_send") {
    handleSend();
    return false;
  }
  if (msg.type === "get_state") {
    getState().then((s) => sendResponse({ espIp: s.espIp, count: purge(s.events).length, pending: s.pending }));
    return true;
  }
  if (msg.type === "set_ip") {
    chrome.storage.local.set({ espIp: msg.ip }).then(() => sendResponse({ ok: true }));
    return true;
  }
  if (msg.type === "test") {
    testConn(msg.ip).then(sendResponse);
    return true;
  }
  if (msg.type === "reset") {
    chrome.storage.local.set({ events: [], pending: 0 }).then(() => {
      updateBadge();
      sendResponse({ ok: true });
    });
    return true;
  }
});

// Reintenta cola + refresca badge cada minuto
chrome.alarms.create("flush", { periodInMinutes: 1 });
chrome.alarms.onAlarm.addListener(async () => {
  const s = await getState();
  if (s.espIp && s.pending > 0) await flush(s.espIp);
  updateBadge();
});
