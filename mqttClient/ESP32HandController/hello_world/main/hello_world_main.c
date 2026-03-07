#include <stdio.h>
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h" // JSON Parser
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/pulse_cnt.h"
#include "esp_adc/adc_oneshot.h" 
#include "esp_err.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "esp_lcd_gc9a01.h"
#include "nvs_flash.h"

// --- CONFIGURATION ---
const char* ssid_primary = "DESKTOP-509JTUI";
const char* pass_primary = "0K367q7;";

const char* ssid_fallback = "H338A_2AC3_2.4G";
const char* pass_fallback = "ucgfFyGgK2CX";

static int wifi_retry_count = 0;
static bool using_fallback = false;
#define MAX_WIFI_RETRIES 3


const char* mqtt_server = "192.168.31.123"; 
const int   mqtt_port = 1883;
const char* topic_command_base = "trains/command";
// We listen to all commands to sync state, and publish to specific ID
const char* topic_subscribe = "trains/command/#"; 

// --- TRAIN CONSTANTS ---
#define TRAIN_ID 1
#define TRAIN_NAME "JR 313 2Car"
#define TRAIN_ADDR 3

// --- Hardware Pin Definitions ---
#define ADC_BUTTON_PIN      11 
#define ADC_UNIT            ADC_UNIT_2 
#define ADC_CHANNEL         ADC_CHANNEL_0 
#define PIN_NUM_MISO       -1
#define PIN_NUM_MOSI        6
#define PIN_NUM_CLK         4
#define PIN_NUM_CS          7
#define PIN_NUM_DC          8
#define PIN_NUM_RST         9
#define PIN_NUM_BK_LIGHT   -1
#define ENCODER_PIN_A       2  
#define ENCODER_PIN_B       3
#define ENCODER_PIN_BTN     10 
#define LCD_H_RES          240
#define LCD_V_RES          240
#define POWER_BTN_PIN  12
#define POWER_LED_PIN  13
#define BTN1  5
#define BTN2  11
#define BTN3  1



static const char *TAG = "TRAIN_CTRL";
static pcnt_unit_handle_t pcnt_unit = NULL;
static int32_t last_pcnt_count = 0;
static esp_mqtt_client_handle_t mqtt_client = NULL;

// --- UI STATE ENUMS ---
typedef enum {
    SCREEN_MAIN = 0,
    SCREEN_TRAIN_LIST,
    SCREEN_FUNC_LIST
} current_screen_t;

// --- UI DATA STRUCT ---
typedef struct {
    // State
    current_screen_t current_screen;
    int speed;          // Current Speed (0-127)
    bool direction_fwd; // true=1, false=0
    bool light_on;      // F0
    bool sound_on;      // F2
    bool state_changed;

    bool power_on;         
    bool power_update_pending; 
    int sent_speed;
    bool sent_direction_fwd;
    bool sent_light_on;
    bool sent_sound_on;

    // Objects - Main Screen
    lv_obj_t *main_cont;
    lv_obj_t *arc;
    lv_obj_t *lbl_speed;
    lv_obj_t *lbl_arrow_left;
    lv_obj_t *lbl_arrow_right;
    lv_obj_t *ind_light;
    lv_obj_t *ind_sound;
    lv_obj_t *lbl_wifi; // To update status
    lv_obj_t *lbl_stats;

    // Objects - Lists
    lv_obj_t *train_list_cont;
    lv_obj_t *train_list;
    lv_obj_t *func_list_cont;
    lv_obj_t *func_list;
    lv_obj_t *lbl_power_status;

    // Input Group reference
    lv_group_t *input_group;
} ui_data_t;

static ui_data_t ui_data; 

// --- HELPER DECLARATIONS ---
static void switch_to_screen(current_screen_t new_screen);
static void update_indicators(void);
static void update_direction_arrows(void);
static void send_train_command(void); // MQTT Sender



static void update_power_indicator(void) {
    if (!ui_data.lbl_power_status) return;
    
    if (ui_data.power_on) {
        lv_obj_add_flag(ui_data.lbl_power_status, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(ui_data.lbl_power_status, LV_OBJ_FLAG_HIDDEN);
    }
}


static void init_power_hardware(void) {
    // LED Config
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << POWER_LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&led_conf);

    // Button Config (Pull-up)
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << POWER_BTN_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, // Connect button between Pin 12 and GND
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&btn_conf);

    // Set initial LED state
    gpio_set_level(POWER_LED_PIN, 0); 
}


// --- MQTT & WIFI HANDLERS ---

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT Connected");
        esp_mqtt_client_subscribe(mqtt_client, topic_subscribe, 0);
        if(ui_data.lbl_wifi) {
            lvgl_port_lock(0);
            lv_label_set_text(ui_data.lbl_wifi, LV_SYMBOL_WIFI " MQTT");
            lv_obj_set_style_text_color(ui_data.lbl_wifi, lv_color_hex(0x00ff00), 0);
            lvgl_port_unlock();
        }
        break;
        
    case MQTT_EVENT_DATA:
        return;
        // Incoming Data Logic
        // We only care if the ID matches our current train
        // Note: event->data is NOT null-terminated!
        char *json_str = malloc(event->data_len + 1);
        memcpy(json_str, event->data, event->data_len);
        json_str[event->data_len] = '\0';

        cJSON *root = cJSON_Parse(json_str);
        if (root) {

            cJSON *pwr_item = cJSON_GetObjectItem(root, "Power");
            if (pwr_item) {
                bool new_pwr = cJSON_IsTrue(pwr_item);
                if (ui_data.power_on != new_pwr) {
                    ui_data.power_on = new_pwr;
                    // Update LED
                    gpio_set_level(POWER_LED_PIN, ui_data.power_on ? 1 : 0);
                    lvgl_port_lock(0);
                    update_power_indicator();
                    lvgl_port_unlock();

                    ESP_LOGI(TAG, "Recv Power Sync: %d", ui_data.power_on);
                }
            }


            cJSON *id_item = cJSON_GetObjectItem(root, "ID");
            if (id_item && id_item->valueint == TRAIN_ID) {
                // It's our train! Update State
                cJSON *spd = cJSON_GetObjectItem(root, "Speed");
                cJSON *dir = cJSON_GetObjectItem(root, "Direction");
                cJSON *funcs = cJSON_GetObjectItem(root, "Functions");

                bool need_ui_update = false;

                if (spd && spd->valueint != ui_data.speed) {
                    ui_data.speed = spd->valueint;
                    need_ui_update = true;
                }
                if (dir) {
                    bool new_dir = (dir->valueint == 1);
                    if (new_dir != ui_data.direction_fwd) {
                        ui_data.direction_fwd = new_dir;
                        need_ui_update = true;
                    }
                }
                
                // Parse Functions Array
                if (funcs && cJSON_IsArray(funcs)) {
                    cJSON *func = NULL;
                    cJSON_ArrayForEach(func, funcs) {
                        cJSON *idx = cJSON_GetObjectItem(func, "FIndex");
                        cJSON *act = cJSON_GetObjectItem(func, "IsActive");
                        if(idx && act) {
                            if(idx->valueint == 0) { // Light
                                if(ui_data.light_on != act->valueint) {
                                    ui_data.light_on = act->valueint;
                                    need_ui_update = true;
                                }
                            }
                            if(idx->valueint == 2) { // Sound
                                if(ui_data.sound_on != act->valueint) {
                                    ui_data.sound_on = act->valueint;
                                    need_ui_update = true;
                                }
                            }
                        }
                    }
                }

                if (need_ui_update) {
                    lvgl_port_lock(0);
                    
                    // UPDATE UI
                    lv_arc_set_value(ui_data.arc, ui_data.speed);
                    lv_label_set_text_fmt(ui_data.lbl_speed, "%d", ui_data.speed);
                    update_direction_arrows();
                    update_indicators();
                    
                    // Arc Color Logic
                    if(ui_data.speed < 60) lv_obj_set_style_arc_color(ui_data.arc, lv_palette_main(LV_PALETTE_TEAL), LV_PART_INDICATOR);
                    else if (ui_data.speed < 100) lv_obj_set_style_arc_color(ui_data.arc, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_INDICATOR);
                    else lv_obj_set_style_arc_color(ui_data.arc, lv_palette_main(LV_PALETTE_RED), LV_PART_INDICATOR);
                    
                    // CRITICAL: SYNC SHADOW STATE
                    // We set these so the sender task knows the current UI state 
                    // matches the server state, so it won't send an echo.
                    ui_data.sent_speed = ui_data.speed;
                    ui_data.sent_direction_fwd = ui_data.direction_fwd;
                    ui_data.sent_light_on = ui_data.light_on;
                    ui_data.sent_sound_on = ui_data.sound_on;
                    ui_data.state_changed = false; // Do NOT trigger a send
            
                    lvgl_port_unlock();
                }
            }
            cJSON_Delete(root);
        }
        free(json_str);
        break;

    default:
        break;
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_retry_count++;
        ESP_LOGW(TAG, "Wi-Fi disconnected. Retry %d/%d", wifi_retry_count, MAX_WIFI_RETRIES);

        if (wifi_retry_count >= MAX_WIFI_RETRIES) {
            // Swap the active network
            using_fallback = !using_fallback;
            wifi_retry_count = 0;
            
            wifi_config_t wifi_config = {0};
            wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
            
            if (using_fallback) {
                ESP_LOGW(TAG, "Switching to fallback network: %s", ssid_fallback);
                strcpy((char*)wifi_config.sta.ssid, ssid_fallback);
                strcpy((char*)wifi_config.sta.password, pass_fallback);
            } else {
                ESP_LOGW(TAG, "Switching to primary network: %s", ssid_primary);
                strcpy((char*)wifi_config.sta.ssid, ssid_primary);
                strcpy((char*)wifi_config.sta.password, pass_primary);
            }
            
            // Apply new config
            esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        }

        esp_wifi_connect();
        
        // Update UI
        if(ui_data.lbl_wifi) {
             lvgl_port_lock(0);
             lv_label_set_text(ui_data.lbl_wifi, LV_SYMBOL_WIFI " ...");
             lv_obj_set_style_text_color(ui_data.lbl_wifi, lv_color_hex(0x888888), 0);
             lvgl_port_unlock();
        }
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Got IP Address! Connected to %s", using_fallback ? ssid_fallback : ssid_primary);
        wifi_retry_count = 0; // Reset counter on successful connection
        esp_mqtt_client_start(mqtt_client);
    }
}

static void start_wifi_mqtt(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    // Start with primary network
    wifi_config_t wifi_config = {0};
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    strcpy((char*)wifi_config.sta.ssid, ssid_primary);
    strcpy((char*)wifi_config.sta.password, pass_primary);
    using_fallback = false;
    
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_set_ps(WIFI_PS_NONE); // Disable power save for minimum latency

    // MQTT Config
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = NULL, 
    };
    char uri[64];
    sprintf(uri, "mqtt://%s:%d", mqtt_server, mqtt_port);
    mqtt_cfg.broker.address.uri = uri;

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
}

// --- COMMAND SENDER ---
static void send_train_command(void) {
    if (mqtt_client == NULL) return;

    // Create JSON Object
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "ID", TRAIN_ID);
    cJSON_AddStringToObject(root, "Name", TRAIN_NAME);
    cJSON_AddNumberToObject(root, "Address", TRAIN_ADDR);
    cJSON_AddNumberToObject(root, "Speed", ui_data.speed);
    cJSON_AddNumberToObject(root, "Direction", ui_data.direction_fwd ? 1 : 0);
    cJSON_AddStringToObject(root, "Topic", "trains/command");

    // Functions Array
    cJSON *funcs = cJSON_CreateArray();
    
    // Light (F0)
    cJSON *f0 = cJSON_CreateObject();
    cJSON_AddNumberToObject(f0, "ID", 1); // ID internal to protocol
    cJSON_AddNumberToObject(f0, "FIndex", 0);
    cJSON_AddStringToObject(f0, "Name", "Light");
    cJSON_AddBoolToObject(f0, "IsActive", ui_data.light_on);
    cJSON_AddNumberToObject(f0, "LocomotiveID", TRAIN_ID);
    cJSON_AddItemToArray(funcs, f0);

    // Sound (F2)
    cJSON *f2 = cJSON_CreateObject();
    cJSON_AddNumberToObject(f2, "ID", 2);
    cJSON_AddNumberToObject(f2, "FIndex", 2);
    cJSON_AddStringToObject(f2, "Name", "Sound");
    cJSON_AddBoolToObject(f2, "IsActive", ui_data.sound_on);
    cJSON_AddNumberToObject(f2, "LocomotiveID", TRAIN_ID);
    cJSON_AddItemToArray(funcs, f2);

    cJSON_AddItemToObject(root, "Functions", funcs);

    // Print to string
    char *payload = cJSON_PrintUnformatted(root);
    
    // Publish
    char topic[64];
    sprintf(topic, "%s/%d", topic_command_base, TRAIN_ID);
    esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 0, 0);

    // Cleanup
    cJSON_Delete(root); // Deletes children too
    free(payload);
}

// --- UI EVENT CALLBACKS ---

static void arc_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * arc = lv_event_get_target(e);

    if(code == LV_EVENT_VALUE_CHANGED) {
        int32_t value = lv_arc_get_value(arc);
        
        // Only send if value actually changed to prevent spam
        if (ui_data.speed != value) {
            ui_data.speed = value;
            lv_label_set_text_fmt(ui_data.lbl_speed, "%d", (int)value);

            // Color logic
            if(value < 60) {
                lv_obj_set_style_arc_color(arc, lv_palette_main(LV_PALETTE_TEAL), LV_PART_INDICATOR);
                lv_obj_set_style_text_color(ui_data.lbl_speed, lv_palette_main(LV_PALETTE_TEAL), 0);
            } else if (value < 100) {
                lv_obj_set_style_arc_color(arc, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_INDICATOR);
                lv_obj_set_style_text_color(ui_data.lbl_speed, lv_palette_main(LV_PALETTE_ORANGE), 0);
            } else {
                lv_obj_set_style_arc_color(arc, lv_palette_main(LV_PALETTE_RED), LV_PART_INDICATOR);
                lv_obj_set_style_text_color(ui_data.lbl_speed, lv_palette_main(LV_PALETTE_RED), 0);
            }
            
            // SEND MQTT COMMAND (Speed Change)
            ui_data.state_changed = true;
        }
    }
    else if(code == LV_EVENT_CLICKED) { // Encoder Button on Main Screen
        ui_data.direction_fwd = !ui_data.direction_fwd;
        
        // UI Updates
        update_direction_arrows();
        ui_data.speed = 0; // Reset speed on dir change
        lv_arc_set_value(arc, 0);
        lv_label_set_text(ui_data.lbl_speed, "0");
        lv_obj_set_style_arc_color(arc, lv_palette_main(LV_PALETTE_TEAL), LV_PART_INDICATOR);
        
        // SEND MQTT COMMAND (Direction Change)
        ui_data.state_changed = true;

        // Keep focus
        if(ui_data.input_group) lv_group_set_editing(ui_data.input_group, true);
    }
}

/* List Item Click Callback (UNCHANGED) */
static void list_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);

    if(code == LV_EVENT_CLICKED) {
        if(ui_data.current_screen == SCREEN_TRAIN_LIST) {
             switch_to_screen(SCREEN_MAIN);
        }
    }
}

/* --- UI BUILDER & HELPERS (MOSTLY UNCHANGED, Added lbl_wifi capture) --- */
// ... [Include switch_to_screen, update_indicators, update_direction_arrows from previous step] ...
// I will just paste the modified UI Builder to capture lbl_wifi

static void update_indicators(void) {
    if (ui_data.light_on) {
        lv_obj_set_style_bg_color(ui_data.ind_light, lv_palette_main(LV_PALETTE_YELLOW), 0);
        lv_obj_set_style_shadow_width(ui_data.ind_light, 10, 0);
    } else {
        lv_obj_set_style_bg_color(ui_data.ind_light, lv_color_hex(0x333333), 0);
        lv_obj_set_style_shadow_width(ui_data.ind_light, 0, 0);
    }
    if (ui_data.sound_on) {
        lv_obj_set_style_bg_color(ui_data.ind_sound, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_set_style_shadow_width(ui_data.ind_sound, 10, 0);
    } else {
        lv_obj_set_style_bg_color(ui_data.ind_sound, lv_color_hex(0x333333), 0);
        lv_obj_set_style_shadow_width(ui_data.ind_sound, 0, 0);
    }
}

static void update_direction_arrows(void) {
    lv_color_t active = lv_palette_main(LV_PALETTE_TEAL);
    lv_color_t inactive = lv_color_hex(0x333333); 
    if (ui_data.direction_fwd) {
        lv_obj_set_style_text_color(ui_data.lbl_arrow_right, active, 0);
        lv_obj_set_style_text_color(ui_data.lbl_arrow_left, inactive, 0);
    } else {
        lv_obj_set_style_text_color(ui_data.lbl_arrow_right, inactive, 0);
        lv_obj_set_style_text_color(ui_data.lbl_arrow_left, active, 0);
    }
}

static void switch_to_screen(current_screen_t new_screen) {
    if (ui_data.current_screen == new_screen) {
        if (new_screen != SCREEN_MAIN) new_screen = SCREEN_MAIN;
        else return;
    }
    ui_data.current_screen = new_screen;

    lv_obj_add_flag(ui_data.main_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_data.train_list_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_data.func_list_cont, LV_OBJ_FLAG_HIDDEN);
    lv_group_remove_all_objs(ui_data.input_group);

    if (new_screen == SCREEN_MAIN) {
        lv_obj_clear_flag(ui_data.main_cont, LV_OBJ_FLAG_HIDDEN);
        lv_group_add_obj(ui_data.input_group, ui_data.arc);
        lv_group_focus_obj(ui_data.arc);
        lv_group_set_editing(ui_data.input_group, true); 
    } 
    else if (new_screen == SCREEN_TRAIN_LIST) {
        lv_obj_clear_flag(ui_data.train_list_cont, LV_OBJ_FLAG_HIDDEN);
        uint32_t cnt = lv_obj_get_child_cnt(ui_data.train_list);
        for(uint32_t i=0; i<cnt; i++) lv_group_add_obj(ui_data.input_group, lv_obj_get_child(ui_data.train_list, i));
        if(cnt > 0) lv_group_focus_obj(lv_obj_get_child(ui_data.train_list, 0));
    } 
    else if (new_screen == SCREEN_FUNC_LIST) {
        lv_obj_clear_flag(ui_data.func_list_cont, LV_OBJ_FLAG_HIDDEN);
        uint32_t cnt = lv_obj_get_child_cnt(ui_data.func_list);
        for(uint32_t i=0; i<cnt; i++) lv_group_add_obj(ui_data.input_group, lv_obj_get_child(ui_data.func_list, i));
         if(cnt > 0) lv_group_focus_obj(lv_obj_get_child(ui_data.func_list, 0));
    }
}

static lv_obj_t* create_indicator(lv_obj_t * parent, const char* text) {
    lv_obj_t * cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, 40, 40);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t * dot = lv_obj_create(cont);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, 12, 12);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(0x333333), 0); 
    lv_obj_set_style_shadow_color(dot, lv_palette_main(LV_PALETTE_YELLOW), 0); 
    lv_obj_t * lbl = lv_label_create(cont);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x888888), 0);
    return dot; 
}

/* --- UI BUILDER --- */
void build_ui(lv_disp_t * disp, lv_indev_t * indev)
{
    lv_obj_t * scr = lv_disp_get_scr_act(disp);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    // --- SETUP INPUT GROUP ---
    // This connects the hardware encoder to LVGL widgets
    ui_data.input_group = lv_group_create();
    lv_indev_set_group(indev, ui_data.input_group);

    // ============================================================
    // 1. MAIN SCREEN CONTAINER
    // ============================================================
    ui_data.main_cont = lv_obj_create(scr);
    lv_obj_remove_style_all(ui_data.main_cont);
    lv_obj_set_size(ui_data.main_cont, 240, 240);
    lv_obj_center(ui_data.main_cont);

    // Arc (Speed Control)
    ui_data.arc = lv_arc_create(ui_data.main_cont);
    lv_obj_set_size(ui_data.arc, 230, 230);
    lv_obj_center(ui_data.arc);
    lv_arc_set_bg_angles(ui_data.arc, 135, 45);
    lv_arc_set_rotation(ui_data.arc, 0);
    lv_obj_remove_style(ui_data.arc, NULL, LV_PART_KNOB); 
    lv_obj_add_flag(ui_data.arc, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_range(ui_data.arc, 0, 127);
    lv_arc_set_value(ui_data.arc, 0);
    
    // Arc Styles
    lv_obj_set_style_arc_color(ui_data.arc, lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_set_style_arc_width(ui_data.arc, 10, LV_PART_MAIN); 
    lv_obj_set_style_arc_color(ui_data.arc, lv_palette_main(LV_PALETTE_TEAL), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(ui_data.arc, 10, LV_PART_INDICATOR);
    
    // Attach Callback
    lv_obj_add_event_cb(ui_data.arc, arc_event_cb, LV_EVENT_ALL, NULL);

    // Top Info Row (WiFi Status)
    lv_obj_t * top_row = lv_obj_create(ui_data.main_cont);
    lv_obj_remove_style_all(top_row);
    lv_obj_set_size(top_row, 120, 30);
    lv_obj_align(top_row, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_flex_flow(top_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    ui_data.lbl_wifi = lv_label_create(top_row);
    lv_label_set_text(ui_data.lbl_wifi, LV_SYMBOL_WIFI " ...");
    lv_obj_set_style_text_color(ui_data.lbl_wifi, lv_color_hex(0x888888), 0); // Grey initially

    ui_data.lbl_power_status = lv_label_create(ui_data.main_cont);
    lv_label_set_text(ui_data.lbl_power_status, LV_SYMBOL_WARNING " PWR OFF");
    lv_obj_set_style_text_color(ui_data.lbl_power_status, lv_color_hex(0xFF4444), 0); // Bright Red text
    lv_obj_set_style_bg_color(ui_data.lbl_power_status, lv_color_hex(0x440000), 0);   // Dark Red background
    lv_obj_set_style_bg_opa(ui_data.lbl_power_status, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_top(ui_data.lbl_power_status, 4, 0);
    lv_obj_set_style_pad_bottom(ui_data.lbl_power_status, 4, 0);
    lv_obj_set_style_pad_left(ui_data.lbl_power_status, 8, 0);
    lv_obj_set_style_pad_right(ui_data.lbl_power_status, 8, 0);
    lv_obj_set_style_radius(ui_data.lbl_power_status, 10, 0); // Rounded pill shape
    lv_obj_set_style_text_font(ui_data.lbl_power_status, &lv_font_montserrat_14, 0);
    lv_obj_align(ui_data.lbl_power_status, LV_ALIGN_TOP_MID, 0, 50); // Tuck it right under the WiFi icon


    ui_data.lbl_stats = lv_label_create(ui_data.main_cont);
    lv_label_set_text(ui_data.lbl_stats, "RSSI: -- | Q: 0 B");
    lv_obj_set_style_text_font(ui_data.lbl_stats, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(ui_data.lbl_stats, lv_color_hex(0x888888), 0);
    lv_obj_align(ui_data.lbl_stats, LV_ALIGN_TOP_MID, 0, 75); 


    // Center Info Column (Name, Direction, Speed)
    lv_obj_t * center = lv_obj_create(ui_data.main_cont);
    lv_obj_remove_style_all(center);
    lv_obj_set_size(center, 180, 120);
    lv_obj_center(center);
    lv_obj_set_flex_flow(center, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(center, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * name = lv_label_create(center);
    lv_label_set_text(name, TRAIN_NAME);
    lv_obj_set_style_text_color(name, lv_color_hex(0x666666), 0);

    lv_obj_t * dir = lv_obj_create(center);
    lv_obj_remove_style_all(dir);
    lv_obj_set_size(dir, 160, 50);
    lv_obj_set_flex_flow(dir, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dir, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(dir, 10, 0);

    ui_data.lbl_arrow_left = lv_label_create(dir);
    lv_label_set_text(ui_data.lbl_arrow_left, LV_SYMBOL_LEFT);
    
    ui_data.lbl_speed = lv_label_create(dir);
    lv_label_set_text(ui_data.lbl_speed, "0");
    lv_obj_set_style_text_font(ui_data.lbl_speed, &lv_font_montserrat_28, 0); 
    
    ui_data.lbl_arrow_right = lv_label_create(dir);
    lv_label_set_text(ui_data.lbl_arrow_right, LV_SYMBOL_RIGHT);

    // Indicator Row (Light/Sound dots)
    lv_obj_t * bot_row = lv_obj_create(ui_data.main_cont);
    lv_obj_remove_style_all(bot_row);
    lv_obj_set_size(bot_row, 140, 50);
    lv_obj_align(bot_row, LV_ALIGN_BOTTOM_MID, 0, -50);
    lv_obj_set_flex_flow(bot_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bot_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(bot_row, 10, 0);
    
    ui_data.ind_light = create_indicator(bot_row, "LIGHT");
    ui_data.ind_sound = create_indicator(bot_row, "SOUND");

    // Footer Labels (To explain Btn3/Btn4)
    lv_obj_t * footer = lv_obj_create(ui_data.main_cont);
    lv_obj_remove_style_all(footer);
    lv_obj_set_size(footer, 120, 20);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    lv_obj_t * lbl_b3 = lv_label_create(footer);
    lv_label_set_text(lbl_b3, "TRAINS");
    lv_obj_set_style_text_font(lbl_b3, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_b3, lv_color_hex(0x555555), 0);

    lv_obj_t * lbl_b4 = lv_label_create(footer);
    lv_label_set_text(lbl_b4, "FUNCS");
    lv_obj_set_style_text_font(lbl_b4, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_b4, lv_color_hex(0x555555), 0);


    // ============================================================
    // 2. TRAIN LIST SCREEN (STYLED)
    // ============================================================
    ui_data.train_list_cont = lv_obj_create(scr);
    lv_obj_remove_style_all(ui_data.train_list_cont);
    lv_obj_set_size(ui_data.train_list_cont, 240, 240);
    lv_obj_center(ui_data.train_list_cont);
    lv_obj_set_style_bg_color(ui_data.train_list_cont, lv_color_black(), 0);
    lv_obj_add_flag(ui_data.train_list_cont, LV_OBJ_FLAG_HIDDEN);

    /* --- Header --- */
    lv_obj_t * tr_head = lv_label_create(ui_data.train_list_cont);
    lv_label_set_text(tr_head, "SELECT TRAIN");
    lv_obj_align(tr_head, LV_ALIGN_TOP_MID, 0, 25);
    lv_obj_set_style_text_font(tr_head, &lv_font_montserrat_14, 0); 
    lv_obj_set_style_text_color(tr_head, lv_palette_main(LV_PALETTE_TEAL), 0);
    lv_obj_set_style_text_decor(tr_head, LV_TEXT_DECOR_UNDERLINE, 0);

    /* --- The List Container --- */
    ui_data.train_list = lv_list_create(ui_data.train_list_cont);
    lv_obj_set_size(ui_data.train_list, 200, 160);
    lv_obj_align(ui_data.train_list, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    // Transparent background so buttons float
    lv_obj_set_style_bg_opa(ui_data.train_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui_data.train_list, 0, 0);
    lv_obj_set_style_pad_column(ui_data.train_list, 10, 0); // Gap between items

    /* --- Styles for List Buttons --- */
    // Normal State
    static lv_style_t style_btn_def;
    lv_style_init(&style_btn_def);
    lv_style_set_bg_color(&style_btn_def, lv_color_hex(0x1a1a1a));
    lv_style_set_text_color(&style_btn_def, lv_color_white());
    lv_style_set_radius(&style_btn_def, 10);
    lv_style_set_border_width(&style_btn_def, 1);
    lv_style_set_border_color(&style_btn_def, lv_color_hex(0x444444));
    lv_style_set_margin_bottom(&style_btn_def, 8); 

    // Focused State (Neon Glow)
    static lv_style_t style_btn_sel;
    lv_style_init(&style_btn_sel);
    lv_style_set_bg_color(&style_btn_sel, lv_palette_main(LV_PALETTE_TEAL));
    lv_style_set_text_color(&style_btn_sel, lv_color_black()); 
    lv_style_set_border_color(&style_btn_sel, lv_palette_main(LV_PALETTE_TEAL));
    lv_style_set_shadow_width(&style_btn_sel, 15);
    lv_style_set_shadow_color(&style_btn_sel, lv_palette_main(LV_PALETTE_TEAL));
    lv_style_set_transform_width(&style_btn_sel, 5); // Grow slightly

    /* --- Add Dummy Trains --- */
    const char * trains[] = {"BR 218", "ICE 3", "V 200", "RE 4/4"};
    const char * icons[] = {LV_SYMBOL_SETTINGS, LV_SYMBOL_CHARGE, LV_SYMBOL_DRIVE, LV_SYMBOL_GPS};
    
    for(int i=0; i<4; i++) {
        lv_obj_t * btn = lv_list_add_btn(ui_data.train_list, icons[i], trains[i]);
        lv_obj_add_event_cb(btn, list_event_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_style(btn, &style_btn_def, 0);
        lv_obj_add_style(btn, &style_btn_sel, LV_STATE_FOCUSED);
    }

    // ============================================================
    // 3. FUNCTION LIST SCREEN
    // ============================================================
    ui_data.func_list_cont = lv_obj_create(scr);
    lv_obj_remove_style_all(ui_data.func_list_cont);
    lv_obj_set_size(ui_data.func_list_cont, 240, 240);
    lv_obj_center(ui_data.func_list_cont);
    lv_obj_set_style_bg_color(ui_data.func_list_cont, lv_color_black(), 0);
    lv_obj_add_flag(ui_data.func_list_cont, LV_OBJ_FLAG_HIDDEN);

    // Header
    lv_obj_t * fn_head = lv_label_create(ui_data.func_list_cont);
    lv_label_set_text(fn_head, "FUNCTIONS");
    lv_obj_align(fn_head, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_text_color(fn_head, lv_palette_main(LV_PALETTE_ORANGE), 0);

    // List
    ui_data.func_list = lv_list_create(ui_data.func_list_cont);
    lv_obj_set_size(ui_data.func_list, 180, 150);
    lv_obj_align(ui_data.func_list, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_color(ui_data.func_list, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(ui_data.func_list, 0, 0);

    // Add Dummy Functions (F1-F4)
    lv_list_add_btn(ui_data.func_list, LV_SYMBOL_VOLUME_MAX, "F1: Horn");
    lv_list_add_btn(ui_data.func_list, LV_SYMBOL_EYE_OPEN, "F2: High Beam");
    lv_list_add_btn(ui_data.func_list, LV_SYMBOL_CHARGE, "F3: Pantograph");
    lv_list_add_btn(ui_data.func_list, LV_SYMBOL_HOME, "F4: Station");

    // ============================================================
    // INITIALIZATION
    // ============================================================
    ui_data.current_screen = SCREEN_MAIN;
    ui_data.direction_fwd = true;
    update_direction_arrows();
    update_indicators();
    update_power_indicator();

    // Start with Arc focused and in editing mode (so turning knob changes speed)
    lv_group_add_obj(ui_data.input_group, ui_data.arc);
    lv_group_focus_obj(ui_data.arc);
    lv_group_set_editing(ui_data.input_group, true);
}


static void button_task(void *arg) {
    // 1. Initialize the new discrete buttons (Active Low with Pullups)
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << BTN1) | (1ULL << BTN2) | (1ULL << BTN3),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, // Assumes buttons connect to GND
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&btn_conf);

    int last_btn = 0;
    int last_pwr_btn_state = 1;

    while (1) {
        // --- Power Button Logic (Unchanged) ---
        int pwr_btn_state = gpio_get_level(POWER_BTN_PIN);
        if (last_pwr_btn_state == 1 && pwr_btn_state == 0) {
            vTaskDelay(pdMS_TO_TICKS(50)); // Debounce
            if (gpio_get_level(POWER_BTN_PIN) == 0) {
                ui_data.power_on = !ui_data.power_on;
                gpio_set_level(POWER_LED_PIN, ui_data.power_on ? 1 : 0);
                ui_data.power_update_pending = true;

                lvgl_port_lock(0);
                update_power_indicator();
                lvgl_port_unlock();


                ESP_LOGI(TAG, "Power Toggled: %d", ui_data.power_on);
            }
        }
        last_pwr_btn_state = pwr_btn_state;

        // --- New Discrete Button Logic ---
        int current_btn = 0;
        
        // Read pins (0 means pressed because of pull-up)
        if (gpio_get_level(BTN1) == 0) current_btn = 1;
        else if (gpio_get_level(BTN3) == 0) current_btn = 2;
        else if (gpio_get_level(BTN2) == 0) current_btn = 3;

        // If state changed, debounce and execute
        if (current_btn != last_btn) {
            vTaskDelay(pdMS_TO_TICKS(50)); // Debounce wait
            
            // Re-read to confirm
            int confirm_btn = 0;
            if (gpio_get_level(BTN1) == 0) confirm_btn = 1;
            else if (gpio_get_level(BTN3) == 0) confirm_btn = 2;
            else if (gpio_get_level(BTN2) == 0) confirm_btn = 3;

            if (confirm_btn == current_btn && current_btn != 0) {
                lvgl_port_lock(0);
                
                if (current_btn == 1) { 
                    // BTN1: Light (F0)
                    if(ui_data.current_screen == SCREEN_MAIN) {
                        ui_data.light_on = !ui_data.light_on;
                        update_indicators();
                        ui_data.state_changed = true;
                    } else {
                        // If in a list, maybe trigger the focused item
                        lv_obj_send_event(lv_group_get_focused(ui_data.input_group), LV_EVENT_CLICKED, NULL);
                    }
                } 
                else if (current_btn == 2) { 
                    // BTN2: Sound (F2)
                    if(ui_data.current_screen == SCREEN_MAIN) {
                        ui_data.sound_on = !ui_data.sound_on;
                        update_indicators();
                        ui_data.state_changed = true;
                    } else {
                        // Exit list back to main
                        switch_to_screen(SCREEN_MAIN);
                    }
                }
                else if (current_btn == 3) { 
                    // BTN3: The New Screen Cycle Logic
                    if (ui_data.current_screen == SCREEN_MAIN) {
                        switch_to_screen(SCREEN_TRAIN_LIST);
                    } else if (ui_data.current_screen == SCREEN_TRAIN_LIST) {
                        switch_to_screen(SCREEN_FUNC_LIST);
                    } else {
                        switch_to_screen(SCREEN_MAIN);
                    }
                }
                lvgl_port_unlock();
            }
            last_btn = confirm_btn;
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Task loop delay
    }
}


static void init_encoder_hardware(void) {
    pcnt_unit_config_t unit_config = { .high_limit = 10000, .low_limit = -10000 };
    pcnt_new_unit(&unit_config, &pcnt_unit);
    pcnt_chan_config_t chan_a_config = { .edge_gpio_num = ENCODER_PIN_A, .level_gpio_num = ENCODER_PIN_B };
    pcnt_channel_handle_t pcnt_chan_a = NULL;
    pcnt_new_channel(pcnt_unit, &chan_a_config, &pcnt_chan_a);
    pcnt_chan_config_t chan_b_config = { .edge_gpio_num = ENCODER_PIN_B, .level_gpio_num = ENCODER_PIN_A };
    pcnt_channel_handle_t pcnt_chan_b = NULL;
    pcnt_new_channel(pcnt_unit, &chan_b_config, &pcnt_chan_b);
    pcnt_channel_set_edge_action(pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
    pcnt_channel_set_level_action(pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
    pcnt_channel_set_edge_action(pcnt_chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE);
    pcnt_channel_set_level_action(pcnt_chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
    pcnt_unit_enable(pcnt_unit);
    pcnt_unit_clear_count(pcnt_unit);
    pcnt_unit_start(pcnt_unit);
    gpio_config_t btn_cfg = { .pin_bit_mask = (1ULL << ENCODER_PIN_BTN), .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE };
    gpio_config(&btn_cfg);
}

static void encoder_read_cb(lv_indev_t * indev, lv_indev_data_t * data) {
    int pcnt_count = 0;
    pcnt_unit_get_count(pcnt_unit, &pcnt_count);
    data->enc_diff = pcnt_count - last_pcnt_count;
    last_pcnt_count = pcnt_count;
    if(gpio_get_level(ENCODER_PIN_BTN) == 0) data->state = LV_INDEV_STATE_PRESSED;
    else data->state = LV_INDEV_STATE_RELEASED;
}


static void mqtt_sender_task(void *arg) {
    while (1) {
        // Bumped to 150ms. It gives the WiFi stack just a tiny bit more breathing 
        // room to clear the outbox without making the UI feel sluggish.
        vTaskDelay(pdMS_TO_TICKS(150)); 

        if (mqtt_client == NULL) continue;

        if (ui_data.power_update_pending) {
            ui_data.power_update_pending = false;

            cJSON *root = cJSON_CreateObject();
            cJSON_AddBoolToObject(root, "Power", ui_data.power_on);
            
            char *payload = cJSON_PrintUnformatted(root);
            esp_mqtt_client_publish(mqtt_client, "trains/command", payload, 0, 0, 0);

            cJSON_Delete(root);
            free(payload);
            ESP_LOGI(TAG, "Sent Power: %d", ui_data.power_on);
            continue; 
        }

        if (ui_data.state_changed) {
            ui_data.state_changed = false;
            bool has_changes = false;

            // Start JSON with just the identifier
            cJSON *root = cJSON_CreateObject();
            cJSON_AddNumberToObject(root, "ID", TRAIN_ID);

            // 1. Check Speed
            if (ui_data.speed != ui_data.sent_speed) {
                cJSON_AddNumberToObject(root, "Speed", ui_data.speed);
                ui_data.sent_speed = ui_data.speed;
                cJSON_AddNumberToObject(root, "Direction", ui_data.direction_fwd ? 1 : 0);
                has_changes = true;
            }else if (ui_data.direction_fwd != ui_data.sent_direction_fwd) {
                cJSON_AddNumberToObject(root, "Direction", ui_data.direction_fwd ? 1 : 0);
                ui_data.sent_direction_fwd = ui_data.direction_fwd;
                has_changes = true;
            }

            // 3. Check Functions
            if (ui_data.light_on != ui_data.sent_light_on || ui_data.sound_on != ui_data.sent_sound_on) {
                cJSON *funcs = cJSON_CreateArray();

                if (ui_data.light_on != ui_data.sent_light_on) {
                    cJSON *f0 = cJSON_CreateObject();
                    cJSON_AddNumberToObject(f0, "ID", 1); 
                    cJSON_AddNumberToObject(f0, "FIndex", 0);
                    cJSON_AddBoolToObject(f0, "IsActive", ui_data.light_on);
                    cJSON_AddItemToArray(funcs, f0);
                    ui_data.sent_light_on = ui_data.light_on;
                }

                if (ui_data.sound_on != ui_data.sent_sound_on) {
                    cJSON *f2 = cJSON_CreateObject();
                    cJSON_AddNumberToObject(f2, "ID", 2);
                    cJSON_AddNumberToObject(f2, "FIndex", 2);
                    cJSON_AddBoolToObject(f2, "IsActive", ui_data.sound_on);
                    cJSON_AddItemToArray(funcs, f2);
                    ui_data.sent_sound_on = ui_data.sound_on;
                }

                cJSON_AddItemToObject(root, "Functions", funcs);
                has_changes = true;
            }

            // Publish ONLY if we actually added new data to the payload
            if (has_changes) {
                char *payload = cJSON_PrintUnformatted(root);
                char topic[64];
                sprintf(topic, "%s/%d", topic_command_base, TRAIN_ID);
                
                esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 0, 0);
                
                ESP_LOGI(TAG, "Delta Sent: %s", payload);
                free(payload);
            }
            
            cJSON_Delete(root); 
        }
    }
}

static void network_stats_task(void *arg) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000)); // Update once a second

        int rssi = 0;
        int queue_size = 0;

        // 1. Get WiFi RSSI
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            rssi = ap_info.rssi;
        }

        // 2. Get MQTT Outbox (Queue) Size in Bytes
        if (mqtt_client != NULL) {
            queue_size = esp_mqtt_client_get_outbox_size(mqtt_client);
        }

        // 3. Update UI Safely
        if (ui_data.lbl_stats && ui_data.current_screen == SCREEN_MAIN) {
            lvgl_port_lock(0);
            lv_label_set_text_fmt(ui_data.lbl_stats, "RSSI: %d | Q: %dB", rssi, queue_size);
            
            // Color code based on network health
            if (rssi < -80 || queue_size > 1024) {
                // Bad signal or heavily backed up queue
                lv_obj_set_style_text_color(ui_data.lbl_stats, lv_palette_main(LV_PALETTE_RED), 0);
            } else if (rssi < -70 || queue_size > 256) {
                // Mediocre signal or slight backup
                lv_obj_set_style_text_color(ui_data.lbl_stats, lv_palette_main(LV_PALETTE_ORANGE), 0);
            } else {
                // Good
                lv_obj_set_style_text_color(ui_data.lbl_stats, lv_color_hex(0x888888), 0);
            }
            lvgl_port_unlock();
        }
    }
}


void app_main(void) {
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      // NVS partition was truncated and needs to be erased
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // 1. Start WiFi & MQTT (Async)
    start_wifi_mqtt();
    // 2. Hardware
    init_power_hardware();
    init_encoder_hardware();
    spi_bus_config_t buscfg = { .sclk_io_num = PIN_NUM_CLK, .mosi_io_num = PIN_NUM_MOSI, .miso_io_num = PIN_NUM_MISO, .quadwp_io_num = -1, .quadhd_io_num = -1, .max_transfer_sz = LCD_H_RES * LCD_V_RES * sizeof(uint16_t) };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = { .dc_gpio_num = PIN_NUM_DC, .cs_gpio_num = PIN_NUM_CS, .pclk_hz = 40 * 1000 * 1000, .lcd_cmd_bits = 8, .lcd_param_bits = 8, .spi_mode = 0, .trans_queue_depth = 10 };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &io_handle));
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = { .reset_gpio_num = PIN_NUM_RST, .rgb_endian = LCD_RGB_ENDIAN_RGB, .bits_per_pixel = 16 };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_cfg.task_stack = 20 * 1024; 
    lvgl_cfg.task_priority = 4;
    lvgl_cfg.task_affinity = 1; 
    lvgl_port_init(&lvgl_cfg);
    const lvgl_port_display_cfg_t disp_cfg = { .io_handle = io_handle, .panel_handle = panel_handle, .buffer_size = LCD_H_RES * 50 * sizeof(uint16_t), .double_buffer = false, .hres = LCD_H_RES, .vres = LCD_V_RES, .monochrome = false, .rotation = { .swap_xy = false, .mirror_x = true, .mirror_y = false }, .flags = { .buff_dma = true, .swap_bytes = 1 } };
    lv_disp_t * disp = lvgl_port_add_disp(&disp_cfg);

    if (disp) {
        lv_indev_t * indev = lv_indev_create(); 
        lv_indev_set_type(indev, LV_INDEV_TYPE_ENCODER);
        lv_indev_set_read_cb(indev, encoder_read_cb);

        lvgl_port_lock(0);
        build_ui(disp, indev);
        lvgl_port_unlock();
        xTaskCreatePinnedToCore(mqtt_sender_task, "mqtt_tx", 4096, NULL, 5, NULL, 1);
        xTaskCreatePinnedToCore(button_task, "btn_task", 4096, NULL, 5, NULL, 1);
        xTaskCreatePinnedToCore(network_stats_task, "net_stats", 2048, NULL, 2, NULL, 1); // <-- NEW

        ESP_LOGI(TAG, "UI initialized");
    }
}