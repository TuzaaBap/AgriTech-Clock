#pragma once

#include <stdint.h>

// Photo: P10-32X16-3535-8S-M1. Three modules in a horizontal chain.
constexpr uint16_t PANEL_WIDTH = 32;
constexpr uint16_t PANEL_HEIGHT = 16;
constexpr uint16_t PANEL_COUNT = 3;
constexpr uint16_t DISPLAY_WIDTH = PANEL_WIDTH * PANEL_COUNT;
constexpr uint8_t DISPLAY_BRIGHTNESS = 155;  // User-selected level, 0..255.
constexpr char DISPLAY_TEXT[] = "AGRITECH";

// WF1 v7.0.1 ESP32-S2, onboard 75EX1 connector.
// Source: mrcodetastic/HD-WF1-WF2-LED-MatrixPanel-DMA,
// src/wf1wf2demo/hd-wf1-esp32s2-config.h.
constexpr int8_t R1_PIN = 2, G1_PIN = 6, B1_PIN = 3;
constexpr int8_t R2_PIN = 4, G2_PIN = 8, B2_PIN = 5;
constexpr int8_t A_PIN = 39, B_PIN = 38, C_PIN = 37;
constexpr int8_t LAT_PIN = 33, OE_PIN = 35, CLK_PIN = 34;
constexpr uint8_t TEST_BUTTON_PIN = 11;

