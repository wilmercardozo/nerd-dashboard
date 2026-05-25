const $ = (id) => document.getElementById(id);
const send = (m) => chrome.runtime.sendMessage(m);

function refresh() {
  send({ type: "get_state" }, (s) => {
    if (!s) return;
    $("ip").value = s.espIp || "";
    $("count").textContent = s.count;
    $("pending").textContent = s.pending ? `· ${s.pending} en cola` : "";
  });
}

function save() {
  const ip = $("ip").value.trim();
  send({ type: "set_ip", ip }, () => {
    $("msg").style.color = "#5dd87a";
    $("msg").textContent = "IP guardada";
  });
}

function test() {
  const ip = $("ip").value.trim();
  $("msg").style.color = "#8b97a8";
  $("msg").textContent = "probando...";
  send({ type: "test", ip }, (r) => {
    if (r && r.ok) {
      $("msg").style.color = "#5dd87a";
      $("msg").textContent = `OK — fw ${r.fw} @ ${r.ip}`;
    } else {
      $("msg").style.color = "#ff5d5d";
      $("msg").textContent = "Sin respuesta: " + (r ? r.error : "error");
    }
  });
}

function reset() {
  send({ type: "reset" }, () => refresh());
}

document.addEventListener("DOMContentLoaded", refresh);
