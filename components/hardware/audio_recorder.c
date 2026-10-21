#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_http_server.h"

#include "inmp441.h"
#include "button.h"
#include "audio_recorder.h"

static const char *TAG = "RECORDER";

#define SAMPLE_RATE              16000
#define MAX_RECORDING_SECONDS    30
#define MAX_AUDIO_BYTES          (MAX_RECORDING_SECONDS * SAMPLE_RATE * sizeof(int16_t)) // ~960 KB in PSRAM

// Standard 44-byte WAV header for 16-bit Mono PCM
typedef struct __attribute__((packed)) {
    char     riff[4];          // "RIFF"
    uint32_t file_size;        // Total file size - 8
    char     wave[4];          // "WAVE"
    char     fmt[4];           // "fmt "
    uint32_t fmt_size;         // 16 for PCM
    uint16_t format_tag;       // 1 for uncompressed PCM
    uint16_t channels;         // 1 (mono)
    uint32_t sample_rate;      // 16000
    uint32_t byte_rate;        // sample_rate * channels * (bits_per_sample/8) = 32000
    uint16_t block_align;      // channels * (bits_per_sample/8) = 2
    uint16_t bits_per_sample;  // 16
    char     data[4];          // "data"
    uint32_t data_size;        // PCM data size in bytes
} wav_header_t;

static int16_t *s_psram_audio_buffer = NULL;
static volatile bool s_is_recording = false;
static size_t s_recorded_bytes = 0;
static int s_file_index = 1;
static char s_last_saved_file[128] = {0};
static TaskHandle_t s_record_task_handle = NULL;
static httpd_handle_t s_webserver = NULL;

// Base64 helper for serial export
static const char s_b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void base64_encode(const uint8_t *in, size_t in_len, char *out)
{
    size_t i = 0, j = 0;
    while (i < in_len) {
        uint32_t a = in[i++];
        uint32_t b = (i < in_len) ? in[i++] : 0;
        uint32_t c = (i < in_len) ? in[i++] : 0;
        uint32_t triple = (a << 16) | (b << 8) | c;

        out[j++] = s_b64_table[(triple >> 18) & 0x3F];
        out[j++] = s_b64_table[(triple >> 12) & 0x3F];
        out[j++] = (i > in_len + 1) ? '=' : s_b64_table[(triple >> 6) & 0x3F];
        out[j++] = (i > in_len) ? '=' : s_b64_table[triple & 0x3F];
    }
    out[j] = '\0';
}

static void build_wav_header(wav_header_t *header, uint32_t pcm_data_len)
{
    memcpy(header->riff, "RIFF", 4);
    header->file_size = sizeof(wav_header_t) - 8 + pcm_data_len;
    memcpy(header->wave, "WAVE", 4);
    memcpy(header->fmt, "fmt ", 4);
    header->fmt_size = 16;
    header->format_tag = 1; // PCM
    header->channels = 1;   // Mono
    header->sample_rate = SAMPLE_RATE;
    header->byte_rate = SAMPLE_RATE * 1 * sizeof(int16_t);
    header->block_align = 1 * sizeof(int16_t);
    header->bits_per_sample = 16;
    memcpy(header->data, "data", 4);
    header->data_size = pcm_data_len;
}

static void sync_file_index(void)
{
    DIR *dir = opendir(RECORDINGS_DIR_PATH);
    if (!dir) return;

    struct dirent *entry;
    int max_idx = 0;
    while ((entry = readdir(dir)) != NULL) {
        int idx = 0;
        if (sscanf(entry->d_name, "rec_%d.wav", &idx) == 1) {
            if (idx > max_idx) {
                max_idx = idx;
            }
        }
    }
    closedir(dir);
    s_file_index = max_idx + 1;
}

void audio_recorder_list_files(void)
{
    DIR *dir = opendir(RECORDINGS_DIR_PATH);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory: %s", RECORDINGS_DIR_PATH);
        return;
    }

    ESP_LOGI(TAG, "\n=======================================================");
    ESP_LOGI(TAG, "       RECORDINGS FOLDER CONTENTS: %s", RECORDINGS_DIR_PATH);
    ESP_LOGI(TAG, "=======================================================");

    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".wav") != NULL) {
            char filepath[320];
            snprintf(filepath, sizeof(filepath), "%s/%s", RECORDINGS_DIR_PATH, entry->d_name);
            struct stat st;
            if (stat(filepath, &st) == 0) {
                float seconds = 0.0f;
                if (st.st_size > (int)sizeof(wav_header_t)) {
                    seconds = (float)(st.st_size - sizeof(wav_header_t)) / (SAMPLE_RATE * sizeof(int16_t));
                }
                count++;
                ESP_LOGI(TAG, " [%d] %-24s | %7ld bytes | ~%.1f sec",
                         count, entry->d_name, (long)st.st_size, seconds);
            }
        }
    }
    closedir(dir);

    size_t total = 0, used = 0;
    if (esp_spiffs_info("storage", &total, &used) == ESP_OK) {
        ESP_LOGI(TAG, "-------------------------------------------------------");
        ESP_LOGI(TAG, " Storage Partition (8MB): Used %u KB / Total %u KB (Free %u KB)",
                 (unsigned int)(used / 1024),
                 (unsigned int)(total / 1024),
                 (unsigned int)((total - used) / 1024));
    }
    ESP_LOGI(TAG, "=======================================================\n");
}

void audio_recorder_export_serial(void)
{
    DIR *dir = opendir(RECORDINGS_DIR_PATH);
    if (!dir) {
        printf("###EXPORT_ERROR:Cannot open directory###\n");
        return;
    }

    printf("\n=== EXPORTING STORED WAV RECORDINGS OVER SERIAL ===\n");

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".wav") != NULL) {
            char filepath[320];
            snprintf(filepath, sizeof(filepath), "%s/%s", RECORDINGS_DIR_PATH, entry->d_name);
            struct stat st;
            if (stat(filepath, &st) == 0) {
                FILE *f = fopen(filepath, "rb");
                if (!f) continue;

                printf("###FILE_START:%s:%ld###\n", entry->d_name, (long)st.st_size);

                uint8_t raw_buf[54]; // 54 bytes encodes to 72 base64 chars
                char b64_buf[80];
                size_t n;
                while ((n = fread(raw_buf, 1, sizeof(raw_buf), f)) > 0) {
                    base64_encode(raw_buf, n, b64_buf);
                    printf("%s\n", b64_buf);
                }

                fclose(f);
                printf("###FILE_END:%s###\n", entry->d_name);
            }
        }
    }
    closedir(dir);

    printf("###ALL_DONE###\n");
}

static void audio_record_task(void *pvParameters)
{
    const size_t chunk_samples = 256;
    int16_t chunk[chunk_samples];

    while (1) {
        if (s_is_recording) {
            size_t samples_read = 0;
            esp_err_t ret = inmp441_read_pcm16(chunk, chunk_samples, &samples_read, pdMS_TO_TICKS(50));
            if (ret == ESP_OK && samples_read > 0) {
                size_t bytes_to_copy = samples_read * sizeof(int16_t);
                if (s_recorded_bytes + bytes_to_copy <= MAX_AUDIO_BYTES) {
                    memcpy((uint8_t *)s_psram_audio_buffer + s_recorded_bytes, chunk, bytes_to_copy);
                    s_recorded_bytes += bytes_to_copy;
                } else {
                    ESP_LOGW(TAG, "Audio buffer full (reached %d seconds max).", MAX_RECORDING_SECONDS);
                    audio_recorder_stop_recording(NULL, 0);
                }
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}

static void serial_cmd_task(void *pvParameters)
{
    char line[128];
    while (1) {
        if (fgets(line, sizeof(line), stdin) != NULL) {
            // Trim newline
            line[strcspn(line, "\r\n")] = 0;

            if (strcasecmp(line, "EXPORT") == 0 || strcasecmp(line, "PULL") == 0 || strcasecmp(line, "CMD:EXPORT") == 0) {
                audio_recorder_export_serial();
            } else if (strcasecmp(line, "LIST") == 0) {
                audio_recorder_list_files();
            } else if (strcasecmp(line, "CLEAR") == 0 || strcasecmp(line, "DELETE_ALL") == 0) {
                DIR *dir = opendir(RECORDINGS_DIR_PATH);
                if (dir) {
                    struct dirent *entry;
                    while ((entry = readdir(dir)) != NULL) {
                        if (strstr(entry->d_name, ".wav") != NULL) {
                            char p[320];
                            snprintf(p, sizeof(p), "%s/%s", RECORDINGS_DIR_PATH, entry->d_name);
                            unlink(p);
                        }
                    }
                    closedir(dir);
                    s_file_index = 1;
                    ESP_LOGI(TAG, "All recordings deleted from storage.");
                    printf("###ALL_CLEARED###\n");
                    audio_recorder_list_files();
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void on_button_press(void)
{
    ESP_LOGI(TAG, "\n[BUTTON EVENT] PRESSED -> Starting voice recording...");
    audio_recorder_start_recording();
}

static void on_button_release(void)
{
    ESP_LOGI(TAG, "[BUTTON EVENT] RELEASED -> Stopping recording & saving to folder...");
    audio_recorder_stop_recording(s_last_saved_file, sizeof(s_last_saved_file));
}

esp_err_t audio_recorder_start_recording(void)
{
    if (s_is_recording) {
        return ESP_OK;
    }
    if (!s_psram_audio_buffer) {
        ESP_LOGE(TAG, "Audio buffer not allocated");
        return ESP_ERR_NO_MEM;
    }

    s_recorded_bytes = 0;
    inmp441_start();
    s_is_recording = true;

    ESP_LOGI(TAG, ">>> [RECORDING ACTIVE] Speak into INMP441 microphone now! <<<");
    return ESP_OK;
}

esp_err_t audio_recorder_stop_recording(char *out_file_path, size_t max_len)
{
    if (!s_is_recording && s_recorded_bytes == 0) {
        return ESP_OK;
    }

    s_is_recording = false;
    vTaskDelay(pdMS_TO_TICKS(60)); // Grace period for reading task
    inmp441_stop();

    if (s_recorded_bytes < (SAMPLE_RATE * sizeof(int16_t) * 0.2)) {
        ESP_LOGW(TAG, "Recording too short (< 200ms), discarded.");
        s_recorded_bytes = 0;
        return ESP_OK;
    }

    char filename[128];
    snprintf(filename, sizeof(filename), "%s/rec_%04d.wav", RECORDINGS_DIR_PATH, s_file_index++);
    if (out_file_path && max_len > 0) {
        strncpy(out_file_path, filename, max_len - 1);
        out_file_path[max_len - 1] = '\0';
    }

    ESP_LOGI(TAG, "Writing WAV file to storage: %s ...", filename);

    FILE *f = fopen(filename, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to create file: %s", filename);
        return ESP_FAIL;
    }

    // 1. Write WAV header
    wav_header_t header;
    build_wav_header(&header, s_recorded_bytes);
    fwrite(&header, 1, sizeof(header), f);

    // 2. Write PCM audio data from PSRAM buffer
    fwrite(s_psram_audio_buffer, 1, s_recorded_bytes, f);

    fclose(f);

    float duration_sec = (float)s_recorded_bytes / (SAMPLE_RATE * sizeof(int16_t));
    ESP_LOGI(TAG, ">>> [RECORDING SAVED] <<<");
    ESP_LOGI(TAG, "  File:     %s", filename);
    ESP_LOGI(TAG, "  Duration: %.2f seconds", duration_sec);
    ESP_LOGI(TAG, "  Size:     %u bytes\n", (unsigned int)(s_recorded_bytes + sizeof(wav_header_t)));

    audio_recorder_list_files();

    s_recorded_bytes = 0;
    return ESP_OK;
}

bool audio_recorder_is_recording(void)
{
    return s_is_recording;
}

/* =========================================================================
 * WEB SERVER IMPLEMENTATION (Stream & Download WAV directly in Browser)
 * ========================================================================= */

static esp_err_t http_index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");

    httpd_resp_send_chunk(req,
        "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<title>Abi AI Assistant - Voice Recordings</title>"
        "<style>"
        "body{font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,sans-serif;background:#0d1117;color:#c9d1d9;margin:0;padding:24px;}"
        ".card{max-width:760px;margin:0 auto;background:#161b22;border:1px solid #30363d;border-radius:12px;padding:24px;}"
        "h1{color:#58a6ff;margin-top:0;font-size:24px;display:flex;align-items:center;gap:10px;}"
        ".item{display:flex;align-items:center;justify-content:space-between;background:#21262d;border:1px solid #30363d;border-radius:8px;padding:12px 16px;margin:12px 0;flex-wrap:wrap;gap:10px;}"
        ".name{font-weight:600;color:#f0f6fc;font-family:monospace;}"
        ".meta{color:#8b949e;font-size:13px;}"
        "audio{height:36px;outline:none;}"
        ".btn{background:#238636;color:#fff;text-decoration:none;padding:8px 14px;border-radius:6px;font-size:13px;font-weight:600;display:inline-block;}"
        ".btn:hover{background:#2ea043;}"
        ".btn-del{background:#da3633;color:#fff;text-decoration:none;padding:8px 14px;border-radius:6px;font-size:13px;font-weight:600;display:inline-block;margin-left:6px;}"
        ".btn-del:hover{background:#f85149;}"
        ".stat{background:#0d1117;border:1px solid #30363d;border-radius:8px;padding:12px;margin:16px 0;font-size:14px;color:#8b949e;}"
        "</style></head><body><div class='card'>"
        "<h1>🎙️ Abi AI Assistant - Voice Recordings</h1>"
        "<p style='color:#8b949e'>INMP441 Microphone (16 kHz, 16-bit Mono WAV) | 8 MB Onboard SPIFFS</p>",
        HTTPD_RESP_USE_STRLEN);

    // List storage partition usage
    size_t total = 0, used = 0;
    char stat_buf[256];
    if (esp_spiffs_info("storage", &total, &used) == ESP_OK) {
        snprintf(stat_buf, sizeof(stat_buf),
                 "<div class='stat'>💾 <b>Storage:</b> Used %u KB / Total %u KB (Free %u KB)</div>",
                 (unsigned int)(used / 1024), (unsigned int)(total / 1024), (unsigned int)((total - used) / 1024));
        httpd_resp_send_chunk(req, stat_buf, HTTPD_RESP_USE_STRLEN);
    }

    DIR *dir = opendir(RECORDINGS_DIR_PATH);
    int count = 0;
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".wav") != NULL) {
                char p[320];
                snprintf(p, sizeof(p), "%s/%s", RECORDINGS_DIR_PATH, entry->d_name);
                struct stat st;
                if (stat(p, &st) == 0) {
                    count++;
                    float sec = (float)(st.st_size > 44 ? (st.st_size - 44) : 0) / (SAMPLE_RATE * 2);
                    char row[2048];
                    snprintf(row, sizeof(row),
                        "<div class='item'>"
                        " <div><div class='name'>%s</div><div class='meta'>Size: %ld KB | Duration: ~%.1f s</div></div>"
                        " <div style='display:flex;align-items:center;gap:8px;'>"
                        "  <audio controls src='/download?file=%s'></audio>"
                        "  <a class='btn' href='/download?file=%s' download>Download .WAV</a>"
                        "  <a class='btn-del' href='/delete?file=%s' onclick=\"return confirm('Delete %s?')\">Delete</a>"
                        " </div>"
                        "</div>",
                        entry->d_name, (long)(st.st_size / 1024), sec,
                        entry->d_name, entry->d_name, entry->d_name, entry->d_name);
                    httpd_resp_send_chunk(req, row, HTTPD_RESP_USE_STRLEN);
                }
            }
        }
        closedir(dir);
    }

    if (count == 0) {
        httpd_resp_send_chunk(req, "<p style='color:#8b949e;text-align:center;padding:30px 0;'>No recordings found yet.<br>Press and hold the button on your ESP32-S3 to record voice!</p>", HTTPD_RESP_USE_STRLEN);
    }

    httpd_resp_send_chunk(req, "<div style='margin-top:20px;text-align:center;'><a href='/' class='btn' style='background:#30363d;'>🔄 Refresh List</a></div></div></body></html>", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, NULL, 0); // End
    return ESP_OK;
}

static esp_err_t http_download_handler(httpd_req_t *req)
{
    char query[128];
    char filename[64] = {0};

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        httpd_query_key_value(query, "file", filename, sizeof(filename));
    }

    if (strlen(filename) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'file' parameter");
        return ESP_FAIL;
    }

    char filepath[320];
    snprintf(filepath, sizeof(filepath), "%s/%s", RECORDINGS_DIR_PATH, filename);

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "audio/wav");
    char dispo[128];
    snprintf(dispo, sizeof(dispo), "attachment; filename=\"%s\"", filename);
    httpd_resp_set_hdr(req, "Content-Disposition", dispo);

    char chunk[1024];
    size_t read_bytes;
    while ((read_bytes = fread(chunk, 1, sizeof(chunk), f)) > 0) {
        if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
            fclose(f);
            return ESP_FAIL;
        }
    }
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t http_delete_handler(httpd_req_t *req)
{
    char query[128];
    char filename[64] = {0};

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        httpd_query_key_value(query, "file", filename, sizeof(filename));
    }

    if (strlen(filename) > 0) {
        char filepath[320];
        snprintf(filepath, sizeof(filepath), "%s/%s", RECORDINGS_DIR_PATH, filename);
        unlink(filepath);
        ESP_LOGI(TAG, "Deleted file: %s", filepath);
    }

    // Redirect back to home
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

esp_err_t audio_recorder_start_webserver(void)
{
    if (s_webserver) {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;
    config.stack_size = 8192;

    esp_err_t ret = httpd_start(&s_webserver, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    httpd_uri_t uri_index = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = http_index_handler,
    };
    httpd_register_uri_handler(s_webserver, &uri_index);

    httpd_uri_t uri_download = {
        .uri = "/download",
        .method = HTTP_GET,
        .handler = http_download_handler,
    };
    httpd_register_uri_handler(s_webserver, &uri_download);

    httpd_uri_t uri_delete = {
        .uri = "/delete",
        .method = HTTP_GET,
        .handler = http_delete_handler,
    };
    httpd_register_uri_handler(s_webserver, &uri_delete);

    ESP_LOGI(TAG, "HTTP Server started! Open http://<ESP32_IP>/ in your browser to play & download recordings!");
    return ESP_OK;
}

esp_err_t audio_recorder_init(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "        INITIALIZING AUDIO RECORDER SYSTEM        ");
    ESP_LOGI(TAG, "==================================================");

    // 1. Mount SPIFFS filesystem on "storage" partition (8MB)
    esp_vfs_spiffs_conf_t spiffs_conf = {
        .base_path = RECORDINGS_STORAGE_MOUNT,
        .partition_label = "storage",
        .max_files = 8,
        .format_if_mount_failed = true,
    };

    esp_err_t ret = esp_vfs_spiffs_register(&spiffs_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        return ret;
    }

    sync_file_index();

    // 2. Allocate 1MB Audio Recording Buffer in 8MB PSRAM
    s_psram_audio_buffer = (int16_t *)heap_caps_malloc(MAX_AUDIO_BYTES, MALLOC_CAP_SPIRAM);
    if (!s_psram_audio_buffer) {
        ESP_LOGW(TAG, "Falling back to internal RAM");
        s_psram_audio_buffer = (int16_t *)malloc(16000 * 5 * sizeof(int16_t));
    } else {
        ESP_LOGI(TAG, "Allocated %d KB audio buffer in Octal PSRAM [OK]", (int)(MAX_AUDIO_BYTES / 1024));
    }

    // 3. Initialize INMP441 Microphone (SCK=4, WS=5, SD=6)
    ret = inmp441_init(INMP441_DEFAULT_SCK_PIN,
                      INMP441_DEFAULT_WS_PIN,
                      INMP441_DEFAULT_SD_PIN,
                      SAMPLE_RATE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize INMP441 microphone: %s", esp_err_to_name(ret));
        return ret;
    }

    // 4. Initialize Push Button (GPIO 7 with internal pull-up + BOOT button GPIO 0)
    ret = button_init(BUTTON_DEFAULT_GPIO_PIN, on_button_press, on_button_release);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize button: %s", esp_err_to_name(ret));
        return ret;
    }

    // 5. Start background audio recording task
    xTaskCreatePinnedToCore(&audio_record_task, "audio_rec_task", 4096, NULL, 5, &s_record_task_handle, 1);

    // 6. Start serial command listener task (listens for 'EXPORT' or 'LIST' commands)
    xTaskCreate(&serial_cmd_task, "serial_cmd_task", 4096, NULL, 2, NULL);

    // 7. Display current folder contents
    audio_recorder_list_files();

    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, " Audio Recorder Ready! Hold GPIO 7 (or BOOT button)");
    ESP_LOGI(TAG, " to record voice, and release to save as .WAV file!");
    ESP_LOGI(TAG, "==================================================");
    return ESP_OK;
}
