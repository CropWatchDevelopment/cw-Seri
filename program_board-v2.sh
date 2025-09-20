#!/usr/bin/env bash
set -Eeuo pipefail
IFS=$'\n\t'

# ===================== Config =====================
WORKSPACE_DIR="/home/kevin/source/STM32CubeIDE/cw-Seri"
PROJECT_NAME="cw-Seri"
BUILD_CONFIG="Debug"

MAIN_C_FILE="$WORKSPACE_DIR/Core/Src/main.c"
MAIN_H_FILE="$WORKSPACE_DIR/Core/Inc/main.h"
BACKUP_FILE="$MAIN_C_FILE.bak"
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

# If we can't parse JOIN_EUI from main.c, fall back to this:
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

# --- WHERE IS DEV_EUI DEFINED? (main.c preferred, then main.h) ---
find_dev_define_file() {
  local pat='^[[:space:]]*#define[[:space:]]+DEV_EUI[[:space:]]+"'
  if [[ -f "$MAIN_C_FILE" ]] && grep -qE "$pat" "$MAIN_C_FILE"; then
    echo -n "$MAIN_C_FILE"; return 0
  fi
  if [[ -f "$MAIN_H_FILE" ]] && grep -qE "$pat" "$MAIN_H_FILE"; then
    echo -n "$MAIN_H_FILE"; return 0
  fi
  # Default to main.c if neither matched (we'll attempt to patch it)
  echo -n "$MAIN_C_FILE"; return 1
}

# Extract JOIN_EUI from main.c (#define JOIN_EUI "hex...")
extract_join_eui_from_main() {
  local v
  v="$(grep -E '^[[:space:]]*#define[[:space:]]+JOIN_EUI[[:space:]]+"[0-9A-Fa-f]+"' "$MAIN_C_FILE" \
       | sed -E 's/.*"([0-9A-Fa-f]+)".*/\1/' | tr 'a-f' 'A-F' || true)"
  if [[ ${#v} -eq 16 && "$v" =~ ^[0-9A-F]{16}$ ]]; then
    echo -n "$v"
  else
    echo -n "$JOIN_EUI_DEFAULT"
  fi
}

# Extract DEV_EUI from an arbitrary file (for verification/logging)
extract_dev_eui_from_file() {
  local f="$1"
  grep -E '^[[:space:]]*#define[[:space:]]+DEV_EUI[[:space:]]+"[0-9A-Fa-f]+"' "$f" \
    | sed -E 's/.*"([0-9A-Fa-f]+)".*/\1/' \
    | tr 'a-f' 'A-F' \
    | head -n1
}

ensure_paths() {
  [[ -d "$WORKSPACE_DIR" ]] || die "Workspace not found: $WORKSPACE_DIR"
  [[ -x "$STM32_PROGRAMMER" ]] || die "STM32_Programmer_CLI not found at $STM32_PROGRAMMER"
  if [[ "$BUILD_MODE" == "cubeide" ]]; then
    [[ -x "$STM32CUBEIDE" ]] || die "CubeIDE headless builder not found at $STM32CUBEIDE"
  fi
  # We tolerate missing DEV_EUI line initially (first run may insert/patch)
}

# Robust patch: replace value inside quotes after #define DEV_EUI
patch_dev_eui() {
  local eui="$1"
  local target
  target="$(find_dev_define_file)"

  [[ -f "$target" ]] || die "Target file for DEV_EUI not found: $target"

  local before
  before="$(extract_dev_eui_from_file "$target" || true)"
  log "DEV_EUI in $(basename "$target") before: ${before:-<none>}"

  # Work on a temp copy; do NOT rely on -i portability
  local tmp tmp2
  tmp="$(mktemp)"; tmp2="$(mktemp)"
  cp "$target" "$tmp"

  # Replace whatever is inside the quotes after DEV_EUI with our new value.
  # Pattern tolerates arbitrary spacing and comments after the string.
  if ! sed -E 's@(^[[:space:]]*#define[[:space:]]+DEV_EUI[[:space:]]*")[^"]+(".*$)@\1'"$eui"'\2@' "$tmp" > "$tmp2"; then
    rm -f "$tmp" "$tmp2"; return 1
  fi

  # If there was no match (define missing), add one near the top just after includes.
  if ! grep -qE '^[[:space:]]*#define[[:space:]]+DEV_EUI[[:space:]]+"' "$tmp2"; then
    if grep -n '^#include' "$tmp2" >/dev/null; then
      local ln
      ln="$(grep -n '^#include' "$tmp2" | head -n1 | cut -d: -f1)"
      awk -v n="$ln" -v val="$eui" 'NR==n{print; print "#define DEV_EUI \"" val "\""; next}1' "$tmp2" > "$tmp"
    else
      printf '#define DEV_EUI "%s"\n' "$eui" | cat - "$tmp2" > "$tmp"
    fi
    mv -f "$tmp" "$tmp2"
  fi

  # Move into place
  cp -f "$tmp2" "$target"
  rm -f "$tmp" "$tmp2"

  local after
  after="$(extract_dev_eui_from_file "$target" || true)"
  log "DEV_EUI in $(basename "$target") after:  ${after:-<none>}"

  [[ "$after" == "$eui" ]]
}

build_project() {
  log "Building ($BUILD_MODE)…"
  export PATH="$TOOLCHAIN_BIN:$PATH"
  if [[ "$BUILD_MODE" == "cubeide" ]]; then
    "$STM32CUBEIDE" -data "$WORKSPACE_DIR/.." -cleanBuild "${PROJECT_NAME}/${BUILD_CONFIG}" || return 1
  else
    # Accept either 'makefile' or 'Makefile'
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

# === CSV helpers ===
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
    NR==1 { next }                          # skip header
    {
      gsub(/^[ \t]+|[ \t]+$/, "", $1)       # trim
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

# Cleanup/rollback if we modified main.c and exit early (only on failures)
restore_original=false
trap 'if $restore_original; then cp -f "$BACKUP_FILE" "$MAIN_C_FILE" && log "Restored original main.c (trap)"; fi' EXIT

# ===================== Main =====================
ensure_paths
log "Starting board programming…"
cp -f "$MAIN_C_FILE" "$BACKUP_FILE"
log "Backup created: $BACKUP_FILE"

JOIN_EUI_CONST="$(extract_join_eui_from_main)"
log "JOIN_EUI (from main.c or default): $JOIN_EUI_CONST"

csv_ensure_header

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

  # Patch DEV_EUI (and verify)
  if ! patch_dev_eui "$eui"; then
    log "Failed to update DEV_EUI in source."
    continue
  fi
  restore_original=true  # only restore on early failures
  log "Updated DEV_EUI in source to: $eui"

  # Build
  if ! build_project; then
    log "Build failed. Restoring original main.c"
    cp -f "$BACKUP_FILE" "$MAIN_C_FILE"
    restore_original=false
    continue
  fi
  log "Build OK."

  # Find ELF
  elf="$(find_elf || true)"
  if [[ -z "${elf:-}" ]]; then
    log "Could not locate .elf in $WORKSPACE_DIR/$BUILD_CONFIG"
    cp -f "$BACKUP_FILE" "$MAIN_C_FILE"; restore_original=false
    continue
  fi
  log "Using ELF: $elf"

  # Flash
  log "Flashing MCU…"
  if flash_board "$elf"; then
    log "Flashed successfully with DEV_EUI=$eui"

    # --- CSV output (after successful flash) ---
    join_eui="$JOIN_EUI_CONST"
    app_key="$(echo -n "${eui}${join_eui}" | tr 'a-f' 'A-F')"
    csv_append_row "$eui" "$join_eui" "$app_key"
    log "CSV updated: $CSV_FILE"

    # Keep latest DEV_EUI in source; refresh backup to the latest
    cp -f "$MAIN_C_FILE" "$BACKUP_FILE"
    restore_original=false
    log "Left DEV_EUI at latest value; backup refreshed."
  else
    log "Flashing failed."
    cp -f "$BACKUP_FILE" "$MAIN_C_FILE"; restore_original=false
    continue
  fi

  # Ready for next board (source retains last DEV_EUI)
done

