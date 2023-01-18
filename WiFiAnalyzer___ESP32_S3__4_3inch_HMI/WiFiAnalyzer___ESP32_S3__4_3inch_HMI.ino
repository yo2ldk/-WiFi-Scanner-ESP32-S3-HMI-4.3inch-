
#define SCAN_INTERVAL 1500
#define LCD_PWR_PIN 2


#include <Arduino_GFX_Library.h>

#define GFX_BL DF_GFX_BL 
#if defined(DISPLAY_DEV_KIT)
Arduino_GFX *gfx = create_default_Arduino_GFX();
#else 
Arduino_ESP32RGBPanel *bus = new Arduino_ESP32RGBPanel(
    GFX_NOT_DEFINED /* CS */, GFX_NOT_DEFINED /* SCK */, GFX_NOT_DEFINED /* SDA */,
    40 /* DE */, 41 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
    45 /* R0 */, 48 /* R1 */, 47 /* R2 */, 21 /* R3 */, 14 /* R4 */,
    5 /* G0 */, 6 /* G1 */, 7 /* G2 */, 15 /* G3 */, 16 /* G4 */, 4 /* G5 */,
    8 /* B0 */, 3 /* B1 */, 46 /* B2 */, 9 /* B3 */, 1 /* B4 */
);

 Arduino_RPi_DPI_RGBPanel *gfx = new Arduino_RPi_DPI_RGBPanel(
   bus,
   800 /* width */, 0 /* hsync_polarity */, 8 /* hsync_front_porch */, 4 /* hsync_pulse_width */, 8 /* hsync_back_porch */,
   480 /* height */, 0 /* vsync_polarity */, 8 /* vsync_front_porch */, 4 /* vsync_pulse_width */, 8 /* vsync_back_porch */,
   1 /* pclk_active_neg */, 16000000 /* prefer_speed */, true /* auto_flush */); 
#endif /* !defined(DISPLAY_DEV_KIT) */
/*******************************************************************************
 * End of Arduino_GFX setting
 ******************************************************************************/

#if defined(ESP32)
#include "WiFi.h"
#else
#include "ESP8266WiFi.h"
#define log_i(format, ...) Serial.printf(format, ##__VA_ARGS__)
#endif

int16_t w, h, text_size, banner_height, graph_baseline, graph_height, channel_width, signal_width;

// RSSI RANGE
#define RSSI_CEILING -40
#define RSSI_FLOOR -100

uint16_t channel_color[] = {
    RED, ORANGE, YELLOW, GREEN, CYAN, BLUE, MAGENTA,
    RED, ORANGE, YELLOW, GREEN, CYAN, BLUE, MAGENTA};

uint8_t scan_count = 0;

void setup()
{
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

#if defined(LCD_PWR_PIN)
  pinMode(LCD_PWR_PIN, OUTPUT);    
  digitalWrite(LCD_PWR_PIN, HIGH); 
#endif

#ifdef GFX_BL
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);
#endif

  // init LCD
  gfx->begin();
  //gfx->fillScreen(BACKGROUND);  
  w = gfx->width();
  h = gfx->height();
  text_size = (h < 200) ? 2 : 3;                     
  banner_height = text_size * 5 * 4;                 
  graph_baseline = h - 20;                            
  graph_height = graph_baseline - banner_height - 30; 
  channel_width = w / 17;
  signal_width = channel_width * 2;

  // init banner
  gfx->setTextSize(text_size);
  gfx->fillScreen(BLACK);
  gfx->setTextColor(RED);
  gfx->setCursor(240, 10);
  gfx->print("ESP");
  gfx->setTextColor(WHITE);
  gfx->print(" WiFi Analyzer");
}

bool matchBssidPrefix(uint8_t *a, uint8_t *b)
{
  for (uint8_t i = 0; i < 5; i++)
  {
    if (a[i] != b[i])
    {
      return false;
    }
  }
  return true;
}

void loop()
{
  uint8_t ap_count_list[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  int32_t noise_list[] = {RSSI_FLOOR, RSSI_FLOOR, RSSI_FLOOR, RSSI_FLOOR, RSSI_FLOOR, RSSI_FLOOR, RSSI_FLOOR, RSSI_FLOOR, RSSI_FLOOR, RSSI_FLOOR, RSSI_FLOOR, RSSI_FLOOR, RSSI_FLOOR, RSSI_FLOOR};
  int32_t peak_list[] = {RSSI_FLOOR, RSSI_FLOOR, RSSI_FLOOR, RSSI_FLOOR, RSSI_FLOOR, RSSI_FLOOR, RSSI_FLOOR, RSSI_FLOOR, RSSI_FLOOR, RSSI_FLOOR, RSSI_FLOOR, RSSI_FLOOR, RSSI_FLOOR, RSSI_FLOOR};
  int16_t peak_id_list[] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
  int32_t channel;
  int16_t idx;
  int32_t rssi;
  uint8_t *bssid;
  String ssid;
  uint16_t color;
  int16_t height, offset, text_width;

#if defined(ESP32)
  int n = WiFi.scanNetworks(false /* async */, true /* show_hidden */, true /* passive */, 500 /* max_ms_per_chan */);
#else
  int n = WiFi.scanNetworks(false /* async */, true /* show_hidden */);
#endif

  gfx->fillRect(0, banner_height, w, h - banner_height, BLACK);
  gfx->setTextSize(1);

  if (n == 0)
  {
    gfx->setTextColor(WHITE);
    gfx->setCursor(240, banner_height +20);
    gfx->println("no networks found");
  }
  else
  {
    for (int i = 0; i < n; i++)
    {
      channel = WiFi.channel(i);
      idx = channel - 1;
      rssi = WiFi.RSSI(i);
      bssid = WiFi.BSSID(i);

      if (peak_list[idx] < rssi)
      {
        peak_list[idx] = rssi;
        peak_id_list[idx] = i;
      }

      bool duplicate_SSID = false;
      for (int j = 0; j < i; j++)
      {
        if ((WiFi.channel(j) == channel) && matchBssidPrefix(WiFi.BSSID(j), bssid))
        {
          duplicate_SSID = true;
          break;
        }
      }

      if (!duplicate_SSID)
      {
        ap_count_list[idx]++;

        int32_t noise = rssi - RSSI_FLOOR;
        noise *= noise;
        if (channel > 4)
        {
          noise_list[idx - 4] += noise;
        }
        if (channel > 3)
        {
          noise_list[idx - 3] += noise;
        }
        if (channel > 2)
        {
          noise_list[idx - 2] += noise;
        }
        if (channel > 1)
        {
          noise_list[idx - 1] += noise;
        }
        noise_list[idx] += noise;
        if (channel < 14)
        {
          noise_list[idx + 1] += noise;
        }
        if (channel < 13)
        {
          noise_list[idx + 2] += noise;
        }
        if (channel < 12)
        {
          noise_list[idx + 3] += noise;
        }
        if (channel < 11)
        {
          noise_list[idx + 4] += noise;
        }
      }
    }

    // plot found WiFi info
    for (int i = 0; i < n; i++)
    {
      channel = WiFi.channel(i);
      idx = channel - 1;
      rssi = WiFi.RSSI(i);
      color = channel_color[idx];
      height = constrain(map(rssi, RSSI_FLOOR, RSSI_CEILING, 1, graph_height), 1, graph_height);
      offset = (channel + 1) * channel_width;

      // trim rssi with RSSI_FLOOR
      if (rssi < RSSI_FLOOR)
      {
        rssi = RSSI_FLOOR;
      }
      gfx->startWrite();
      gfx->drawEllipseHelper(offset, graph_baseline + 1, signal_width, height, 0b0011, color);
      gfx->endWrite();

      if (i == peak_id_list[idx])
      {
        // Print SSID, signal strengh and if not encrypted
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0)
        {
          ssid = WiFi.BSSIDstr(i);
        }
        text_width = (ssid.length() + 6) * 6;
        if (text_width > w)
        {
          offset = 0;
        }
        else
        {
          offset -= signal_width;
          if ((offset + text_width) > w)
          {
            offset = w - text_width;
          }
        }
        gfx->setTextColor(color);
        gfx->setCursor(offset, graph_baseline - 10 - height);
        gfx->setTextSize(2);
        gfx->print(ssid);
        gfx->print('(');
        gfx->print(rssi);
        gfx->print(')');
#if defined(ESP32)
        if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN)
#else
        if (WiFi.encryptionType(i) == ENC_TYPE_NONE)
#endif
        {
          gfx->print('*');
        }
      }
    }
  }

  // print WiFi stat

  gfx->setTextSize(text_size - 1);
  gfx->setTextColor(WHITE);  
  gfx->setCursor(130, banner_height +5);                      
  gfx->print(n);
  gfx->print(" networks found, lesser noise channels: ");
  bool listed_first_channel = false;
  int32_t min_noise = noise_list[0];          
  for (channel = 1; channel <= 14; channel++) 
  {
    idx = channel - 1;
    log_i("min_noise: %d, noise_list[%d]: %d", min_noise, idx, noise_list[idx]);
    if (noise_list[idx] < min_noise)
    {
      min_noise = noise_list[idx];
    }
  }

  for (channel = 1; channel <= 11; channel++) 
  {
    idx = channel - 1;
    if (noise_list[idx] == min_noise)
    {
      if (!listed_first_channel)
      {
        listed_first_channel = true;
      }
      else
      {
        gfx->print(", ");
      }
      gfx->print(channel);
    }
  }

  // draw graph base axle
  gfx->drawFastHLine(0, graph_baseline, 320, WHITE);
  for (channel = 1; channel <= 14; channel++)
  {
    idx = channel - 1;
    offset = (channel + 1) * channel_width;
    gfx->setTextColor(channel_color[idx]);
    gfx->setCursor(offset - ((channel < 10) ? 3 : 6), graph_baseline + 2);
    gfx->print(channel);
    if (ap_count_list[idx] > 0)
    {
      gfx->setCursor(offset - ((ap_count_list[idx] < 10) ? 9 : 12), graph_baseline + 8 + 2);
      gfx->print('{');
      gfx->print(ap_count_list[idx]);
      gfx->print('}');
    }
  }

  // Wait a bit before scanning again
  delay(SCAN_INTERVAL);

#if defined(SCAN_COUNT_SLEEP)
  // POWER SAVING
  if (++scan_count >= SCAN_COUNT_SLEEP)
  {
#if defined(LCD_PWR_PIN)
    pinMode(LCD_PWR_PIN, INPUT); 
#endif

#if defined(GFX_BL)
    pinMode(GFX_BL, INPUT); 
#endif

#if defined(ESP32)
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_36, LOW);
    esp_deep_sleep_start();
#else
    ESP.deepSleep(0);
#endif
  }
#endif 
}
