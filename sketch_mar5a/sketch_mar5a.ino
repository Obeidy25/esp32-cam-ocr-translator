#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// ===== Network Settings =====
const char* ssid = "aa";
const char* password = "123456789";

// ===== Python Server Address =====
#define SERVER_HOST "192.168.9.111"
#define SERVER_PORT 5000

// ===== Camera Pins (AI-Thinker ESP32-CAM) =====
#define FLASH_LED_PIN     4
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ===== Connection Settings =====
#define WIFI_MAX_RETRIES     20
#define WIFI_RECONNECT_MS    10000
#define SERVER_TIMEOUT_MS    15000

WebServer server(80);
unsigned long lastWiFiCheck = 0;
framesize_t currentFramesize = FRAMESIZE_QVGA;

// Function Prototypes
void startCamera();
void connectToWiFi();
void checkWiFiConnection();
void handleRoot();
void handleCapture();
void handleStream();
void handleLed();
void handleResolution();
void sendToServer(uint8_t * buf, size_t len, String provider);

void startCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA; // Smooth stream at 320x240
  config.jpeg_quality = 12;           // Compression for Wi-Fi speed
  config.fb_count = 3;                // 3 buffers for smoother streaming
  config.grab_mode = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[FAIL] Camera init: 0x%x\n", err);
    delay(5000);
    ESP.restart();
  }
  Serial.println("[OK] Camera initialized");
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < WIFI_MAX_RETRIES) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[OK] WiFi Connected");
    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());
    Serial.printf("Server: %s:%d\n", SERVER_HOST, SERVER_PORT);
  } else {
    Serial.println("\n[FAIL] WiFi failed, restarting...");
    delay(5000);
    ESP.restart();
  }
}

void checkWiFiConnection() {
  if (millis() - lastWiFiCheck < WIFI_RECONNECT_MS) return;
  lastWiFiCheck = millis();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WARN] WiFi lost, reconnecting...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    int a = 0;
    while (WiFi.status() != WL_CONNECTED && a < 10) { delay(500); a++; }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("[OK] WiFi reconnected");
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\nESP32-CAM OCR Translator v2.0");
  Serial.println("==============================");

  startCamera();
  connectToWiFi();

  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);

  server.on("/", handleRoot);
  server.on("/capture", handleCapture);
  server.on("/stream", handleStream);
  server.on("/led", handleLed);
  server.on("/res", handleResolution);
  server.begin();

  // Start mDNS so device is always at http://esp32cam.local
  if (MDNS.begin("esp32cam")) {
    Serial.println("[OK] mDNS: http://esp32cam.local");
  }

  Serial.printf("[OK] Web UI: http://%s\n", WiFi.localIP().toString().c_str());
}

void loop() {
  server.handleClient();
  checkWiFiConnection();
}

// ==================== WEB UI ====================

const char* WEB_HTML = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>OCR Translator</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
:root{
  --bg:#0f0f1a;
  --card:#1a1a2e;
  --card2:#16213e;
  --accent:#7c3aed;
  --accent2:#a78bfa;
  --green:#10b981;
  --red:#ef4444;
  --text:#e2e8f0;
  --text2:#94a3b8;
  --border:#2d2d44;
}
body{
  background:var(--bg);
  color:var(--text);
  font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;
  min-height:100vh;
  display:flex;
  align-items:center;
  justify-content:center;
  padding:20px;
}
.app{max-width:600px;width:100%}
.header{text-align:center;margin-bottom:24px}
.header h1{
  font-size:24px;font-weight:700;
  background:linear-gradient(135deg,var(--accent),var(--accent2));
  -webkit-background-clip:text;-webkit-text-fill-color:transparent;
}
.header p{color:var(--text2);font-size:13px;margin-top:4px}
.camera-card{
  background:var(--card);
  border:1px solid var(--border);
  border-radius:16px;
  overflow:hidden;
  margin-bottom:16px;
}
.camera-view{
  width:100%;aspect-ratio:4/3;
  background:#000;
  display:flex;align-items:center;justify-content:center;
  position:relative;
}
.camera-view img{width:100%;height:100%;object-fit:cover}
.camera-view .ph{color:var(--text2);font-size:14px}
.controls{padding:16px;display:flex;gap:10px;align-items:center;flex-wrap:wrap;justify-content:center}
.model-select{
  flex:1;padding:10px 12px;
  background:var(--card2);color:var(--text);
  border:1px solid var(--border);border-radius:10px;
  font-size:14px;font-family:inherit;
  cursor:pointer;appearance:none;
  background-image:url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='12' fill='%2394a3b8' viewBox='0 0 16 16'%3E%3Cpath d='M8 11L3 6h10z'/%3E%3C/svg%3E");
  background-repeat:no-repeat;
  background-position:right 12px center;
  padding-right:32px;
}
.model-select:focus{outline:none;border-color:var(--accent)}
.quality-select{max-width:110px}
.btn{
  padding:10px 24px;border:none;border-radius:10px;
  font-size:14px;font-weight:600;cursor:pointer;
  font-family:inherit;transition:all .2s;
  display:flex;align-items:center;gap:6px;
}
.btn-flash{
  background:var(--card2);color:var(--text2);
  padding:10px;
}
.btn-flash:hover{color:var(--text)}
.btn-flash.on{background:var(--accent);color:#fff}
.btn-capture{
  background:linear-gradient(135deg,var(--accent),#6d28d9);
  color:#fff;
}
.btn-capture:hover{transform:translateY(-1px);box-shadow:0 8px 20px rgba(124,58,237,.3)}
.btn-capture:active{transform:translateY(0)}
.btn-capture:disabled{opacity:.5;cursor:not-allowed;transform:none}
.result-card{
  background:var(--card);
  border:1px solid var(--border);
  border-radius:16px;
  overflow:hidden;
  margin-bottom:12px;
  display:none;
}
.result-card.show{display:block;animation:fadeIn .3s ease}
@keyframes fadeIn{from{opacity:0;transform:translateY(8px)}to{opacity:1;transform:translateY(0)}}
.result-header{
  padding:12px 16px;
  border-bottom:1px solid var(--border);
  display:flex;align-items:center;justify-content:space-between;
}
.result-header .label{font-size:12px;font-weight:600;text-transform:uppercase;letter-spacing:.5px;color:var(--text2)}
.result-header .badge{
  font-size:11px;padding:3px 8px;border-radius:20px;
  background:rgba(124,58,237,.15);color:var(--accent2);
}
.result-body{padding:16px}
.result-body .text{font-size:15px;line-height:1.7;word-wrap:break-word}
.result-body .text.original{color:var(--text2);font-style:italic}
.result-body .text.translated{color:var(--text)}
.loading-card{
  background:var(--card);
  border:1px solid var(--border);
  border-radius:16px;
  padding:24px;text-align:center;
  display:none;margin-bottom:12px;
}
.loading-card.show{display:block}
.spinner{
  width:32px;height:32px;margin:0 auto 12px;
  border:3px solid var(--border);
  border-top-color:var(--accent);
  border-radius:50%;
  animation:spin .8s linear infinite;
}
@keyframes spin{to{transform:rotate(360deg)}}
.loading-card p{color:var(--text2);font-size:14px}
.status{
  text-align:center;font-size:12px;
  color:var(--text2);margin-top:8px;
  display:flex;align-items:center;justify-content:center;gap:6px;
}
.dot{width:6px;height:6px;border-radius:50%;background:var(--green);display:inline-block}
.dot.off{background:var(--red)}
.act-btn{
  padding:6px 12px;border:1px solid var(--border);border-radius:8px;
  background:var(--card2);color:var(--text2);font-size:12px;
  cursor:pointer;font-family:inherit;transition:all .2s;
  display:flex;align-items:center;gap:4px;
}
.act-btn:hover{color:var(--text);border-color:var(--accent)}
.act-btn.ok{border-color:var(--green);color:var(--green)}
.act-btn.sending{opacity:.6;pointer-events:none}
.act-btn.err{border-color:var(--red);color:var(--red)}
.actions{display:flex;gap:8px;padding:8px 16px;border-top:1px solid var(--border);flex-wrap:wrap}
</style>
</head>
<body>
<div class="app">
  <div class="header">
    <h1>OCR Translator</h1>
    <p>Capture &bull; Extract &bull; Translate</p>
  </div>

  <div class="camera-card">
    <div class="camera-view">
      <img id="stream" style="display:none"/>
      <span class="ph" id="ph">Loading camera...</span>
    </div>
    <div class="controls">
      <select class="model-select" id="modelSelect" onchange="verifyCurrentProvider()">
        <option value="">Loading models...</option>
      </select>
      <select class="model-select" id="voiceSelect" style="max-width:120px">
        <option value="">Voice...</option>
      </select>
      <button class="btn btn-flash" id="flashBtn" onclick="toggleFlash()" title="Flashlight">
        <svg width="20" height="20" fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" d="M13 10V3L4 14h7v7l9-11h-7z"/></svg>
      </button>
      <select class="model-select quality-select" id="resSelect" onchange="changeResolution()">
        <option value="QVGA">Fast</option>
        <option value="VGA">Normal</option>
        <option value="SVGA">High</option>
      </select>
      <button class="btn btn-capture" id="captureBtn" onclick="capture()">
        <svg width="16" height="16" fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24"><circle cx="12" cy="12" r="10"/><circle cx="12" cy="12" r="4"/></svg>
        Capture
      </button>
    </div>
  </div>

  <div class="loading-card" id="loading">
    <div class="spinner"></div>
    <p>Processing with AI...</p>
  </div>

  <div class="result-card" id="originalCard">
    <div class="result-header">
      <span class="label">Extracted Text</span>
      <span class="badge" id="langBadge"></span>
    </div>
    <div class="result-body">
      <div class="text original" id="originalText"></div>
    </div>
    <div class="actions">
      <button class="act-btn" onclick="copyText('originalText',this)">📋 Copy</button>
      <button class="act-btn" onclick="speakText('originalText')">🔊 Listen</button>
      <button class="act-btn" onclick="sendTelegram('originalText','Extracted Text',this)">✈️ Telegram</button>
      <button class="act-btn" onclick="sendEmail('originalText','Extracted Text')">✉️ Email</button>
    </div>
  </div>

  <div class="result-card" id="translationCard">
    <div class="result-header">
      <span class="label">Translation</span>
      <span class="badge" id="providerBadge"></span>
    </div>
    <div class="result-body">
      <div class="text translated" id="translatedText"></div>
    </div>
    <div class="actions">
      <button class="act-btn" onclick="copyText('translatedText',this)">📋 Copy</button>
      <button class="act-btn" onclick="speakText('translatedText')">🔊 Listen</button>
      <button class="act-btn" onclick="sendTelegram('translatedText','Translation',this)">✈️ Telegram</button>
      <button class="act-btn" onclick="sendEmail('translatedText','Translation')">✉️ Email</button>
    </div>
  </div>

  <div class="status" id="statusBar">
    <span class="dot" id="dot"></span>
    <span id="statusText">Ready</span>
  </div>
</div>

<script>
var refreshTimer=null;
var serverBase='http://__SERVER_ADDRESS__';

function refreshStream(){
  var img=document.getElementById('stream');
  var ph=document.getElementById('ph');
  img.onload=function(){ph.style.display='none';img.style.display='block'};
  img.onerror=function(){ph.textContent='Camera offline';ph.style.display='block';img.style.display='none'};
  img.src='/stream?'+Date.now();
}

function loadProviders(){
  fetch(serverBase+'/providers')
    .then(function(r){return r.json()})
    .then(function(d){
      var sel=document.getElementById('modelSelect');
      sel.innerHTML='';
      if(d.providers.length===0){
        sel.innerHTML='<option value="">No AI models configured</option>';
        document.getElementById('captureBtn').disabled=true;
        return;
      }
      d.providers.forEach(function(p){
        var opt=document.createElement('option');
        opt.value=p.id;opt.textContent=p.name;
        if(p.id===d.default)opt.selected=true;
        sel.appendChild(opt);
      });
      verifyCurrentProvider();
    })
    .catch(function(){
      document.getElementById('modelSelect').innerHTML='<option value="">Server offline</option>';
      document.getElementById('dot').className='dot off';
      document.getElementById('statusText').textContent='Server unreachable';
    });
}

var voiceLoadAttempts = 0;
function loadVoices(){
  var voices=speechSynthesis.getVoices();
  var sel=document.getElementById('voiceSelect');
  
  // Aggressively retry if voices aren't loaded yet (Chrome/Android bug)
  if(voices.length===0 && voiceLoadAttempts < 15){
    voiceLoadAttempts++;
    setTimeout(loadVoices, 300);
    return;
  }
  
  sel.innerHTML='<option value="">Auto Voice</option>';
  if(voices.length===0) return;
  
  var hasArabic = false;
  voices.forEach(function(v, i){
    if(v.lang.startsWith('ar')) hasArabic = true;
    var opt=document.createElement('option');
    opt.value=i;
    var shortName = v.name.split(' ').slice(0,2).join(' ');
    opt.textContent=shortName+' ('+v.lang.split('-')[0]+')';
    sel.appendChild(opt);
  });
  
  // Show explicit warning if OS lacks Arabic pack
  if(voices.length > 0 && !hasArabic){
    var opt=document.createElement('option');
    opt.disabled = true;
    opt.textContent="⚠ No Arabic installed on device";
    sel.appendChild(opt);
  }
}
// Some browsers load voices asynchronously
if(speechSynthesis.onvoiceschanged !== undefined) {
  speechSynthesis.onvoiceschanged = loadVoices;
}

function verifyCurrentProvider(){
  var provider=document.getElementById('modelSelect').value;
  if(!provider)return;
  var btn=document.getElementById('captureBtn');
  btn.disabled=true;
  setStatus('off','Verifying API Key...');
  
  fetch(serverBase+'/verify?provider='+encodeURIComponent(provider))
    .then(function(r){return r.json()})
    .then(function(d){
      if(d.success){
        setStatus('on','API Ready');
        btn.disabled=false;
      } else {
        setStatus('off','API Error: '+d.message);
      }
    })
    .catch(function(e){
      setStatus('off','Verify Error: Server unreachable');
    });
}

var flashOn=false;
function toggleFlash(){
  flashOn=!flashOn;
  var btn=document.getElementById('flashBtn');
  if(flashOn){
    btn.classList.add('on');
    fetch('/led?state=on');
  } else {
    btn.classList.remove('on');
    fetch('/led?state=off');
  }
}

function capture(){
  var btn=document.getElementById('captureBtn');
  btn.disabled=true;
  document.getElementById('loading').classList.add('show');
  document.getElementById('originalCard').classList.remove('show');
  document.getElementById('translationCard').classList.remove('show');
  setStatus('on','Processing...');
  if(refreshTimer)clearInterval(refreshTimer);

  var provider=document.getElementById('modelSelect').value;

  fetch('/capture?provider='+encodeURIComponent(provider))
    .then(function(r){return r.json()})
    .then(function(d){
      document.getElementById('loading').classList.remove('show');
      if(d.success){
        if(d.text){
          document.getElementById('originalText').textContent=d.text;
          document.getElementById('langBadge').textContent=d.source_language||'';
          document.getElementById('originalCard').classList.add('show');
        }
        if(d.translated_text){
          document.getElementById('translatedText').textContent=d.translated_text;
          document.getElementById('providerBadge').textContent=d.provider||'';
          document.getElementById('translationCard').classList.add('show');
        }
        var c=d.confidence?(d.confidence*100).toFixed(0)+'%':'';
        setStatus('on','Done'+(c?' | Confidence: '+c:''));
      } else {
        setStatus('off','Error: '+(d.error||'Failed'));
      }
      btn.disabled=false;
      refreshTimer=setInterval(refreshStream,500);
    })
    .catch(function(e){
      document.getElementById('loading').classList.remove('show');
      setStatus('off','Connection error');
      btn.disabled=false;
      refreshTimer=setInterval(refreshStream,500);
    });
}

function setStatus(state,msg){
  document.getElementById('dot').className='dot'+(state==='off'?' off':'');
  document.getElementById('statusText').textContent=msg;
}

function changeResolution(){
  var size=document.getElementById('resSelect').value;
  localStorage.setItem('esp_res',size);
  fetch('/res?size='+size);
}

function copyText(id,btn){
  var t=document.getElementById(id).textContent;
  function onSuccess(){
    btn.classList.add('ok');btn.textContent='\u2705 Copied!';
    setTimeout(function(){btn.classList.remove('ok');btn.textContent='\ud83d\udccb Copy'},2000);
  }
  function fallbackCopy(text){
    var ta=document.createElement('textarea');
    ta.value=text;
    ta.style.position='fixed';ta.style.left='-9999px';
    document.body.appendChild(ta);
    ta.select();
    try{document.execCommand('copy');onSuccess();}catch(e){alert('Copy failed');}
    document.body.removeChild(ta);
  }
  if(navigator.clipboard&&window.isSecureContext){
    navigator.clipboard.writeText(t).then(onSuccess).catch(function(){fallbackCopy(t);});
  } else {
    fallbackCopy(t);
  }
}

function speakText(id){
  var t=document.getElementById(id).textContent;
  if(!t)return;
  speechSynthesis.cancel();
  var u=new SpeechSynthesisUtterance(t);
  
  var sel=document.getElementById('voiceSelect');
  var idx=sel.value;
  var voices=speechSynthesis.getVoices();
  
  if(idx !== "" && voices[idx]){
    // Use manually selected voice
    u.voice=voices[idx];
    u.lang=voices[idx].lang;
  } else {
    // Fallback to auto-detection if "Auto Voice" is selected
    var isArabic=/[\u0600-\u06FF]/.test(t);
    if(isArabic){
      u.lang='ar';
      for(var i=0;i<voices.length;i++){
        if(voices[i].lang.startsWith('ar')){
          u.voice=voices[i];
          break;
        }
      }
    } else {
      u.lang='en';
    }
  }
  
  u.rate=0.9;
  speechSynthesis.speak(u);
}

function sendTelegram(id,label,btn){
  var t=document.getElementById(id).textContent;
  if(!t)return;
  var orig=btn.textContent;
  btn.textContent='⏳ Sending...';
  btn.classList.add('sending');
  fetch(serverBase+'/send-telegram',{
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify({text:t,label:label})
  })
  .then(function(r){return r.json()})
  .then(function(d){
    btn.classList.remove('sending');
    if(d.success){
      btn.classList.add('ok');btn.textContent='\u2705 Sent!';
      setTimeout(function(){btn.classList.remove('ok');btn.textContent=orig},2000);
    } else {
      btn.classList.add('err');btn.textContent='\u274c '+(d.error||'Failed');
      setTimeout(function(){btn.classList.remove('err');btn.textContent=orig},3000);
    }
  })
  .catch(function(){
    btn.classList.remove('sending');
    btn.classList.add('err');btn.textContent='\u274c Server offline';
    setTimeout(function(){btn.classList.remove('err');btn.textContent=orig},3000);
  });
}

function sendEmail(id,label){
  var t=document.getElementById(id).textContent;
  if(!t)return;
  var subject=encodeURIComponent('OCR Translator - '+label);
  var body=encodeURIComponent(label+':\n\n'+t);
  window.location.href='mailto:?subject='+subject+'&body='+body;
}

window.onload=function(){
  var savedRes=localStorage.getItem('esp_res');
  if(savedRes){
    document.getElementById('resSelect').value=savedRes;
    fetch('/res?size='+savedRes);
  }
  refreshStream();
  refreshTimer=setInterval(refreshStream,500);
  loadProviders();
  // Attempt to load voices immediately (works on some browsers)
  setTimeout(loadVoices, 500);
};
</script>
</body>
</html>
)rawhtml";

void handleRoot() {
  String html = String(WEB_HTML);
  html.replace("__SERVER_ADDRESS__", String(SERVER_HOST) + ":" + String(SERVER_PORT));
  server.send(200, "text/html; charset=utf-8", html);
}

// ==================== CAPTURE ====================

void handleCapture() {
  String provider = server.arg("provider");

  sensor_t * s = esp_camera_sensor_get();

  // Upgrade to VGA for OCR ONLY if current is lower than VGA
  if (currentFramesize < FRAMESIZE_VGA) {
    Serial.print("[CAP] Upgrading to VGA for capture...");
    s->set_framesize(s, FRAMESIZE_VGA);
    delay(500);
  }

  // Flush 2 frames to get fresh one
  camera_fb_t * fb = esp_camera_fb_get(); if(fb) esp_camera_fb_return(fb);
  fb = esp_camera_fb_get(); if(fb) esp_camera_fb_return(fb);
  fb = esp_camera_fb_get();

  if (!fb) {
    s->set_framesize(s, currentFramesize);
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Camera capture failed\"}");
    return;
  }

  Serial.printf("[OK] Captured %d bytes\n", fb->len);
  sendToServer(fb->buf, fb->len, provider);
  esp_camera_fb_return(fb);

  s->set_framesize(s, currentFramesize);
  Serial.println("[CAP] Reverted to user resolution");
}

// ==================== SEND TO SERVER ====================

void sendToServer(uint8_t * buf, size_t len, String provider) {
  WiFiClient client;
  client.setTimeout(SERVER_TIMEOUT_MS);

  if (!client.connect(SERVER_HOST, SERVER_PORT)) {
    Serial.println("[FAIL] Server connection failed");
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Server connection failed\"}");
    return;
  }

  // Build path with provider param
  String path = "/process-image";
  if (provider.length() > 0) {
    path += "?provider=" + provider;
  }

  String header = "POST " + path + " HTTP/1.1\r\n";
  header += "Host: " + String(SERVER_HOST) + "\r\n";
  header += "Content-Type: image/jpeg\r\n";
  header += "Content-Length: " + String(len) + "\r\n";
  header += "Connection: close\r\n\r\n";

  client.print(header);

  // Send in chunks
  size_t sent = 0;
  while (sent < len) {
    size_t chunk = min((size_t)1024, len - sent);
    client.write(buf + sent, chunk);
    sent += chunk;
  }

  Serial.printf("[OK] Sent %d bytes\n", sent);

  // Read response with timeout
  String response = "";
  bool headersEnded = false;
  unsigned long timeout = millis() + SERVER_TIMEOUT_MS;

  while ((client.connected() || client.available()) && millis() < timeout) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      if (!headersEnded) {
        if (line == "\r" || line.length() == 0) headersEnded = true;
      } else {
        response += line + "\n";
      }
    } else {
      delay(10);
    }
  }
  client.stop();

  if (millis() >= timeout) {
    server.send(504, "application/json", "{\"success\":false,\"error\":\"AI processing timeout\"}");
    return;
  }

  if (response.length() > 0) {
    server.send(200, "application/json", response);
  } else {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"No response from server\"}");
  }
}

// ==================== STREAM ====================

void handleStream() {
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Camera failed");
    return;
  }
  server.setContentLength(fb->len);
  server.send(200, "image/jpeg");
  server.client().write(fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

// ==================== RESOLUTION ====================

void handleResolution() {
  String size = server.arg("size");
  sensor_t * s = esp_camera_sensor_get();
  if (size == "QVGA") { currentFramesize = FRAMESIZE_QVGA; s->set_framesize(s, FRAMESIZE_QVGA); }
  else if (size == "VGA") { currentFramesize = FRAMESIZE_VGA; s->set_framesize(s, FRAMESIZE_VGA); }
  else if (size == "SVGA") { currentFramesize = FRAMESIZE_SVGA; s->set_framesize(s, FRAMESIZE_SVGA); }
  server.send(200, "text/plain", "OK");
}

// ==================== LED ====================

void handleLed() {
  String state = server.arg("state");
  if (state == "on") {
    digitalWrite(FLASH_LED_PIN, HIGH);
  } else {
    digitalWrite(FLASH_LED_PIN, LOW);
  }
  server.send(200, "text/plain", "OK");
}
