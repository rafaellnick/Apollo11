#ifndef APOLLO11_EMBEDDED_ESP8266_WEB_DSKY_PAGE_H
#define APOLLO11_EMBEDDED_ESP8266_WEB_DSKY_PAGE_H

#include <Arduino.h>

const char kDskyIndexHtml[] PROGMEM = R"DSKYHTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>AGC Web DSKY</title>
  <style>
    :root {
      color-scheme: dark;
      --case: #1c2224;
      --case-edge: #0b0f10;
      --panel: #242d2f;
      --display: #07120d;
      --display-hot: #8fffc2;
      --display-dim: #27513d;
      --ink: #e9f4ec;
      --muted: #8fa19a;
      --amber: #ffbf47;
      --red: #ff5f57;
      --green: #38d47a;
      --key: #d7d0bf;
      --key-text: #1d211f;
    }

    * {
      box-sizing: border-box;
    }

    body {
      margin: 0;
      min-height: 100vh;
      background:
        radial-gradient(circle at 20% 10%, rgba(143, 255, 194, 0.10), transparent 32rem),
        linear-gradient(145deg, #071012, #162022 42%, #090d0e);
      color: var(--ink);
      font-family: Georgia, "Times New Roman", serif;
    }

    main {
      width: min(980px, calc(100vw - 24px));
      margin: 0 auto;
      padding: 18px 0 28px;
    }

    .topbar {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 12px;
      margin-bottom: 12px;
      color: var(--muted);
      font: 14px/1.3 Verdana, sans-serif;
    }

    .shell {
      display: grid;
      grid-template-columns: 1.05fr 0.95fr;
      gap: 16px;
      padding: 18px;
      border: 1px solid #3c484b;
      border-radius: 26px;
      background: linear-gradient(160deg, var(--case), var(--case-edge));
      box-shadow: 0 28px 80px rgba(0, 0, 0, 0.52), inset 0 1px 0 rgba(255, 255, 255, 0.08);
    }

    .display,
    .keys,
    .console {
      border-radius: 18px;
      background: var(--panel);
      box-shadow: inset 0 1px 0 rgba(255, 255, 255, 0.06), inset 0 -2px 8px rgba(0, 0, 0, 0.32);
    }

    .display {
      padding: 16px;
    }

    .readout {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 10px;
      margin-bottom: 12px;
    }

    .field,
    .register,
    .meta {
      border: 1px solid #173326;
      border-radius: 12px;
      background: linear-gradient(180deg, #0a1911, var(--display));
      color: var(--display-hot);
      text-shadow: 0 0 14px rgba(143, 255, 194, 0.45);
      box-shadow: inset 0 0 18px rgba(0, 0, 0, 0.72);
    }

    .field {
      padding: 10px;
      text-align: center;
    }

    .label {
      display: block;
      margin-bottom: 4px;
      color: var(--display-dim);
      font: 700 12px/1 Verdana, sans-serif;
      letter-spacing: 0.12em;
    }

    .value {
      font: 700 42px/1 "Courier New", monospace;
      letter-spacing: 0.08em;
    }

    .registers {
      display: grid;
      gap: 8px;
      margin-bottom: 12px;
    }

    .register {
      display: grid;
      grid-template-columns: 54px 1fr;
      align-items: center;
      padding: 10px 12px;
    }

    .register strong {
      color: var(--display-dim);
      font: 700 13px/1 Verdana, sans-serif;
      letter-spacing: 0.10em;
    }

    .register span {
      text-align: right;
      font: 700 34px/1 "Courier New", monospace;
      letter-spacing: 0.08em;
    }

    .lamp-grid {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 8px;
      margin: 12px 0;
    }

    .lamp {
      border-radius: 999px;
      padding: 8px 10px;
      background: #151b1c;
      color: #596763;
      font: 700 12px/1 Verdana, sans-serif;
      letter-spacing: 0.06em;
      text-align: center;
    }

    .lamp.on {
      background: radial-gradient(circle at 25% 15%, #fff0ba, var(--amber) 42%, #a86500);
      color: #261900;
      box-shadow: 0 0 16px rgba(255, 191, 71, 0.55);
    }

    .meta {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 8px;
      padding: 10px;
      color: var(--muted);
      font: 13px/1.35 Verdana, sans-serif;
    }

    .phase-row {
      grid-column: 1 / -1;
    }

    .meta b {
      color: var(--display-hot);
      font-family: "Courier New", monospace;
    }

    .keys {
      padding: 16px;
    }

    .key-grid {
      display: grid;
      grid-template-columns: repeat(4, 1fr);
      gap: 10px;
    }

    button {
      appearance: none;
      min-height: 54px;
      border: 0;
      border-radius: 12px;
      background: linear-gradient(180deg, #f3eddd, var(--key));
      color: var(--key-text);
      box-shadow: 0 5px 0 #8a8372, 0 12px 20px rgba(0, 0, 0, 0.25);
      cursor: pointer;
      font: 800 16px/1 Verdana, sans-serif;
      letter-spacing: 0.05em;
    }

    button:active {
      transform: translateY(4px);
      box-shadow: 0 1px 0 #8a8372, 0 6px 10px rgba(0, 0, 0, 0.25);
    }

    button.action {
      background: linear-gradient(180deg, #ffe1a3, #d99223);
      box-shadow: 0 5px 0 #81500e, 0 12px 20px rgba(0, 0, 0, 0.25);
    }

    button.danger {
      background: linear-gradient(180deg, #ff9a8e, #cc463c);
      box-shadow: 0 5px 0 #7b211d, 0 12px 20px rgba(0, 0, 0, 0.25);
    }

    .console {
      grid-column: 1 / -1;
      padding: 14px;
      font: 13px/1.45 "Courier New", monospace;
      color: var(--muted);
    }

    .console form {
      display: flex;
      gap: 8px;
      margin-top: 10px;
    }

    input {
      min-width: 0;
      flex: 1;
      border: 1px solid #344346;
      border-radius: 10px;
      background: #0b1112;
      color: var(--ink);
      padding: 12px;
      font: 14px/1 "Courier New", monospace;
    }

    .status-dot {
      width: 12px;
      height: 12px;
      border-radius: 50%;
      background: var(--red);
      box-shadow: 0 0 14px rgba(255, 95, 87, 0.7);
      display: inline-block;
      margin-right: 8px;
      vertical-align: -1px;
    }

    .status-dot.ok {
      background: var(--green);
      box-shadow: 0 0 14px rgba(56, 212, 122, 0.7);
    }

    @media (max-width: 760px) {
      .shell {
        grid-template-columns: 1fr;
        padding: 12px;
      }

      .value {
        font-size: 34px;
      }

      .register span {
        font-size: 28px;
      }
    }
  </style>
</head>
<body>
  <main>
    <div class="topbar">
      <div><span id="httpDot" class="status-dot"></span><span id="httpStatus">connecting</span></div>
      <div id="ipInfo">AGC Web DSKY</div>
    </div>

    <section class="shell" aria-label="AGC Web DSKY">
      <section class="display">
        <div class="readout">
          <div class="field"><span class="label">PROG</span><span id="program" class="value">00</span></div>
          <div class="field"><span class="label">VERB</span><span id="verb" class="value">16</span></div>
          <div class="field"><span class="label">NOUN</span><span id="noun" class="value">36</span></div>
        </div>

        <div class="registers">
          <div class="register"><strong id="r1Name">R1</strong><span id="r1">+00000</span></div>
          <div class="register"><strong id="r2Name">R2</strong><span id="r2">+00000</span></div>
          <div class="register"><strong id="r3Name">R3</strong><span id="r3">+00000</span></div>
        </div>

        <div class="lamp-grid" id="lamps">
          <div class="lamp" data-bit="0">COMP ACTY</div>
          <div class="lamp" data-bit="1">UPLINK ACTY</div>
          <div class="lamp" data-bit="2">TEMP</div>
          <div class="lamp" data-bit="3">GIMBAL LOCK</div>
          <div class="lamp" data-bit="4">PROG</div>
          <div class="lamp" data-bit="5">KEY REL</div>
          <div class="lamp" data-bit="6">OPR ERR</div>
          <div class="lamp" data-bit="7">STBY</div>
          <div class="lamp" data-bit="8">NO ATT</div>
          <div class="lamp" data-bit="9">TRACKER</div>
        </div>

        <div class="meta">
          <div class="phase-row">PHASE <b id="phase">IDLE</b></div>
          <div>ALARM <b id="alarm">0000</b></div>
          <div>LINK <b id="link">DOWN</b></div>
          <div>SEC <b id="cycles">0</b></div>
          <div>LAST KEY <b id="lastKey">NONE</b></div>
        </div>
      </section>

      <section class="keys">
        <div class="key-grid">
          <button class="action" data-key="VERB">VERB</button>
          <button class="action" data-key="NOUN">NOUN</button>
          <button data-key="PLUS">+</button>
          <button data-key="MINUS">-</button>
          <button data-key="7">7</button>
          <button data-key="8">8</button>
          <button data-key="9">9</button>
          <button class="danger" data-key="CLR">CLR</button>
          <button data-key="4">4</button>
          <button data-key="5">5</button>
          <button data-key="6">6</button>
          <button class="action" data-key="PRO">PRO</button>
          <button data-key="1">1</button>
          <button data-key="2">2</button>
          <button data-key="3">3</button>
          <button class="action" data-key="KEYREL">KEY REL</button>
          <button data-key="0">0</button>
          <button class="action" data-key="ENTR">ENTR</button>
          <button class="danger" data-key="RSET">RSET</button>
          <button id="refreshButton" type="button">STATUS</button>
        </div>
      </section>

      <section class="console">
        <div id="consoleLine">Waiting for ESP8266...</div>
        <form id="rawForm">
          <input id="rawInput" autocomplete="off" placeholder="Raw command to ESP32, example: STATUS">
          <button type="submit">SEND</button>
        </form>
      </section>
    </section>
  </main>

  <script>
    const ids = ["program", "verb", "noun", "r1", "r2", "r3", "r1Name", "r2Name", "r3Name", "phase", "alarm", "link", "cycles", "lastKey"];
    const el = Object.fromEntries(ids.map((id) => [id, document.getElementById(id)]));
    const httpDot = document.getElementById("httpDot");
    const httpStatus = document.getElementById("httpStatus");
    const ipInfo = document.getElementById("ipInfo");
    const consoleLine = document.getElementById("consoleLine");
    const rawForm = document.getElementById("rawForm");
    const rawInput = document.getElementById("rawInput");

    function pad2(value) {
      return String(value ?? 0).padStart(2, "0");
    }

    function padAlarm(value) {
      return String(value ?? 0).padStart(4, "0");
    }

    function setHttp(ok, text) {
      httpDot.classList.toggle("ok", ok);
      httpStatus.textContent = text;
    }

    function render(state) {
      el.program.textContent = pad2(state.program);
      el.verb.textContent = pad2(state.verb);
      el.noun.textContent = pad2(state.noun);
      el.r1.textContent = state.r1 || "+00000";
      el.r2.textContent = state.r2 || "+00000";
      el.r3.textContent = state.r3 || "+00000";
      el.r1Name.textContent = state.r1Label || "R1";
      el.r2Name.textContent = state.r2Label || "R2";
      el.r3Name.textContent = state.r3Label || "R3";
      el.phase.textContent = state.phase || "IDLE";
      el.alarm.textContent = padAlarm(state.alarm);
      el.link.textContent = state.link ? "UP" : "DOWN";
      el.cycles.textContent = state.missionSeconds ?? 0;
      el.lastKey.textContent = state.lastKey || "NONE";

      document.querySelectorAll("[data-bit]").forEach((lamp) => {
        const bit = Number(lamp.dataset.bit);
        lamp.classList.toggle("on", (state.lamps & (1 << bit)) !== 0);
      });

      ipInfo.textContent = `${state.mode || "?"} ${state.ip || ""}`;
      consoleLine.textContent = `Wi-Fi ${state.wifi || "unknown"} | flash ${state.flash ? "yes" : "no"} | uptime ${Math.floor((state.uptimeMs || 0) / 1000)}s`;
    }

    async function refresh() {
      try {
        const response = await fetch("/api/state", { cache: "no-store" });
        if (!response.ok) {
          throw new Error(`HTTP ${response.status}`);
        }
        render(await response.json());
        setHttp(true, "connected to ESP8266");
      } catch (error) {
        setHttp(false, `lost web link: ${error.message}`);
      }
    }

    async function sendKey(key) {
      await fetch(`/api/key?name=${encodeURIComponent(key)}`, { method: "POST" });
      await refresh();
    }

    async function sendRaw(line) {
      await fetch(`/api/raw?line=${encodeURIComponent(line)}`, { method: "POST" });
      await refresh();
    }

    document.querySelectorAll("[data-key]").forEach((button) => {
      button.addEventListener("click", () => {
        sendKey(button.dataset.key).catch((error) => setHttp(false, error.message));
      });
    });

    document.getElementById("refreshButton").addEventListener("click", refresh);

    rawForm.addEventListener("submit", (event) => {
      event.preventDefault();
      const line = rawInput.value.trim();
      rawInput.value = "";
      if (line) {
        sendRaw(line).catch((error) => setHttp(false, error.message));
      }
    });

    refresh();
    setInterval(refresh, 500);
  </script>
</body>
</html>
)DSKYHTML";

#endif
