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
#include "AvatarAssets.h"

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
</style>
</head>
<body>
<header>
  <strong>Chatter</strong>
  <span><a href="#" onclick="showProfile();return false;">me</a> | <a href="#" onclick="showPair();return false;">pair</a> | <a href="#" onclick="silence();return false;">silence</a></span>
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
  </div>
</div>

<div id="pairView" class="hidden">
  <header><a href="#" onclick="showFriends();return false;">&larr; back</a></header>
  <div id="pairList"></div>
</div>

<script>
let currentConvo = null;
let pairTimer = null;
let selectedAvatar = 0;

function escapeHtml(s){
  return String(s).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
}

function show(view){
  ['friendsView','convoView','pairView','profileView'].forEach(v=>
    document.getElementById(v).classList.toggle('hidden', v!==view));
}

function showFriends(){
  show('friendsView');
  if(pairTimer){ clearInterval(pairTimer); pairTimer=null; }
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
  }).catch(()=>{});
}

function silence(){
  fetch('/api/silence', {method:'POST'});
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
    for(let i=0;i<15;i++){
      const img = document.createElement('img');
      img.src = '/api/avatar?i='+i;
      if(i===selectedAvatar) img.className='sel';
      img.onclick = ()=>{ selectedAvatar=i;
        grid.querySelectorAll('img').forEach((x,j)=>x.className=(j===i?'sel':'')); };
      grid.appendChild(img);
    }
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
    msgs.forEach(m=>{
      const el = document.createElement('div');
      el.className = 'msg ' + (m.outgoing ? 'out' : 'in');
      const txt = (m.type === 'text' ? m.text : '[pic '+m.pic+']') + (m.outgoing ? (m.received?' (ack)':' (sending...)') : '');
      const span = document.createElement('span');
      span.innerText = txt;
      const del = document.createElement('button');
      del.className = 'del'; del.innerText = '✕';
      del.onclick = ()=>deleteMessage(m.uid);
      el.appendChild(span); el.appendChild(del);
      div.appendChild(el);
    });
    div.scrollTop = div.scrollHeight;
  });
}

function deleteMessage(msgUid){
  if(!confirm('Delete this message?')) return;
  fetch('/api/messages/delete', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'convo='+currentConvo+'&msg='+msgUid}).then(()=>loadConvo());
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
  show('pairView');
  fetch('/api/pair/start', {method:'POST'}).then(()=>{
    refreshPair();
    pairTimer = setInterval(refreshPair, 2000);
  });
}

function refreshPair(){
  fetch('/api/pair/discovered').then(r=>r.json()).then(list=>{
    const div = document.getElementById('pairList');
    div.innerHTML = '<p>Scanning for nearby Chatters...</p>';
    list.forEach(p=>{
      const el = document.createElement('div');
      el.className = 'friend';
      el.innerHTML = '<span>'+escapeHtml(p.nickname)+'</span><button>Pair</button>';
      el.querySelector('button').onclick = ()=>confirmPair(p.index);
      div.appendChild(el);
    });
  });
}

function confirmPair(index){
  fetch('/api/pair/confirm', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'index='+index}).then(()=>{
      const check = setInterval(()=>{
        fetch('/api/pair/status').then(r=>r.json()).then(s=>{
          if(s.done){
            clearInterval(check);
            alert(s.success ? 'Paired!' : 'Pairing failed');
            showFriends();
          }
        });
      }, 1000);
    });
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

	// Resolve every hostname to ourselves so phone-OS internet-check probes
	// (which would otherwise fail against this internet-less AP) land on
	// handleCaptivePortal() below instead. See the dnsServer comment in the
	// header for why this matters.
	dnsServer.start(53, "*", WiFi.softAPIP());

	server.on("/", HTTP_GET, [this](){ handleRoot(); });
	server.on("/api/friends", HTTP_GET, [this](){ handleFriends(); });
	server.on("/api/convos", HTTP_GET, [this](){ handleConvos(); });
	server.on("/api/convo", HTTP_GET, [this](){ handleConvo(); });
	server.on("/api/messages", HTTP_POST, [this](){ handleSendMessage(); });
	server.on("/api/broadcast", HTTP_POST, [this](){ handleBroadcast(); });
	server.on("/api/read", HTTP_POST, [this](){ handleMarkRead(); });
	server.on("/api/pending", HTTP_GET, [this](){ handlePending(); });
	server.on("/api/status", HTTP_GET, [this](){ handleStatus(); });
	server.on("/api/profile", HTTP_GET, [this](){ handleGetProfile(); });
	server.on("/api/profile", HTTP_POST, [this](){ handleSetProfile(); });
	server.on("/api/avatar", HTTP_GET, [this](){ handleAvatar(); });
	server.on("/api/silence", HTTP_POST, [this](){ handleSilence(); });
	server.on("/api/friends/delete", HTTP_POST, [this](){ handleDeleteFriend(); });
	server.on("/api/messages/delete", HTTP_POST, [this](){ handleDeleteMessage(); });
	server.on("/api/pair/start", HTTP_POST, [this](){ handlePairStart(); });
	server.on("/api/pair/discovered", HTTP_GET, [this](){ handlePairDiscovered(); });
	server.on("/api/pair/confirm", HTTP_POST, [this](){ handlePairConfirm(); });
	server.on("/api/pair/status", HTTP_GET, [this](){ handlePairStatus(); });
	server.on("/api/pair/cancel", HTTP_POST, [this](){ handlePairCancel(); });

	// Known OS internet-connectivity-check paths -- redirect them to "/" so
	// the resulting "sign in to network" prompt (Android) or captive-portal
	// popup (iOS/macOS) opens straight into the Chatter UI.
	server.on("/generate_204", HTTP_GET, [this](){ handleCaptivePortal(); });        // Android
	server.on("/gen_204", HTTP_GET, [this](){ handleCaptivePortal(); });             // Android (older)
	server.on("/hotspot-detect.html", HTTP_GET, [this](){ handleCaptivePortal(); }); // iOS/macOS
	server.on("/library/test/success.html", HTTP_GET, [this](){ handleCaptivePortal(); }); // iOS/macOS (older)
	server.on("/ncsi.txt", HTTP_GET, [this](){ handleCaptivePortal(); });            // Windows
	server.on("/connecttest.txt", HTTP_GET, [this](){ handleCaptivePortal(); });     // Windows

	server.onNotFound([this](){ handleNotFound(); });

	server.begin();
	LoopManager::addListener(this);

	lastStationNum = WiFi.softAPgetStationNum();
	touchActivity();   // start the shutdown-on-battery idle timer from boot

	// Audible "I booted and the AP is up" -- the only boot indicator with no LCD.
	playCue(BootCue, sizeof(BootCue) / sizeof(BootCue[0]));

	printf("WebUI: AP \"%s\" started, connect and browse http://192.168.4.1/\n", ssid);
}

void WebUIService::loop(uint micros){
	server.handleClient();
	dnsServer.processNextRequest();

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
	}else if(now < lastStationNum){
		playCue(DisconnectCue, sizeof(DisconnectCue) / sizeof(DisconnectCue[0]));
	}
	lastStationNum = now;
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

// Serve one built-in avatar (index 0-14) as a PNG, straight from flash. The
// images are embedded in the firmware (src/Services/AvatarAssets.h, generated
// by data/Avatars/large/png2c.py) rather than read from SPIFFS, so getting
// avatars onto the device is just a normal sketch upload -- no separate SPIFFS
// data flash (which IDE 2.x can't do for this board and which kept landing the
// image at the wrong partition offset). PNG keeps the avatars' transparent
// corners so they composite cleanly over the page background.
void WebUIService::handleAvatar(){
	int idx = server.arg("i").toInt();
	if(idx < 0 || idx > 14){
		server.send(404, "text/plain", "no such avatar");
		return;
	}

	server.send_P(200, "image/png", (PGM_P) AvatarPng[idx], AvatarPngLen[idx]);
}

// With the DNS catch-all resolving every host to us, anything we don't have an
// explicit route for is almost certainly an OS captive-portal probe or a
// browser poking at a random host -- send it all to the portal page so the
// network gets recognized as a captive portal instead of "no internet".
void WebUIService::handleNotFound(){
	redirectToPortal();
}

// Hit by the OS's own internet-connectivity probe (Android/iOS/Windows all
// have one), which dnsServer routes here by resolving the probe's hostname to
// us. Redirecting -- rather than answering the probe's expected "everything's
// fine" body -- is what makes the OS treat this as a captive portal and pop
// its sign-in UI onto our page, instead of deciding there's "no internet" and
// deprioritizing/dropping the connection.
void WebUIService::handleCaptivePortal(){
	redirectToPortal();
}

// Redirect to the ABSOLUTE gateway URL (http://192.168.4.1/), not a relative
// "/". Windows' captive-portal assistant resolves the Location against the
// probe's hostname (www.msftconnecttest.com), so a relative path can send it
// back to a Microsoft host instead of to us; the absolute IP URL pins it to
// the device. (Note: we only serve HTTP/80 -- if the OS insists on HTTPS the
// auto-popup can still fail, in which case browsing to http://192.168.4.1
// by hand always works.)
void WebUIService::redirectToPortal(){
	String url = "http://" + WiFi.softAPIP().toString() + "/";
	server.sendHeader("Location", url, true);
	server.send(302, "text/html",
		"<!DOCTYPE html><meta http-equiv=refresh content=\"0;url=" + url + "\">"
		"<a href=\"" + url + "\">Open Chatter</a>");
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
