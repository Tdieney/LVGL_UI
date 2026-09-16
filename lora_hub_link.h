#ifndef LORA_HUB_LINK_H
#define LORA_HUB_LINK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Smart Hub Local Link Bookkeeping
 *
 * Receive timestamps, link quality, and dirty-check revision counter
 * owned and populated by the Smart Hub firmware / simulator application.
 *
 * These variables are Hub-local receive state and do not cross the radio wire.
 * ========================================================================= */

extern volatile uint32_t lora_last_rx_tick_ms; /* Timestamp (lv_tick_get) of last valid status frame */
extern volatile uint32_t lora_rx_revision;     /* Incremented on each newly received status frame */

/* Link quality of the frames THIS Hub receives from the Node, measured locally
 * by the Hub's own SX127x after each status frame (RegPktSnrValue /
 * RegPktRssiValue). Never transmitted: only the receiving side can measure it.
 *
 * Drive the signal indicator from SNR, not RSSI. LoRa demodulates below the
 * noise floor, so what matters is the margin of SNR above the demodulator limit
 * of the chosen spreading factor (about -7.5 dB at SF7 down to -20 dB at SF12).
 * A low RSSI with positive SNR is a healthy link.
 *
 * RSSI is kept for diagnostics only -- it separates "weak signal" from "strong
 * signal plus strong interference", which look identical in SNR alone. */
extern volatile int8_t   lora_last_snr;        /* SNR of last status frame heard by Hub, whole dB */
extern volatile int8_t   lora_last_rssi;       /* RSSI of last status frame heard by Hub, dBm (diagnostics) */

/* =========================================================================
 * LoRa Demodulator Limits and SNR Margin Mapping
 * ========================================================================= */

/* Active LoRa Spreading Factor (SF7..SF12).
 * NOTE: This configuration constant MUST match the radio driver's actual
 * transceiver configuration (RegModemConfig2 / SpreadingFactor). A mismatch
 * here silently skews every signal bar threshold calculation. */
#ifndef LORA_SPREADING_FACTOR
#define LORA_SPREADING_FACTOR  7u
#endif

/* Demodulator limit thresholds in tenths of a dB:
 * SF7:  -7.5 dB (-75 tenths)
 * SF8: -10.0 dB (-100 tenths)
 * SF9: -12.5 dB (-125 tenths)
 * SF10: -15.0 dB (-150 tenths)
 * SF11: -17.5 dB (-175 tenths)
 * SF12: -20.0 dB (-200 tenths)
 */
static inline int16_t lora_get_demod_limit_tenths_db(uint8_t sf)
{
    switch (sf) {
        case 7:  return -75;
        case 8:  return -100;
        case 9:  return -125;
        case 10: return -150;
        case 11: return -175;
        case 12: return -200;
        default: return -75; /* Default fallback to SF7 */
    }
}

/* Base SNR margin thresholds (in tenths of a dB above demodulator limit):
 * >= 10.0 dB (100 tenths): 4 bars
 * 5.0 - 10.0 dB (50..99 tenths): 3 bars
 * 2.0 - 5.0 dB (20..49 tenths): 2 bars
 * 0.0 - 2.0 dB (0..19 tenths): 1 bar
 * < 0.0 dB (negative margin): disconnected (below demodulator limit)
 */
#define LORA_SNR_THRESH_4_BARS_TENTHS_DB   100  /* 10.0 dB */
#define LORA_SNR_THRESH_3_BARS_TENTHS_DB    50  /*  5.0 dB */
#define LORA_SNR_THRESH_2_BARS_TENTHS_DB    20  /*  2.0 dB */
#define LORA_SNR_THRESH_1_BAR_TENTHS_DB      0  /*  0.0 dB */

/* Hysteresis: Schmitt-trigger overshoot margin required to step UP a level.
 * Real per-packet SNR jitters by several dB, so bare threshold comparison
 * causes redraw flicker whenever SNR oscillates around a boundary.
 * Requiring 1.0 dB (10 tenths dB) overshoot to step up prevents boundary flicker
 * without per-packet queue/buffer allocation. */
#define LORA_SNR_HYSTERESIS_TENTHS_DB       10  /* 1.0 dB */

/* Compute SNR margin in tenths of a dB above demodulator limit for a given SF */
static inline int16_t lora_snr_margin_tenths_db(int8_t snr_db, uint8_t sf)
{
    return (int16_t)((int16_t)snr_db * 10 - lora_get_demod_limit_tenths_db(sf));
}

/* Map SNR margin (tenths of dB) to bar count (0..4) with hysteresis */
static inline uint8_t lora_margin_to_bars(int16_t margin_tenths, uint8_t current_bars)
{
    if (margin_tenths < LORA_SNR_THRESH_1_BAR_TENTHS_DB) {
        /* Below demodulator limit (return 0).
         * Note: the UI header in ui.c clamps this to 1 bar while status frames are
         * arriving within LORA_LINK_TIMEOUT_MS, reserving 0 bars / 'Disconnected'
         * strictly for link staleness/timeout. A diagnostics view may inspect 0 bars
         * to distinguish sub-limit reception from 1 bar. */
        return 0;
    }

    /* 4 bars threshold */
    int16_t t4 = LORA_SNR_THRESH_4_BARS_TENTHS_DB;
    if (current_bars < 4) t4 += LORA_SNR_HYSTERESIS_TENTHS_DB;
    if (margin_tenths >= t4) return 4;

    /* 3 bars threshold */
    int16_t t3 = LORA_SNR_THRESH_3_BARS_TENTHS_DB;
    if (current_bars < 3) t3 += LORA_SNR_HYSTERESIS_TENTHS_DB;
    if (margin_tenths >= t3) return 3;

    /* 2 bars threshold */
    int16_t t2 = LORA_SNR_THRESH_2_BARS_TENTHS_DB;
    if (current_bars < 2) t2 += LORA_SNR_HYSTERESIS_TENTHS_DB;
    if (margin_tenths >= t2) return 2;

    return 1;
}

/* Map whole-dB SNR to bar count (0..4) using specified spreading factor */
static inline uint8_t lora_snr_to_bars_sf(int8_t snr_db, uint8_t current_bars, uint8_t sf)
{
    int16_t margin_tenths = lora_snr_margin_tenths_db(snr_db, sf);
    return lora_margin_to_bars(margin_tenths, current_bars);
}

/* Map whole-dB SNR to bar count (0..4) using configured LORA_SPREADING_FACTOR */
static inline uint8_t lora_snr_to_bars(int8_t snr_db, uint8_t current_bars)
{
    return lora_snr_to_bars_sf(snr_db, current_bars, LORA_SPREADING_FACTOR);
}

#ifdef __cplusplus
}
#endif

#endif /* LORA_HUB_LINK_H */
