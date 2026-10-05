// 璃奈ちゃんボード用：「名探偵プリキュア！」を左から右へ流す
// ESP32 + WS2812B 16x16パネル（左下スタート／ジグザグ／横方向）
// スマホでWi-Fi「RinaBoard」に接続 → ブラウザで http://192.168.4.1/ を開く
//
// 必要ライブラリ: Adafruit NeoPixel
//   arduino-cli lib install "Adafruit NeoPixel"

#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <Adafruit_NeoPixel.h>

// ====== 設定（環境に合わせて変更） ======
#define LED_PIN     14     // パネルのDINにつないでいるGPIO番号（D14）
#define PANEL_W     16
#define PANEL_H     16
#define NUM_LEDS    (PANEL_W * PANEL_H)
#define MAX_BRIGHT  60     // モバイルバッテリー保護のための明るさ上限（最大255）

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

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
WebServer server(80);

bool     isOn    = true;
uint8_t  bright  = 20;
uint16_t speedMs = 80;            // 1列ずれるまでの時間(ms)。小さいほど速い
uint8_t  colR = 255, colG = 105, colB = 180;
int      offsetX = TEXT_LEN + PANEL_W - 1;
unsigned long lastStep = 0;

// 画面座標(x:左0〜15, y:上0〜15) → LED番号
// 配線：左下スタート、1行ごとに向きが反転するジグザグ、横方向
int ledIndex(int x, int y) {
  int r = (PANEL_H - 1) - y;                       // 下から数えた行
  return r * PANEL_W + ((r % 2 == 0) ? x : (PANEL_W - 1 - x));
}

void drawFrame() {
  strip.clear();
  if (isOn) {
    uint32_t c = strip.Color(colR, colG, colB);
    for (int x = 0; x < PANEL_W; x++) {
      int col = offsetX + x - PANEL_W;             // 最初と最後は空白で流れる
      if (col < 0 || col >= TEXT_LEN) continue;
      uint16_t bits = pgm_read_word(&TEXT_COLS[col]);
      for (int y = 0; y < PANEL_H; y++) {
        if (bits & (1u << y)) strip.setPixelColor(ledIndex(x, y), c);
      }
    }
  }
  strip.setBrightness(bright);
  strip.show();
}

const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="ja"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>りなちゃんボード</title>
<style>
body{font-family:sans-serif;margin:0;padding:20px;max-width:420px;margin:auto}
h1{font-size:20px}
label{display:block;margin:18px 0 6px}
input[type=range]{width:100%}
input[type=color]{width:100%;height:44px;border:none;padding:0}
button{width:100%;padding:14px;font-size:18px;border:none;border-radius:10px;background:#e8559a;color:#fff}
</style></head><body>
<h1>りなちゃんボード</h1>
<button id="p">ON / OFF</button>
<label>明るさ <span id="bv"></span></label>
<input id="b" type="range" min="1" max="%MAXB%" value="20">
<label>流れる速さ</label>
<input id="s" type="range" min="20" max="300" value="240">
<label>色</label>
<input id="c" type="color" value="#ff69b4">
<p style="margin-top:28px"><a href="/update">ファームウェア更新</a></p>
<script>
let on=1;
const q=(k,v)=>fetch('/set?'+k+'='+encodeURIComponent(v));
document.getElementById('p').onclick=()=>{on=on?0:1;q('on',on)};
const b=document.getElementById('b'),bv=document.getElementById('bv');
b.oninput=()=>{bv.textContent=b.value;q('b',b.value)};
bv.textContent=b.value;
document.getElementById('s').oninput=e=>q('s',320-e.target.value);
document.getElementById('c').oninput=e=>q('c',e.target.value.slice(1));
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
  strip.begin();
  strip.setBrightness(bright);
  strip.show();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);

  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.on("/update", HTTP_GET, handleUpdatePage);
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server.begin();
}

void loop() {
  server.handleClient();
  unsigned long now = millis();
  if (now - lastStep >= speedMs) {
    lastStep = now;
    offsetX--;                                    // 左から右へ流れる
    if (offsetX < 0) offsetX = TEXT_LEN + PANEL_W - 1;
    drawFrame();
  }
}
