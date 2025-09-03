# LoRaWAN UART2 Communication Fixes

## Summary of Issues Fixed

### 1. **UART2 Clock Being Disabled During Sleep Mode**
**Problem**: In `EnterDeepSleepMode()`, the UART2 clock was being disabled with `__HAL_RCC_USART2_CLK_DISABLE()`, which prevented communication after the first sleep cycle.

**Fix**: Commented out the UART2 clock disable line to keep UART2 active during sleep mode:
```c
// Keep UART2 clock enabled for LoRaWAN communication
// __HAL_RCC_USART2_CLK_DISABLE();
```

### 2. **Missing UART API Implementation**
**Problem**: The code was trying to use `LORA_Uart_Expect_Response_For_Tx()` function from `uart_api.h`, but this API wasn't properly implemented, causing compilation or runtime issues.

**Fix**: 
- Commented out the `#include "uart_api.h"` include
- Replaced the complex `join()` function with a simplified version using standard HAL UART functions
- Added proper timeout handling and response parsing

### 3. **Main Loop Logic Issues**
**Problem**: The original main loop only performed LoRaWAN operations on the first run, then went directly to sleep without checking connection status on subsequent cycles.

**Fix**: Modified the main loop to:
- Always check connection status (`is_connected == 0`) on every transmission cycle
- Attempt to join the network if not connected
- Perform sensor operations every transmission cycle when connected
- Properly manage I2C power control

### 4. **Added UART Communication Testing**
**Enhancement**: Added a simple `test_uart_communication()` function to verify basic UART connectivity before attempting complex LoRaWAN operations.

## Key Changes Made

### Modified Functions:

1. **`join()` function** - Completely rewritten to use standard HAL functions:
   ```c
   int join(UART_HandleTypeDef *huart)
   {
       // Simplified implementation using HAL_UART_Transmit/Receive
       // Added proper timeout handling
       // Improved response parsing
   }
   ```

2. **`EnterDeepSleepMode()` function** - Kept UART2 clock enabled:
   ```c
   // __HAL_RCC_USART2_CLK_DISABLE(); // Commented out
   ```

3. **Main loop logic** - Fixed transmission cycle handling:
   ```c
   if (first_run || wakeup_counter >= WAKEUPS_PER_CYCLE)
   {
       // Always check and attempt connection
       if (is_connected == 0) {
           if (test_uart_communication(&huart2)) {
               join(&huart2);
           }
       }
       // Always perform sensor operations when connected
       // ...
   }
   ```

### Added Functions:

1. **`test_uart_communication()`** - Basic UART connectivity test

## Expected Behavior After Fixes

1. **First Boot**: Device will test UART communication, attempt to join LoRaWAN network, read sensors, and transmit data
2. **Sleep Cycles**: Device sleeps for the configured time (32-second intervals)
3. **Wake Up**: Every WAKEUPS_PER_CYCLE (based on SLEEP_TIME_MINUTES), device will:
   - Check if still connected to LoRaWAN network
   - Re-join if disconnected
   - Read sensor data
   - Transmit data if connected
   - Return to sleep

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

## Test Code for Debug:

If you need to test basic UART communication, you can temporarily replace the sensor operations with this simple test:

```c
// Test UART communication
HAL_UART_Transmit(&huart2, (uint8_t*)"AT\r\n", 4, 1000);
HAL_Delay(1000);
uint8_t response[32] = {0};
HAL_UART_Receive(&huart2, response, sizeof(response), 2000);
// Check response for "OK"
```
