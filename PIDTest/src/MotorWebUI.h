/**
 * ============================================================================
 * 檔案: MotorWebUI.h
 * 描述: PID 步階響應測試專用 Web 介面 (Ollie System - 三色極簡版)
 * 更新: 
 * 1. 更新 PID 預設參數為 kp=2.0, ki=5.0, kd=0.001
 * 2. 視覺收斂為三色系：碳黑、螢光綠 (Left)、亮青藍 (Right)
 * 3. 強化圖表與 Log 的顏色對應性與滾動條修正
 * ============================================================================
 */
#ifndef MOTOR_WEBUI_H
#define MOTOR_WEBUI_H

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="zh-TW"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Ollie PID Tuner</title><script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<style>
  :root { --bg: #121212; --panel: #1e1e1e; --left: #00ff66; --right: #00e5ff; --txt: #e0e0e0; --dim: #666; }
  body{background:var(--bg);color:var(--txt);font-family:monospace;margin:10px;display:flex;flex-direction:column;height:95vh;box-sizing:border-box;overflow:hidden;}
  .flex{display:flex;gap:15px;} .col{flex-direction:column;} .flex-1{flex:1;min-height:0;}
  .panel{background:var(--panel);border-radius:4px;padding:12px;display:flex;flex-direction:column;border:1px solid #2a2a2a;}
  .log-container{flex:1; display:flex; flex-direction:column; min-height:0;}
  .log-box{font-size:12px;overflow-y:scroll;overflow-x:hidden;white-space:pre;line-height:1.4;flex:1;background:#161616;padding:8px;border:1px solid #222;}
  .ctrl-row{display:flex;gap:20px;align-items:center;margin-bottom:10px;}
  .input-grp{display:flex;align-items:center;gap:8px;font-size:11px;color:var(--dim);}
  input{background:var(--bg);border:1px solid #333;color:var(--txt);width:50px;text-align:center;padding:4px;border-radius:2px;}
  button{background:#333;color:var(--txt);border:none;padding:0 30px;cursor:pointer;font-weight:bold;height:35px;border-radius:2px;transition:0.2s;}
  button:hover{background:#444;}
  button.active{background:var(--txt);color:var(--bg);}
  .label-l{color:var(--left);font-weight:bold;width:45px;}
  .label-r{color:var(--right);font-weight:bold;width:45px;}
  .label-t{color:var(--dim);font-weight:bold;width:45px;}
  /* 滾動條樣式 */
  .log-box::-webkit-scrollbar {width:4px;}
  .log-box::-webkit-scrollbar-track {background:transparent;}
  .log-box::-webkit-scrollbar-thumb {background:#444;border-radius:2px;}
</style></head>
<body>
  <div class="flex" style="justify-content:space-between;margin-bottom:8px;font-size:0.75rem;color:var(--dim);letter-spacing:1px;">
    <b>OLLIE PID TUNER v4.5</b><span>IP: <span id="ip"></span> | WS: <span id="ws">--</span></span>
  </div>
  
  <div class="flex flex-1" style="margin-bottom:10px; min-height:0;">
    <div class="panel flex-1" style="position:relative"><canvas id="chart"></canvas></div>
    <div class="panel" style="width:320px; min-height:0;">
      <div style="font-size:10px;color:var(--dim);border-bottom:1px solid #333;padding-bottom:5px;margin-bottom:5px;text-align:center;">TIME   | TAR | <span style="color:var(--left)">LEFT</span>  | <span style="color:var(--right)">RIGHT</span></div>
      <div class="log-container"><div id="logBox" class="log-box"></div></div>
    </div>
  </div>

  <div class="panel col">
    <div class="ctrl-row">
      <!-- 更新 PID 初始參數 -->
      <div class="input-grp"><span class="label-l">L-PID</span> P <input id="lkp" value="2.0"> I <input id="lki" value="5.0"> D <input id="lkd" value="0.001"></div>
      <div class="input-grp"><span class="label-r">R-PID</span> P <input id="rkp" value="2.0"> I <input id="rki" value="5.0"> D <input id="rkd" value="0.001"></div>
    </div>
    <div class="ctrl-row" style="margin-bottom:0;">
      <div class="input-grp"><span class="label-t">TEST</span> Start <input id="tStart" value="20"> End <input id="tEnd" value="100"> Step <input id="tStep" value="0"> Interval <input id="tInt" value="1000"> Duration <input id="tDur" value="5000"></div>
      <div style="flex:1;"></div>
      <button id="btn" onclick="toggle()">START TEST</button>
    </div>
  </div>

<script>
  const $ = id => document.getElementById(id);
  $('ip').innerText = location.hostname;
  let ws, run = false;
  
  const c = new Chart($('chart').getContext('2d'), {
    type: 'line',
    data: { datasets: [
      {label:'Target', borderColor:'#666', data:[], borderDash:[5,5], pointRadius:0, borderWidth:1, fill:false},
      {label:'Left', borderColor:'#00ff66', data:[], pointRadius:0, borderWidth:1.5, fill:false, tension:0.1},
      {label:'Right', borderColor:'#00e5ff', data:[], pointRadius:0, borderWidth:1.5, fill:false, tension:0.1}
    ]},
    options: { 
      animation:false, responsive:true, maintainAspectRatio:false,
      scales: { 
        x: { type:'linear', min:0, max:30000, grid:{color:'#252525'}, ticks:{display:false} }, 
        y: { min:0, max:300, grid:{color:'#252525'}, ticks:{color:'#555'} } 
      },
      plugins: { legend: { display:true, align:'end', labels:{color:'#888', boxWidth:12, font:{size:10}} } }
    }
  });

  function init() {
    ws = new WebSocket(`ws://${location.hostname}/ws`);
    ws.onopen = () => $('ws').innerText = 'ON';
    ws.onclose = () => { $('ws').innerText = 'OFF'; setTimeout(init, 2000); };
    ws.onmessage = e => {
      let p = e.data.split(',');
      if(p[0] === 'D') {
        let t=parseInt(p[1]), tar=parseFloat(p[2]), l=parseFloat(p[3]), r=parseFloat(p[4]);
        c.data.datasets[0].data.push({x:t, y:tar});
        c.data.datasets[1].data.push({x:t, y:l});
        c.data.datasets[2].data.push({x:t, y:r});
        
        let mx = t > 30000 ? t - 30000 : 0;
        c.options.scales.x.min = mx; c.options.scales.x.max = mx + 30000;
        if(c.data.datasets[0].data.length > 800) c.data.datasets.forEach(d => d.data.shift());
        c.update('none');

        const row = `${String(t).padStart(6)} | ${String(Math.round(tar)).padStart(3)} | <span style="color:var(--left)">${l.toFixed(1).padStart(5)}</span> | <span style="color:var(--right)">${r.toFixed(1).padStart(5)}</span>\n`;
        const div = document.createElement('div'); div.innerHTML = row;
        $('logBox').appendChild(div);
        if($('logBox').childNodes.length > 100) $('logBox').removeChild($('logBox').firstChild);
        $('logBox').scrollTop = $('logBox').scrollHeight;
      } else if(p[0] === 'DONE') setUI(false);
    };
  }

  function toggle() {
    if(!run) {
      c.data.datasets.forEach(d=>d.data=[]); c.update('none'); $('logBox').innerHTML='';
      let v = ['lkp','lki','lkd','rkp','rki','rkd','tStart','tEnd','tStep','tInt','tDur'].map(i=>$(i).value).join(',');
      ws.send("START,"+v); setUI(true);
    } else { ws.send('STOP'); setUI(false); }
  }

  function setUI(s) { run=s; $('btn').innerText = s?"STOP":"START TEST"; $('btn').className = s?"active":""; }
  window.onload = init;
</script></body></html>
)rawliteral";

class MotorWebUI {
private:
    AsyncWebServer server;
    AsyncWebSocket ws;
public:
    MotorWebUI() : server(80), ws("/ws") {}
    void begin(const char* ssid, const char* pass) {
        WiFi.begin(ssid, pass);
        while (WiFi.status() != WL_CONNECTED) delay(500);

        Serial.println(WiFi.localIP());
        server.addHandler(&ws);
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){ r->send(200, "text/html", index_html); });
        server.begin();
    }
    void broadcastData(unsigned long t, float target, float left, float right) {
        if (ws.count() > 0 && ws.availableForWriteAll()) {
            char buf[128];
            snprintf(buf, sizeof(buf), "D,%lu,%.1f,%.1f,%.1f", t, target, left, right);
            ws.textAll(buf);
        }
    }
    void notifyDone() { ws.textAll("DONE"); }
    void cleanup() { ws.cleanupClients(); }
    AsyncWebSocket* getWS() { return &ws; }
};
#endif