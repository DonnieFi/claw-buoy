#include "claw_box_display.h"
#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "esp_sntp.h"
#include "nvs.h"
#include "cJSON.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_console.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_openclaw_node_example_json.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lvgl.h"
#include "claw_logo_small.h"
#include "claw_box_touch.h"
#include "buoy_ui_rules.h"
#include "buoy_site_config.h"

bool claw_box_pin_reserved(int pin)
{
    /* Protect I2C even when the display is disabled. */
    if (pin == 21 || pin == 22) return true;
#ifdef CONFIG_CLAW_BOX_DISPLAY
    const int pins[] = {CLAW_BACKLIGHT_GPIO, 18, 23, 19, 27, 26, 25, 33, 32, 14, 13};
    for (size_t i = 0; i < sizeof(pins)/sizeof(pins[0]); ++i)
        if (pin == pins[i]) return true;
#endif
    return false;
}
#ifdef CONFIG_CLAW_BOX_DISPLAY
static const char *TAG = "claw_box";
/* Omarchy-inspired warm terminal palette; one set shared by every page. */
#define BUOY_BG 0x1D2021
#define BUOY_SURFACE 0x2D353B
#define BUOY_INK 0xEBDDB2
#define BUOY_MUTED 0xB6B0A0
#define BUOY_ACCENT 0x83C092
#define BUOY_PRESSED 0xE67E80
#define BUOY_SUN 0xDBBC7F
#define CLAW_DRAW_BUFFER_BYTES (320 * 4 * 2)
static SemaphoreHandle_t lock;
static char status_text[64] = "Connecting to OpenClaw...";
static char reply_text[1025] = "Waiting for agent reply";
static char sender_text[64] = "Auggie";
static int64_t message_started_us;
static char queued_text[3][1025], queued_sender[3][64];
static unsigned queued_count;
static bool sensor_valid;
static double temperature, humidity;
static int64_t sensor_time_us;

typedef enum { SCREEN_CONNECTING, SCREEN_LOGO, SCREEN_MESSAGE } screen_mode_t;
static screen_mode_t screen_mode = SCREEN_CONNECTING;
static bool dirty = true, ready;
static esp_lcd_panel_handle_t panel;
static lv_obj_t *status_label, *reply_label, *from_label, *logo, *message_panel, *telemetry, *temperature_label, *humidity_label;
static lv_obj_t *content_label, *content_header, *content_footer, *message_footer;
static unsigned content_page[2], content_pages[2]={1,1}, message_page, message_pages=1;
static char page_text[1100], rendered_temperature[40], rendered_humidity[24];
static lv_obj_t *weather_graphic;
static buoy_weather_t weather_kind=BUOY_WEATHER_UNKNOWN, rendered_weather_kind;
/* Native LVGL vector drawing: one retained object, no bitmap/canvas buffer. */
static void weather_shape(lv_layer_t *layer,int x,int y,int w,int h,int radius,uint32_t color)
{
    lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.bg_color=lv_color_hex(color);d.radius=radius;
    lv_area_t a={x,y,x+w-1,y+h-1};lv_draw_rect(layer,&d,&a);
}
static void weather_line(lv_layer_t *layer,int x1,int y1,int x2,int y2,uint32_t color,int width)
{
    lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);d.color=lv_color_hex(color);d.width=width;d.round_start=d.round_end=1;
    d.p1.x=x1;d.p1.y=y1;d.p2.x=x2;d.p2.y=y2;lv_draw_line(layer,&d);
}
static void weather_face(lv_layer_t *layer,int x,int y)
{
    weather_shape(layer,x+2,y,2,3,1,BUOY_BG);
    weather_shape(layer,x+10,y,2,3,1,BUOY_BG);
    weather_line(layer,x+5,y+7,x+9,y+7,BUOY_BG,2);
}
static void draw_weather_graphic(lv_event_t *event)
{
    lv_layer_t *layer=lv_event_get_layer(event);lv_area_t a;lv_obj_get_coords(lv_event_get_target(event),&a);
    int x=a.x1,y=a.y1;buoy_weather_t kind=rendered_weather_kind;
    if(kind==BUOY_WEATHER_SUNNY||kind==BUOY_WEATHER_MIXED){
        weather_shape(layer,x+26,y+6,24,24,LV_RADIUS_CIRCLE,BUOY_SUN);
        weather_face(layer,x+31,y+13);
        if(kind==BUOY_WEATHER_SUNNY){
            weather_line(layer,x+38,y+1,x+38,y+4,BUOY_SUN,2);weather_line(layer,x+38,y+32,x+38,y+36,BUOY_SUN,2);
            weather_line(layer,x+20,y+18,x+24,y+18,BUOY_SUN,2);weather_line(layer,x+52,y+18,x+56,y+18,BUOY_SUN,2);
            weather_line(layer,x+24,y+4,x+27,y+7,BUOY_SUN,2);weather_line(layer,x+49,y+29,x+52,y+32,BUOY_SUN,2);
            return;
        }
    }
    if(kind==BUOY_WEATHER_NIGHT){
        weather_shape(layer,x+20,y+3,34,34,LV_RADIUS_CIRCLE,BUOY_INK);
        weather_shape(layer,x+31,y,30,30,LV_RADIUS_CIRCLE,BUOY_BG);
        weather_shape(layer,x+6,y+6,3,3,LV_RADIUS_CIRCLE,BUOY_SUN);
        weather_shape(layer,x+52,y+35,3,3,LV_RADIUS_CIRCLE,BUOY_SUN);return;
    }
    weather_shape(layer,x+8,y+16,26,24,LV_RADIUS_CIRCLE,BUOY_MUTED);
    weather_shape(layer,x+24,y+9,28,30,LV_RADIUS_CIRCLE,BUOY_MUTED);
    weather_shape(layer,x+3,y+27,54,14,7,BUOY_MUTED);
    weather_face(layer,x+22,y+25);
    if(kind==BUOY_WEATHER_RAIN)for(int i=0;i<3;++i)weather_line(layer,x+16+i*14,y+43,x+12+i*14,y+48,BUOY_ACCENT,2);
    if(kind==BUOY_WEATHER_SNOW)for(int i=0;i<3;++i){weather_line(layer,x+14+i*15,y+43,x+14+i*15,y+49,BUOY_ACCENT,2);weather_line(layer,x+11+i*15,y+46,x+17+i*15,y+46,BUOY_ACCENT,2);}
    if(kind==BUOY_WEATHER_STORM){weather_line(layer,x+32,y+35,x+25,y+43,BUOY_SUN,4);weather_line(layer,x+25,y+43,x+33,y+43,BUOY_SUN,4);weather_line(layer,x+33,y+43,x+27,y+49,BUOY_SUN,4);}
    if(kind==BUOY_WEATHER_FOG){weather_line(layer,x+8,y+44,x+54,y+44,BUOY_MUTED,2);weather_line(layer,x+16,y+49,x+47,y+49,BUOY_MUTED,2);}
}
static char agenda_date[11];
static time_t agenda_updated, weather_updated;
static bool agenda_offline=true, weather_offline=true, ping_test_mode=true;
static unsigned touch_releases;
/* The sampler publishes contact only. UI feedback never dispatches actions. */
static atomic_uint contact_preview;
static lv_obj_t *touch_halo;
static int64_t feedback_until_us;
static int feedback_x,feedback_y;
static int feedback_page=-1,feedback_overlay=-1;
static unsigned glyph_width(uint32_t cp) { return lv_font_get_glyph_width(&lv_font_montserrat_14,cp,0); }
static void advance_message(int64_t now)
{
    if(message_page+1<message_pages)++message_page;
    else {
        message_page=0;
        if(!buoy_advance_message(reply_text,sizeof(reply_text),sender_text,sizeof(sender_text),queued_text,queued_sender,&queued_count)){screen_mode=SCREEN_LOGO;reply_text[0]=0;message_pages=1;}
    }
    message_started_us=now;dirty=true;
}
static int page; /* 0 Home, 1 Today, 2 Actions */
static int overlay; /* 1 weather, 2 alarm editor */
static bool asleep;
static unsigned backlight_percent = 100;
static unsigned backlight_applied_percent = 100;
static bool backlight_ready;

static esp_err_t backlight_init(void)
{
    const ledc_timer_config_t timer = {
        .speed_mode=LEDC_LOW_SPEED_MODE, .duty_resolution=LEDC_TIMER_10_BIT,
        .timer_num=LEDC_TIMER_0, .freq_hz=5000, .clk_cfg=LEDC_AUTO_CLK};
    esp_err_t err=ledc_timer_config(&timer);
    if(err!=ESP_OK)return err;
    const ledc_channel_config_t channel = {
        .gpio_num=CLAW_BACKLIGHT_GPIO, .speed_mode=LEDC_LOW_SPEED_MODE,
        .channel=LEDC_CHANNEL_0, .timer_sel=LEDC_TIMER_0,
        .intr_type=LEDC_INTR_DISABLE, .duty=1023, .hpoint=0};
    err=ledc_channel_config(&channel);
    backlight_ready=(err==ESP_OK);
    return err;
}
static void backlight_apply(void)
{
    unsigned target=asleep?0:backlight_percent;
    if(!backlight_ready||target==backlight_applied_percent)return;
    esp_err_t err=ledc_set_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,(target*1023+50)/100);
    if(err==ESP_OK)err=ledc_update_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0);
    if(err==ESP_OK)backlight_applied_percent=target;
    else ESP_LOGE(TAG,"backlight PWM update failed: %s",esp_err_to_name(err));
}
static lv_obj_t *sleep_cover;
static int64_t last_touch_us, manual_night_until_us;
static int alarm_minute = 420, last_alarm_day = -1;
static bool alarm_enabled;
static uint16_t last_raw_x, last_raw_y;
static int touch_x,touch_y;
static bool touch_calibrated,wake_requested;
static bool touch_test, press_consumed, affine_valid;
static int32_t cal_x0,cal_y0,cal_x1,cal_y1,cal_x2,cal_y2;
static buoy_affine_t affine;
static lv_obj_t *test_panel,*test_label;
static unsigned touch_presses;
static int clamp_pixel(int value,int max);
static int map_x(int x,int y) { return clamp_pixel(affine_valid?buoy_affine_axis(affine.x,x,y):buoy_axis(y,cal_y0,cal_y1,40,280),319); }
static int map_y(int x,int y) { return clamp_pixel(affine_valid?buoy_affine_axis(affine.y,x,y):buoy_axis(x,cal_x0,cal_x1,40,190),239); }
static int calibration_stage=1;
static lv_obj_t *cal_dot,*cal_label,*alarm_toggle,*alarm_toggle_label,*back_button,*back_label;
static int pending_action;
static int64_t last_request_us[5];
static esp_openclaw_node_handle_t display_node;
static char agenda_text[1025] = "Calendar not connected yet.";
static char weather_text[513] = "Tap Weather to request a forecast.";
static char action_status[96] = "";
static lv_obj_t *clock_label, *day_label, *action_tiles[4], *action_labels[4];
static const char *action_names[] = {BUOY_PING_ONE_LABEL, BUOY_PING_TWO_LABEL, "Weather", "Alarm"};

static void save_calibration(void)
{
 nvs_handle_t h;
 if(nvs_open("buoy_ui",NVS_READWRITE,&h)==ESP_OK){nvs_set_i32(h,"cx0",cal_x0);nvs_set_i32(h,"cy0",cal_y0);nvs_set_i32(h,"cx1",cal_x1);nvs_set_i32(h,"cy1",cal_y1);nvs_set_u8(h,"affine",affine_valid);nvs_set_i32(h,"cx2",cal_x2);nvs_set_i32(h,"cy2",cal_y2);nvs_commit(h);nvs_close(h);}
}
static void save_integration_state(void)
{
    nvs_handle_t h;
    if(nvs_open("buoy_ui",NVS_READWRITE,&h)!=ESP_OK)return;
    nvs_set_u8(h,"ping_test",ping_test_mode);
    nvs_set_str(h,"weather",weather_text);
    nvs_set_i64(h,"weather_at",weather_updated);
    nvs_set_u8(h,"weather_off",weather_offline);
    nvs_set_u8(h,"weather_kind",weather_kind);
    nvs_commit(h);nvs_close(h);
}
static void save_alarm(void)
{
    nvs_handle_t h;
    if (nvs_open("buoy_ui", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u16(h, "alarm_min", alarm_minute);
        nvs_set_u8(h, "alarm_on", alarm_enabled);
        nvs_commit(h); nvs_close(h);
    }
}
static void action_result(esp_openclaw_node_handle_t node,
    const esp_openclaw_node_gateway_result_t *result, void *ctx)
{
    (void)node;
    xSemaphoreTake(lock, portMAX_DELAY);
    int action=(int)(uintptr_t)ctx;
    if(!result->ok){if(action==3){weather_offline=true;}
        if(action==4){agenda_offline=true;}}
    snprintf(action_status, sizeof(action_status), "%s", result->ok ? "Request accepted" : "Request failed - try again");
    dirty = true;
    xSemaphoreGive(lock);
}
static void request_action(int action)
{
    int64_t now = esp_timer_get_time();
    xSemaphoreTake(lock,portMAX_DELAY);
    if (touch_test || !touch_calibrated || calibration_stage || action < 1 || action > 4 || (last_request_us[action] && now-last_request_us[action] < 15000000)) {
        xSemaphoreGive(lock);return;
    }
    if(action<=2 && ping_test_mode){
        snprintf(action_status,sizeof(action_status),"TEST: %s - no message sent",action_names[action-1]);dirty=true;
        xSemaphoreGive(lock);return;
    }
    last_request_us[action] = now;
    xSemaphoreGive(lock);
    const char *messages[] = {"", BUOY_PING_ONE_MESSAGE, BUOY_PING_TWO_MESSAGE, BUOY_WEATHER_MESSAGE, BUOY_AGENDA_MESSAGE};
    cJSON *params = cJSON_CreateObject();
    cJSON *payload = cJSON_CreateObject();
    if (!params || !payload) { cJSON_Delete(params); cJSON_Delete(payload); return; }
    cJSON_AddStringToObject(params, "event", "agent.request");
    cJSON_AddStringToObject(payload, "message", messages[action]);
    cJSON_AddStringToObject(payload, "sessionKey", BUOY_ACTION_SESSION_KEY);
    const char *target=action==1?BUOY_PING_ONE_TARGET:BUOY_PING_TWO_TARGET;
    bool deliver=action<=2 && target[0]!='\0';
    cJSON_AddBoolToObject(payload, "deliver", deliver);
    if (deliver) {
        cJSON_AddStringToObject(payload, "channel", "discord");
        cJSON_AddStringToObject(payload, "to", target);
    }
    cJSON_AddNumberToObject(payload, "timeoutSeconds", 90);
    cJSON_AddItemToObject(params, "payload", payload);
    char *json = cJSON_PrintUnformatted(params); cJSON_Delete(params);
    xSemaphoreTake(lock,portMAX_DELAY);
    snprintf(action_status,sizeof(action_status),"Requesting...");dirty=true;
    xSemaphoreGive(lock);
    esp_err_t err = json ? esp_openclaw_node_gateway_request(display_node, "node.event", json, action_result, (void *)(uintptr_t)action) : ESP_ERR_NO_MEM;
    free(json);
    if(err!=ESP_OK){
        xSemaphoreTake(lock,portMAX_DELAY);
        snprintf(action_status,sizeof(action_status),"Offline - try again");
        if(action==3){weather_offline=true;}
        if(action==4){agenda_offline=true;}
        dirty=true;xSemaphoreGive(lock);
    }
}
static QueueHandle_t touch_queue;
static atomic_uint dropped_strokes;
static void touch_task(void *arg)
{
    (void)arg;buoy_touch_tracker_t tracker={0};buoy_touch_stroke_t stroke;
    TickType_t last=xTaskGetTickCount();
    for(;;){
        uint16_t x=0,y=0;bool valid=claw_box_touch_read(&x,&y);
        claw_box_touch_diagnostics_t d;claw_box_touch_get_diagnostics(&d);
        bool event=buoy_touch_event(&tracker,valid,x,y,d.pressure,esp_timer_get_time(),&stroke);
        unsigned preview=tracker.down ? (1U<<24)|((unsigned)tracker.stroke.last_x<<12)|tracker.stroke.last_y : 0;
        /* Clear the preview before publishing a release: the UI must not tint
           a destination page with the old contact after navigation. */
        atomic_store(&contact_preview,preview);
        if(event){
            /* Whole events only; release never causes a second button action. */
            if(xQueueSend(touch_queue,&stroke,0)!=pdTRUE)atomic_fetch_add(&dropped_strokes,1);
        }
        vTaskDelayUntil(&last,pdMS_TO_TICKS(10));
    }
}
static uint32_t ticks(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

/* XPT2046 landscape mapping. Raw Y increases toward screen left.
 * Edge navigation needs only the horizontal half; button placement uses both axes.
 * Calibration values are inspectable via display.state. */
static int clamp_pixel(int value, int max) { return value < 0 ? 0 : value > max ? max : value; }
static void poll_touch(void)
{
    buoy_touch_stroke_t stroke;
    for(unsigned processed=0;processed<8&&xQueueReceive(touch_queue,&stroke,0)==pdTRUE;++processed){
        int64_t now=esp_timer_get_time();
        xSemaphoreTake(lock,portMAX_DELAY);
        int start_x=map_x(stroke.first_x,stroke.first_y);
        int start_y=map_y(stroke.first_x,stroke.first_y);
        int excursion=0;
        for(int i=0;i<4;++i){
            int rx=(i&1)?stroke.max_x:stroke.min_x,ry=(i&2)?stroke.max_y:stroke.min_y;
            int dx=abs(map_x(rx,ry)-start_x),dy=abs(map_y(rx,ry)-start_y);
            if(dx>excursion)excursion=dx;
            if(dy>excursion)excursion=dy;
        }
        if(stroke.press){
            ++touch_presses;press_consumed=false;
            touch_x=start_x;touch_y=start_y;last_raw_x=stroke.first_x;last_raw_y=stroke.first_y;
            last_touch_us=now;manual_night_until_us=now+300000000;
            bool waking=asleep;
            if(waking){asleep=false;esp_lcd_panel_disp_on_off(panel,true);}
            feedback_x=start_x;feedback_y=start_y;feedback_until_us=now+500000;
            feedback_page=page;feedback_overlay=overlay;
            /* Wake has immediate visual receipt but never sends an accidental ping. */
            if(waking){snprintf(action_status,sizeof(action_status),"Awake - tap a button");press_consumed=true;}
            else if(touch_test){press_consumed=true;}
            else if(!calibration_stage&&screen_mode!=SCREEN_MESSAGE&&buoy_press_target(page,overlay,start_x,start_y)){
                int target=buoy_action_target(overlay==2,start_x,start_y);
                if(overlay==2){
                    if(buoy_alarm_back(start_x,start_y))overlay=0;
                    else if(target>=0){alarm_minute=buoy_alarm_step(alarm_minute,target==0?-60:target==1?60:target==2?-5:5);save_alarm();}
                    else if(buoy_alarm_toggle(start_x,start_y)){alarm_enabled=!alarm_enabled;save_alarm();}
                }else if(target<2)pending_action=target+1;
                else if(target==2){overlay=1;content_page[1]=0;pending_action=3;}
                else overlay=2;
                press_consumed=true;
            }
            dirty=true;xSemaphoreGive(lock);continue;
        }
        if(press_consumed||touch_test){press_consumed=false;++touch_releases;dirty=true;xSemaphoreGive(lock);continue;}
        feedback_page=page;feedback_overlay=overlay;
        ++touch_releases;
        last_raw_x=stroke.peak_x;last_raw_y=stroke.peak_y;
        last_touch_us=now;manual_night_until_us=now+300000000;
        if(asleep){asleep=false;esp_lcd_panel_disp_on_off(panel,true);}
        else if(calibration_stage){
            pending_action=0;
            if(calibration_stage==1){affine_valid=false;cal_x0=last_raw_x;cal_y0=last_raw_y;calibration_stage=2;}
            else if(calibration_stage==2){
                cal_x1=last_raw_x;cal_y1=last_raw_y;
                if(abs(cal_x1-cal_x0)<500||abs(cal_y1-cal_y0)<500)calibration_stage=1;
                else calibration_stage=3;
            }else{
                cal_x2=last_raw_x;cal_y2=last_raw_y;
                affine_valid=buoy_affine_fit(&affine,cal_x0,cal_y0,cal_x1,cal_y1,cal_x2,cal_y2);
                if(!affine_valid)calibration_stage=1;
                else{touch_calibrated=true;calibration_stage=0;page=0;overlay=0;save_calibration();}
            }
        }else{
            touch_x=map_x(last_raw_x,last_raw_y);
            touch_y=map_y(last_raw_x,last_raw_y);
            int end_x=map_x(stroke.last_x,stroke.last_y);
            int end_y=map_y(stroke.last_x,stroke.last_y);
            buoy_gesture_t gesture=buoy_gesture(end_x-start_x,end_y-start_y,excursion,stroke.ended_us-stroke.started_us);
            int target=buoy_action_target(overlay==2,touch_x,touch_y);
            if(screen_mode==SCREEN_MESSAGE){
                if(gesture==BUOY_TAP||gesture==BUOY_UP||gesture==BUOY_LEFT)advance_message(now);
                else if((gesture==BUOY_DOWN||gesture==BUOY_RIGHT)&&message_page){--message_page;message_started_us=now;}
            }else if(gesture==BUOY_UP||gesture==BUOY_DOWN){
                if(page==1||overlay==1){int which=overlay==1?1:0;
                    if(gesture==BUOY_UP&&content_page[which]+1<content_pages[which])++content_page[which];
                    if(gesture==BUOY_DOWN&&content_page[which])--content_page[which];
                }
            }else if(gesture==BUOY_LEFT||gesture==BUOY_RIGHT){
                overlay=0;page=(page+(gesture==BUOY_LEFT?1:2))%3;if(page==1)pending_action=4;
            }else if(gesture==BUOY_TAP){
                if((page==1||overlay==1)&&touch_y>=190){
                    int which=overlay==1?1:0;
                    if(touch_x>=160&&content_page[which]+1<content_pages[which])++content_page[which];
                    else if(touch_x<160&&content_page[which])--content_page[which];
                }else if(page==2&&overlay==2){
                    /* Controls only activate on down, never by dragging into one. */
                    if(touch_x<24||touch_x>=296)overlay=0;
                }else if(overlay==1)overlay=0;
                else if(page==2&&target>=0){ /* Already handled on press. */ }
                else{page=buoy_next_page(page,touch_x);if(page==1)pending_action=4;}
            } /* Other drags and long holds deliberately do nothing. */
        }
        if(!calibration_stage){
            feedback_x=map_x(last_raw_x,last_raw_y);
            feedback_y=map_y(last_raw_x,last_raw_y);
            feedback_until_us=now+300000;
        }
        dirty=true;
        ESP_LOGI(TAG,"touch page=%d pixel=%d,%d raw=%u,%u asleep=%d",page,touch_x,touch_y,last_raw_x,last_raw_y,asleep);
        xSemaphoreGive(lock);
    }
}
static int serial_page(int argc,char **argv)
{
    if(argc!=2)return 1;
    int target=!strcmp(argv[1],"home")?0:!strcmp(argv[1],"today")?1:!strcmp(argv[1],"actions")?2:-1;
    if(target<0)return 1;
    xSemaphoreTake(lock,portMAX_DELAY);page=target;overlay=0;dirty=true;xSemaphoreGive(lock);return 0;
}

void claw_box_display_status(const char *text)
{
    if (!lock) return;
    xSemaphoreTake(lock, portMAX_DELAY);
    snprintf(status_text, sizeof(status_text), "%s", text);
    if(screen_mode!=SCREEN_MESSAGE)screen_mode=SCREEN_CONNECTING;
    agenda_offline=weather_offline=true;
    dirty = true;
    xSemaphoreGive(lock);
}
void claw_box_display_connected(void)
{
    if (!lock) return;
    xSemaphoreTake(lock, portMAX_DELAY);
    if(screen_mode!=SCREEN_MESSAGE)screen_mode=SCREEN_LOGO;
    dirty = true;
    xSemaphoreGive(lock);
}
void claw_box_display_sensor(double t, double h)
{
    if (!lock) return;
    xSemaphoreTake(lock, portMAX_DELAY);
    temperature = t; humidity = h; sensor_valid = true;
    sensor_time_us = esp_timer_get_time();
    dirty = true;
    xSemaphoreGive(lock);
}
static bool flushed(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *event, void *ctx)
{
    (void)io; (void)event;
    lv_display_flush_ready(ctx);
    return false;
}
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    lv_draw_sw_rgb565_swap(pixels, (area->x2-area->x1+1)*(area->y2-area->y1+1));
    esp_err_t err = esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1, area->x2+1, area->y2+1, pixels);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "draw failed: %s", esp_err_to_name(err));
        lv_display_flush_ready(display);
    }
}
/* Independent, small retained overlay: immediate contact feedback, not success.
 * Visual receipt is separate from action success; wake contacts remain consumed. */
static void render_touch_feedback(int64_t now)
{
    unsigned contact=atomic_load(&contact_preview);
    bool enabled=!asleep&&!calibration_stage;
    if(touch_test&&!asleep){
        claw_box_touch_diagnostics_t d;claw_box_touch_get_diagnostics(&d);
        lv_obj_remove_flag(test_panel,LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text_fmt(test_label,"TOUCH SELF TEST\nRaw X %u  Y %u\nPressure %u (min %d)\nPixel %d,%d\nNo actions sent\nExit: display.update touchTest=false",d.rawX,d.rawY,d.pressure,CLAW_TOUCH_PRESSURE_THRESHOLD,map_x(d.rawX,d.rawY),map_y(d.rawX,d.rawY));
    }else lv_obj_add_flag(test_panel,LV_OBJ_FLAG_HIDDEN);
    bool down=enabled&&(contact&(1U<<24));
    if(down){
        feedback_x=map_x((contact>>12)&4095,contact&4095);
        feedback_y=map_y((contact>>12)&4095,contact&4095);
        feedback_until_us=now+300000;
        feedback_page=page;feedback_overlay=overlay;
    }
    bool visible=enabled&&now<feedback_until_us;
    if(visible){
        lv_obj_set_pos(touch_halo,feedback_x-9,feedback_y-9);
        lv_obj_remove_flag(touch_halo,LV_OBJ_FLAG_HIDDEN);
    }else lv_obj_add_flag(touch_halo,LV_OBJ_FLAG_HIDDEN);
    /* Only tint a matching screen's target; never a newly navigated-to tile. */
    bool same=visible&&feedback_page==page&&feedback_overlay==overlay&&screen_mode!=SCREEN_MESSAGE;
    int target=same&&page==2?buoy_action_target(overlay==2,feedback_x,feedback_y):-1;
    static int previous_target=-2;
    if(target!=previous_target)for(int i=0;i<4;++i){
        lv_obj_set_style_bg_color(action_tiles[i],lv_color_hex(i==target?BUOY_PRESSED:BUOY_SURFACE),0);
        lv_obj_set_style_text_color(action_tiles[i],lv_color_hex(i==target?BUOY_BG:BUOY_INK),0);
    }
    previous_target=target;
    bool toggle=same&&page==2&&overlay==2&&buoy_alarm_toggle(feedback_x,feedback_y);
    static int previous_toggle=-1;
    if(toggle!=previous_toggle){
        lv_obj_set_style_bg_color(alarm_toggle,lv_color_hex(toggle?BUOY_PRESSED:BUOY_SURFACE),0);
        lv_obj_set_style_text_color(alarm_toggle,lv_color_hex(toggle?BUOY_BG:BUOY_INK),0);
        previous_toggle=toggle;
    }
    bool back=same&&page==2&&overlay==2&&buoy_alarm_back(feedback_x,feedback_y);
    static int previous_back=-1;
    if(back!=previous_back){lv_obj_set_style_bg_color(back_button,lv_color_hex(back?BUOY_PRESSED:BUOY_ACCENT),0);previous_back=back;}
}
static void ui_task(void *arg)
{
    (void)arg;
    int last_second=-1;
    for (;;) {
        poll_touch();
        const int64_t now_us=esp_timer_get_time();
        time_t now=time(NULL);struct tm t;localtime_r(&now,&t);
        bool time_valid=now>1700000000;
        xSemaphoreTake(lock,portMAX_DELAY);
        if(wake_requested){wake_requested=false;asleep=false;last_touch_us=now_us;manual_night_until_us=now_us+300000000;esp_lcd_panel_disp_on_off(panel,true);dirty=true;}
        if(t.tm_sec!=last_second) {
            /* Full content refresh only when clock minute, animation or model
               changes; do not invalidate every screen on each idle second. */
            static int last_minute=-1;
            last_second=t.tm_sec;
            if(t.tm_min!=last_minute||t.tm_sec<4){dirty=true;last_minute=t.tm_min;}
        }
        if(time_valid && alarm_enabled && t.tm_hour*60+t.tm_min==alarm_minute && last_alarm_day!=t.tm_yday) {
            last_alarm_day=t.tm_yday;asleep=false;last_touch_us=now_us;manual_night_until_us=now_us+300000000;page=0;overlay=0;
            esp_lcd_panel_disp_on_off(panel,true);dirty=true;
        }
        bool night=time_valid && buoy_dark_hour(t.tm_hour);
        if(!asleep && ((now_us-last_touch_us>=300000000) || (night && now_us>=manual_night_until_us))) {
            /* Keep LCD scanning: DISPOFF leaves this always-lit panel white.
               An opaque LVGL top-layer cover instead writes black pixels. */
            asleep=true;
        }
        /* Pause each unread page's dwell timer while asleep. */
        if(asleep&&screen_mode==SCREEN_MESSAGE)message_started_us=0;
        if(screen_mode==SCREEN_MESSAGE && !asleep) {
            if(!message_started_us)message_started_us=now_us;
            if(now_us-message_started_us>6000000)advance_message(now_us);
        }
        if(dirty && !asleep) {
            lv_obj_add_flag(logo,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(status_label,LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(message_panel,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(telemetry,LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(weather_graphic,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(content_label,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(content_header,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(content_footer,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(clock_label,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(day_label,LV_OBJ_FLAG_HIDDEN);
            for(int i=0;i<4;++i)lv_obj_add_flag(action_tiles[i],LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(cal_dot,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(cal_label,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(alarm_toggle,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(back_button,LV_OBJ_FLAG_HIDDEN);
            if(calibration_stage){
                lv_obj_remove_flag(cal_dot,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(cal_label,LV_OBJ_FLAG_HIDDEN);
                int x=calibration_stage==1?40:280,y=calibration_stage==2?190:40;
                lv_obj_set_pos(cal_dot,x-6,y-6);lv_obj_set_pos(cal_label,50,calibration_stage==3?155:85);
                lv_label_set_text_fmt(cal_label,"Touch calibration %d/3\nTap the teal dot\nPings disabled",calibration_stage);
            }else if(screen_mode==SCREEN_MESSAGE) {
                lv_obj_remove_flag(message_panel,LV_OBJ_FLAG_HIDDEN);
                lv_label_set_text_fmt(from_label,"From: %s",sender_text);
                lv_obj_set_style_text_font(reply_label,&lv_font_montserrat_14,0);
                message_pages=buoy_text_page(reply_text,message_page,260,6,glyph_width,page_text,sizeof(page_text));
                lv_label_set_text(reply_label,page_text);
                lv_label_set_text_fmt(message_footer,"%u/%u  Tap for next",message_page+1,message_pages);
            } else if(page==0) {
                page_text[0]=0;
                lv_obj_remove_flag(clock_label,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(day_label,LV_OBJ_FLAG_HIDDEN);
                char text[64];if(time_valid)strftime(text,sizeof(text),"%I:%M %p",&t);else snprintf(text,sizeof(text),"--:--");
                lv_label_set_text(clock_label,text);
                if(time_valid)strftime(text,sizeof(text),"%A, %b %d",&t);else snprintf(text,sizeof(text),"Synchronizing clock...");
                lv_label_set_text(day_label,text);
                /* Preserve native flash-resident mascot, animate position only. */
                int anim=(now_us/1000000)%60;
                lv_obj_remove_flag(logo,LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_pos(logo,212+(anim<3?(anim%2?3:-3):0),52);
                lv_obj_remove_flag(telemetry,LV_OBJ_FLAG_HIDDEN);
                if(screen_mode==SCREEN_CONNECTING){lv_label_set_text(status_label,status_text);lv_obj_remove_flag(status_label,LV_OBJ_FLAG_HIDDEN);}
            } else if(page==1 || overlay==1) {
                lv_obj_remove_flag(content_label,LV_OBJ_FLAG_HIDDEN);
                int which=overlay==1?1:0;
                lv_obj_set_width(content_header,which?210:264);
                if(which){rendered_weather_kind=weather_kind;lv_obj_remove_flag(weather_graphic,LV_OBJ_FLAG_HIDDEN);lv_obj_invalidate(weather_graphic);}
                char date[11],stamp[24];
                strftime(date,sizeof(date),"%Y-%m-%d",&t);
                const char *text=which?weather_text:agenda_text;
                bool expired=!which&&buoy_agenda_expired(time_valid,agenda_date,date);
                bool old=buoy_content_stale(now,which?weather_updated:agenda_updated,which?10800:3600);
                bool offline=which?weather_offline:agenda_offline;
                if(expired)text="No agenda for today. Refresh required.";
                time_t updated=which?weather_updated:agenda_updated;struct tm observed;
                if(updated>0){localtime_r(&updated,&observed);strftime(stamp,sizeof(stamp),"%m/%d %H:%M",&observed);}else snprintf(stamp,sizeof(stamp),"not loaded");
                lv_obj_remove_flag(content_header,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(content_footer,LV_OBJ_FLAG_HIDDEN);
                lv_label_set_text_fmt(content_header,"%s %s\n%s%s",which?"WEATHER":agenda_date,stamp,expired?"EXPIRED":old?"STALE":"Current",offline?" / offline":"");
                content_pages[which]=buoy_text_page(text,content_page[which],260,6,glyph_width,page_text,sizeof(page_text));
                if(content_page[which]>=content_pages[which]){content_page[which]=0;buoy_text_page(text,0,260,6,glyph_width,page_text,sizeof(page_text));}
                lv_label_set_text(content_label,page_text);
                lv_label_set_text_fmt(content_footer,"< Prev   %u/%u   Next >\nSwipe up/down for more",content_page[which]+1,content_pages[which]);
            } else {
                for(int i=0;i<4;++i) {
                    /* Visual card inset; original generous hit rectangles stay unchanged. */
                    lv_obj_set_pos(action_tiles[i],38+(i%2)*132,buoy_tile_top(overlay==2,i)+7);
                    lv_obj_set_size(action_tiles[i],110,buoy_tile_height(overlay==2)-14);
                    lv_obj_remove_flag(action_tiles[i],LV_OBJ_FLAG_HIDDEN);
                    lv_obj_center(action_labels[i]);
                    char test_text[48];
                    buoy_test_label(test_text,sizeof(test_text),action_names[i]);
                    lv_label_set_text(action_labels[i],overlay==2?(const char*[]){"Hour -","Hour +","5 min -","5 min +"}[i]:(ping_test_mode&&i<2?test_text:action_names[i]));
                }
                lv_obj_remove_flag(status_label,LV_OBJ_FLAG_HIDDEN);
                lv_label_set_text_fmt(status_label,"Alarm %02d:%02d  %s%s%s",alarm_minute/60,alarm_minute%60,alarm_enabled?"ON":"OFF",action_status[0]?"\n":"",action_status);
            }
            if(page==2&&overlay==2&&!calibration_stage&&screen_mode!=SCREEN_MESSAGE){lv_obj_add_flag(status_label,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(alarm_toggle,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(back_button,LV_OBJ_FLAG_HIDDEN);lv_label_set_text_fmt(alarm_toggle_label,"Alarm %02d:%02d  %s",alarm_minute/60,alarm_minute%60,alarm_enabled?"ON":"OFF");lv_obj_center(alarm_toggle_label);}
            if(sensor_valid) {
                bool stale=buoy_sensor_stale(now_us,sensor_time_us);
                buoy_format_sensor(rendered_temperature,sizeof(rendered_temperature),rendered_humidity,sizeof(rendered_humidity),temperature,humidity,stale);
                lv_label_set_text(temperature_label,rendered_temperature);
                lv_label_set_text(humidity_label,rendered_humidity);
                lv_obj_set_style_text_font(temperature_label,stale?&lv_font_montserrat_14:&lv_font_montserrat_24,0);
            } else {lv_label_set_text(temperature_label,"--.- C");lv_label_set_text(humidity_label,"--% RH");}
            dirty=false;
        }
        if(asleep)lv_obj_remove_flag(sleep_cover,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(sleep_cover,LV_OBJ_FLAG_HIDDEN);
        render_touch_feedback(now_us);
        backlight_apply();
        int action=pending_action;pending_action=0;
        xSemaphoreGive(lock);
        if(action)request_action(action);
        lv_timer_handler();vTaskDelay(pdMS_TO_TICKS(10));
    }
}
static esp_err_t set_reply(const char *text, const char *sender)
{
    if (!ready) return ESP_ERR_INVALID_STATE;
    if (!text || strlen(text) > 1024 || !sender || !sender[0] || strlen(sender) > 63) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(lock, portMAX_DELAY);
    if(screen_mode==SCREEN_MESSAGE){
        if(queued_count==3){xSemaphoreGive(lock);return ESP_ERR_NO_MEM;}
        snprintf(queued_text[queued_count],sizeof(queued_text[0]),"%s",text);
        snprintf(queued_sender[queued_count],sizeof(queued_sender[0]),"%s",sender);
        ++queued_count;xSemaphoreGive(lock);return ESP_OK;
    }
    snprintf(reply_text, sizeof(reply_text), "%s", text);
    snprintf(sender_text, sizeof(sender_text), "%s", sender);
    message_started_us = 0;
    message_page=0;
    screen_mode = SCREEN_MESSAGE;
    dirty = true;
    xSemaphoreGive(lock);
    ESP_LOGI(TAG, "text queued (%u bytes); visual confirmation still required", (unsigned)strlen(text));
    return ESP_OK;
}
static int serial_text(int argc, char **argv)
{
    if (argc != 2) { printf("usage: display-text \"agent reply\"\n"); return 1; }
    return set_reply(argv[1], "Auggie") == ESP_OK ? 0 : 1;
}
static esp_err_t text_command(esp_openclaw_node_handle_t node, void *ctx,
    const char *json, size_t len, char **out, esp_openclaw_node_error_t *error)
{
    (void)node; (void)ctx; (void)len;
    cJSON *params = NULL;
    esp_err_t err = esp_openclaw_node_example_parse_json_params(json, &params, error);
    if (err != ESP_OK) return err;
    cJSON *text = cJSON_GetObjectItemCaseSensitive(params, "text");
    cJSON *from = cJSON_GetObjectItemCaseSensitive(params, "from");
    err = cJSON_IsString(text) && (from == NULL || cJSON_IsString(from))
        ? set_reply(text->valuestring, from ? from->valuestring : "Auggie") : ESP_ERR_INVALID_ARG;
    cJSON_Delete(params);
    if (err != ESP_OK) { error->message = "display unavailable or invalid text/from (max 1024/63 bytes)"; return err; }
    cJSON *payload = cJSON_CreateObject();
    if (!payload) return ESP_ERR_NO_MEM;
    cJSON_AddBoolToObject(payload, "queued", true);
    cJSON_AddNumberToObject(payload, "flashMs", 0);
    cJSON_AddNumberToObject(payload, "holdMs", 6000);
    return esp_openclaw_node_example_take_json_payload(payload, out);
}

static bool valid_epoch(const cJSON *v)
{
    return !v||(cJSON_IsNumber(v)&&v->valuedouble>=1700000000&&v->valuedouble<=4102444800.0&&v->valuedouble==(double)(int64_t)v->valuedouble);
}
static esp_err_t update_command(esp_openclaw_node_handle_t node,void *ctx,const char *json,size_t len,char **out,esp_openclaw_node_error_t *error)
{
    (void)node;(void)ctx;(void)len;
    cJSON *p=NULL;esp_err_t err=esp_openclaw_node_example_parse_json_params(json,&p,error);if(err!=ESP_OK)return err;
    cJSON *a=cJSON_GetObjectItemCaseSensitive(p,"agenda"),*w=cJSON_GetObjectItemCaseSensitive(p,"weather"),*cal=cJSON_GetObjectItemCaseSensitive(p,"calibrate"),*corners=cJSON_GetObjectItemCaseSensitive(p,"calibrationCorners");
    cJSON *touchtest=cJSON_GetObjectItemCaseSensitive(p,"touchTest");
    cJSON *brightness=cJSON_GetObjectItemCaseSensitive(p,"brightnessPercent");
    cJSON *kind=cJSON_GetObjectItemCaseSensitive(p,"weatherCondition");
    cJSON *date=cJSON_GetObjectItemCaseSensitive(p,"agendaDate"),*at=cJSON_GetObjectItemCaseSensitive(p,"agendaUpdatedAt"),*wt=cJSON_GetObjectItemCaseSensitive(p,"weatherUpdatedAt");
    cJSON *aa=cJSON_GetObjectItemCaseSensitive(p,"agendaAvailable"),*wa=cJSON_GetObjectItemCaseSensitive(p,"weatherAvailable"),*test=cJSON_GetObjectItemCaseSensitive(p,"pingTestMode"),*wake=cJSON_GetObjectItemCaseSensitive(p,"wake");
    cJSON *pg=cJSON_GetObjectItemCaseSensitive(p,"page"),*ov=cJSON_GetObjectItemCaseSensitive(p,"overlay"),*cp=cJSON_GetObjectItemCaseSensitive(p,"contentPage"),*next=cJSON_GetObjectItemCaseSensitive(p,"messageNext");
    if((touchtest&&!cJSON_IsBool(touchtest))||(brightness&&(!cJSON_IsNumber(brightness)||brightness->valuedouble!=brightness->valueint||brightness->valueint<1||brightness->valueint>100))||
       (a&&(!cJSON_IsString(a)||strlen(a->valuestring)>1024))||(w&&(!cJSON_IsString(w)||strlen(w->valuestring)>512))||
       (kind&&(!w||!cJSON_IsString(kind)||buoy_weather_parse(kind->valuestring)<0))||
       (date&&(!a||!cJSON_IsString(date)||!buoy_valid_date(date->valuestring)))||
       !valid_epoch(at)||!valid_epoch(wt)||(at&&!a)||(wt&&!w)||
       (aa&&!cJSON_IsBool(aa))||(wa&&!cJSON_IsBool(wa))||(test&&!cJSON_IsBool(test))||(wake&&!cJSON_IsBool(wake))||(cal&&!cJSON_IsBool(cal))||(next&&!cJSON_IsBool(next))||
       (pg&&(!cJSON_IsNumber(pg)||pg->valuedouble!=pg->valueint||pg->valueint<1||pg->valueint>3))||
       (ov&&(!cJSON_IsNumber(ov)||ov->valuedouble!=ov->valueint||ov->valueint<0||ov->valueint>2))||
       (cp&&(!cJSON_IsNumber(cp)||cp->valuedouble!=cp->valueint||cp->valueint<0||cp->valueint>200))){cJSON_Delete(p);return ESP_ERR_INVALID_ARG;}
    time_t now=time(NULL);
    if(now>1700000000&&((at&&at->valuedouble>now+300)||(wt&&wt->valuedouble>now+300))){cJSON_Delete(p);return ESP_ERR_INVALID_ARG;}
    int32_t values[4]={0};
    if(corners){
        if(!cJSON_IsArray(corners)||cJSON_GetArraySize(corners)!=4){cJSON_Delete(p);return ESP_ERR_INVALID_ARG;}
        for(int i=0;i<4;++i){cJSON *v=cJSON_GetArrayItem(corners,i);if(!cJSON_IsNumber(v)||v->valuedouble!=v->valueint||v->valueint<100||v->valueint>4000){cJSON_Delete(p);return ESP_ERR_INVALID_ARG;}values[i]=v->valueint;}
        if(abs(values[2]-values[0])<500||abs(values[3]-values[1])<500){cJSON_Delete(p);return ESP_ERR_INVALID_ARG;}
    }
    xSemaphoreTake(lock,portMAX_DELAY);
    if(touchtest){touch_test=cJSON_IsTrue(touchtest);pending_action=0;wake_requested=true;}
    if(brightness)backlight_percent=(unsigned)brightness->valueint;
    if(corners){affine_valid=false;cal_x0=values[0];cal_y0=values[1];cal_x1=values[2];cal_y1=values[3];touch_calibrated=true;calibration_stage=0;pending_action=0;page=0;overlay=0;wake_requested=true;save_calibration();}
    if(cJSON_IsTrue(cal)){touch_test=false;touch_calibrated=false;calibration_stage=1;pending_action=0;ping_test_mode=true;wake_requested=true;}
    if(a){
        snprintf(agenda_text,sizeof(agenda_text),"%s",a->valuestring);
        agenda_updated=at?(time_t)at->valuedouble:now;content_page[0]=0;agenda_offline=aa?!cJSON_IsTrue(aa):false;
        if(date)snprintf(agenda_date,sizeof(agenda_date),"%s",date->valuestring);
        else if(now>1700000000){struct tm t;localtime_r(&now,&t);strftime(agenda_date,sizeof(agenda_date),"%Y-%m-%d",&t);}
        else agenda_date[0]=0;
    }
    if(kind)weather_kind=buoy_weather_parse(kind->valuestring);
    else if(w)weather_kind=BUOY_WEATHER_UNKNOWN;
    if(w){snprintf(weather_text,sizeof(weather_text),"%s",w->valuestring);weather_updated=wt?(time_t)wt->valuedouble:now;content_page[1]=0;weather_offline=wa?!cJSON_IsTrue(wa):false;}
    if(aa){agenda_offline=!cJSON_IsTrue(aa);}
    if(wa){weather_offline=!cJSON_IsTrue(wa);}
    if(test)ping_test_mode=cJSON_IsTrue(test);
    if(test||w||wa||cJSON_IsTrue(cal))save_integration_state();
    if(pg){page=pg->valueint-1;overlay=0;}if(ov)overlay=ov->valueint;
    if(cp)content_page[overlay==1?1:0]=cp->valueint;
    if(cJSON_IsTrue(next)&&screen_mode==SCREEN_MESSAGE)advance_message(esp_timer_get_time());
    if(cJSON_IsTrue(wake))wake_requested=true;
    dirty=true;xSemaphoreGive(lock);cJSON_Delete(p);
    *out=strdup("{\"updated\":true}");return *out?ESP_OK:ESP_ERR_NO_MEM;
}
static esp_err_t state_command(esp_openclaw_node_handle_t node,void *ctx,const char *json,size_t len,char **out,esp_openclaw_node_error_t *error)
{
    (void)node;(void)ctx;(void)json;(void)len;(void)error;
    cJSON *p=cJSON_CreateObject();if(!p)return ESP_ERR_NO_MEM;
    xSemaphoreTake(lock,portMAX_DELAY);
    cJSON_AddStringToObject(p,"firmwareRelease","claw-buoy-v9-upstream-touch");
    cJSON_AddStringToObject(p,"uiTheme","warm-terminal");
    cJSON_AddBoolToObject(p,"touchTest",touch_test);cJSON_AddBoolToObject(p,"calibrationAffine",affine_valid);
    cJSON_AddNumberToObject(p,"calX2",cal_x2);cJSON_AddNumberToObject(p,"calY2",cal_y2);
    cJSON_AddNumberToObject(p,"touchPresses",touch_presses);cJSON_AddNumberToObject(p,"touchPressureThreshold",CLAW_TOUCH_PRESSURE_THRESHOLD);
    cJSON_AddStringToObject(p,"touchFeedback","contact-halo-and-control-tint");
    cJSON_AddNumberToObject(p,"backlightGpio",CLAW_BACKLIGHT_GPIO);
    cJSON_AddBoolToObject(p,"backlightPwmReady",backlight_ready);
    cJSON_AddNumberToObject(p,"brightnessPercent",backlight_percent);
    cJSON_AddNumberToObject(p,"backlightAppliedPercent",backlight_applied_percent);
    cJSON_AddStringToObject(p,"weatherCondition",buoy_weather_name(weather_kind));
    cJSON_AddBoolToObject(p,"pingTestMode",ping_test_mode);
    cJSON_AddStringToObject(p,"temperatureLabel",rendered_temperature);cJSON_AddStringToObject(p,"humidityLabel",rendered_humidity);
    cJSON_AddBoolToObject(p,"sensorStale",!sensor_valid||buoy_sensor_stale(esp_timer_get_time(),sensor_time_us));
    cJSON_AddNumberToObject(p,"sensorAgeSeconds",(esp_timer_get_time()-sensor_time_us)/1000000);
    cJSON_AddNumberToObject(p,"agendaUpdatedAt",agenda_updated);cJSON_AddNumberToObject(p,"weatherUpdatedAt",weather_updated);
    cJSON_AddStringToObject(p,"agendaDate",agenda_date);
    char today[11];time_t now=time(NULL);struct tm t;localtime_r(&now,&t);strftime(today,sizeof(today),"%Y-%m-%d",&t);
    cJSON_AddBoolToObject(p,"agendaExpired",buoy_agenda_expired(now>1700000000,agenda_date,today));
    cJSON_AddBoolToObject(p,"agendaStale",buoy_content_stale(now,agenda_updated,3600));
    cJSON_AddBoolToObject(p,"weatherStale",buoy_content_stale(now,weather_updated,10800));
    cJSON_AddBoolToObject(p,"agendaOffline",agenda_offline);cJSON_AddBoolToObject(p,"weatherOffline",weather_offline);
    cJSON_AddNumberToObject(p,"contentPage",content_page[overlay==1?1:0]+1);cJSON_AddNumberToObject(p,"contentPages",content_pages[overlay==1?1:0]);
    cJSON_AddNumberToObject(p,"messagePage",message_page+1);cJSON_AddNumberToObject(p,"messagePages",message_pages);
    cJSON_AddBoolToObject(p,"messageVisible",screen_mode==SCREEN_MESSAGE);cJSON_AddStringToObject(p,"messageText",reply_text);
    cJSON_AddStringToObject(p,"pageText",page_text);
    cJSON_AddNumberToObject(p,"page",page+1);cJSON_AddBoolToObject(p,"asleep",asleep);cJSON_AddStringToObject(p,"sleepDisplayMode","black-pixels-pwm-off");cJSON_AddNumberToObject(p,"idleSeconds",(esp_timer_get_time()-last_touch_us)/1000000);cJSON_AddBoolToObject(p,"alarmEnabled",alarm_enabled);
    cJSON_AddNumberToObject(p,"alarmMinute",alarm_minute);cJSON_AddBoolToObject(p,"clockSynced",time(NULL)>1700000000);
    cJSON_AddBoolToObject(p,"sensorValid",sensor_valid);
    cJSON_AddNumberToObject(p,"temperatureC",temperature);
    cJSON_AddStringToObject(p,"actionStatus",action_status);
    cJSON_AddNumberToObject(p,"queuedNotifications",queued_count);
    cJSON_AddBoolToObject(p,"touchCalibrated",touch_calibrated);cJSON_AddNumberToObject(p,"calibrationStage",calibration_stage);cJSON_AddNumberToObject(p,"overlay",overlay);
    cJSON_AddNumberToObject(p,"calX0",cal_x0);cJSON_AddNumberToObject(p,"calY0",cal_y0);cJSON_AddNumberToObject(p,"calX1",cal_x1);cJSON_AddNumberToObject(p,"calY1",cal_y1);
    claw_box_touch_diagnostics_t diagnostics;claw_box_touch_get_diagnostics(&diagnostics);
    cJSON_AddNumberToObject(p,"touchDroppedStrokes",atomic_load(&dropped_strokes));
    cJSON_AddNumberToObject(p,"touchPolls",diagnostics.polls);cJSON_AddBoolToObject(p,"touchInitialized",diagnostics.initialized);
    cJSON_AddNumberToObject(p,"touchIrqLevel",diagnostics.irqLevel);cJSON_AddNumberToObject(p,"touchIrqHighSamples",diagnostics.irqHighSamples);cJSON_AddNumberToObject(p,"touchIrqLowSamples",diagnostics.irqLowSamples);
    cJSON_AddNumberToObject(p,"touchSpiErrors",diagnostics.spiErrors);cJSON_AddNumberToObject(p,"touchReleases",touch_releases);
    cJSON_AddNumberToObject(p,"touchRawX",diagnostics.rawX);cJSON_AddNumberToObject(p,"touchRawY",diagnostics.rawY);
    cJSON_AddNumberToObject(p,"touchPenSamples",diagnostics.penSamples);cJSON_AddNumberToObject(p,"touchValidSamples",diagnostics.validSamples);
    cJSON_AddNumberToObject(p,"touchLowPressureSamples",diagnostics.lowPressureSamples);cJSON_AddNumberToObject(p,"touchBadRawSamples",diagnostics.badRawSamples);cJSON_AddNumberToObject(p,"touchPressure",diagnostics.pressure);
    cJSON_AddNumberToObject(p,"rawX",last_raw_x);cJSON_AddNumberToObject(p,"rawY",last_raw_y);
    cJSON_AddNumberToObject(p,"pixelX",touch_x);cJSON_AddNumberToObject(p,"pixelY",touch_y);
    cJSON_AddStringToObject(p,"agenda",agenda_text);cJSON_AddStringToObject(p,"weather",weather_text);
    xSemaphoreGive(lock);return esp_openclaw_node_example_take_json_payload(p,out);
}

#else
void claw_box_display_status(const char *text) { (void)text; }
void claw_box_display_connected(void) {}
void claw_box_display_sensor(double t, double h) { (void)t; (void)h; }
#endif

esp_err_t claw_box_display_start(esp_openclaw_node_handle_t node)
{
#ifndef CONFIG_CLAW_BOX_DISPLAY
    (void)node;
    return ESP_OK;
#else
    /* All outputs avoid boot strapping, flash GPIO6-11, UART0, and AHT20 21/22.
       XPT2046 uses its own SPI3 bus. LED control moves from 3V3 to GPIO17. */
    esp_err_t pwm_err=backlight_init();
    if(pwm_err!=ESP_OK)return pwm_err;
    gpio_config_t cs = {.pin_bit_mask = (1ULL << CLAW_TOUCH_CS) | (1ULL << CLAW_TFT_CS), .mode = GPIO_MODE_OUTPUT};
    esp_err_t err = gpio_config(&cs);
    if (err != ESP_OK) return err;
    gpio_set_level(CLAW_TOUCH_CS, 1);
    gpio_set_level(CLAW_TFT_CS, 1);
    spi_bus_config_t bus = {.sclk_io_num=CLAW_TFT_SCK, .mosi_io_num=CLAW_TFT_MOSI,
        .miso_io_num=-1, .quadwp_io_num=-1, .quadhd_io_num=-1, .max_transfer_sz=CLAW_DRAW_BUFFER_BYTES};
    err = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) return err;
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = {.dc_gpio_num=CLAW_TFT_DC, .cs_gpio_num=CLAW_TFT_CS,
        .pclk_hz=10000000, .lcd_cmd_bits=8, .lcd_param_bits=8, .spi_mode=0, .trans_queue_depth=4};
    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_cfg, &io);
    if (err != ESP_OK) goto fail_bus;
    esp_lcd_panel_dev_config_t cfg = {.reset_gpio_num=CLAW_TFT_RESET,
        .rgb_ele_order=LCD_RGB_ELEMENT_ORDER_BGR, .bits_per_pixel=16};
    err = esp_lcd_new_panel_ili9341(io, &cfg, &panel);
    if (err != ESP_OK) goto fail_io;
#define PANEL_STEP(call) do { err = (call); if (err != ESP_OK) goto fail_panel; } while (0)
    PANEL_STEP(esp_lcd_panel_reset(panel));
    PANEL_STEP(esp_lcd_panel_init(panel));
    PANEL_STEP(esp_lcd_panel_swap_xy(panel, true));
    PANEL_STEP(esp_lcd_panel_mirror(panel, false, false));
    PANEL_STEP(esp_lcd_panel_disp_on_off(panel, true));
    void *buffer = heap_caps_malloc(CLAW_DRAW_BUFFER_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    lock = xSemaphoreCreateMutex();
    if (!buffer || !lock) { free(buffer); if (lock) vSemaphoreDelete(lock); lock=NULL; err=ESP_ERR_NO_MEM; goto fail_panel; }
    lv_init();
    lv_tick_set_cb(ticks);
    lv_display_t *display = lv_display_create(320, 240);
    if (!display) { free(buffer); vSemaphoreDelete(lock); lock=NULL; err=ESP_ERR_NO_MEM; goto fail_panel; }
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, buffer, NULL, CLAW_DRAW_BUFFER_BYTES, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    esp_lcd_panel_io_callbacks_t callbacks = {.on_color_trans_done=flushed};
    err = esp_lcd_panel_io_register_event_callbacks(io, &callbacks, display);
    if (err != ESP_OK) goto fail_lvgl;
    /* Top-layer cover hides every page and notification, including the title.
       Touch is independently acquired; the first wake stroke stays consumed. */
    sleep_cover=lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(sleep_cover);
    lv_obj_set_pos(sleep_cover,0,0);lv_obj_set_size(sleep_cover,320,240);
    lv_obj_set_style_bg_color(sleep_cover,lv_color_hex(0x000000),0);
    lv_obj_set_style_bg_opa(sleep_cover,LV_OPA_COVER,0);
    lv_obj_remove_flag(sleep_cover,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(sleep_cover,LV_OBJ_FLAG_HIDDEN);
    test_panel=lv_obj_create(lv_layer_top());lv_obj_set_pos(test_panel,0,0);lv_obj_set_size(test_panel,320,240);
    lv_obj_set_style_bg_color(test_panel,lv_color_hex(BUOY_BG),0);lv_obj_set_style_bg_opa(test_panel,LV_OPA_COVER,0);
    test_label=lv_label_create(test_panel);lv_obj_set_width(test_label,290);lv_obj_set_style_text_color(test_label,lv_color_hex(BUOY_INK),0);
    lv_obj_set_style_text_font(test_label,&lv_font_montserrat_14,0);lv_obj_add_flag(test_panel,LV_OBJ_FLAG_HIDDEN);
    touch_halo=lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(touch_halo);lv_obj_set_size(touch_halo,18,18);
    lv_obj_set_style_radius(touch_halo,LV_RADIUS_CIRCLE,0);
    lv_obj_set_style_bg_color(touch_halo,lv_color_hex(BUOY_PRESSED),0);
    lv_obj_set_style_bg_opa(touch_halo,LV_OPA_50,0);
    lv_obj_set_style_border_color(touch_halo,lv_color_hex(BUOY_INK),0);
    lv_obj_set_style_border_width(touch_halo,2,0);
    lv_obj_remove_flag(touch_halo,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(touch_halo,LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(sleep_cover);
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(BUOY_BG), 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(BUOY_INK), 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "CLAW BUOY");
    lv_obj_set_pos(title, 12, 3);
    lv_obj_set_style_text_color(title, lv_color_hex(BUOY_ACCENT), 0);
    content_label=lv_label_create(screen);
    lv_obj_set_pos(content_label,28,78);lv_obj_set_size(content_label,264,108);
    content_header=lv_label_create(screen);lv_obj_set_pos(content_header,28,27);lv_obj_set_size(content_header,264,48);lv_obj_set_style_text_font(content_header,&lv_font_montserrat_14,0);
    content_footer=lv_label_create(screen);lv_obj_set_pos(content_footer,28,190);lv_obj_set_size(content_footer,264,48);lv_obj_set_style_text_font(content_footer,&lv_font_montserrat_14,0);
    lv_label_set_long_mode(content_label,LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(content_label,&lv_font_montserrat_14,0);
    weather_graphic=lv_obj_create(screen);lv_obj_set_pos(weather_graphic,248,24);lv_obj_set_size(weather_graphic,64,52);
    lv_obj_set_style_bg_opa(weather_graphic,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(weather_graphic,0,0);lv_obj_set_style_pad_all(weather_graphic,0,0);lv_obj_remove_flag(weather_graphic,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(weather_graphic,draw_weather_graphic,LV_EVENT_DRAW_MAIN,NULL);lv_obj_add_flag(weather_graphic,LV_OBJ_FLAG_HIDDEN);
    clock_label=lv_label_create(screen);lv_obj_set_pos(clock_label,24,72);
    lv_obj_set_style_text_font(clock_label,&lv_font_montserrat_24,0);
    day_label=lv_label_create(screen);lv_obj_set_pos(day_label,24,138);lv_obj_set_style_text_color(day_label,lv_color_hex(BUOY_MUTED),0);
    for(int i=0;i<4;++i) {
        action_tiles[i]=lv_obj_create(screen);lv_obj_set_pos(action_tiles[i],30+(i%2)*132,36+(i/2)*64);lv_obj_set_size(action_tiles[i],126,58);
        lv_obj_set_style_pad_all(action_tiles[i],0,0);
        lv_obj_set_style_radius(action_tiles[i],8,0);
        lv_obj_set_style_bg_color(action_tiles[i],lv_color_hex(BUOY_SURFACE),0);lv_obj_set_style_border_width(action_tiles[i],1,0);
        lv_obj_set_style_border_color(action_tiles[i],lv_color_hex(BUOY_ACCENT),0);lv_obj_remove_flag(action_tiles[i],LV_OBJ_FLAG_SCROLLABLE);
        action_labels[i]=lv_label_create(action_tiles[i]);lv_label_set_text(action_labels[i],action_names[i]);lv_obj_center(action_labels[i]);
    }
    cal_dot=lv_obj_create(screen);lv_obj_set_size(cal_dot,12,12);lv_obj_set_style_radius(cal_dot,LV_RADIUS_CIRCLE,0);lv_obj_set_style_border_width(cal_dot,0,0);lv_obj_set_style_bg_color(cal_dot,lv_color_hex(BUOY_ACCENT),0);
    cal_label=lv_label_create(screen);lv_obj_set_width(cal_label,222);lv_obj_set_style_text_align(cal_label,LV_TEXT_ALIGN_CENTER,0);
    alarm_toggle=lv_obj_create(screen);lv_obj_set_pos(alarm_toggle,30,140);lv_obj_set_size(alarm_toggle,260,44);lv_obj_set_style_pad_all(alarm_toggle,0,0);lv_obj_set_style_bg_color(alarm_toggle,lv_color_hex(BUOY_SURFACE),0);lv_obj_remove_flag(alarm_toggle,LV_OBJ_FLAG_SCROLLABLE);
    alarm_toggle_label=lv_label_create(alarm_toggle);
    back_button=lv_obj_create(screen);lv_obj_set_pos(back_button,24,188);lv_obj_set_size(back_button,272,48);
    lv_obj_set_style_pad_all(back_button,0,0);lv_obj_set_style_bg_color(back_button,lv_color_hex(BUOY_ACCENT),0);lv_obj_remove_flag(back_button,LV_OBJ_FLAG_SCROLLABLE);
    back_label=lv_label_create(back_button);lv_label_set_text(back_label,"< Back to Actions");lv_obj_set_style_text_color(back_label,lv_color_hex(BUOY_BG),0);lv_obj_center(back_label);
    logo = lv_image_create(screen);
    lv_image_set_src(logo, &claw_logo);
    /* Native 96x70 drawing avoids the temporary layers used by scaling.
       Keep the lower rows for status/sensor/reply instead of a tagline. */
    lv_obj_set_pos(logo, 40, 24);
    lv_obj_add_flag(logo, LV_OBJ_FLAG_HIDDEN);
    status_label = lv_label_create(screen);
    lv_obj_set_pos(status_label, 8, 180);
    lv_obj_set_width(status_label, 304);
    lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_CENTER, 0);
    telemetry = lv_obj_create(screen);
    lv_obj_set_pos(telemetry, 8, 200);
    lv_obj_set_size(telemetry, 304, 36);
    lv_obj_set_style_pad_all(telemetry, 0, 0);
    lv_obj_set_style_border_width(telemetry, 0, 0);
    lv_obj_set_style_radius(telemetry, 8, 0);
    lv_obj_set_style_bg_color(telemetry, lv_color_hex(BUOY_SURFACE), 0);
    lv_obj_remove_flag(telemetry, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(telemetry, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *stem = lv_obj_create(telemetry);
    lv_obj_set_pos(stem, 13, 5); lv_obj_set_size(stem, 6, 19);
    lv_obj_set_style_radius(stem, 3, 0); lv_obj_set_style_border_width(stem, 0, 0);
    lv_obj_set_style_bg_color(stem, lv_color_hex(BUOY_ACCENT), 0);
    lv_obj_t *bulb = lv_obj_create(telemetry);
    lv_obj_set_pos(bulb, 11, 21); lv_obj_set_size(bulb, 10, 10);
    lv_obj_set_style_radius(bulb, LV_RADIUS_CIRCLE, 0); lv_obj_set_style_border_width(bulb, 0, 0);
    lv_obj_set_style_bg_color(bulb, lv_color_hex(BUOY_ACCENT), 0);
    temperature_label = lv_label_create(telemetry);
    lv_obj_set_pos(temperature_label, 28, 3);
    lv_obj_set_style_text_font(temperature_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(telemetry,lv_color_hex(BUOY_INK),0);
    humidity_label = lv_label_create(telemetry);
    lv_obj_set_pos(humidity_label, 216, 10);
    lv_obj_set_style_text_color(humidity_label, lv_color_hex(BUOY_ACCENT), 0);
    message_panel = lv_obj_create(screen);
    lv_obj_set_size(message_panel, 296, 204);
    lv_obj_set_pos(message_panel, 12, 18);
    lv_obj_set_style_pad_all(message_panel, 14, 0);
    lv_obj_set_style_border_width(message_panel, 2, 0);
    lv_obj_set_style_radius(message_panel, 12, 0);
    lv_obj_set_style_bg_color(message_panel, lv_color_hex(BUOY_SURFACE), 0);
    lv_obj_set_style_border_color(message_panel,lv_color_hex(BUOY_ACCENT),0);
    lv_obj_set_style_text_color(message_panel,lv_color_hex(BUOY_INK),0);
    lv_obj_remove_flag(message_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(message_panel, LV_OBJ_FLAG_HIDDEN);
    from_label = lv_label_create(message_panel);
    lv_obj_set_width(from_label, 264);
    lv_obj_set_height(from_label, 18);
    lv_label_set_long_mode(from_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(from_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(from_label, lv_color_hex(BUOY_ACCENT), 0);
    reply_label = lv_label_create(message_panel);
    lv_obj_set_width(reply_label, 264);
    lv_label_set_long_mode(reply_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(reply_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(reply_label,0,26);lv_obj_set_height(reply_label,112);
    message_footer=lv_label_create(message_panel);lv_obj_set_pos(message_footer,0,150);lv_obj_set_width(message_footer,264);
    lv_obj_set_style_text_font(message_footer,&lv_font_montserrat_14,0);lv_obj_set_style_text_align(message_footer,LV_TEXT_ALIGN_CENTER,0);
    esp_console_cmd_t serial = {.command="display-text", .help="Show quoted agent reply (max 1024 bytes)", .func=serial_text};
    err = esp_console_cmd_register(&serial);
    if (err != ESP_OK) goto fail_lvgl;
    const esp_console_cmd_t page_cmd = {.command="display-page", .help="Select left/home or right/hello page", .func=serial_page};
    err = esp_console_cmd_register(&page_cmd);
    if (err != ESP_OK) goto fail_lvgl;
    static const esp_openclaw_node_command_t command = {.name="display.text", .handler=text_command};
    err = esp_openclaw_node_register_capability(node, "display");
    if (err != ESP_OK) goto fail_lvgl;
    err = esp_openclaw_node_register_command(node, &command);
    if (err != ESP_OK) goto fail_lvgl;
    static const esp_openclaw_node_command_t update={.name="display.update",.handler=update_command};
    static const esp_openclaw_node_command_t state={.name="display.state",.handler=state_command};
    err=esp_openclaw_node_register_command(node,&update);if(err!=ESP_OK)goto fail_lvgl;
    err=esp_openclaw_node_register_command(node,&state);if(err!=ESP_OK)goto fail_lvgl;
    err = claw_box_touch_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "touch initialization failed: %s", esp_err_to_name(err));
        goto fail_lvgl;
    }
    display_node=node;last_touch_us=esp_timer_get_time();
    nvs_handle_t h;
    if(nvs_open("buoy_ui",NVS_READONLY,&h)==ESP_OK){uint16_t m;uint8_t on;if(nvs_get_u16(h,"alarm_min",&m)==ESP_OK && m<1440)alarm_minute=m;if(nvs_get_u8(h,"alarm_on",&on)==ESP_OK)alarm_enabled=on;nvs_close(h);}
    if(nvs_open("buoy_ui",NVS_READONLY,&h)==ESP_OK){
        bool have=nvs_get_i32(h,"cx0",&cal_x0)==ESP_OK&&nvs_get_i32(h,"cy0",&cal_y0)==ESP_OK&&nvs_get_i32(h,"cx1",&cal_x1)==ESP_OK&&nvs_get_i32(h,"cy1",&cal_y1)==ESP_OK;
        if(have&&abs(cal_x1-cal_x0)>=500&&abs(cal_y1-cal_y0)>=500){touch_calibrated=true;calibration_stage=0;}uint8_t use_affine=0;
        if(touch_calibrated&&nvs_get_u8(h,"affine",&use_affine)==ESP_OK&&use_affine&&
           nvs_get_i32(h,"cx2",&cal_x2)==ESP_OK&&nvs_get_i32(h,"cy2",&cal_y2)==ESP_OK)
            affine_valid=buoy_affine_fit(&affine,cal_x0,cal_y0,cal_x1,cal_y1,cal_x2,cal_y2);
        nvs_close(h);
    }
    if(nvs_open("buoy_ui",NVS_READONLY,&h)==ESP_OK){
        uint8_t mode,offline,kind;int64_t updated;size_t bytes=sizeof(weather_text);
        if(nvs_get_u8(h,"ping_test",&mode)==ESP_OK)ping_test_mode=mode!=0;
        if(nvs_get_str(h,"weather",weather_text,&bytes)==ESP_OK&&nvs_get_i64(h,"weather_at",&updated)==ESP_OK)weather_updated=updated;
        if(nvs_get_u8(h,"weather_off",&offline)==ESP_OK)weather_offline=offline!=0;
        if(nvs_get_u8(h,"weather_kind",&kind)==ESP_OK&&kind<=BUOY_WEATHER_MIXED)weather_kind=kind;
        nvs_close(h);
    }
    if(calibration_stage){ping_test_mode=true;wake_requested=true;}
    setenv("TZ",BUOY_TIMEZONE,1);tzset();
    if(esp_sntp_enabled()) { esp_sntp_stop(); }
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);esp_sntp_setservername(0,"pool.ntp.org");esp_sntp_init();
    touch_queue=xQueueCreate(8,sizeof(buoy_touch_stroke_t));
    if(!touch_queue){err=ESP_ERR_NO_MEM;goto fail_lvgl;}
    TaskHandle_t input_task=NULL;
    if(xTaskCreate(touch_task,"buoy_touch",2048,NULL,3,&input_task)!=pdPASS){vQueueDelete(touch_queue);touch_queue=NULL;err=ESP_ERR_NO_MEM;goto fail_lvgl;}
    if (xTaskCreate(ui_task, "claw_display", 6144, NULL, 2, NULL) != pdPASS) {vTaskDelete(input_task);vQueueDelete(touch_queue);touch_queue=NULL; err=ESP_ERR_NO_MEM; goto fail_lvgl; }
    ready = true;
    ESP_LOGI(TAG, "ILI9341 initialized 320x240; touch SPI3 CLK14 DIN13 DO19 CS33 IRQ32; three pages; AHT20 GPIO21/22 unchanged");
    return ESP_OK;
fail_lvgl:
    lv_display_delete(display);
    free(buffer);
    vSemaphoreDelete(lock);
    lock = NULL;
fail_panel:
    esp_lcd_panel_del(panel);
    panel = NULL;
fail_io:
    esp_lcd_panel_io_del(io);
fail_bus:
    spi_bus_free(SPI2_HOST);
    return err;
#endif
}
