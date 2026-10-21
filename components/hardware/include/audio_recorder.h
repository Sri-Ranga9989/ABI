#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RECORDINGS_STORAGE_MOUNT   "/storage"
#define RECORDINGS_DIR_PATH        "/storage"

/**
 * @brief Initialize the audio recorder subsystem.
 *        - Mounts 8MB SPIFFS partition at /storage
 *        - Configures INMP441 microphone (SCK=4, WS=5, SD=6)
 *        - Configures Push Button on GPIO 7 (and BOOT button on GPIO 0)
 *        - Prepares PSRAM recording memory buffer
 *        - Starts serial console command listener (for pull_recordings.py)
 *
 * @return ESP_OK on success
 */
esp_err_t audio_recorder_init(void);

/**
 * @brief Start recording voice audio from the INMP441 microphone.
 */
esp_err_t audio_recorder_start_recording(void);

/**
 * @brief Stop recording, create standard WAV file, and write to storage.
 *
 * @param out_file_path  Buffer to copy the saved WAV file path (can be NULL)
 * @param max_len        Size of out_file_path buffer
 * @return ESP_OK on success
 */
esp_err_t audio_recorder_stop_recording(char *out_file_path, size_t max_len);

/**
 * @brief Check if recording is currently active.
 */
bool audio_recorder_is_recording(void);

/**
 * @brief List all recorded WAV files stored in the storage directory.
 */
void audio_recorder_list_files(void);

/**
 * @brief Export all stored recordings over the USB serial port in Base64
 *        format for extraction by the Python script pull_recordings.py.
 */
void audio_recorder_export_serial(void);

/**
 * @brief Start the HTTP web server on port 80 for browser playback and downloading.
 */
esp_err_t audio_recorder_start_webserver(void);

#ifdef __cplusplus
}
#endif
