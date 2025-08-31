# LoRaWAN UART2 Communication Fixes

## Issues Found and Fixed:

### 1. **Missing Function Declaration**
- **Problem**: `LoRaWAN_SendHex` function was called but not declared in function prototypes
- **Fix**: Added declaration to function prototypes section

### 2. **GPIO Configuration Issue During Sleep**
- **Problem**: UART pins PA2/PA3 were being configured as analog pins during deep sleep mode
- **Fix**: Excluded UART pins from analog configuration to maintain UART functionality

### 3. **UART Peripheral Deinitialization**
- **Problem**: UART2 was being deinitialized during sleep mode, breaking LoRaWAN communication
- **Fix**: Keep UART2 initialized and clock enabled during sleep mode

### 4. **Missing Error Handling**
- **Problem**: No proper error checking for LoRaWAN transmission status
- **Fix**: Added error handling and response checking in transmission functions

### 5. **Insufficient Module Initialization**
- **Problem**: No proper wake-up sequence for LoRaWAN module on startup
- **Fix**: Added proper initialization sequence with AT commands

## Key Changes Made:

1. **Function Declaration**: Added `LoRaWAN_SendHex` to function prototypes
2. **Sleep Mode**: Modified `EnterDeepSleepMode()` to preserve UART2 functionality
3. **GPIO Config**: Updated `ConfigureGPIOForLowPower()` to exclude UART pins
4. **Error Handling**: Enhanced transmission functions with proper error checking
5. **Initialization**: Added proper LoRaWAN module wake-up sequence

## Debugging Steps:

### For Basic Communication Test:
1. Uncomment the test code section in main function (around line 731)
2. This will send a simple "TEST123" message every startup
3. Check if the LoRaWAN module responds

### For Production Debugging:
1. Enable debug output by modifying `dbg_print` functions
2. Check `is_connected` status before transmission
3. Monitor consecutive_errors counter
4. Verify sensor data is valid before transmission

## Common Issues to Check:

1. **LoRaWAN Module Configuration**:
   - Ensure OTAA keys are properly set
   - Verify region settings (AS923-1 for Japan)
   - Check if module is joined to network

2. **Hardware Connections**:
   - Verify PA2 (TX) and PA3 (RX) connections to LoRaWAN module
   - Check power supply to LoRaWAN module
   - Ensure proper grounding

3. **Timing Issues**:
   - LoRaWAN module may need more time to boot
   - Sleep/wake cycles might interrupt ongoing transmissions
   - RTC timing might need adjustment

## Next Steps if Issues Persist:

1. Use the commented test code to verify basic UART communication
2. Check LoRaWAN module documentation for specific AT command requirements
3. Monitor UART traffic with logic analyzer or debug probe
4. Verify network coverage and LoRaWAN gateway availability
