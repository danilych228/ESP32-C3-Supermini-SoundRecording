# ESP32-C3-Supermini-SoundRecording
Short guide, how to record sound without I2S.

##1. Wiring

***Connection Scheme

     3.3V
      │
      │
    ┌─┴─┐
    │   │ 8.2 кОм (pull-up)
    │   │
    ─┬─┘
      │
      ├──────────────────── GPIO3 (ADC1_CH3)
      │
 ┌────┴────┐
 │         │
 │   MIC   │  ← Электретный капсюль
 │         │
 └────┬────┘
      │
      │
     GND
