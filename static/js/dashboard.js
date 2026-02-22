/* ============================================================
   VibroJaw — dashboard.js
   Socket.IO client + Chart.js + all dashboard interactivity
   ============================================================ */

let socket       = null;
let trendChart   = null;
let activeUserId = 'esp32-001';

// Chart ring buffers (max 60 points = 5 min of history)
const MAX_PTS   = 60;
const chartLbls = [];
const chartEpm  = [];
const chartInt  = [];

// Running counters (updated locally, no re-fetch needed per event)
let totalEvents  = 0;
let therapyCt    = 0;
let intensitySum = 0.0;

// ── Entry point called from dashboard.html ─────────────────
function initDashboard(userId) {
  activeUserId = userId;
  loadInitialData().then(() => {
    buildChart();
    connectSocket();
  });
}

// ============================================================
//  INITIAL DATA LOAD (fills chart + table before socket starts)
// ============================================================
async function loadInitialData() {
  try {
    const stats    = await apiFetch(`/api/stats?userId=${activeUserId}`);
    applyStats(stats);

    const summaries = await apiFetch(`/api/summaries?userId=${activeUserId}&limit=60`);
    // API returns newest-first; chart wants oldest-first
    [...summaries].reverse().forEach(s =>
      pushChartPoint(s.eventsPerMinute, s.avgIntensity, s.timestamp)
    );

    const events = await apiFetch(`/api/readings?userId=${activeUserId}&limit=50`);
    events.forEach(e => addRow(e, false));

    refreshCountLabel();
  } catch (e) {
    console.warn('[VibroJaw] Initial load skipped:', e.message);
  }
}

// ============================================================
//  SOCKET.IO — real-time connection
// ============================================================
function connectSocket() {
  socket = io({ transports: ['websocket', 'polling'] });

  socket.on('connect', () => {
    setWsStatus('live', 'Live');
    console.log('[VibroJaw] Socket connected');
  });

  socket.on('disconnect', () => {
    setWsStatus('off', 'Disconnected');
  });

  // New single clench event
  socket.on('new_event', (event) => {
    if (event.userId !== activeUserId) return;
    onNewEvent(event);
  });

  // 5-second summary batch
  socket.on('new_summary', (summary) => {
    if (summary.userId !== activeUserId) return;
    onNewSummary(summary);
  });
}

// ============================================================
//  EVENT HANDLERS
// ============================================================
function onNewEvent(event) {
  totalEvents++;
  intensitySum += event.intensity;
  if (event.therapyTriggered) therapyCt++;

  // Update stat cards
  setText('totalEvents',  totalEvents);
  setText('therapyCount', therapyCt);
  setHTML('avgIntensity',
    `${avg(intensitySum, totalEvents)}<span class="stat-unit">%</span>`);

  addRow(event, true);
  refreshCountLabel();
  flashCard('cardTotal');
}

function onNewSummary(summary) {
  pushChartPoint(summary.eventsPerMinute, summary.avgIntensity, summary.timestamp);
  updateStatusCard(summary.eventsPerMinute, summary.healthStatus);
}

// ============================================================
//  CHART
// ============================================================
function buildChart() {
  const ctx = document.getElementById('trendChart');
  if (!ctx) return;

  trendChart = new Chart(ctx, {
    type: 'line',
    data: {
      labels:   chartLbls,
      datasets: [
        {
          label:               'Events / min',
          data:                chartEpm,
          borderColor:         '#00d4ff',
          backgroundColor:     'rgba(0,212,255,0.07)',
          borderWidth:         2,
          pointRadius:         3,
          pointHoverRadius:    5,
          pointBackgroundColor:'#00d4ff',
          tension:             0.45,
          fill:                true,
          yAxisID:             'y',
        },
        {
          label:               'Avg Intensity (%)',
          data:                chartInt,
          borderColor:         '#ff6b6b',
          backgroundColor:     'rgba(255,107,107,0.05)',
          borderWidth:         2,
          pointRadius:         2,
          pointBackgroundColor:'#ff6b6b',
          tension:             0.45,
          fill:                true,
          yAxisID:             'y1',
        },
      ],
    },
    options: {
      responsive:          true,
      maintainAspectRatio: false,
      animation:           { duration: 350 },
      interaction:         { mode: 'index', intersect: false },
      plugins: {
        legend: {
          display: true,
          labels:  { color: '#8892b0', font: { family: "'IBM Plex Mono', monospace", size: 11 }, boxWidth: 12 },
        },
        tooltip: {
          backgroundColor: '#0d1f35',
          borderColor:     '#1e3a5f',
          borderWidth:     1,
          titleColor:      '#00d4ff',
          bodyColor:       '#ccd6f6',
          padding:         10,
          callbacks: {
            label: ctx => `${ctx.dataset.label}: ${ctx.parsed.y.toFixed(1)}`,
          },
        },
      },
      scales: {
        x: {
          grid:  { color: 'rgba(255,255,255,0.04)' },
          ticks: { color: '#4a5568', font: { family: "'IBM Plex Mono', monospace", size: 10 }, maxTicksLimit: 8 },
        },
        y: {
          type: 'linear', position: 'left', min: 0,
          title: { display: true, text: 'Events / min', color: '#00d4ff', font: { size: 11 } },
          grid:  { color: 'rgba(255,255,255,0.04)' },
          ticks: { color: '#4a5568', font: { family: "'IBM Plex Mono', monospace", size: 10 } },
        },
        y1: {
          type: 'linear', position: 'right', min: 0, max: 100,
          title: { display: true, text: 'Intensity %', color: '#ff6b6b', font: { size: 11 } },
          grid:  { drawOnChartArea: false },
          ticks: { color: '#4a5568', font: { family: "'IBM Plex Mono', monospace", size: 10 } },
        },
      },
    },
  });
}

function pushChartPoint(epm, intensity, timestamp) {
  if (!trendChart) return;
  chartLbls.push(fmtChartTime(timestamp));
  chartEpm.push(+parseFloat(epm).toFixed(1));
  chartInt.push(+parseFloat(intensity || 0).toFixed(1));
  if (chartLbls.length > MAX_PTS) {
    chartLbls.shift(); chartEpm.shift(); chartInt.shift();
  }
  trendChart.update('none');
}

// ============================================================
//  EVENT TABLE
// ============================================================
function addRow(event, animate) {
  const tbody = document.getElementById('eventBody');
  const empty = tbody.querySelector('.empty-row');
  if (empty) empty.remove();

  const tr  = document.createElement('tr');
  if (animate) tr.classList.add('row-new');

  const pct     = parseFloat(event.intensity).toFixed(1);
  const barW    = Math.min(80, Math.round(parseFloat(event.intensity) * 0.8));
  const therapy = event.therapyTriggered;
  const status  = intensityStatus(event.intensity);

  tr.innerHTML = `
    <td class="mono">${fmtTableTime(event.timestamp)}</td>
    <td>
      <div class="ibar-wrap">
        <div class="ibar" style="width:${barW}px"></div>
        <span class="ibar-val mono">${pct}%</span>
      </div>
    </td>
    <td class="mono">${event.durationMs}ms</td>
    <td class="${therapy ? 'th-yes' : 'th-no'}">${therapy ? '◉ Yes' : '○ No'}</td>
    <td><span class="spill ${status.toLowerCase()}">${status}</span></td>
  `;

  tbody.insertBefore(tr, tbody.firstChild);
  while (tbody.children.length > 100) tbody.removeChild(tbody.lastChild);
}

// ============================================================
//  STATUS CARD
// ============================================================
function updateStatusCard(epm, status) {
  const card  = document.getElementById('statusCard');
  const val   = document.getElementById('statusValue');
  const sub   = document.getElementById('statusSub');

  if (!card || !val) return;

  val.textContent = status;
  sub.textContent = `${parseFloat(epm).toFixed(1)} events / min`;

  // Clear old state classes
  card.classList.remove('s-calm', 's-elevated', 's-concerning');
  val.classList.remove('sv-calm', 'sv-elevated', 'sv-concerning');

  const cls = status.toLowerCase();
  card.classList.add(`s-${cls}`);
  val.classList.add(`sv-${cls}`);
}

function applyStats(stats) {
  totalEvents  = stats.totalEvents  || 0;
  therapyCt    = stats.therapyCount || 0;
  const avgI   = stats.avgIntensity || 0;
  intensitySum = avgI * totalEvents;

  setText('totalEvents',  totalEvents);
  setText('therapyCount', therapyCt);
  setHTML('avgIntensity', `${avgI}<span class="stat-unit">%</span>`);

  if (stats.currentStatus) {
    updateStatusCard(stats.currentEpm || 0, stats.currentStatus);
  }
}

// ============================================================
//  CONFIG PUSH
// ============================================================
function updateThresholdDisplay(v) { setText('thresholdVal', v); }
function updateVibIntDisplay(v)    { setText('vibIntVal',    v); }
function updateVibDurDisplay(v)    { setText('vibDurVal',    v); }

function setPreset(val) {
  const slider = document.getElementById('thresholdSlider');
  if (slider) { slider.value = val; updateThresholdDisplay(val); }
  document.querySelectorAll('.preset-btn').forEach(b => b.classList.remove('active'));
  if (event && event.target) event.target.classList.add('active');
}

async function pushConfig() {
  const cfg = {
    userId:             activeUserId,
    threshold:          parseInt(document.getElementById('thresholdSlider').value),
    vibrationEnabled:   document.getElementById('vibEnabled').checked,
    vibrationIntensity: parseInt(document.getElementById('vibIntSlider').value),
    vibrationDuration:  parseInt(document.getElementById('vibDurSlider').value),
  };
  const fb = document.getElementById('cfgFeedback');
  fb.textContent = 'Pushing…'; fb.className = 'cfg-feedback';

  try {
    await fetch('/api/config', {
      method: 'PUT', headers: {'Content-Type':'application/json'},
      body: JSON.stringify(cfg),
    });
    fb.textContent = '✓ Config sent — ESP32 updates within 10 s';
    fb.className = 'cfg-feedback ok';
  } catch {
    fb.textContent = '✗ Push failed'; fb.className = 'cfg-feedback err';
  }
  setTimeout(() => { fb.textContent = ''; fb.className = 'cfg-feedback'; }, 4000);
}

// ============================================================
//  SIMULATOR
// ============================================================
async function startSim() {
  const scenario = document.getElementById('scenarioSelect').value;
  await fetch('/api/simulator/start', {
    method: 'POST', headers: {'Content-Type':'application/json'},
    body: JSON.stringify({ scenario }),
  });
  activeUserId = 'simulator';
  setSimBadge(true, scenario);
  // Clear display so it starts fresh
  await resetDisplay();
}

async function stopSim() {
  await fetch('/api/simulator/stop', { method: 'POST' });
  activeUserId = 'esp32-001';
  setSimBadge(false, '');
}

function setSimBadge(on, scenario) {
  const badge = document.getElementById('simBadge');
  if (!badge) return;
  badge.textContent = on ? `● ${scenario}` : '○ STOPPED';
  badge.className   = on ? 'sim-badge on' : 'sim-badge';
}

// ============================================================
//  RESET
// ============================================================
async function resetData() {
  if (!confirm('Clear all session data for current device?')) return;
  await fetch(`/api/data?userId=${activeUserId}`, { method: 'DELETE' });
  await resetDisplay();
}

async function resetDisplay() {
  totalEvents = 0; therapyCt = 0; intensitySum = 0;
  chartLbls.length = 0; chartEpm.length = 0; chartInt.length = 0;
  if (trendChart) trendChart.update();
  setText('totalEvents', '0'); setText('therapyCount', '0');
  setHTML('avgIntensity', '0<span class="stat-unit">%</span>');
  document.getElementById('eventBody').innerHTML =
    '<tr class="empty-row"><td colspan="5">No events yet — waiting for data…</td></tr>';
  refreshCountLabel();
}

// ============================================================
//  HELPERS
// ============================================================
function setWsStatus(state, label) {
  const dot = document.getElementById('wsDot');
  const lbl = document.getElementById('wsLabel');
  if (dot) { dot.className = `ws-dot ${state}`; }
  if (lbl) lbl.textContent = label;
}

function flashCard(id) {
  const card = document.getElementById(id);
  if (!card) return;
  card.classList.remove('flash');
  void card.offsetWidth;
  card.classList.add('flash');
}

function refreshCountLabel() {
  const n = document.querySelectorAll('#eventBody tr:not(.empty-row)').length;
  setText('eventCount', `${n} event${n !== 1 ? 's' : ''} recorded`);
}

function intensityStatus(i) {
  if (i < 40) return 'Normal';
  if (i < 70) return 'Elevated';
  return 'Concerning';
}

function avg(sum, count) {
  return count > 0 ? (sum / count).toFixed(1) : '0.0';
}

function fmtChartTime(ts) {
  if (!ts) return '';
  const d = new Date(ts);
  return [d.getHours(), d.getMinutes(), d.getSeconds()]
    .map(n => String(n).padStart(2, '0')).join(':');
}

function fmtTableTime(ts) {
  if (!ts) return '';
  return new Date(ts).toLocaleTimeString('en-US',
    { hour12: false, hour: '2-digit', minute: '2-digit', second: '2-digit' });
}

async function apiFetch(url) {
  const r = await fetch(url);
  if (!r.ok) throw new Error(`HTTP ${r.status}`);
  return r.json();
}

function setText(id, val) {
  const el = document.getElementById(id);
  if (el) el.textContent = val;
}
function setHTML(id, html) {
  const el = document.getElementById(id);
  if (el) el.innerHTML = html;
}