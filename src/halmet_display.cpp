
#include "halmet_display.h"

#include <WiFi.h>

namespace halmet {

// OLED display width and height, in pixels
const int kScreenWidth = 128;
const int kScreenHeight = 64;

static int RowY(int row) {
  // Row 0 is reserved for a double-height status band at the bottom.
  // Regular rows start at the top.
  if (row == 0) return 48;  // Status band at bottom (y=48 to y=63)
  return 8 * (row - 1);     // Normal rows at top
}

static int RowHeight(int row) {
  return row == 0 ? 16 : 8;
}

/// Draw a compact SignalK status icon (12x14 pixels)
/// Shows the K symbol with a status indicator dot
static void DrawSignalKIcon(Adafruit_SSD1306* display, int x, int y,
                            bool connected) {
  const int color = SSD1306_WHITE;
  const int inactive_color = SSD1306_WHITE;

  // Draw K symbol: vertical line + two diagonal lines
  // Vertical bar (left edge of K)
  display->drawLine(x + 2, y + 2, x + 2, y + 11, color);

  // Upper diagonal (/ shape)
  display->drawLine(x + 2, y + 6, x + 7, y + 2, color);

  // Lower diagonal (\ shape)
  display->drawLine(x + 2, y + 6, x + 7, y + 11, color);

  // Status indicator dot (top-right)
  if (connected) {
    // ON: Filled circle with brighter appearance
    display->fillCircle(x + 9, y + 2, 1, color);
    display->drawCircle(x + 9, y + 2, 2, color);  // Outer ring for active status
  } else {
    // OFF: Outlined circle
    display->drawCircle(x + 9, y + 2, 1, inactive_color);
  }
}

/// Draw a compact Clipper status icon (12x14 pixels)
static void DrawClipperIcon(Adafruit_SSD1306* display, int x, int y,
                            bool connected) {
  const int color = SSD1306_WHITE;

  // Stylized "C" glyph.
  display->drawLine(x + 2, y + 2, x + 7, y + 2, color);
  display->drawLine(x + 2, y + 11, x + 7, y + 11, color);
  display->drawLine(x + 2, y + 2, x + 2, y + 11, color);

  // Status indicator dot (top-right)
  if (connected) {
    display->fillCircle(x + 9, y + 2, 1, color);
    display->drawCircle(x + 9, y + 2, 2, color);
  } else {
    display->drawCircle(x + 9, y + 2, 1, color);
  }
}

bool InitializeSSD1306(const std::shared_ptr<sensesp::SensESPBaseApp> sensesp_app,
                       Adafruit_SSD1306** display, TwoWire* i2c) {
  *display = new Adafruit_SSD1306(kScreenWidth, kScreenHeight, i2c, -1);
  bool init_successful = (*display)->begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!init_successful) {
    debugD("SSD1306 allocation failed");
    return false;
  }
  delay(100);
  (*display)->setRotation(2);
  (*display)->clearDisplay();
  (*display)->setTextSize(1);
  (*display)->setTextColor(SSD1306_WHITE);
  (*display)->setCursor(0, 0);
  (*display)->printf("Host: %s\n", sensesp_app->get_hostname().c_str());
  (*display)->display();

  return true;
}

/// Clear a text row on an Adafruit graphics display
void ClearRow(Adafruit_SSD1306* display, int row) {
  display->fillRect(0, RowY(row), kScreenWidth, RowHeight(row), 0);
}

void PrintStatusLine(Adafruit_SSD1306* display, bool signalk_connected,
                     bool ds1603_connected, bool clipper_connected) {
  ClearRow(display, 0);

  // Status band is at the bottom (y=48-63). Add offset to all y coordinates.
  const int status_y_offset = 48;

  // WiFi icon: 1..4 bars by RSSI strength.
  // Thresholds tuned for typical ESP32 RSSI quality buckets.
  const int wifi_x = 0;
  int wifi_level = 0;
  if (WiFi.status() == WL_CONNECTED) {
    long rssi = WiFi.RSSI();
    if (rssi >= -55) {
      wifi_level = 4;
    } else if (rssi >= -67) {
      wifi_level = 3;
    } else if (rssi >= -75) {
      wifi_level = 2;
    } else if (rssi >= -85) {
      wifi_level = 1;
    }
  }

  const int bar_heights[4] = {4, 7, 10, 13};
  for (int i = 0; i < 4; i++) {
    int bar_x = wifi_x + i * 4;
    int bar_h = bar_heights[i];
    int bar_y = status_y_offset + 15 - bar_h;
    if (i < wifi_level) {
      display->fillRect(bar_x, bar_y, 3, bar_h, SSD1306_WHITE);
    } else {
      display->drawRect(bar_x, bar_y, 3, bar_h, SSD1306_WHITE);
    }
  }
  display->drawLine(wifi_x - 1, status_y_offset + 15, wifi_x + 15, status_y_offset + 15, SSD1306_WHITE);

  // SignalK status icon with on/off indicator.
  const int sk_x = 20;
  DrawSignalKIcon(display, sk_x, status_y_offset + 2, signalk_connected);

  // Clipper status icon with on/off indicator.
  const int clipper_x = 38;
  DrawClipperIcon(display, clipper_x, status_y_offset + 2, clipper_connected);

  // Fuel sensor icon for DS1603L: pump body + hose/nozzle.
  const int fuel_x = 72;
  const int fuel_bottom = status_y_offset + 15;
  const int fuel_top = fuel_bottom - 10;
  display->drawRect(fuel_x + 1, fuel_top, 8, 11, SSD1306_WHITE);      // pump body
  if (ds1603_connected) {
    display->fillRect(fuel_x + 3, fuel_top + 2, 4, 3, SSD1306_WHITE);  // display window (on)
  } else {
    display->drawRect(fuel_x + 3, fuel_top + 2, 4, 3, SSD1306_WHITE);  // display window (off)
  }
  display->drawLine(fuel_x + 2, fuel_bottom - 1, fuel_x + 7, fuel_bottom - 1,
                    SSD1306_WHITE);                                     // base
  display->drawLine(fuel_x + 8, fuel_top + 2, fuel_x + 11, fuel_top + 1,
                    SSD1306_WHITE);                                     // hose top
  display->drawLine(fuel_x + 11, fuel_top + 1, fuel_x + 11, fuel_top + 6,
                    SSD1306_WHITE);                                     // hose drop
  display->drawLine(fuel_x + 10, fuel_top + 6, fuel_x + 12, fuel_top + 6,
                    SSD1306_WHITE);                                     // nozzle
  if (!ds1603_connected) {
    display->drawLine(fuel_x, fuel_bottom, fuel_x + 13, fuel_top - 1,
                      SSD1306_WHITE);
  }

  display->setTextSize(1);

  display->display();
}

void PrintValue(Adafruit_SSD1306* display, int row, String title, float value) {
  ClearRow(display, row);
  display->setCursor(0, RowY(row));
  display->printf("%s: %.1f", title.c_str(), value);
  display->display();
}

void PrintValue(Adafruit_SSD1306* display, int row, String title,
                String value) {
  ClearRow(display, row);
  display->setCursor(0, RowY(row));
  display->printf("%s: %s", title.c_str(), value.c_str());
  display->display();
}

}  // namespace halmet
