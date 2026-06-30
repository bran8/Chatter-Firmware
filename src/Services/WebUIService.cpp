#include "WebUIService.h"
#include <Loop/LoopManager.h>
#include "../Storage/Storage.h"
#include "MessageService.h"
#include "ProfileService.h"
#include "BuzzerService.h"
#include <Audio/Piezo.h>
#include <Battery/BatteryService.h>
#include <Settings.h>
#include "SleepService.h"
#include <LITTLEFS.h>

WebUIService WebUI;

// Short non-blocking melodies played through the buzzer. Durations are in ms;
// the sequencer in updateCue() leaves a small gap after each note so adjacent
// notes stay distinct. Frequencies are plain Hz (not musical-note macros) to
// keep this file self-contained.
static const WebUIService::CueNote BootCue[]       = {{523, 90}, {659, 90}, {784, 140}};  // C5-E5-G5 rising: AP is up
static const WebUIService::CueNote ConnectCue[]    = {{523, 80}, {784, 120}};             // C5-G5 rising: phone joined
static const WebUIService::CueNote DisconnectCue[] = {{784, 80}, {523, 120}};             // G5-C5 falling: phone left
static const WebUIService::CueNote LowBattCue[]    = {{660, 120}, {523, 120}, {392, 220}}; // E5-C5-G4 falling: pack draining

// Silence inserted after each note, in ms, so back-to-back notes are audible.
static const uint16_t CueGapMs = 40;

// Battery-percentage thresholds for the backup-power alert (with hysteresis).
static const uint8_t BattWarnPct = 25;
static const uint8_t BattCritPct = 8;
static const uint8_t BattClearMargin = 5;   // must recover this far above to re-arm

// USB-present detection for the shutdown-on-battery timer. getPercentage()
// clamps to 100% at 4500mV, so a reading at/above this means the charger is
// holding the rail up (USB connected); below it we're running on the pack.
static const uint16_t UsbPresentMv = 4500;

// Verbose serial logging: per-request traces + a 5s health/load heartbeat on
// the Serial Monitor (115200). Flip to false to silence it for production.
static const bool VerboseLog = true;

// Change this before flashing -- WPA2 requires at least 8 characters.
static const char* AP_PASSWORD = "chatterwifi";

static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Chatter</title>
<style>
body{font-family:sans-serif;margin:0;background:#111;color:#eee}
header{padding:10px;background:#222;display:flex;justify-content:space-between}
#friends,#convo{padding:10px}
.friend,.msg{padding:8px;margin:6px 0;border-radius:6px;background:#222}
.friend{display:flex;justify-content:space-between;cursor:pointer}
.msg.out{background:#264}
.msg.in{background:#225}
input,button{font-size:1em;padding:6px;margin:4px 0}
#compose{display:flex;gap:6px;padding:10px}
#compose input{flex:1}
a{color:#8cf}
.hidden{display:none}
.friend img,.av{width:42px;height:42px;border-radius:6px;vertical-align:middle;background:#333}
.frow{display:flex;align-items:center;gap:8px;flex:1;cursor:pointer;min-width:0}
.frow span{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.del{background:#622;color:#fdd;border:none;border-radius:6px;padding:4px 8px;cursor:pointer}
.msg{display:flex;justify-content:space-between;align-items:flex-start;gap:8px}
#banner{background:#622;color:#fee;padding:8px 10px;text-align:center;font-weight:bold}
#avgrid{display:grid;grid-template-columns:repeat(5,1fr);gap:8px;padding:10px}
#avgrid img{width:100%;height:auto;border-radius:8px;border:2px solid transparent;cursor:pointer}
#avgrid img.sel{border-color:#8cf}
.field{padding:0 10px}
.overlay{position:fixed;inset:0;background:rgba(0,0,0,.6);z-index:9}
.picker{position:fixed;bottom:0;left:0;right:0;background:#222;padding:12px;border-radius:12px 12px 0 0;z-index:10}
.pic-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:8px;margin-bottom:10px}
.pic-grid img{width:100%;aspect-ratio:1;border-radius:6px;cursor:pointer;border:2px solid #444;object-fit:cover}
.pic-grid img:active{border-color:#8cf}
.msg img{max-width:120px;border-radius:6px;display:block}
</style>
</head>
<body>
<header>
  <strong>Chatter</strong>
  <span><a href="#" onclick="showProfile();return false;">me</a> | <a href="#" onclick="showPair();return false;">pair</a> | <a href="#" onclick="silence();return false;">silence</a> | <a href="#" onclick="stopPending();return false;">stop sending</a></span>
</header>
<div id="banner" class="hidden"></div>
<div id="status" class="field" title="battery / pending / connected clients" style="opacity:.7;font-size:.85em">--</div>

<div id="friendsView">
  <div id="friends"></div>
  <button onclick="broadcast()">Broadcast message...</button>
</div>

<div id="profileView" class="hidden">
  <header><a href="#" onclick="showFriends();return false;">&larr; back</a> &nbsp; <strong>My profile</strong></header>
  <div class="field"><label>Name <input id="profName" maxlength="20"></label></div>
  <div class="field"><label>Color hue <input id="profHue" type="range" min="0" max="359"></label></div>
  <p class="field">Pick an avatar:</p>
  <div id="avgrid"></div>
  <div class="field"><button onclick="saveProfile()">Save profile</button></div>
</div>

<div id="convoView" class="hidden">
  <header><a href="#" onclick="showFriends();return false;">&larr; back</a></header>
  <div id="convo"></div>
  <div id="compose">
    <input id="composeText" placeholder="Message">
    <button onclick="send()">Send</button>
    <button onclick="showMemePicker()">Pic</button>
  </div>
</div>

<div id="memePicker" class="hidden">
  <div class="overlay" onclick="hideMemePicker()"></div>
  <div class="picker">
    <div class="pic-grid" id="picGrid"></div>
    <button onclick="hideMemePicker()" style="width:100%">Cancel</button>
  </div>
</div>

<div id="pairView" class="hidden">
  <header><a href="#" onclick="showFriends();return false;">&larr; back</a></header>
  <div id="pairList"></div>
</div>

<script>
let currentConvo = null;
let pairTimer = null;        // /api/pair/discovered scan poll (2s)
let pairStatusTimer = null;  // /api/pair/status result poll (1s)
let pairing = false;         // a pair-confirm is in progress (greys the buttons, blocks re-taps)
let selectedAvatar = 0;
let avatarLoadToken = 0;     // bumped each time the avatar grid loads; cancels a stale chain

// Sequential image loader. Same reason as the avatar grid: the device serves one
// connection at a time, so firing every <img> src at once opens parallel requests
// that stall behind each other and block /api/status, making the UI feel hung.
// Each channel keeps exactly one request in flight; a new load on the same channel
// bumps the token and cancels the stale chain.
let seqTokens = {};
function loadImagesSeq(channel, pairs){
  const token = (seqTokens[channel] = (seqTokens[channel] || 0) + 1);
  let n = 0;
  const next = ()=>{
    if(n >= pairs.length || token !== seqTokens[channel]) return;
    const p = pairs[n];
    p.img.onload = p.img.onerror = ()=>{ n++; next(); };
    p.img.src = p.src;
  };
  next();
}

// In-flight guards: a periodic poll skips its tick if its own previous request
// hasn't returned yet, so a briefly-slow (single-client) server can't let
// requests pile up and overwhelm the WebUI.
let statusBusy = false, pairBusy = false, pairStatusBusy = false;

function escapeHtml(s){
  return String(s).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
}

// Stop all pairing polls. Both timers are single-instance and cleared here on
// every navigation, so they can never leak/stack and hammer the device (a
// leaked status poll was firing many /api/pair/status hits per second).
function stopPairPolling(){
  if(pairTimer){ clearInterval(pairTimer); pairTimer = null; }
  if(pairStatusTimer){ clearInterval(pairStatusTimer); pairStatusTimer = null; }
  pairing = false;
  // Clear in-flight guards too, so a request that was still pending when we
  // tore down can't leave a flag stuck-true and block the next pairing session.
  pairBusy = false;
  pairStatusBusy = false;
}

function show(view){
  // Any view switch stops pairing polls so a leftover poll can't keep hitting
  // the device after you navigate away (showPair restarts them right after).
  stopPairPolling();
  ['friendsView','convoView','pairView','profileView'].forEach(v=>
    document.getElementById(v).classList.toggle('hidden', v!==view));
}

function showFriends(){
  show('friendsView');
  loadFriends();
}

function loadFriends(){
  fetch('/api/convos').then(r=>r.json()).then(convos=>{
    fetch('/api/friends').then(r=>r.json()).then(friends=>{
      const byUid = {};
      friends.forEach(f=>byUid[f.uid]=f);
      const div = document.getElementById('friends');
      div.innerHTML = '';
      convos.forEach(c=>{
        const f = byUid[c.uid] || {nickname:'(unknown)', avatar:99};
        const el = document.createElement('div');
        el.className = 'friend';
        const row = document.createElement('div');
        row.className = 'frow';
        row.innerHTML = '<img class="av" src="/api/avatar?i='+f.avatar+'" onerror="this.style.visibility=\'hidden\'">'+
          '<span>'+escapeHtml(f.nickname)+(c.unread?' *':'')+'</span>'+
          '<span style="opacity:.6">'+escapeHtml(c.lastMessage||'')+'</span>';
        row.onclick = ()=>openConvo(c.uid, f.nickname);
        const del = document.createElement('button');
        del.className = 'del'; del.innerText = '✕';
        del.onclick = (e)=>{ e.stopPropagation(); deleteFriend(c.uid, f.nickname); };
        el.appendChild(row); el.appendChild(del);
        div.appendChild(el);
      });
    });
  });
}

function refreshStatus(){
  if(statusBusy) return;          // previous status request still in flight
  statusBusy = true;
  fetch('/api/status').then(r=>r.json()).then(s=>{
    const mins = Math.floor(s.uptime/60);
    document.getElementById('status').innerText =
      'batt ' + s.battery + '% (' + (s.mv/1000).toFixed(2) + 'V) | pending ' + s.pending +
      ' | clients ' + s.clients + ' | up ' + mins + 'm';
    const banner = document.getElementById('banner');
    if(s.lowBattery){
      banner.classList.remove('hidden');
      banner.innerText = (s.criticalBattery ? '⚠ CRITICAL BATTERY ' : '⚠ ON BATTERY — low ')
        + '(' + s.battery + '%) — check USB power';
    }else{
      banner.classList.add('hidden');
    }
  }).catch(()=>{}).finally(()=>{ statusBusy = false; });
}

function silence(){
  fetch('/api/silence', {method:'POST'});
}

function stopPending(){
  // No confirm/alert dialogs: a suppressed "don't show more alerts" choice in the
  // browser would auto-cancel confirm() and make the button appear dead. Just clear
  // the queue and let the status line ("pending N") reflect the result.
  fetch('/api/pending/clear', {method:'POST'}).then(()=>refreshStatus()).catch(()=>{});
}

function deleteFriend(uid, name){
  if(!confirm('Delete '+name+' and the whole conversation?')) return;
  fetch('/api/friends/delete', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'uid='+uid}).then(()=>loadFriends());
}

function showProfile(){
  show('profileView');
  fetch('/api/profile').then(r=>r.json()).then(p=>{
    document.getElementById('profName').value = p.nickname;
    document.getElementById('profHue').value = p.hue;
    selectedAvatar = p.avatar;
    const grid = document.getElementById('avgrid');
    grid.innerHTML = '';
    const imgs = [];
    for(let i=0;i<15;i++){
      const img = document.createElement('img');
      if(i===selectedAvatar) img.className='sel';
      img.onclick = ()=>{ selectedAvatar=i;
        grid.querySelectorAll('img').forEach((x,j)=>x.className=(j===i?'sel':'')); };
      grid.appendChild(img);
      imgs.push(img);
    }
    // Load avatars ONE AT A TIME. The device serves a single connection at a
    // time, so setting all 15 <img> srcs at once makes the browser open
    // parallel connections that stall behind each other (and block /api/status
    // etc.), making the whole UI feel hung. Chaining each load off the previous
    // keeps exactly one avatar request in flight. The token cancels this chain
    // if the grid is rebuilt (e.g. the profile view is reopened) before it ends.
    const token = ++avatarLoadToken;
    let n = 0;
    const loadNext = ()=>{
      if(n >= imgs.length || token !== avatarLoadToken) return;
      const img = imgs[n];
      img.onload = img.onerror = ()=>{ n++; loadNext(); };
      img.src = '/api/avatar?i=' + n;
    };
    loadNext();
  });
}

function saveProfile(){
  const name = document.getElementById('profName').value;
  const hue = document.getElementById('profHue').value;
  fetch('/api/profile', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'nickname='+encodeURIComponent(name)+'&avatar='+selectedAvatar+'&hue='+hue})
    .then(()=>{ alert('Profile saved'); showFriends(); });
}

function openConvo(uid, nickname){
  currentConvo = uid;
  show('convoView');
  fetch('/api/read', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:'uid='+uid});
  loadConvo();
}

function loadConvo(){
  if(!currentConvo) return;
  fetch('/api/convo?uid='+currentConvo).then(r=>r.json()).then(msgs=>{
    const div = document.getElementById('convo');
    div.innerHTML = '';
    const picPairs = [];
    msgs.forEach(m=>{
      const el = document.createElement('div');
      el.className = 'msg ' + (m.outgoing ? 'out' : 'in');
      const span = document.createElement('span');
      if(m.type === 'pic'){
        const img = document.createElement('img');
        // src set later by loadImagesSeq so the pics load one at a time.
        picPairs.push({img, src:'/api/pic?i=' + m.pic});
        span.appendChild(img);
      } else {
        span.innerText = m.type === 'text' ? m.text : '[unknown]';
      }
      if(m.outgoing){ const s = document.createElement('small'); s.style.opacity='.6'; s.innerText=' '+(m.received?'(ack)':'(sending...)'); span.appendChild(s); }
      const del = document.createElement('button');
      del.className = 'del'; del.innerText = '✕';
      del.onclick = ()=>deleteMessage(m.uid);
      el.appendChild(span); el.appendChild(del);
      div.appendChild(el);
    });
    div.scrollTop = div.scrollHeight;
    loadImagesSeq('convo', picPairs);   // throttle the pic loads to one at a time
  });
}

function deleteMessage(msgUid){
  if(!confirm('Delete this message?')) return;
  fetch('/api/messages/delete', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'convo='+currentConvo+'&msg='+msgUid}).then(()=>loadConvo());
}

function showMemePicker(){
  const grid = document.getElementById('picGrid');
  if(!grid.children.length){
    const picPairs = [];
    for(let i=0;i<8;i++){
      const img = document.createElement('img');
      img.onclick = ()=>{ sendPic(i); hideMemePicker(); };
      grid.appendChild(img);
      picPairs.push({img, src:'/api/pic?i='+i});   // src set by loadImagesSeq below
    }
    loadImagesSeq('picker', picPairs);   // load the 8 thumbnails one at a time
  }
  document.getElementById('memePicker').classList.remove('hidden');
}
function hideMemePicker(){
  document.getElementById('memePicker').classList.add('hidden');
}
function sendPic(index){
  if(!currentConvo) return;
  fetch('/api/sendpic',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'convo='+currentConvo+'&pic='+index}).then(()=>loadConvo());
}
function send(){
  const text = document.getElementById('composeText').value;
  if(!text || !currentConvo) return;
  fetch('/api/messages', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'convo='+currentConvo+'&text='+encodeURIComponent(text)})
    .then(()=>{ document.getElementById('composeText').value=''; loadConvo(); });
}

function broadcast(){
  const text = prompt('Broadcast message to all friends:');
  if(!text) return;
  fetch('/api/broadcast', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'text='+encodeURIComponent(text)}).then(()=>loadFriends());
}

function showPair(){
  show('pairView');   // stops any leftover pair polls (see show())
  fetch('/api/pair/start', {method:'POST'}).then(()=>{
    refreshPair();
    pairTimer = setInterval(refreshPair, 2000);
  });
}

function refreshPair(){
  if(pairing || pairBusy) return;   // not while confirming, and no overlapping scans
  pairBusy = true;
  fetch('/api/pair/discovered').then(r=>r.json()).then(list=>{
    if(pairing) return;             // a pair was confirmed while this was in flight
    const div = document.getElementById('pairList');
    div.innerHTML = '<p>Scanning for nearby Chatters...</p>';
    list.forEach(p=>{
      const el = document.createElement('div');
      el.className = 'friend';
      el.innerHTML = '<span>'+escapeHtml(p.nickname)+'</span><button>Pair</button>';
      el.querySelector('button').onclick = (e)=>confirmPair(p.index, e.target);
      div.appendChild(el);
    });
  }).catch(()=>{}).finally(()=>{ pairBusy = false; });
}

function confirmPair(index, btn){
  if(pairing) return;               // already pairing -- ignore further taps
  pairing = true;

  // Grey out every Pair button so none can be tapped again while we wait.
  document.querySelectorAll('#pairList button').forEach(b=>{ b.disabled = true; });
  if(btn){ btn.textContent = 'Pairing...'; }

  fetch('/api/pair/confirm', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'index='+index}).then(()=>{
      // Stop scanning while we wait, and ensure only one status poll runs.
      if(pairTimer){ clearInterval(pairTimer); pairTimer = null; }
      if(pairStatusTimer){ clearInterval(pairStatusTimer); pairStatusTimer = null; }
      pairStatusTimer = setInterval(()=>{
        if(pairStatusBusy) return;  // previous status request still in flight
        pairStatusBusy = true;
        fetch('/api/pair/status').then(r=>r.json()).then(s=>{
          if(s.done){
            stopPairPolling();
            alert(s.success ? 'Paired!' : 'Pairing failed');
            showFriends();
          }
        }).catch(()=>{}).finally(()=>{ pairStatusBusy = false; });
      }, 1000);
    }).catch(()=>{ pairing = false; });  // confirm failed: unstick UI; scan re-renders buttons
}

showFriends();
refreshStatus();
setInterval(refreshStatus, 10000);
</script>
</body>
</html>
)rawliteral";

void WebUIService::begin(){
	char ssid[32];
	snprintf(ssid, sizeof(ssid), "Chatter-%04X", (unsigned) (ESP.getEfuseMac() & 0xFFFF));

	WiFi.mode(WIFI_AP);
	WiFi.softAP(ssid, AP_PASSWORD);

	// trace() (verbose console log of every request) runs first in each lambda,
	// so all routes get traced from this one place without touching the handlers.
	server.on("/", HTTP_GET, [this](){ trace(); handleRoot(); });
	server.on("/api/friends", HTTP_GET, [this](){ trace(); handleFriends(); });
	server.on("/api/convos", HTTP_GET, [this](){ trace(); handleConvos(); });
	server.on("/api/convo", HTTP_GET, [this](){ trace(); handleConvo(); });
	server.on("/api/messages", HTTP_POST, [this](){ trace(); handleSendMessage(); });
	server.on("/api/broadcast", HTTP_POST, [this](){ trace(); handleBroadcast(); });
	server.on("/api/read", HTTP_POST, [this](){ trace(); handleMarkRead(); });
	server.on("/api/pending", HTTP_GET, [this](){ trace(); handlePending(); });
	server.on("/api/pending/clear", HTTP_POST, [this](){ trace(); handleClearPending(); });
	server.on("/api/status", HTTP_GET, [this](){ trace(); handleStatus(); });
	server.on("/api/profile", HTTP_GET, [this](){ trace(); handleGetProfile(); });
	server.on("/api/profile", HTTP_POST, [this](){ trace(); handleSetProfile(); });
	server.on("/api/avatar", HTTP_GET, [this](){ trace(); handleAvatar(); });
	server.on("/api/pic", HTTP_GET, [this](){ trace(); handlePic(); });
	server.on("/api/sendpic", HTTP_POST, [this](){ trace(); handleSendPic(); });
	server.on("/api/silence", HTTP_POST, [this](){ trace(); handleSilence(); });
	server.on("/api/friends/delete", HTTP_POST, [this](){ trace(); handleDeleteFriend(); });
	server.on("/api/messages/delete", HTTP_POST, [this](){ trace(); handleDeleteMessage(); });
	server.on("/api/pair/start", HTTP_POST, [this](){ trace(); handlePairStart(); });
	server.on("/api/pair/discovered", HTTP_GET, [this](){ trace(); handlePairDiscovered(); });
	server.on("/api/pair/confirm", HTTP_POST, [this](){ trace(); handlePairConfirm(); });
	server.on("/api/pair/status", HTTP_GET, [this](){ trace(); handlePairStatus(); });
	server.on("/api/pair/cancel", HTTP_POST, [this](){ trace(); handlePairCancel(); });
	server.onNotFound([this](){ trace(); handleNotFound(); });

	server.begin();
	LoopManager::addListener(this);

	lastStationNum = WiFi.softAPgetStationNum();
	touchActivity();   // start the shutdown-on-battery idle timer from boot

	// Audible "I booted and the AP is up" -- the only boot indicator with no LCD.
	playCue(BootCue, sizeof(BootCue) / sizeof(BootCue[0]));

	printf("WebUI: AP \"%s\" started, browse http://%s/  (free heap %u)\n",
		ssid, WiFi.softAPIP().toString().c_str(), ESP.getFreeHeap());
}

void WebUIService::loop(uint micros){
	server.handleClient();

	// Check the AP client count a couple of times a second -- no need to query
	// the WiFi driver on every loop tick.
	stationPollTimer += micros;
	if(stationPollTimer >= 500000){
		stationPollTimer = 0;
		pollStations();
	}

	// Battery moves slowly; check every ~5s.
	batteryPollTimer += micros;
	if(batteryPollTimer >= 5000000){
		batteryPollTimer = 0;
		pollBattery();
	}

	// Verbose health/load heartbeat (~5s) -- heap + request rate are our proxies
	// for socket pressure since the Arduino API exposes no raw socket count.
	statsLogTimer += micros;
	if(statsLogTimer >= 5000000){
		statsLogTimer = 0;
		logStats();
	}

	updateCue(micros);
}

void WebUIService::pollBattery(){
	uint8_t pct = Battery.getPercentage();

	// Critical crossing (downward) -- chirp once and latch.
	if(pct <= BattCritPct){
		if(!criticalBattery){
			criticalBattery = true;
			lowBattery = true;
			playCue(LowBattCue, sizeof(LowBattCue) / sizeof(LowBattCue[0]));
		}
	}else if(pct > BattCritPct + BattClearMargin){
		criticalBattery = false;
	}

	// Warn crossing (downward) -- chirp once and latch.
	if(pct <= BattWarnPct){
		if(!lowBattery){
			lowBattery = true;
			playCue(LowBattCue, sizeof(LowBattCue) / sizeof(LowBattCue[0]));
		}
	}else if(pct > BattWarnPct + BattClearMargin){
		lowBattery = false;   // recovered (e.g. USB power restored, pack charging)
	}

	// Shutdown-on-battery: this unit normally lives on USB with the pack as
	// backup. When USB drops (power outage), don't drain the pack indefinitely
	// running the AP for nobody -- power off after the configured idle time.
	// On USB we treat every poll as activity so the deadline never arrives;
	// active web use (see touchActivity()) likewise defers it. We never cut
	// power with messages still queued for delivery.
	//
	// Note: turnOff() is a true deep-sleep power-down -- only the physical
	// button wakes it. There's no USB-detect pin to auto-wake on power return,
	// so if an outage outlasts shutdownTime the unit stays off until pressed.
	uint32_t shutdownSecs = ShutdownSeconds[Settings.get().shutdownTime];
	if(Battery.getVoltage() >= UsbPresentMv){
		touchActivity();                 // on USB == not idle
	}else if(shutdownSecs != 0 && Messages.pendingCount() == 0){
		if(millis() - lastWebActivity >= shutdownSecs * 1000UL){
			Sleep.turnOff();
		}
	}
}

// Mark "the web UI is being used right now" -- defers the shutdown-on-battery
// timer. Called from the page's status heartbeat and from user actions, so an
// open/active page keeps the unit alive even on battery.
void WebUIService::touchActivity(){
	lastWebActivity = millis();
}

void WebUIService::pollStations(){
	uint8_t now = WiFi.softAPgetStationNum();
	if(now > lastStationNum){
		playCue(ConnectCue, sizeof(ConnectCue) / sizeof(ConnectCue[0]));
		if(VerboseLog) Serial.printf("[WebUI] + client joined: %u station(s) connected, free heap %u\n",
			now, ESP.getFreeHeap());
	}else if(now < lastStationNum){
		playCue(DisconnectCue, sizeof(DisconnectCue) / sizeof(DisconnectCue[0]));
		if(VerboseLog) Serial.printf("[WebUI] - client left: %u station(s) connected, free heap %u\n",
			now, ESP.getFreeHeap());
	}
	lastStationNum = now;
}

// Per-request verbose trace -- runs at the top of every route lambda (see
// begin()). Logs method, path, client IP, a running request counter, and free
// heap so a connection storm or heap leak is visible request-by-request.
void WebUIService::trace(){
	requestCount++;
	if(!VerboseLog) return;
	Serial.printf("[WebUI] #%lu %s %s  from %s  heap=%u sta=%u\n",
		(unsigned long) requestCount,
		server.method() == HTTP_GET ? "GET" : "POST",
		server.uri().c_str(),
		server.client().remoteIP().toString().c_str(),
		ESP.getFreeHeap(),
		WiFi.softAPgetStationNum());
}

// Periodic health line + alerts. The ESP32/Arduino stack exposes no raw lwIP
// socket count, so we surface the next-best signals: free heap (drops as
// sockets/buffers fill), request rate (spikes during a connection storm), and
// station count. Two heuristic alerts flag the conditions that precede the
// "page unreachable" wedge.
void WebUIService::logStats(){
	if(!VerboseLog) return;
	uint32_t heap = ESP.getFreeHeap();
	uint32_t minHeap = ESP.getMinFreeHeap();
	uint32_t reqDelta = requestCount - lastStatRequestCount;
	lastStatRequestCount = requestCount;

	Serial.printf("[WebUI] stats: sta=%u heap=%u minHeap=%u req/5s=%lu total=%lu pending=%d up=%lus\n",
		WiFi.softAPgetStationNum(), heap, minHeap,
		(unsigned long) reqDelta, (unsigned long) requestCount,
		Messages.pendingCount(), (unsigned long) (millis() / 1000));

	if(reqDelta > 30){
		Serial.printf("[WebUI] !! high request rate (%lu in 5s) -- possible connection storm filling sockets\n",
			(unsigned long) reqDelta);
	}
	if(heap < 25000){
		Serial.printf("[WebUI] !! low free heap (%u) -- TCP sockets/buffers may be exhausting; new connections can fail\n",
			heap);
	}
}

void WebUIService::playCue(const CueNote* notes, uint8_t len){
	if(len == 0) return;
	activeCue = notes;
	activeCueLen = len;
	cueIndex = 0;
	cueTimer = 0;
	Piezo.tone(notes[0].freq, notes[0].durMs);   // honors mute; silent if sound off
}

void WebUIService::updateCue(uint micros){
	if(!activeCue) return;

	cueTimer += micros;
	uint32_t slotMicros = ((uint32_t) activeCue[cueIndex].durMs + CueGapMs) * 1000;
	if(cueTimer < slotMicros) return;

	cueTimer = 0;
	cueIndex++;
	if(cueIndex >= activeCueLen){
		activeCue = nullptr;   // melody finished
		return;
	}
	Piezo.tone(activeCue[cueIndex].freq, activeCue[cueIndex].durMs);
}

void WebUIService::handleRoot(){
	server.send_P(200, "text/html", INDEX_HTML);
}

void WebUIService::handleFriends(){
	UID_t self = ESP.getEfuseMac();
	String json = "[";
	bool first = true;
	for(UID_t uid : Storage.Friends.all()){
		Friend fren = Storage.Friends.get(uid);
		if(fren.uid == 0) continue;

		if(!first) json += ",";
		first = false;

		json += "{\"uid\":\"" + uidToHex(fren.uid) + "\"";
		json += ",\"me\":" + String(fren.uid == self ? "true" : "false");
		json += ",\"nickname\":\"" + jsonEscape(fren.profile.nickname) + "\"";
		json += ",\"avatar\":" + String(fren.profile.avatar);
		json += ",\"hue\":" + String(fren.profile.hue) + "}";
	}
	json += "]";
	server.send(200, "application/json", json);
}

void WebUIService::handleConvos(){
	String json = "[";
	bool first = true;
	for(UID_t uid : Storage.Convos.all()){
		Convo convo = Storage.Convos.get(uid);
		if(convo.uid == 0) continue;

		Message last = Messages.getLastMessage(convo.uid);
		String lastText;
		if(last.uid != 0){
			lastText = last.getType() == Message::TEXT ? String(last.getText().c_str()) : String("[pic]");
		}

		if(!first) json += ",";
		first = false;

		json += "{\"uid\":\"" + uidToHex(convo.uid) + "\"";
		json += ",\"unread\":" + String(convo.unread ? "true" : "false");
		json += ",\"lastMessage\":\"" + jsonEscape(lastText) + "\"}";
	}
	json += "]";
	server.send(200, "application/json", json);
}

void WebUIService::handleConvo(){
	UID_t uid = hexToUid(server.arg("uid"));
	Convo convo = Storage.Convos.get(uid);

	String json = "[";
	bool first = true;
	if(convo.uid != 0){
		for(UID_t msgUid : convo.messages){
			Message msg = Storage.Messages.get(msgUid);
			if(msg.uid == 0) continue;

			if(!first) json += ",";
			first = false;

			json += "{\"uid\":\"" + uidToHex(msg.uid) + "\"";
			json += ",\"outgoing\":" + String(msg.outgoing ? "true" : "false");
			json += ",\"received\":" + String(msg.received ? "true" : "false");
			if(msg.getType() == Message::TEXT){
				json += ",\"type\":\"text\",\"text\":\"" + jsonEscape(msg.getText().c_str()) + "\"";
			}else if(msg.getType() == Message::PIC){
				json += ",\"type\":\"pic\",\"pic\":" + String(msg.getPic());
			}else{
				json += ",\"type\":\"none\"";
			}
			json += "}";
		}
	}
	json += "]";
	server.send(200, "application/json", json);
}

void WebUIService::handleSendMessage(){
	UID_t convo = hexToUid(server.arg("convo"));
	String text = server.arg("text");

	if(convo == 0 || text.length() == 0){
		server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing convo or text\"}");
		return;
	}

	touchActivity();
	Message msg = Messages.sendText(convo, text.c_str());
	server.send(200, "application/json", String("{\"ok\":") + (msg.uid != 0 ? "true" : "false") + "}");
}

void WebUIService::handleBroadcast(){
	String text = server.arg("text");
	if(text.length() == 0){
		server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing text\"}");
		return;
	}

	touchActivity();
	int count = Messages.broadcastText(text.c_str());
	server.send(200, "application/json", "{\"ok\":true,\"count\":" + String(count) + "}");
}

void WebUIService::handleMarkRead(){
	UID_t uid = hexToUid(server.arg("uid"));
	bool ok = Messages.markRead(uid);
	server.send(200, "application/json", String("{\"ok\":") + (ok ? "true" : "false") + "}");
}

void WebUIService::handlePending(){
	server.send(200, "application/json", "{\"pending\":" + String(Messages.pendingCount()) + "}");
}

// Stop retrying every message currently pending (snapshots their UIDs so the
// retry loop skips them). Messages are NOT deleted -- they stay in the convo as
// undelivered and can be retried per-message later. New sends are unaffected.
void WebUIService::handleClearPending(){
	touchActivity();
	int count = Messages.abortPending();
	server.send(200, "application/json", "{\"ok\":true,\"count\":" + String(count) + "}");
}

void WebUIService::handleStatus(){
	// The page polls this every 10s while open, so it doubles as the "UI is in
	// use" heartbeat that holds off the shutdown-on-battery timer.
	touchActivity();

	// One poll for the header: battery, link, and health. Watching mV over time
	// is also a handy way to gauge how fast Wi-Fi drains the pack on backup power.
	String json = "{";
	json += "\"battery\":" + String(Battery.getPercentage());
	json += ",\"mv\":" + String(Battery.getVoltage());
	json += ",\"pending\":" + String(Messages.pendingCount());
	json += ",\"clients\":" + String(WiFi.softAPgetStationNum());
	json += ",\"uptime\":" + String(millis() / 1000);
	json += ",\"heap\":" + String(ESP.getFreeHeap());
	json += ",\"lowBattery\":" + String(lowBattery ? "true" : "false");
	json += ",\"criticalBattery\":" + String(criticalBattery ? "true" : "false");
	json += "}";
	server.send(200, "application/json", json);
}

void WebUIService::handleGetProfile(){
	const Profile& p = Profiles.getMyProfile();
	String json = "{\"nickname\":\"" + jsonEscape(p.nickname) + "\"";
	json += ",\"avatar\":" + String(p.avatar);
	json += ",\"hue\":" + String(p.hue) + "}";
	server.send(200, "application/json", json);
}

void WebUIService::handleSetProfile(){
	Profile p = Profiles.getMyProfile();

	if(server.hasArg("nickname")){
		strncpy(p.nickname, server.arg("nickname").c_str(), sizeof(p.nickname) - 1);
		p.nickname[sizeof(p.nickname) - 1] = '\0';
	}
	if(server.hasArg("avatar")) p.avatar = (uint8_t) server.arg("avatar").toInt();
	if(server.hasArg("hue")) p.hue = (uint16_t) server.arg("hue").toInt();

	Profiles.setMyProfile(p);
	server.send(200, "application/json", "{\"ok\":true}");
}

void WebUIService::handlePairStart(){
	if(pairService){
		delete pairService;
		pairService = nullptr;
	}

	pairResultReady = false;
	pairResultSuccess = false;

	pairService = new PairService();
	pairService->setDoneCallback(&WebUIService::onPairDone, this);
	pairService->begin();

	server.send(200, "application/json", "{\"ok\":true}");
}

void WebUIService::handlePairDiscovered(){
	String json = "[";
	if(pairService){
		const auto& profiles = pairService->getFoundProfiles();
		for(size_t i = 0; i < profiles.size(); i++){
			if(i != 0) json += ",";
			json += "{\"index\":" + String((int) i) + ",\"nickname\":\"" + jsonEscape(profiles[i].nickname) + "\"";
			json += ",\"avatar\":" + String(profiles[i].avatar) + ",\"hue\":" + String(profiles[i].hue) + "}";
		}
	}
	json += "]";
	server.send(200, "application/json", json);
}

void WebUIService::handlePairConfirm(){
	if(!pairService){
		server.send(400, "application/json", "{\"ok\":false,\"error\":\"no pairing in progress\"}");
		return;
	}

	pairResultReady = false;
	pairService->requestPair((uint32_t) server.arg("index").toInt());
	server.send(200, "application/json", "{\"ok\":true}");
}

void WebUIService::handlePairStatus(){
	String json = "{\"active\":" + String(pairService ? "true" : "false");
	json += ",\"done\":" + String(pairResultReady ? "true" : "false");
	json += ",\"success\":" + String(pairResultSuccess ? "true" : "false") + "}";
	server.send(200, "application/json", json);
}

void WebUIService::handlePairCancel(){
	if(pairService){
		delete pairService;
		pairService = nullptr;
	}
	server.send(200, "application/json", "{\"ok\":true}");
}

void WebUIService::handleSilence(){
	Buzz.silenceAlert();
	server.send(200, "application/json", "{\"ok\":true}");
}

void WebUIService::handleDeleteFriend(){
	UID_t uid = hexToUid(server.arg("uid"));
	bool ok = Messages.deleteFriend(uid);
	server.send(200, "application/json", String("{\"ok\":") + (ok ? "true" : "false") + "}");
}

void WebUIService::handleDeleteMessage(){
	UID_t convo = hexToUid(server.arg("convo"));
	UID_t msg = hexToUid(server.arg("msg"));
	bool ok = Messages.deleteMessage(convo, msg);
	server.send(200, "application/json", String("{\"ok\":") + (ok ? "true" : "false") + "}");
}

// Serve one avatar PNG (index 0-14) from LittleFS at /Avatars/large/N.png.
// Flash the data/ folder with mklittlefs before using this. To revert to
// the embedded PROGMEM approach (no data flash required) do: git revert HEAD.
void WebUIService::handleAvatar(){
	int idx = server.arg("i").toInt();
	if(idx < 0 || idx > 14){
		server.send(404, "text/plain", "no such avatar");
		return;
	}
	String path = "/Avatars/large/" + String(idx + 1) + ".png";
	File f = LITTLEFS.open(path, "r");
	if(!f){
		server.send(404, "text/plain", "avatar not on flash");
		return;
	}
	server.streamFile(f, "image/png");
	f.close();
}

void WebUIService::handlePic(){
	int idx = server.arg("i").toInt();
	if(idx < 0 || idx > 7){
		server.send(404, "text/plain", "no such pic");
		return;
	}
	String path = "/Pics/" + String(idx) + ".png";
	File f = LITTLEFS.open(path, "r");
	if(!f){
		server.send(404, "text/plain", "pic not on flash");
		return;
	}
	server.streamFile(f, "image/png");
	f.close();
}

void WebUIService::handleSendPic(){
	UID_t convo = hexToUid(server.arg("convo"));
	int pic = server.arg("pic").toInt();
	if(convo == 0 || pic < 0 || pic > 7){
		server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing convo or invalid pic\"}");
		return;
	}
	touchActivity();
	Message msg = Messages.sendPic(convo, (uint8_t) pic);
	server.send(200, "application/json", String("{\"ok\":") + (msg.uid != 0 ? "true" : "false") + "}");
}

void WebUIService::handleNotFound(){
	server.send(404, "application/json", "{\"error\":\"not found\"}");
}

void WebUIService::onPairDone(bool success, void* ctx){
	auto* self = static_cast<WebUIService*>(ctx);
	self->pairResultReady = true;
	self->pairResultSuccess = success;
}

String WebUIService::jsonEscape(const String& s){
	String out;
	out.reserve(s.length());
	for(size_t i = 0; i < s.length(); i++){
		char c = s[i];
		switch(c){
			case '"': out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default:
				if((uint8_t) c < 0x20){
					char buf[8];
					snprintf(buf, sizeof(buf), "\\u%04x", c);
					out += buf;
				}else{
					out += c;
				}
		}
	}
	return out;
}

String WebUIService::uidToHex(UID_t uid){
	char buf[17];
	snprintf(buf, sizeof(buf), "%016llx", (unsigned long long) uid);
	return String(buf);
}

UID_t WebUIService::hexToUid(const String& s){
	return (UID_t) strtoull(s.c_str(), nullptr, 16);
}
