const $ = (id) => document.getElementById(id);

function setSlot(id, value) {
  const el = $(id);
  const text = String(value || "UNKNOWN").toUpperCase();
  const span = el.querySelector("span");
  
  if (span.textContent !== text) {
    span.textContent = text;
    span.style.transform = "scale(1.1)";
    setTimeout(() => span.style.transform = "scale(1)", 200);
  }
  
  el.classList.remove("occupied", "available");
  if (String(value).toLowerCase() === "occupied") {
      el.classList.add("occupied");
  } else if (["free","available"].includes(String(value).toLowerCase())) {
      el.classList.add("available");
  }
}

function formatTime(value) {
  if (!value) return "--:--:--";
  const d = new Date(value);
  return isNaN(d) ? "--:--:--" : d.toLocaleTimeString([], { hour12: false });
}

function animateValue(obj, start, end, duration) {
  let startTimestamp = null;
  const step = (timestamp) => {
    if (!startTimestamp) startTimestamp = timestamp;
    const progress = Math.min((timestamp - startTimestamp) / duration, 1);
    obj.innerHTML = Math.floor(progress * (end - start) + start);
    if (progress < 1) {
      window.requestAnimationFrame(step);
    } else {
        obj.innerHTML = end;
    }
  };
  window.requestAnimationFrame(step);
}

let lastOccupied = -1;
let lastAvailable = -1;

function updateStatus(data) {
  setSlot("slot1", data.slots.slot1);
  setSlot("slot2", data.slots.slot2);
  setSlot("slot3", data.slots.slot3);

  const occ = data.occupiedCount ?? 0;
  if (occ !== lastOccupied) {
      animateValue($("occupied"), Math.max(0, lastOccupied), occ, 500);
      lastOccupied = occ;
  }
  
  const av = data.availableCount ?? 0;
  if (av !== lastAvailable) {
      animateValue($("available"), Math.max(0, lastAvailable), av, 500);
      lastAvailable = av;
  }
  
  const rate = Number(data.occupancyRate ?? 0);
  $("rate").textContent = `${rate}%`;
  $("rateBar").style.width = `${Math.max(0, Math.min(100, rate))}%`;

  $("entranceGate").textContent = String(data.entranceGate || "closed").toUpperCase();
  $("exitGate").textContent = String(data.exitGate || "closed").toUpperCase();
  $("rssi").textContent = data.wifiRssi == null ? "-- dBm" : `${data.wifiRssi} dBm`;

  const now = Date.now();
  const deviceTime = data.timeInstant ? new Date(data.timeInstant).getTime() : NaN;
  const fresh = !isNaN(deviceTime) && (now - deviceTime) <= 60000;
  
  const pill = $("deviceStatus");
  pill.classList.toggle("online", fresh);
  pill.classList.toggle("offline", !fresh);
  pill.innerHTML = fresh ? `<span style="color:#10b981;margin-right:6px;font-size:16px;">●</span> ESP32 ONLINE` : `<span style="color:#ef4444;margin-right:6px;font-size:16px;">●</span> ESP32 OFFLINE`;
  
  $("staleNote").classList.toggle("hidden", fresh);
  $("lastUpdate").textContent = `Last update: ${formatTime(data.timeInstant)}`;
}

function drawChart(history) {
  const canvas = $("chart");
  const ctx = canvas.getContext("2d");
  const rect = canvas.getBoundingClientRect();
  const dpr = window.devicePixelRatio || 1;
  canvas.width = rect.width * dpr;
  canvas.height = rect.height * dpr;
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

  const w = rect.width, h = rect.height;
  ctx.clearRect(0, 0, w, h);

  if (!history.length) {
    $("chartEmpty").classList.remove("hidden");
    return;
  }
  $("chartEmpty").classList.add("hidden");

  const pad = { l: 40, r: 20, t: 30, b: 40 };
  const cw = w - pad.l - pad.r, ch = h - pad.t - pad.b;
  const vals = history.map(x => Number(x.occupied_count || 0));
  const max = Math.max(3, ...vals);

  // Draw grid
  ctx.strokeStyle = "rgba(255,255,255,0.05)"; 
  ctx.lineWidth = 1;
  for(let i=0; i<=3; i++){
    const y = pad.t + ch * (i/3);
    ctx.beginPath(); ctx.moveTo(pad.l, y); ctx.lineTo(w - pad.r, y); ctx.stroke();
    
    ctx.fillStyle = "#64748b"; 
    ctx.font = "12px 'JetBrains Mono', monospace"; 
    ctx.fillText(String(Math.round(max*(1 - i/3))), 10, y + 4);
  }

  // Draw filled area gradient
  if (vals.length > 0) {
      const gradient = ctx.createLinearGradient(0, pad.t, 0, h - pad.b);
      gradient.addColorStop(0, "rgba(56, 189, 248, 0.4)");
      gradient.addColorStop(1, "rgba(56, 189, 248, 0.0)");
      
      ctx.beginPath();
      ctx.moveTo(pad.l, h - pad.b);
      
      vals.forEach((v, i) => {
        const x = pad.l + (vals.length === 1 ? cw/2 : cw * i / (vals.length - 1));
        const y = pad.t + ch * (1 - v/max);
        ctx.lineTo(x, y);
      });
      
      ctx.lineTo(pad.l + cw, h - pad.b);
      ctx.closePath();
      ctx.fillStyle = gradient;
      ctx.fill();
  }

  // Draw smooth line
  ctx.strokeStyle = "#38bdf8"; 
  ctx.lineWidth = 3;
  ctx.lineJoin = "round";
  ctx.lineCap = "round";
  ctx.beginPath();
  
  vals.forEach((v, i) => {
    const x = pad.l + (vals.length === 1 ? cw/2 : cw * i / (vals.length - 1));
    const y = pad.t + ch * (1 - v/max);
    if(i === 0) ctx.moveTo(x, y); 
    else ctx.lineTo(x, y);
  });
  ctx.stroke();
  
  // Draw points
  ctx.fillStyle = "#0f172a";
  vals.forEach((v, i) => {
    const x = pad.l + (vals.length === 1 ? cw/2 : cw * i / (vals.length - 1));
    const y = pad.t + ch * (1 - v/max);
    ctx.beginPath();
    ctx.arc(x, y, 4, 0, Math.PI * 2);
    ctx.fill();
    ctx.stroke();
  });

  // X axis labels
  ctx.fillStyle = "#64748b"; 
  ctx.font = "12px 'JetBrains Mono', monospace";
  history.forEach((item, i) => {
    if(i === 0 || i === history.length - 1 || i === Math.floor(history.length/2)){
      const x = pad.l + (vals.length === 1 ? cw/2 : cw * i / (vals.length - 1));
      ctx.fillText(formatTime(item.timestamp), Math.max(0, x - 28), h - 12);
    }
  });
}

async function loadDashboard(){
  try{
    const status = await fetch("/api/parking/status").then(r=>r.json());
    updateStatus(status);

    const history = await fetch("/api/parking/history?limit=60").then(r=>r.json());
    drawChart(history.data || []);

    const events = await fetch("/api/parking/events?limit=8").then(r=>r.json());
    const box = $("events");
    box.innerHTML = "";
    (events.data || []).forEach(e=>{
      const row = document.createElement("div");
      row.className = "event-row";
      row.innerHTML = `<span>${formatTime(e.timestamp)}</span><span>${escapeHtml(e.event)}</span>`;
      box.appendChild(row);
    });
    if(!events.data?.length){
      box.innerHTML = '<div class="event-row" style="color:#64748b;font-style:italic"><span>--:--:--</span><span>No events yet</span></div>';
    }
  }catch(err){
    const pill = $("deviceStatus");
    pill.className = "status-pill offline";
    pill.innerHTML = `<span style="color:#ef4444;margin-right:6px;font-size:16px;">●</span> BACKEND OFFLINE`;
    console.error(err);
  }
}

function escapeHtml(s){
  return String(s).replace(/[&<>"']/g, c => ({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[c]));
}

// =========================================================
// GATE CONTROL
// =========================================================

const GATE_LABELS = {
  entrance: "ENTRANCE",
  exit: "EXIT"
};

function setGateMsg(text, state) {
  const msg = $("gateMessage");
  msg.textContent = text;
  msg.className = `muted gate-msg-${state}`;
  msg.style.cssText = "transition:all 0.3s ease;font-size:14px;padding:14px;border-radius:8px;line-height:1.5;border:1px solid;";
}

async function controlGate(gate, action) {
  const endpoint = `/api/gates/${gate}/${action}`;
  const label = `${GATE_LABELS[gate] || gate.toUpperCase()} GATE`;

  // Lock all buttons during request
  const allBtns = document.querySelectorAll(".command-placeholder button");
  allBtns.forEach(b => { b.disabled = true; b.classList.add("loading"); });

  setGateMsg(`⏳ Sending ${action.toUpperCase()} command to ${label}...`, "sending");

  try {
    const response = await fetch(endpoint, { method: "POST" });
    const data = await response.json();

    if (!response.ok) {
      throw new Error(data.detail || `HTTP ${response.status}`);
    }

    setGateMsg(`✅ ${label} ${action.toUpperCase()} command sent — ESP32 responding via MQTT`, "success");

    // Refresh dashboard quickly to show new gate state
    setTimeout(loadDashboard, 800);

    // Auto-reset message after 4s
    setTimeout(() => setGateMsg("Ready — select a gate command above.", "idle"), 4000);

  } catch (error) {
    console.error("Gate control error:", error);
    setGateMsg(`❌ ERROR: ${error.message}`, "error");
  } finally {
    // Always re-enable buttons
    allBtns.forEach(b => { b.disabled = false; b.classList.remove("loading"); });
  }
}

loadDashboard();
setInterval(loadDashboard, 5000);
window.addEventListener("resize", () => loadDashboard());
