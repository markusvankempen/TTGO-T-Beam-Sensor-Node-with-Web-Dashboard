#!/usr/bin/env python3
"""
Splits src/main.cpp (~10,840 lines) into 15 focused header files + slim main.cpp.
Single-translation-unit pattern: all headers are included once in main.cpp.
Run from the project root or src/ directory.
"""
import os, shutil

SRC = os.path.join(os.path.dirname(__file__), 'main.cpp')
DIR = os.path.dirname(__file__)

with open(SRC, 'r', encoding='utf-8') as f:
    raw = f.readlines()  # 0-indexed; line N is raw[N-1]

TOTAL = len(raw)
print(f"Read {TOTAL} lines from main.cpp")

def L(start, end=None):
    """Return joined text for lines start..end (1-indexed, inclusive).
    If end is None, reads to end of file."""
    if end is None:
        end = TOTAL
    return ''.join(raw[start - 1 : end])

def write_header(name, parts, note=''):
    """Create src/<name> with #pragma once then the concatenated line ranges."""
    content = '#pragma once\n'
    if note:
        content += f'// {note}\n'
    for start, end in parts:
        content += L(start, end)
    path = os.path.join(DIR, name)
    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)
    total_lines = sum(e - s + 1 for s, e in parts)
    print(f"  Created {name:30s}  ({total_lines:5d} source lines)")

# ── Backup original ──────────────────────────────────────────────────────────
bak = SRC + '.bak'
shutil.copy(SRC, bak)
print(f"Backed up original to main.cpp.bak")

print("\nCreating header files...")

# 1. config.h  — firmware version constants + all pin/timing constants (175-256)
write_header('config.h', [(175, 256)])

# 2. globals.h  — all object instances + global variables + forward declarations (257-414)
write_header('globals.h', [(257, 414)])

# 3. debug_serial.h  — DebugSerial class + #define Serial (95-174) + buffer functions (605-779)
#    logSystemDetailed() calls printSystemUtcToSerial() which is defined in gps_module.h (included later).
#    Inject a forward declaration between the two parts.
_ds_content = '#pragma once\n' + L(95, 174) + '\n// Forward declaration: printSystemUtcToSerial is defined in gps_module.h\nvoid printSystemUtcToSerial();\n\n' + L(605, 779)
with open(os.path.join(DIR, 'debug_serial.h'), 'w', encoding='utf-8') as _f:
    _f.write(_ds_content)
print(f"  Created {'debug_serial.h':30s}  ({(174-95+1) + (779-605+1):5d} source lines)")

# 4. axp_power.h  — AXP192 read/write helpers + setupAxp192PowerForGps (780-979)
write_header('axp_power.h', [(780, 979)])

# 5. sensors.h  — readBatteryVoltage docstring (980-1117) + initBme/showBootScreen/updateSensorReadings (1891-1970)
#    NOTE: end at 1117 (not 1113) because disableDisplay() closes at line 1116
write_header('sensors.h', [(980, 1117), (1891, 1970)])

# 6. gps_module.h  — UBX+GPS time sync + printSystemUtcToSerial (415-604) + display helpers+startGpsSerial (1650-1704)
#    NOTE: part 2 starts at 1650 (currentShiftX), not 1600 (which is inside queueCayenneUplink's body)
write_header('gps_module.h', [(415, 604), (1650, 1704)])

# 7. display_oled.h  — OLED draw pages + logGpsDetailed (1705-1890) + renderDisplayPage (1971-2000) + showBootSplash (10392-10427)
write_header('display_oled.h', [(1705, 1890), (1971, 2000), (10392, 10427)])

# 8. lorawan_module.h  — os_getArtEui (1118) + printLoRaStatus + sleep vars + processDownlinkCommand + onEvent + setupLoRa (1118-1472)
#    NOTE: start at 1118 (not 1114): lines 1114-1117 = end of disableDisplay() which belongs in sensors.h
write_header('lorawan_module.h', [(1118, 1472)])

# 9. pax_counter.h  — performPaxScan + queueCayenneUplink (1473-1649)
#    NOTE: end at 1649 (not 1599): queueCayenneUplink() closes at line 1648, blank at 1649
write_header('pax_counter.h', [(1473, 1649)])

# 10. nvs_settings.h  — loadLoRaKeys/saveLoRaKeys/clearLoRaKeys + hex utils + loadSystemSettings/saveSystemSettings (2001-2370)
write_header('nvs_settings.h', [(2001, 2370)])

# 11. mqtt_manager.h  — connectMQTT + publishMQTT (2371-2570)
write_header('mqtt_manager.h', [(2371, 2570)])

# 12. web_pages.h  — getJsonSensorData + getHtmlPage (2571-3097)
write_header('web_pages.h', [(2571, 3097)])

# 13. web_server.h  — full setupWebServer() with all routes (3098-9126)
write_header('web_server.h', [(3098, 9126)])

# 14. wifi_manager.h  — showWiFiStatusOnDisplay + configureWiFiRadio + fetchNtpTime + setupWiFi (9127-9428)
write_header('wifi_manager.h', [(9127, 9428)])

# 15. serial_commands.h  — printSerialMenu + printSystemStatus + handleSerialCommands (9429-10391)
write_header('serial_commands.h', [(9429, 10391)])

# ── New slim main.cpp ────────────────────────────────────────────────────────
print("\nCreating slim main.cpp...")

includes_block = L(1, 94)   # original file header + all #include statements
setup_loop     = L(10428)   # setup() through end of file (loop + EOF)

new_main = (
    includes_block
    + '\n// ========== Module Headers (single-translation-unit pattern) ==========\n'
    + '#include "config.h"\n'
    + '#include "globals.h"\n'
    + '#include "debug_serial.h"\n'
    + '#include "axp_power.h"\n'
    + '#include "sensors.h"\n'
    + '#include "gps_module.h"\n'
    + '#include "display_oled.h"\n'
    + '#include "lorawan_module.h"\n'
    + '#include "pax_counter.h"\n'
    + '#include "nvs_settings.h"\n'
    + '#include "mqtt_manager.h"\n'
    + '#include "web_pages.h"\n'
    + '#include "web_server.h"\n'
    + '#include "wifi_manager.h"\n'
    + '#include "serial_commands.h"\n'
    + '\n'
    + setup_loop
)

with open(SRC, 'w', encoding='utf-8') as f:
    f.write(new_main)

new_lines = new_main.count('\n')
setup_loop_lines = setup_loop.count('\n')
print(f"  Written new main.cpp ({new_lines} lines total, {setup_loop_lines} lines from setup/loop)")

print("\nDone! Summary:")
print("  15 header files created in src/")
print("  src/main.cpp rewritten (includes + setup + loop only)")
print("  src/main.cpp.bak  = original backup")
print("\nNext: cd to project root and run: pio run")
