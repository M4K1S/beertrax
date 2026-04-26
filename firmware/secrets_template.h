/**
 * Beer Trax — Secrets Template
 * =============================
 * Copy this file, rename it to secrets.h, and fill in your values.
 * secrets.h is listed in .gitignore and will never be committed to the repo.
 *
 * Usage:
 *   1. Duplicate this file in the same firmware/ folder
 *   2. Rename the copy to secrets.h
 *   3. Fill in every field below
 *   4. Open main.ino in Arduino IDE — it will include secrets.h automatically
 */

#ifndef SECRETS_H
#define SECRETS_H

// ─────────────────────────────────────────────
// Wi-Fi credentials
// ─────────────────────────────────────────────
#define WIFI_SSID        "your-wifi-network-name"
#define WIFI_PASSWORD    "your-wifi-password"

// ─────────────────────────────────────────────
// Admin panel PIN
// Must be a numeric string (e.g. "7392").
// This is what you type into the admin login
// prompt on the web interface.
// ─────────────────────────────────────────────
#define ADMIN_CODE       "0000"

// ─────────────────────────────────────────────
// NTP / Timezone
// GMT offset in seconds:
//   EST  = -18000  (UTC-5)
//   MST  = -25200  (UTC-7)
//   PST  = -28800  (UTC-8)
// Daylight saving offset: 3600 = +1 hr, 0 = none
// ─────────────────────────────────────────────
#define GMT_OFFSET_SECONDS   -18000
#define DST_OFFSET_SECONDS    3600

// ─────────────────────────────────────────────
// Scheduled daily reset time  (HH:MM:SS, 24-hr)
// The system prints an EOD report, saves to SD,
// resets all totalizers, and reboots at this time.
// ─────────────────────────────────────────────
#define DAILY_RESET_TIME   "02:30:00"

#endif // SECRETS_H
