/* hrp_config.h - compile-time configuration for the open TH01-style engine.
 *
 * This engine is original code. It ships no artwork, music or data belonging to
 * any commercial release; the player supplies their own data package (see
 * docs/TH01_DATA_FORMAT.md) or plays the built-in procedurally generated
 * demonstration set.
 */
#ifndef HRP_CONFIG_H
#define HRP_CONFIG_H

/* Logical render resolution. 640x360 is 16:9 and scales exactly 2x to the
 * 1280x720 handheld screen, which keeps the software renderer free of
 * fractional scaling. */
#define HRP_SCREEN_W 640
#define HRP_SCREEN_H 360
#define HRP_FPS      60

/* Play field inside the logical screen. */
#define HRP_FIELD_LEFT   24
#define HRP_FIELD_RIGHT  616
#define HRP_FIELD_TOP    44
#define HRP_PLAYER_Y     318

/* Pool sizes. */
#define HRP_MAX_CARDS     160
#define HRP_MAX_BULLETS   256
#define HRP_MAX_SHOTS     128
#define HRP_MAX_PARTICLES 192
#define HRP_MAX_DROPS     32
#define HRP_MAX_STAGES    20
#define HRP_STAGE_NAME    24

/* Card grid geometry used by the demonstration data set. */
#define HRP_CARD_W 40
#define HRP_CARD_H 16
#define HRP_CARD_GAP_X 2
#define HRP_CARD_GAP_Y 2

/* Open data package identification. */
#define HRP_PKG_MAGIC   "TH01OPEN"
#define HRP_PKG_VERSION 1

#endif /* HRP_CONFIG_H */
