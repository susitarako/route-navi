// 璃奈ちゃんボード用：「名探偵プリキュア！」の文字流し＋表情の表示（スマホで描いて保存できる）
// ESP32 + WS2812B 16x16パネル（左下スタート／ジグザグ／横方向）
// スマホでWi-Fi「RinaBoard」に接続 → ブラウザで http://192.168.4.1/ を開く
//
// 必要ライブラリ: Adafruit NeoPixel
//   arduino-cli lib install "Adafruit NeoPixel"

#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>

// ====== 設定（環境に合わせて変更） ======
#define LED_PIN     14     // パネルのDINにつないでいるGPIO番号（D14）
#define PANEL_W     16
#define PANEL_H     16
#define NUM_LEDS    (PANEL_W * PANEL_H)
#define MAX_BRIGHT  60     // モバイルバッテリー保護のための明るさ上限（最大255）
#define FACE_SLOTS  8      // 保存できる表情の数

const char* AP_SSID = "RinaBoard";
const char* AP_PASS = "rina1234";   // 8文字以上

// ====== 文字データ ======
// 「名探偵プリキュア！」16x16ドット x 9文字 = 144列
// 1列ごとに16bit。bit0が一番上の行、bit15が一番下の行
const int TEXT_LEN = 144;
const uint16_t TEXT_COLS[TEXT_LEN] PROGMEM = {
  0x0000, 0x0420, 0x0420, 0x0410, 0x0210, 0x0218, 0x7F24, 0x4147,
  0x4184, 0x4144, 0x4124, 0x4114, 0x410C, 0x4100, 0x7F00, 0x0000,
  0x0000, 0x0210, 0x4210, 0x7FFF, 0x0110, 0x2200, 0x224E, 0x1242,
  0x0E22, 0x021A, 0x7F82, 0x023A, 0x0E42, 0x1242, 0x224E, 0x2260,
  0x0000, 0x0040, 0x0020, 0x7FF8, 0x0007, 0x4000, 0x5FF8, 0x3248,
  0x1248, 0x124F, 0x124A, 0x124A, 0x324A, 0x5FFA, 0x4002, 0x4000,
  0x0000, 0x0000, 0x0000, 0x0008, 0x0008, 0x2008, 0x2008, 0x1008,
  0x1008, 0x0808, 0x0408, 0x0208, 0x018E, 0x0079, 0x0009, 0x0006,
  0x0000, 0x0000, 0x0000, 0x0000, 0x01FC, 0x0000, 0x0000, 0x4000,
  0x4000, 0x2000, 0x1800, 0x07FE, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0200, 0x0220, 0x0220, 0x0220, 0x0220, 0x022E,
  0x03F0, 0x7D10, 0x0110, 0x0110, 0x0110, 0x0100, 0x0100, 0x0000,
  0x0000, 0x0000, 0x0000, 0x4000, 0x4000, 0x4100, 0x4100, 0x4100,
  0x7100, 0x4F00, 0x4000, 0x4000, 0x4000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0008, 0x0008, 0x2008, 0x1008, 0x0C08,
  0x03E8, 0x0008, 0x0008, 0x0048, 0x0028, 0x0018, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0078, 0x33FC, 0x0078,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

// ====== 表情 ======
// 色番号 → RGB（0は消灯）。ページ側(JavaScript)の PAL と同じ順番にすること
const uint8_t PAL[8][3] = {
  {  0,   0,   0},   // 0 消灯
  {255, 105, 180},   // 1 ピンク
  { 80, 200, 255},   // 2 水色
  {255, 255, 255},   // 3 白
  {170,  80, 255},   // 4 紫
  {255,  40,  60},   // 5 赤
  {255, 200,  40},   // 6 黄
  { 60, 220, 120},   // 7 緑
};

// 最初に入っている表情（1マス1文字の色番号、画面の左上から右へ・下へ 256文字）
const char* const PRESETS[] = {
  "0000000000000000000000000000000000000000000000000000000000000000000000000000000000001000000100000001010000101000001000100100010000000000000000000000000000000000000100000000100000001000000100000000011111100000000000000000000000000000000000000000000000000000",  // にっこり
  "0000000000000000000000000000000000011110011110000000000000000000000033000033000000030030030030000003003003003000000033000033000000000000000000000000000000000000000000011000000000000010010000000000001001000000000000011000000000000000000000000000000000000000",  // びっくり
  "0000000000000000000000000000000000000000000000000000110000110000001100000000110000000000000000000000110000110000000011000011000000000000000000000000200000000000000020000000000000022201100000000000001001000000000001000010000000000000000000000000000000000000",  // しょんぼり
  "0000000000000000000000000000000000000000000000000000000000000000000100000000100000001000000100000000010000100000000010000001000000010000000010000000000000000000000055555555000000000555555000000000055555500000000000555500000000000000000000000000000000000000",  // わらい
  "0000000000000000000000000000000000000000000000000000000000000000000000000000000000001100001100000000110000110000000000000000000000000000000000000000000000000000000000000000000000000111111000000000000000000000000000000000000000000000000000000000000000000000",  // ふつう
  "0000000000000000000000000000000000000000000000000000000000000000000505000050500000555550055555000055555005555500000555000055500000005000000500000000000000000000000000000000000000000100001000000000001111000000000000000000000000000000000000000000000000000000",  // ハート
};
const int N_PRESETS = sizeof(PRESETS) / sizeof(PRESETS[0]);

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
WebServer server(80);
Preferences prefs;

bool     isOn    = true;
uint8_t  bright  = 20;
uint16_t speedMs = 80;            // 1列ずれるまでの時間(ms)。小さいほど速い
uint8_t  colR = 255, colG = 105, colB = 180;   // 文字の色
uint8_t  mode = 0;                // 0:文字を流す 1:表情
uint8_t  slot = 0;                // 表示中の表情の番号
uint8_t  faces[FACE_SLOTS][256];  // 表情データ（色番号）
int      offsetX = 0;
bool     dirty = true;            // 設定が変わったらすぐ描き直す
unsigned long lastStep = 0;

// 画面座標(x:左0〜15, y:上0〜15) → LED番号
// 配線：左下スタート、1行ごとに向きが反転するジグザグ、横方向
int ledIndex(int x, int y) {
  int r = (PANEL_H - 1) - y;                       // 下から数えた行
  return r * PANEL_W + ((r % 2 == 0) ? x : (PANEL_W - 1 - x));
}

void drawText() {
  uint32_t c = strip.Color(colR, colG, colB);
  for (int x = 0; x < PANEL_W; x++) {
    int col = offsetX + x - PANEL_W;               // 最初と最後は空白で流れる
    if (col < 0 || col >= TEXT_LEN) continue;
    uint16_t bits = pgm_read_word(&TEXT_COLS[col]);
    for (int y = 0; y < PANEL_H; y++) {
      if (bits & (1u << y)) strip.setPixelColor(ledIndex(x, y), c);
    }
  }
}

void drawFace() {
  for (int y = 0; y < PANEL_H; y++) {
    for (int x = 0; x < PANEL_W; x++) {
      uint8_t v = faces[slot][y * PANEL_W + x];
      if (v == 0 || v > 7) continue;
      strip.setPixelColor(ledIndex(x, y), strip.Color(PAL[v][0], PAL[v][1], PAL[v][2]));
    }
  }
}

void drawFrame() {
  strip.setBrightness(bright);
  strip.clear();
  if (isOn) {
    if (mode == 0) drawText(); else drawFace();
  }
  strip.show();
}

int hexv(char ch) {
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
  if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
  return -1;
}

void faceKey(int i, char* key) { snprintf(key, 4, "f%d", i); }

void loadPreset(int i) {
  memset(faces[i], 0, 256);
  if (i >= N_PRESETS) return;
  for (int j = 0; j < 256 && PRESETS[i][j]; j++) {
    int v = hexv(PRESETS[i][j]);
    faces[i][j] = (v >= 0 && v <= 7) ? v : 0;
  }
}

void loadFaces() {
  for (int i = 0; i < FACE_SLOTS; i++) {
    char key[4];
    faceKey(i, key);
    if (prefs.getBytesLength(key) == 256) {
      prefs.getBytes(key, faces[i], 256);
    } else {
      loadPreset(i);                               // 初回は最初の表情を入れる
      prefs.putBytes(key, faces[i], 256);
    }
  }
}

const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="ja"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>りなちゃんボード</title>
<style>
*{box-sizing:border-box}
body{font-family:sans-serif;margin:auto;padding:20px;max-width:420px}
h1{font-size:20px}
h2{font-size:16px;margin:28px 0 8px}
label{display:block;margin:18px 0 6px}
input[type=range]{width:100%}
input[type=color]{width:100%;height:44px;border:none;padding:0}
button{width:100%;padding:14px;font-size:18px;border:none;border-radius:10px;background:#e8559a;color:#fff}
button.s{background:#7a4fd6;font-size:15px;padding:10px}
button.on{outline:3px solid #222}
.fg{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin-top:12px}
.fc canvas{width:100%;height:auto;display:block;background:#111;border-radius:6px;image-rendering:pixelated}
.fc button{margin-top:6px;padding:6px;font-size:13px;background:#888}
#ed{margin-top:20px;padding:14px;border:1px solid #ccc;border-radius:12px}
#ed canvas{width:100%;height:auto;display:block;touch-action:none;background:#111;border-radius:6px;margin-top:10px}
.sw{display:flex;gap:8px;margin:12px 0;flex-wrap:wrap}
.sw span{width:34px;height:34px;border-radius:50%;border:3px solid transparent;box-shadow:0 0 0 1px #ccc}
.sw span.sel{border-color:#222}
.row{display:flex;gap:8px;margin-top:10px}
.row button{font-size:15px;padding:10px}
</style></head><body>
<h1>りなちゃんボード</h1>
<button id="p">ON / OFF</button>
<label>明るさ <span id="bv"></span></label>
<input id="b" type="range" min="1" max="%MAXB%" value="20">
<h2>表示するもの</h2>
<button id="t" class="s">文字を流す（名探偵プリキュア！）</button>
<div class="fg" id="fg"></div>
<div id="ed" hidden>
<b id="et"></b>
<canvas id="ec" width="320" height="320"></canvas>
<div class="sw" id="sw"></div>
<div class="row"><button id="er" class="s">消しゴム</button><button id="cl" class="s">全消去</button></div>
<div class="row"><button id="sv">保存して表示</button><button id="cx" class="s">閉じる</button></div>
</div>
<h2>文字の設定</h2>
<label>流れる速さ</label>
<input id="s" type="range" min="20" max="300" value="240">
<label>文字の色</label>
<input id="c" type="color" value="#ff69b4">
<p style="margin-top:28px"><a href="/update">ファームウェア更新</a></p>
<script>
const PAL=['#000','#ff69b4','#50c8ff','#ffffff','#aa50ff','#ff283c','#ffc828','#3cdc78'];
const $=id=>document.getElementById(id);
const q=u=>fetch(u).catch(()=>{});
let on=1,faces=[],cur=[],slot=0,sel=1,shown=-1,down=false;
const b=$('b'),bv=$('bv'),ec=$('ec'),sw=$('sw');
function paint(cv,d,cell,gap){
  const x=cv.getContext('2d');x.fillStyle='#111';x.fillRect(0,0,cv.width,cv.height);
  for(let i=0;i<256;i++){const v=d[i]||0;x.fillStyle=v?PAL[v]:'#26262e';
    x.fillRect((i%16)*cell+gap,Math.floor(i/16)*cell+gap,cell-2*gap,cell-2*gap);}
}
function mark(i){
  shown=i;$('t').classList.toggle('on',i<0);
  document.querySelectorAll('#fg canvas').forEach((c,k)=>{c.style.outline=(k===i)?'3px solid #e8559a':'none'});
}
function build(){
  const g=$('fg');g.innerHTML='';
  for(let i=0;i<8;i++){
    const d=document.createElement('div');d.className='fc';
    const cv=document.createElement('canvas');cv.width=cv.height=64;
    paint(cv,faces[i]||[],4,0);
    cv.onclick=()=>{q('/mode?m=face&slot='+i);mark(i)};
    const e=document.createElement('button');e.textContent='編集';
    e.onclick=()=>edit(i);
    d.append(cv,e);g.append(d);
  }
  mark(shown);
}
function edit(i){
  slot=i;cur=(faces[i]||[]).slice();while(cur.length<256)cur.push(0);
  $('et').textContent='表情'+(i+1)+'を編集';$('ed').hidden=false;redraw();
  $('ed').scrollIntoView({behavior:'smooth'});
}
function redraw(){paint(ec,cur,20,1)}
function pick(v){sel=v;[...sw.children].forEach((s,k)=>s.classList.toggle('sel',k+1===v))}
for(let v=1;v<PAL.length;v++){const s=document.createElement('span');s.style.background=PAL[v];s.onclick=()=>pick(v);sw.append(s)}
pick(1);
function dot(e){
  const r=ec.getBoundingClientRect();
  const x=Math.floor((e.clientX-r.left)/r.width*16),y=Math.floor((e.clientY-r.top)/r.height*16);
  if(x<0||x>15||y<0||y>15)return;
  const k=y*16+x;if(cur[k]!==sel){cur[k]=sel;redraw()}
}
ec.onpointerdown=e=>{down=true;ec.setPointerCapture(e.pointerId);dot(e)};
ec.onpointermove=e=>{if(down)dot(e)};
ec.onpointerup=ec.onpointercancel=()=>{down=false};
$('er').onclick=()=>{sel=0;[...sw.children].forEach(s=>s.classList.remove('sel'))};
$('cl').onclick=()=>{cur=new Array(256).fill(0);redraw()};
$('cx').onclick=()=>{$('ed').hidden=true};
$('sv').onclick=()=>{
  const d=cur.map(v=>v.toString(16)).join('');
  fetch('/face',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'slot='+slot+'&d='+d})
    .then(r=>{if(!r.ok)throw 0;faces[slot]=cur.slice();build();mark(slot);$('ed').hidden=true})
    .catch(()=>alert('保存できませんでした'));
};
$('p').onclick=()=>{on=on?0:1;q('/set?on='+on)};
b.oninput=()=>{bv.textContent=b.value;q('/set?b='+b.value)};
$('s').oninput=e=>q('/set?s='+(320-e.target.value));
$('c').oninput=e=>q('/set?c='+e.target.value.slice(1));
$('t').onclick=()=>{q('/mode?m=text');mark(-1)};
fetch('/state').then(r=>r.json()).then(s=>{
  on=s.on;b.value=s.b;bv.textContent=s.b;$('s').value=320-s.s;$('c').value='#'+s.c;mark(s.mode?s.slot:-1);
}).catch(()=>{});
fetch('/faces').then(r=>r.text()).then(t=>{
  faces=t.trim().split('\n').map(l=>[...l].map(ch=>parseInt(ch,16)));build();
}).catch(()=>{});
</script></body></html>)HTML";

void handleRoot() {
  String html = FPSTR(PAGE);
  html.replace("%MAXB%", String(MAX_BRIGHT));
  server.send(200, "text/html; charset=utf-8", html);
}

void handleSet() {
  if (server.hasArg("on")) isOn = server.arg("on") == "1";
  if (server.hasArg("b"))  bright = constrain(server.arg("b").toInt(), 1, MAX_BRIGHT);
  if (server.hasArg("s"))  speedMs = constrain(server.arg("s").toInt(), 20, 300);
  if (server.hasArg("c")) {
    long v = strtol(server.arg("c").c_str(), nullptr, 16);
    colR = (v >> 16) & 0xFF;
    colG = (v >> 8) & 0xFF;
    colB = v & 0xFF;
  }
  dirty = true;
  server.send(200, "text/plain", "ok");
}

void handleState() {
  char buf[128];
  snprintf(buf, sizeof(buf), "{\"on\":%d,\"b\":%d,\"s\":%d,\"mode\":%d,\"slot\":%d,\"c\":\"%02x%02x%02x\"}",
           isOn ? 1 : 0, (int)bright, (int)speedMs, (int)mode, (int)slot, colR, colG, colB);
  server.send(200, "application/json", buf);
}

void handleMode() {
  if (server.hasArg("m")) {
    if (server.arg("m") == "face") {
      int s = server.arg("slot").toInt();
      if (s >= 0 && s < FACE_SLOTS) { mode = 1; slot = s; }
    } else {
      mode = 0;
      offsetX = 0;
    }
    prefs.putUChar("mode", mode);
    prefs.putUChar("slot", slot);
    dirty = true;
  }
  server.send(200, "text/plain", "ok");
}

void handleFaces() {
  String out;
  out.reserve(FACE_SLOTS * 257 + 1);
  for (int i = 0; i < FACE_SLOTS; i++) {
    for (int j = 0; j < 256; j++) out += "0123456789abcdef"[faces[i][j] & 15];
    out += '\n';
  }
  server.send(200, "text/plain", out);
}

void handleFace() {
  int s = server.arg("slot").toInt();
  String d = server.arg("d");
  if (!server.hasArg("slot") || s < 0 || s >= FACE_SLOTS || d.length() != 256) {
    server.send(400, "text/plain", "bad request");
    return;
  }
  for (int j = 0; j < 256; j++) {
    int v = hexv(d.charAt(j));
    faces[s][j] = (v >= 0 && v <= 7) ? v : 0;       // 範囲外の色は消灯にする
  }
  char key[4];
  faceKey(s, key);
  prefs.putBytes(key, faces[s], 256);
  mode = 1;
  slot = s;
  prefs.putUChar("mode", mode);
  prefs.putUChar("slot", slot);
  dirty = true;
  server.send(200, "text/plain", "ok");
}

// ====== Wi-Fi経由のファームウェア更新（スマホのブラウザから .bin を送る） ======
const char UPDATE_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="ja"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ファームウェア更新</title></head>
<body style="font-family:sans-serif;padding:20px;max-width:420px;margin:auto">
<h1 style="font-size:20px">ファームウェア更新</h1>
<p>rina_text.app.bin を選んで「更新する」を押します。終わると自動で再起動します。</p>
<form method="POST" action="/update" enctype="multipart/form-data">
<input type="file" name="fw" accept=".bin"><br><br>
<button style="width:100%;padding:14px;font-size:18px">更新する</button>
</form></body></html>)HTML";

void handleUpdatePage() {
  server.send(200, "text/html; charset=utf-8", FPSTR(UPDATE_PAGE));
}

void handleUpdateDone() {
  bool ok = !Update.hasError();
  server.send(200, "text/plain; charset=utf-8", ok ? "OK。再起動します" : "失敗しました。もう一度試してください");
  delay(500);
  if (ok) ESP.restart();
}

void handleUpdateUpload() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    Update.begin(UPDATE_SIZE_UNKNOWN);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    Update.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    Update.end(true);
  }
}

void setup() {
  prefs.begin("rina", false);
  mode = prefs.getUChar("mode", 0);
  slot = prefs.getUChar("slot", 0);
  if (mode > 1) mode = 0;
  if (slot >= FACE_SLOTS) slot = 0;
  loadFaces();

  strip.begin();
  strip.setBrightness(bright);
  strip.show();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);

  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.on("/state", handleState);
  server.on("/mode", handleMode);
  server.on("/faces", handleFaces);
  server.on("/face", HTTP_POST, handleFace);
  server.on("/update", HTTP_GET, handleUpdatePage);
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server.begin();
}

void loop() {
  server.handleClient();
  unsigned long now = millis();
  bool step = (mode == 0) && (now - lastStep >= speedMs);
  if (step) {
    lastStep = now;
    offsetX = (offsetX + 1) % (TEXT_LEN + PANEL_W);   // 右から左へ流れる
  }
  if (step || dirty) {
    dirty = false;
    drawFrame();
  }
}
