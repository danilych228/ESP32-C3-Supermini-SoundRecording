# ESP32-C3-Supermini-SoundRecording
Short guide, how to record sound without I2S.

## Connection Tab

| Mic pins | Esp pins |
|:---:|:---:|
| **GND** | `GND` |
| **SIG** | `GPIO3` |
| **SIG** | `3.3V` (pull-up with **8k2** resistor)|

## ️!!!!!!!!

> 8k2 resistor need be connected **between 3.3V и SIG** (parallel to mic), not sequence!  

## Mermaid-scheme

```mermaid
graph LR
    VCC[3.3V] --> R[8k2 resistor]
    R --> SIG[SIG / GPIO3]
    MIC[mic] --> SIG
    MIC --> GND[GND]
    SIG -.-> ESP[ESP32-C3 ADC1_CH3]
