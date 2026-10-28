/**
 * TDR Edge-AI Live Diagnostic Dashboard Client
 * --------------------------------------------
 * Handles real-time telemetry streaming, oscilloscope waveform rendering,
 * and edge neural network classification visualization.
 */

// UI Elements
const statusText = document.getElementById('status-text');
const statusDot = document.querySelector('.status-dot');
const boardPortChip = document.getElementById('board-port-chip');
const heroCard = document.getElementById('hero-card');
const classificationTitle = document.getElementById('ml-classification-title');
const classificationDesc = document.getElementById('ml-classification-desc');
const modelArch = document.getElementById('model-arch');
const modelLatency = document.getElementById('model-latency');
const modelGamma = document.getElementById('model-gamma');
const confidenceVal = document.getElementById('confidence-val');
const confidenceCircle = document.getElementById('confidence-circle');

// Metric Elements
const metricVoltage = document.getElementById('metric-voltage');
const metricDistance = document.getElementById('metric-distance');
const metricTime = document.getElementById('metric-time');
const metricRaw = document.getElementById('metric-raw');
const barVoltage = document.getElementById('bar-voltage');
const barDistance = document.getElementById('bar-distance');
const barTime = document.getElementById('bar-time');

// Probabilities
const probHealthy = document.getElementById('prob-healthy');
const probOpen = document.getElementById('prob-open');
const probShort = document.getElementById('prob-short');
const probTap = document.getElementById('prob-tap');
const barHealthy = document.getElementById('prob-bar-healthy');
const barOpen = document.getElementById('prob-bar-open');
const barShort = document.getElementById('prob-bar-short');
const barTap = document.getElementById('prob-bar-tap');

// Event Log
const eventLog = document.getElementById('event-log');
const clearLogBtn = document.getElementById('clear-log-btn');
const demoToggleBtn = document.getElementById('demo-toggle-btn');

// Canvas Oscilloscope
const canvas = document.getElementById('tdr-canvas');
const ctx = canvas.getContext('2d');

let lastState = null;
let isDemoMode = false;
let demoInterval = null;
let currentTelemetry = {
  voltage: 1.88,
  distance_cm: 0.0,
  time_ns: 0.0,
  status: 'HEALTHY',
  ml_prediction: {
    class_name: 'HEALTHY (NORMAL LINE)',
    confidence: 99.4,
    inference_latency_ms: 3.4,
    gamma_reflection: '+0.02',
    probabilities: { 'Healthy': 99.4, 'Open Circuit': 0.3, 'Short Circuit': 0.2, 'Tapping / Theft': 0.1 }
  }
};

// Resize Canvas
function resizeCanvas() {
  const rect = canvas.parentElement.getBoundingClientRect();
  canvas.width = rect.width * window.devicePixelRatio;
  canvas.height = rect.height * window.devicePixelRatio;
  ctx.scale(window.devicePixelRatio, window.devicePixelRatio);
}
window.addEventListener('resize', resizeCanvas);
resizeCanvas();

// Append Log Entry
function addLogEntry(type, message) {
  const now = new Date();
  const timeStr = now.toTimeString().split(' ')[0] + '.' + String(now.getMilliseconds()).padStart(3, '0');
  const entry = document.createElement('div');
  entry.className = `log-entry ${type.toLowerCase()}`;
  entry.innerHTML = `<span class="log-time">[${timeStr}]</span> ${message}`;
  eventLog.prepend(entry);

  // Keep max 50 entries
  while (eventLog.children.length > 50) {
    eventLog.removeChild(eventLog.lastChild);
  }
}

// Clear Log
clearLogBtn.addEventListener('click', () => {
  eventLog.innerHTML = `<div class="log-entry system"><span class="log-time">[SYSTEM]</span> Log cleared.</div>`;
});

// Update Dashboard UI with Telemetry Object
function updateDashboard(data) {
  currentTelemetry = data;

  // 1. Connection Header
  if (data.connected) {
    statusDot.className = 'status-dot pulsating';
    statusText.textContent = 'ONLINE (ST-LINK VCP)';
    boardPortChip.textContent = `PORT: ${data.port || '/dev/ttyACM0'}`;
  } else if (!isDemoMode) {
    statusDot.className = 'status-dot disconnected';
    statusText.textContent = 'WAITING FOR BOARD...';
  }

  // 2. Metrics
  const v = Number(data.voltage || 0).toFixed(2);
  const d = Number(data.distance_cm || 0).toFixed(1);
  const t = Number(data.time_ns || 0).toFixed(1);

  metricVoltage.textContent = v;
  metricDistance.textContent = d;
  metricTime.textContent = t;
  metricRaw.textContent = data.raw || `V:${v},T:${t},D:${d},STATUS:${data.status}`;

  // Gauge Bars
  barVoltage.style.width = Math.min(100, Math.max(0, (Number(v) / 3.3) * 100)) + '%';
  barDistance.style.width = Math.min(100, Math.max(0, (Number(d) / 16.0) * 100)) + '%';
  barTime.style.width = Math.min(100, Math.max(0, (Number(t) / 1.6) * 100)) + '%';

  // 3. ML Model Predictions
  const ml = data.ml_prediction || {};
  const status = data.status || 'HEALTHY';

  classificationTitle.textContent = ml.class_name || status;
  modelLatency.textContent = `${ml.inference_latency_ms || 3.4} ms`;
  modelGamma.textContent = ml.gamma_reflection || '+0.00';

  // Confidence Ring (perimeter = 2 * PI * 50 = 314)
  const conf = Number(ml.confidence || 98.5);
  confidenceVal.textContent = `${conf.toFixed(1)}%`;
  const offset = 314 - (314 * (conf / 100));
  confidenceCircle.style.strokeDashoffset = offset;

  // Hero Card Styling
  heroCard.className = 'card hero-classification-card';
  if (status === 'OPEN') {
    heroCard.classList.add('status-open');
    confidenceCircle.style.stroke = 'var(--color-rose)';
    classificationDesc.textContent = `High-impedance discontinuity detected at d = ${d} cm. Positive voltage reflection coefficient (\u0393 \u2248 +1.0) confirmed by 1D-CNN.`;
  } else if (status === 'SHORT') {
    heroCard.classList.add('status-short');
    confidenceCircle.style.stroke = 'var(--color-amber)';
    classificationDesc.textContent = `Low-impedance short-circuit fault detected at d = ${d} cm. Negative inverted reflection (\u0393 \u2248 -1.0) phase detected.`;
  } else {
    heroCard.classList.add('status-healthy');
    confidenceCircle.style.stroke = 'var(--color-emerald)';
    classificationDesc.textContent = `Line terminated with intact impedance load (V = ${v}V). Reflection amplitude within nominal grid noise tolerance threshold.`;
  }

  // 4. Softmax Probability Bars
  const probs = ml.probabilities || {};
  const pHealthy = (probs['Healthy'] || 0).toFixed(1);
  const pOpen = (probs['Open Circuit'] || 0).toFixed(1);
  const pShort = (probs['Short Circuit'] || 0).toFixed(1);
  const pTap = (probs['Tapping / Theft'] || 0).toFixed(1);

  probHealthy.textContent = `${pHealthy}%`;
  probOpen.textContent = `${pOpen}%`;
  probShort.textContent = `${pShort}%`;
  probTap.textContent = `${pTap}%`;

  barHealthy.style.width = `${pHealthy}%`;
  barOpen.style.width = `${pOpen}%`;
  barShort.style.width = `${pShort}%`;
  barTap.style.width = `${pTap}%`;

  // 5. Check if state changed for log
  if (lastState !== status) {
    if (status === 'OPEN') {
      addLogEntry('open', `[FAULT DETECTED] OPEN CIRCUIT at ${d} cm (t = ${t} ns, Conf: ${conf}%)`);
    } else if (status === 'SHORT') {
      addLogEntry('short', `[FAULT DETECTED] SHORT CIRCUIT at ${d} cm (t = ${t} ns, Conf: ${conf}%)`);
    } else {
      addLogEntry('healthy', `[STATUS NORMAL] Line Intact & Healthy (V = ${v}V, Conf: ${conf}%)`);
    }
    lastState = status;
  }
}

// Draw Real-time Oscilloscope TDR Waveform
function renderTDRWaveform() {
  const w = canvas.parentElement.clientWidth;
  const h = canvas.parentElement.clientHeight;
  const midY = h / 2;

  ctx.clearRect(0, 0, w, h);

  // 1. Oscilloscope Grid Lines
  ctx.strokeStyle = 'rgba(255, 255, 255, 0.05)';
  ctx.lineWidth = 1;
  const gridX = 10;
  const gridY = 6;

  for (let i = 0; i <= gridX; i++) {
    const x = (w / gridX) * i;
    ctx.beginPath();
    ctx.moveTo(x, 0);
    ctx.lineTo(x, h);
    ctx.stroke();
  }
  for (let j = 0; j <= gridY; j++) {
    const y = (h / gridY) * j;
    ctx.beginPath();
    ctx.moveTo(0, y);
    ctx.lineTo(w, y);
    ctx.stroke();
  }

  // Center Baseline
  ctx.strokeStyle = 'rgba(255, 255, 255, 0.15)';
  ctx.beginPath();
  ctx.moveTo(0, midY);
  ctx.lineTo(w, midY);
  ctx.stroke();

  // 2. Compute TDR Waveform Points
  const points = 300;
  const status = currentTelemetry.status || 'HEALTHY';
  const dist = Number(currentTelemetry.distance_cm || 0);
  
  // Position reflection peak proportional to distance along line (16 cm breadboard)
  // Maps 0..16 cm to screen position (x = 100 to w - 40)
  const pulseStartX = 40;
  const maxReflectX = w - 50;
  const reflectX = pulseStartX + 40 + (dist / 16.0) * (maxReflectX - pulseStartX - 40);

  // Reflection polarity and amplitude based on fault
  let reflectAmp = 0;
  if (status === 'OPEN') {
    reflectAmp = 70; // Positive peak (+1.0 Gamma)
  } else if (status === 'SHORT') {
    reflectAmp = -70; // Negative peak (-1.0 Gamma)
  } else {
    reflectAmp = (Math.random() - 0.5) * 6; // Minor reflection noise
  }

  // Draw Waveform Trace
  ctx.beginPath();
  ctx.lineWidth = 2.5;
  ctx.strokeStyle = status === 'OPEN' ? '#f43f5e' : (status === 'SHORT' ? '#f59e0b' : '#00f3ff');
  ctx.shadowColor = ctx.strokeStyle;
  ctx.shadowBlur = 10;

  for (let i = 0; i <= points; i++) {
    const x = (w / points) * i;
    let y = midY;

    // A. Injected Excitation Pulse (at x ~ 40)
    const distToPulse = x - pulseStartX;
    if (distToPulse >= -15 && distToPulse <= 15) {
      // Gaussian pulse
      y -= 75 * Math.exp(-Math.pow(distToPulse / 7, 2));
    }

    // B. Reflected Echo Pulse (at x ~ reflectX)
    if (status !== 'HEALTHY' || Math.abs(reflectAmp) > 4) {
      const distToReflect = x - reflectX;
      if (distToReflect >= -20 && distToReflect <= 20) {
        y -= reflectAmp * Math.exp(-Math.pow(distToReflect / 9, 2));
      }
    }

    // C. Cable High-Frequency Grid Noise (AWGN)
    const noise = (Math.random() - 0.5) * 3.5;
    y += noise;

    if (i === 0) {
      ctx.moveTo(x, y);
    } else {
      ctx.lineTo(x, y);
    }
  }
  ctx.stroke();
  ctx.shadowBlur = 0;

  // 3. Annotations & Markers
  // Marker at Injected Pulse
  ctx.fillStyle = '#00f3ff';
  ctx.font = '10px "JetBrains Mono"';
  ctx.fillText('TX PULSE (t=0)', pulseStartX - 25, midY - 85);

  // Marker at Reflection
  if (status !== 'HEALTHY' && dist > 0.1) {
    ctx.fillStyle = status === 'OPEN' ? '#f43f5e' : '#f59e0b';
    const label = `${status} ECHO (${dist.toFixed(1)} cm / ${currentTelemetry.time_ns} ns)`;
    ctx.fillText(label, Math.max(80, reflectX - 50), status === 'OPEN' ? (midY - 82) : (midY + 92));
    
    // Draw vertical dotted line
    ctx.setLineDash([4, 4]);
    ctx.strokeStyle = ctx.fillStyle;
    ctx.beginPath();
    ctx.moveTo(reflectX, 30);
    ctx.lineTo(reflectX, h - 30);
    ctx.stroke();
    ctx.setLineDash([]);
  }

  requestAnimationFrame(renderTDRWaveform);
}
requestAnimationFrame(renderTDRWaveform);

// Server-Sent Events (SSE) Stream Connection
function connectSSE() {
  const eventSource = new EventSource('/api/stream');

  eventSource.onopen = () => {
    statusDot.className = 'status-dot pulsating';
    statusText.textContent = 'ONLINE (STREAMING)';
    addLogEntry('system', 'Connected to SSE real-time telemetry stream.');
  };

  eventSource.onmessage = (event) => {
    if (isDemoMode) return;
    try {
      const data = JSON.parse(event.data);
      updateDashboard(data);
    } catch (e) {
      console.error('Error parsing SSE event:', e);
    }
  };

  eventSource.onerror = () => {
    statusDot.className = 'status-dot disconnected';
    statusText.textContent = 'RECONNECTING...';
    // Fallback polling
    setTimeout(pollLatest, 1000);
  };
}

// Fallback Polling Function
async function pollLatest() {
  if (isDemoMode) return;
  try {
    const res = await fetch('/api/latest');
    if (res.ok) {
      const data = await res.json();
      updateDashboard(data);
    }
  } catch (e) {}
}

// Demo Mode Toggle (for offline / presentation simulation)
demoToggleBtn.addEventListener('click', () => {
  isDemoMode = !isDemoMode;
  if (isDemoMode) {
    demoToggleBtn.style.background = 'var(--color-rose)';
    demoToggleBtn.style.color = '#fff';
    document.getElementById('demo-mode-label').textContent = 'EXIT DEMO';
    addLogEntry('system', 'Switched to Interactive Demonstration Mode.');

    let simStep = 0;
    demoInterval = setInterval(() => {
      simStep = (simStep + 1) % 3;
      if (simStep === 0) {
        updateDashboard({
          voltage: 1.88,
          distance_cm: 0.0,
          time_ns: 0.0,
          status: 'HEALTHY',
          raw: 'V:1.88,T:0.0,D:0.0,STATUS:HEALTHY',
          connected: true,
          port: 'SIMULATION',
          ml_prediction: {
            class_name: 'HEALTHY (NORMAL LINE)',
            confidence: 99.6,
            inference_latency_ms: 3.3,
            gamma_reflection: '+0.01',
            probabilities: { 'Healthy': 99.6, 'Open Circuit': 0.2, 'Short Circuit': 0.1, 'Tapping / Theft': 0.1 }
          }
        });
      } else if (simStep === 1) {
        const d = (11.0 + Math.random() * 4.0).toFixed(1);
        const t = (d * 0.1).toFixed(1);
        updateDashboard({
          voltage: 3.25,
          distance_cm: Number(d),
          time_ns: Number(t),
          status: 'OPEN',
          raw: `V:3.25,T:${t},D:${d},STATUS:OPEN`,
          connected: true,
          port: 'SIMULATION',
          ml_prediction: {
            class_name: 'FAULT: OPEN CIRCUIT',
            confidence: 99.2,
            inference_latency_ms: 3.6,
            gamma_reflection: '+0.97',
            probabilities: { 'Healthy': 0.4, 'Open Circuit': 99.2, 'Short Circuit': 0.1, 'Tapping / Theft': 0.3 }
          }
        });
      } else {
        const d = (2.0 + Math.random() * 3.0).toFixed(1);
        const t = (d * 0.1).toFixed(1);
        updateDashboard({
          voltage: 0.08,
          distance_cm: Number(d),
          time_ns: Number(t),
          status: 'SHORT',
          raw: `V:0.08,T:${t},D:${d},STATUS:SHORT`,
          connected: true,
          port: 'SIMULATION',
          ml_prediction: {
            class_name: 'FAULT: SHORT CIRCUIT',
            confidence: 98.9,
            inference_latency_ms: 3.5,
            gamma_reflection: '-0.96',
            probabilities: { 'Healthy': 0.3, 'Open Circuit': 0.2, 'Short Circuit': 98.9, 'Tapping / Theft': 0.6 }
          }
        });
      }
    }, 2500);
  } else {
    clearInterval(demoInterval);
    demoToggleBtn.style.background = 'rgba(139, 92, 246, 0.15)';
    demoToggleBtn.style.color = 'var(--color-violet)';
    document.getElementById('demo-mode-label').textContent = 'DEMO MODE';
    addLogEntry('system', 'Exited Demo Mode. Resumed live hardware telemetry.');
    pollLatest();
  }
});

// Initialize on Load: immediate fast polling + SSE
pollLatest();
setInterval(pollLatest, 250);
connectSSE();
