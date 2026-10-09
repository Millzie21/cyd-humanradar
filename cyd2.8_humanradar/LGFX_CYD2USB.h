#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// Define PANEL_INVERT before including this file to override (true if colors look inverted)
#ifndef PANEL_INVERT
#define PANEL_INVERT false
#endif

// ---------------- LovyanGFX config for CYD2USB ----------------
// ESP32-2432S028 2.8" two-USB version (ST7789)
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel;   // use Panel_ILI9341 if your board is the 1-USB CYD
  lgfx::Bus_SPI      _bus;
  lgfx::Light_PWM    _light;

public:
  LGFX() {
    {
      auto cfg = _bus.config();
      cfg.spi_host    = HSPI_HOST;
      cfg.spi_mode    = 0;
      cfg.freq_write  = 80000000;
      cfg.freq_read   = 16000000;
      cfg.spi_3wire   = false;
      cfg.use_lock    = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = 14;
      cfg.pin_mosi = 13;
      cfg.pin_miso = 12;
      cfg.pin_dc   = 2;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      auto cfg = _panel.config();
      cfg.pin_cs   = 15;
      cfg.pin_rst  = -1;
      cfg.pin_busy = -1;
      cfg.panel_width  = 240;
      cfg.panel_height = 320;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation  = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable   = true;
      cfg.invert     = PANEL_INVERT;
      cfg.rgb_order  = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      _panel.config(cfg);
    }
    {
      auto cfg = _light.config();
      cfg.pin_bl      = 21;
      cfg.invert      = false;
      cfg.freq        = 44100;
      cfg.pwm_channel = 7;
      _light.config(cfg);
      _panel.setLight(&_light);
    }
    setPanel(&_panel);
  }
};