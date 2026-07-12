#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>

const char* ssid = "YOUR NET SSID";
const char* password = "YOUR NET PWD";

#define MIC_PIN 3
#define SAMPLE_RATE 16000
#define BUFFER_SIZE 4096

WebServer server(80);

uint8_t* tempBuffer = nullptr;
uint32_t bufferIndex = 0;
uint32_t totalSamples = 0;
volatile bool isRecording = false;
File audioFile;
TaskHandle_t recordTaskHandle = NULL;

extern const char index_html[];

void createWavHeader(uint8_t* header) {
  const uint8_t wavHeader[] = {
    0x52, 0x49, 0x46, 0x46, 0, 0, 0, 0, 0x57, 0x41, 0x56, 0x45, 
    0x66, 0x6D, 0x74, 0x20, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 
    0x01, 0x00, 0, 0, 0, 0, 0, 0, 0, 0, 0x02, 0x00, 0x10, 0x00, 
    0x64, 0x61, 0x74, 0x61, 0, 0, 0, 0
  };
  memcpy(header, wavHeader, 44);
  
  uint32_t byteRate = SAMPLE_RATE * 2;
  header[24] = SAMPLE_RATE & 0xFF; header[25] = (SAMPLE_RATE >> 8) & 0xFF;
  header[26] = (SAMPLE_RATE >> 16) & 0xFF; header[27] = (SAMPLE_RATE >> 24) & 0xFF;
  header[28] = byteRate & 0xFF; header[29] = (byteRate >> 8) & 0xFF;
  header[30] = (byteRate >> 16) & 0xFF; header[31] = (byteRate >> 24) & 0xFF;
}

void updateWavHeader() {
  uint32_t dataSize = totalSamples * 2;
  uint32_t fileSize = dataSize + 36;
  
  audioFile.seek(0);
  uint8_t header[44];
  createWavHeader(header);
  
  header[4] = fileSize & 0xFF; header[5] = (fileSize >> 8) & 0xFF; 
  header[6] = (fileSize >> 16) & 0xFF; header[7] = (fileSize >> 24) & 0xFF;
  header[40] = dataSize & 0xFF; header[41] = (dataSize >> 8) & 0xFF;
  header[42] = (dataSize >> 16) & 0xFF; header[43] = (dataSize >> 24) & 0xFF;
  
  audioFile.write(header, 44);
}

void flushBuffer() {
  if (bufferIndex > 0) {
    audioFile.write(tempBuffer, bufferIndex);
    bufferIndex = 0;
  }
}

void recordTask(void *pvParameters) {
  totalSamples = 0;
  bufferIndex = 0;
  uint32_t nextSampleTime = micros();
  uint32_t sampleInterval = 1000000 / SAMPLE_RATE;
  
  while (isRecording) {
    int val = analogRead(MIC_PIN); 
    int16_t sample = (val - 2048) * 16; 
    
    tempBuffer[bufferIndex++] = sample & 0xFF;
    tempBuffer[bufferIndex++] = (sample >> 8) & 0xFF;
    totalSamples++;
    
    if (bufferIndex >= BUFFER_SIZE) {
      flushBuffer();
    }
    
    nextSampleTime += sampleInterval; 
    int32_t delayTime = nextSampleTime - micros();
    if (delayTime > 0) delayMicroseconds(delayTime);
    else nextSampleTime = micros(); 
  }
  
  flushBuffer();
  updateWavHeader();
  audioFile.close();
  
  isRecording = false;
  vTaskDelete(NULL);
}

void setup() {
  Serial.begin(115200);
  
  tempBuffer = (uint8_t*)malloc(BUFFER_SIZE);
  if (!tempBuffer) { 
    Serial.println("RAM Error!"); 
    while(1); 
  }
  
  SPIFFS.begin(true);
  
  uint32_t totalBytes = SPIFFS.totalBytes();
  uint32_t usedBytes = SPIFFS.usedBytes();
  Serial.printf("SPIFFS: %u КБ всего, %u КБ свободно\n", totalBytes/1024, (totalBytes-usedBytes)/1024);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nIP: " + WiFi.localIP().toString());

  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", index_html);
  });

  server.on("/start", HTTP_GET, []() {
    if (isRecording) { server.send(400, "text/plain", "Already recording"); return; }
    
    audioFile = SPIFFS.open("/record.wav", FILE_WRITE);
    if (!audioFile) { server.send(500, "text/plain", "Cannot open file"); return; }
    
    uint8_t header[44];
    createWavHeader(header);
    audioFile.write(header, 44);
    
    isRecording = true;
    xTaskCreate(recordTask, "recordTask", 4096, NULL, 2, &recordTaskHandle);
    server.send(200, "text/plain", "Started");
  });

  server.on("/stop", HTTP_GET, []() {
    if (!isRecording) { server.send(400, "text/plain", "Not recording"); return; }
    isRecording = false;
    delay(200);
    server.send(200, "text/plain", "Saved");
  });

  server.on("/audio.wav", HTTP_GET, []() {
    if (!SPIFFS.exists("/record.wav")) { server.send(404, "text/plain", "No file"); return; }
    File file = SPIFFS.open("/record.wav", FILE_READ);
    server.streamFile(file, "audio/wav");
    file.close();
  });

  server.on("/download", HTTP_GET, []() {
    if (!SPIFFS.exists("/record.wav")) { server.send(404, "text/plain", "No file"); return; }
    File file = SPIFFS.open("/record.wav", FILE_READ);
    server.sendHeader("Content-Disposition", "attachment; filename=\"esp32_record.wav\"");
    server.streamFile(file, "audio/wav");
    file.close();
  });

  server.begin();
}

void loop() {
  server.handleClient();
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru" class="dark">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP Voice Recorder</title>
  <script src="https://cdn.tailwindcss.com"></script>
  <script>
    tailwind.config = { darkMode: 'class', theme: { extend: { colors: { dark: '#121212', card: '#1e1e1e' } } } }
  </script>
  <style>
    body { background-color: #121212; }
    .pulse { animation: pulse 1.5s cubic-bezier(0.4, 0, 0.6, 1) infinite; }
    @keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: .5; } }
  </style>
</head>
<body class="text-gray-200 flex items-center justify-center min-h-screen font-sans">
  <div class="bg-card p-8 rounded-2xl shadow-2xl w-full max-w-md border border-gray-800">
    <h1 class="text-2xl font-bold text-center mb-6 text-white">🎙️ ESP32-C3 Recorder</h1>
    
    <div class="flex gap-4 mb-6">
      <button id="btnRec" onclick="toggleRec()" class="flex-1 bg-red-600 hover:bg-red-700 text-white font-bold py-3 px-4 rounded-lg transition flex items-center justify-center gap-2">
        <span id="recIcon">⏺</span> <span id="recText">Записать</span>
      </button>
      <button id="btnPlay" onclick="playAudio()" disabled class="flex-1 bg-green-600 hover:bg-green-700 disabled:bg-gray-700 disabled:cursor-not-allowed text-white font-bold py-3 px-4 rounded-lg transition">
        ▶ Слушать
      </button>
    </div>

    <div id="status" class="text-center text-gray-400 text-sm h-5 mb-4">Готов к записи (16 кГц, до 30 сек)</div>
    
    <audio id="audioPlayer" class="hidden"></audio>
    
    <div class="mt-6 pt-4 border-t border-gray-700 text-center">
      <a href="/download" class="text-blue-400 hover:text-blue-300 text-sm underline">⬇ Скачать WAV файл</a>
    </div>
  </div>

  <script>
    var isRec = false;
    var btnRec, recIcon, recText, status, btnPlay;

    document.addEventListener('DOMContentLoaded', function() {
      btnRec = document.getElementById('btnRec');
      recIcon = document.getElementById('recIcon');
      recText = document.getElementById('recText');
      status = document.getElementById('status');
      btnPlay = document.getElementById('btnPlay');
    });

    var toggleRec = function() {
      if (!isRec) {
        status.innerText = 'Идет запись...';
        status.className = 'text-center text-red-400 text-sm h-5 mb-4 pulse';
        btnRec.classList.replace('bg-red-600', 'bg-gray-600');
        recText.innerText = 'Стоп';
        recIcon.innerText = '⏹';
        isRec = true;
        fetch('/start');
      } else {
        status.innerText = 'Сохранение...';
        status.className = 'text-center text-yellow-400 text-sm h-5 mb-4';
        btnRec.disabled = true;
        fetch('/stop');
        
        setTimeout(function() {
          status.innerText = 'Готово!';
          status.className = 'text-center text-green-400 text-sm h-5 mb-4';
          btnRec.disabled = false;
          btnRec.classList.replace('bg-gray-600', 'bg-red-600');
          recText.innerText = 'Записать';
          recIcon.innerText = '⏺';
          btnPlay.disabled = false;
          isRec = false;
        }, 500);
      }
    }

    var playAudio = function() {
      var player = document.getElementById('audioPlayer');
      player.src = '/audio.wav?_=' + new Date().getTime(); 
      player.play();
    }
  </script>
</body>
</html>
)rawliteral";
