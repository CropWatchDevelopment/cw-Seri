#!/usr/bin/env bash
set -Eeuo pipefail
IFS=$'\n\t'

# ===================== Config =====================
WORKSPACE_DIR="/home/kevin/source/STM32CubeIDE/cw-Seri"
PROJECT_NAME="cw-Seri"
BUILD_CONFIG="Debug"

CONFIG_C_FILE="$WORKSPACE_DIR/Core/Src/lorawan_config.c"
BACKUP_FILE="$CONFIG_C_FILE.bak"
LOG_FILE="$WORKSPACE_DIR/program_board.log"

# Tools
STM32CUBEIDE="/opt/st/stm32cubeide_1.19.0/headless-build.sh"
TOOLCHAIN_BIN="/opt/st/stm32cubeide_1.19.0/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.linux64_1.0.0.202410170706/tools/bin"
STM32_PROGRAMMER="/opt/st/stm32cubeide_1.19.0/plugins/com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.linux64_2.2.200.202503041107/tools/bin/STM32_Programmer_CLI"

# Build mode: "make" (fast) or "cubeide" (headless builder)
BUILD_MODE="make"

# ST-LINK options (optional)
STLINK_FREQ="4000"   # kHz
STLINK_SN=""         # set to lock to a specific probe (optional)

# ======= TTI CSV constants (per docs/screenshot) =======
TTI_FREQ_PLAN_ID="AS_920_923_LBT"
TTI_LORAWAN_VER="MAC_V1_0_4"
TTI_LORAWAN_PHY="RP002_V1_0_3"

# If we can't parse JOIN_EUI from config, fall back to this:
JOIN_EUI_DEFAULT="0025CA00000055F7"

# Output CSV (daily file)
CSV_FILE="$WORKSPACE_DIR/DeviceCsvImport-$(date +%F).csv"
# ===================================================

log() { echo "$(date '+%F %T') | $*" | tee -a "$LOG_FILE"; }
die() { log "ERROR: $*"; exit 1; }

validate_dev_eui() {
  local raw="$1"
  local cleaned
  cleaned="$(echo -n "$raw" | tr -d -c '0-9A-Fa-f' | tr 'a-f' 'A-F')"
  if [[ ${#cleaned} -eq 16 && "$cleaned" =~ ^[0-9A-F]{16}$ ]]; then
    echo -n "$cleaned"; return 0
  else
    return 1
  fi
}

extract_join_eui_from_config() {
  local v
  v="$(grep -E 'g_lorawan_join_eui\[]\s*=\s*"[0-9A-Fa-f]+"' "$CONFIG_C_FILE" \
       | sed -E 's/.*"([0-9A-Fa-f]+)".*/\1/' | tr 'a-f' 'A-F' || true)"
  if [[ ${#v} -eq 16 && "$v" =~ ^[0-9A-F]{16}$ ]]; then
    echo -n "$v"
  else
    echo -n "$JOIN_EUI_DEFAULT"
  fi
}

extract_dev_eui_from_config() {
  grep -E 'g_lorawan_dev_eui\[]\s*=\s*"[0-9A-Fa-f]+"' "$CONFIG_C_FILE" \
    | sed -E 's/.*"([0-9A-Fa-f]+)".*/\1/' \
    | tr 'a-f' 'A-F' \
    | head -n1
}

ensure_paths() {
  [[ -d "$WORKSPACE_DIR" ]] || die "Workspace not found: $WORKSPACE_DIR"
  [[ -x "$STM32_PROGRAMMER" ]] || die "STM32_Programmer_CLI not found at $STM32_PROGRAMMER"
  [[ -f "$CONFIG_C_FILE" ]] || die "Config file not found: $CONFIG_C_FILE"
  if [[ "$BUILD_MODE" == "cubeide" ]]; then
    [[ -x "$STM32CUBEIDE" ]] || die "CubeIDE headless builder not found at $STM32CUBEIDE"
  fi
}

patch_dev_eui() {
  local eui="$1"

  local before
  before="$(extract_dev_eui_from_config || true)"
  log "DEV_EUI in $(basename "$CONFIG_C_FILE") before: ${before:-<none>}"

  local tmp tmp2
  tmp="$(mktemp)"
  tmp2="$(mktemp)"
  cp "$CONFIG_C_FILE" "$tmp"

  if ! sed -E "s@(g_lorawan_dev_eui\\[]\\s*=\\s*")[^"]+(\".*)@\\1${eui}\\2@" "$tmp" > "$tmp2"; then
    log "Failed to update DEV_EUI in lorawan_config.c"
    rm -f "$tmp" "$tmp2"
    return 1
  fi

  mv -f "$tmp2" "$CONFIG_C_FILE"
  rm -f "$tmp"

  local after
  after="$(extract_dev_eui_from_config || true)"
  log "DEV_EUI in $(basename "$CONFIG_C_FILE") after:  ${after:-<none>}"

  [[ "$after" == "$eui" ]]
}

build_project() {
  log "Building ($BUILD_MODE)…"
  export PATH="$TOOLCHAIN_BIN:$PATH"
  if [[ "$BUILD_MODE" == "cubeide" ]]; then
    "$STM32CUBEIDE" -data "$WORKSPACE_DIR/.." -cleanBuild "${PROJECT_NAME}/${BUILD_CONFIG}" || return 1
  else
    local mf_lower="$WORKSPACE_DIR/$BUILD_CONFIG/makefile"
    local mf_upper="$WORKSPACE_DIR/$BUILD_CONFIG/Makefile"
    [[ -f "$mf_lower" || -f "$mf_upper" ]] || die "Makefile/makefile missing in $WORKSPACE_DIR/$BUILD_CONFIG (build once in IDE or switch BUILD_MODE)."
    make -C "$WORKSPACE_DIR/$BUILD_CONFIG" -j"$(nproc)" all || return 1
  fi
  return 0
}

find_elf() {
  local a="$WORKSPACE_DIR/$BUILD_CONFIG/${PROJECT_NAME}.elf"
  local b="$WORKSPACE_DIR/$BUILD_CONFIG/seri.elf"
  if   [[ -f "$a" ]]; then echo -n "$a"
  elif [[ -f "$b" ]]; then echo -n "$b"
  else
    find "$WORKSPACE_DIR/$BUILD_CONFIG" -maxdepth 1 -type f -name '*.elf' | head -n1
  fi
}

flash_board() {
  local elf="$1"
  local connect=(-c "port=SWD" "freq=$STLINK_FREQ")
  [[ -n "$STLINK_SN" ]] && connect+=("sn=$STLINK_SN")
  "$STM32_PROGRAMMER" "${connect[@]}" -w "$elf" -v -rst
}

csv_ensure_header() {
  if [[ ! -f "$CSV_FILE" ]]; then
    echo "dev_eui;join_eui;frequency_plan_id;lorawan_version;lorawan_phy_version;app_key" > "$CSV_FILE"
    log "Created CSV: $CSV_FILE (with header)"
  fi
}

csv_has_dev_eui() {
  local dev_eui="$1"
  [[ -f "$CSV_FILE" ]] || return 1
  awk -F';' -v dev_eui="$dev_eui" '
    NR==1 { next }
    {
      gsub(/^[ \t]+|[ \t]+$/, "", $1)
      if (toupper($1) == toupper(dev_eui)) exit 0
    }
    END { exit 1 }
  ' "$CSV_FILE"
}

csv_append_row() {
  local dev_eui="$1" join_eui="$2" app_key="$3"
  if csv_has_dev_eui "$dev_eui"; then
    log "CSV already contains DEV_EUI=${dev_eui}; skipping duplicate."
    return 0
  fi
  echo "${dev_eui};${join_eui};${TTI_FREQ_PLAN_ID};${TTI_LORAWAN_VER};${TTI_LORAWAN_PHY};${app_key}" >> "$CSV_FILE"
  log "Appended to CSV: DEV_EUI=${dev_eui}, JOIN_EUI=${join_eui}"
}

restore_original=false
trap 'if $restore_original; then cp -f "$BACKUP_FILE" "$CONFIG_C_FILE" && log "Restored original lorawan_config.c (trap)"; fi' EXIT

# ===================== Main =====================
ensure_paths
csv_ensure_header
log "Starting board programming…"
cp -f "$CONFIG_C_FILE" "$BACKUP_FILE"
log "Backup created: $BACKUP_FILE"

JOIN_EUI_CONST="$(extract_join_eui_from_config)"
log "JOIN_EUI (from config or default): $JOIN_EUI_CONST"

while true; do
  echo
  echo "Scan the barcode (DEV_EUI) then press Enter (blank Enter to quit):"
  read -r scanned

  if [[ -z "${scanned:-}" ]]; then
    log "Exiting."
    break
  fi

  eui="$(validate_dev_eui "$scanned" || true)"
  if [[ -z "${eui:-}" ]]; then
    log "Invalid DEV_EUI. Expect 16 hex chars. Got: '$scanned'"
    continue
  fi
  log "DEV_EUI (scanned): $eui"

  if ! patch_dev_eui "$eui"; then
    log "Failed to update DEV_EUI in lorawan_config.c"
    continue
  fi
  restore_original=true
  log "Updated DEV_EUI in lorawan_config.c to: $eui"

  if ! build_project; then
    log "Build failed. Restoring original lorawan_config.c"
    cp -f "$BACKUP_FILE" "$CONFIG_C_FILE"
    restore_original=false
    continue
  fi
  log "Build OK."

  elf="$(find_elf || true)"
  if [[ -z "${elf:-}" ]]; then
    log "Could not locate .elf in $WORKSPACE_DIR/$BUILD_CONFIG"
    cp -f "$BACKUP_FILE" "$CONFIG_C_FILE"
    restore_original=false
    continue
  fi
  log "Using ELF: $elf"

  log "Flashing MCU…"
  if flash_board "$elf"; then
    log "Flashed successfully with DEV_EUI=$eui"

    join_eui="$JOIN_EUI_CONST"
    app_key="$(echo -n "${eui}${join_eui}" | tr 'a-f' 'A-F')"
    csv_append_row "$eui" "$join_eui" "$app_key"
    log "CSV updated: $CSV_FILE"

    cp -f "$CONFIG_C_FILE" "$BACKUP_FILE"
    restore_original=false
    log "Left DEV_EUI at latest value; backup refreshed."
  else
    log "Flashing failed."
    cp -f "$BACKUP_FILE" "$CONFIG_C_FILE"
    restore_original=false
    continue
  fi

done

