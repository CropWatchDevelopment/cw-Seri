#!/bin/bash

# Script to automate STM32 board programming via barcode reader
# Captures DEV_EUI from barcode scan, updates main.c, builds, and flashes

# Configuration
WORKSPACE_DIR="/home/kevin/source/STM32CubeIDE/cw-Seri"
MAIN_C_FILE="$WORKSPACE_DIR/Core/Src/main.c"
BACKUP_FILE="$MAIN_C_FILE.bak"
LOG_FILE="$WORKSPACE_DIR/program_board.log"

# STM32CubeIDE paths (adjust if installed elsewhere)
STM32CUBEIDE="/opt/st/stm32cubeide_1.19.0/headless-build.sh"
TOOLCHAIN_BIN="/opt/st/stm32cubeide_1.19.0/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.linux64_1.0.0.202410170706/tools/bin"
STM32_PROGRAMMER="/opt/st/stm32cubeide_1.19.0/plugins/com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.linux64_2.2.200.202503041107/tools/bin/STM32_Programmer_CLI"

# Build and flash settings
PROJECT_NAME="cw-Seri"
BUILD_CONFIG="Debug"
BINARY_FILE="$WORKSPACE_DIR/$BUILD_CONFIG/seri.elf"

# Function to log messages
log() {
    echo "$(date): $1" >> "$LOG_FILE"
    echo "$1"
}

# Function to validate DEV_EUI (16 hex characters)
validate_dev_eui() {
    local eui="$1"
    if [[ ${#eui} -eq 16 && "$eui" =~ ^[0-9A-Fa-f]{16}$ ]]; then
        return 0
    else
        return 1
    fi
}

# Main script
log "Starting board programming process..."

# Create initial backup of main.c
cp "$MAIN_C_FILE" "$BACKUP_FILE"
log "Initial backup created: $BACKUP_FILE"

while true; do
    # Step 1: Capture input from barcode reader
    echo "Scan the barcode (DEV_EUI) and press Enter (or just Enter to exit):"
    read -r scanned_eui

    if [ -z "$scanned_eui" ]; then
        log "Exiting program."
        exit 0
    fi

    # Step 2: Validate input
    if ! validate_dev_eui "$scanned_eui"; then
        log "Error: Invalid DEV_EUI format. Must be 16 hexadecimal characters. Received: $scanned_eui"
        continue
    fi

    log "Valid DEV_EUI scanned: $scanned_eui"

    # Step 3: Update DEV_EUI in main.c
    # Use sed to replace the line
    sed -i "s/#define DEV_EUI  \"[0-9A-Fa-f]*\"/#define DEV_EUI  \"$scanned_eui\"/g" "$MAIN_C_FILE"

    if [ $? -eq 0 ]; then
        log "DEV_EUI updated in $MAIN_C_FILE"
    else
        log "Error: Failed to update DEV_EUI in $MAIN_C_FILE"
        continue
    fi

    # Step 4: Build the project
    log "Building project..."
    export PATH="$TOOLCHAIN_BIN:$PATH"
    cd "$WORKSPACE_DIR/$BUILD_CONFIG"
    make all
    BUILD_EXIT_CODE=$?

    if [ $BUILD_EXIT_CODE -ne 0 ]; then
        log "Error: Build failed with exit code $BUILD_EXIT_CODE"
        # Restore backup
        cp "$BACKUP_FILE" "$MAIN_C_FILE"
        continue
    fi

    log "Build successful."

    # Step 5: Flash the MCU
    log "Flashing MCU..."
    if [ -f "$STM32_PROGRAMMER" ]; then
        "$STM32_PROGRAMMER" -c port=SWD -w "$BINARY_FILE" -v
        FLASH_EXIT_CODE=$?
    else
        log "Error: STM32CubeProgrammer not found at $STM32_PROGRAMMER. Please update the path."
        # Restore backup
        cp "$BACKUP_FILE" "$MAIN_C_FILE"
        continue
    fi

    if [ $FLASH_EXIT_CODE -ne 0 ]; then
        log "Error: Flashing failed with exit code $FLASH_EXIT_CODE"
        # Restore backup
        cp "$BACKUP_FILE" "$MAIN_C_FILE"
        continue
    fi

    log "Flashing successful. Board programmed with DEV_EUI: $scanned_eui"

    # Restore original main.c for next board
    cp "$BACKUP_FILE" "$MAIN_C_FILE"
    log "Restored original main.c for next board."
done
