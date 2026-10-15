#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static inline int buoy_next_page(int page, int pixel_x) { return (page + (pixel_x >= 160 ? 1 : 2)) % 3; }
static inline int buoy_alarm_step(int minute, int delta) { return (minute + delta + 1440) % 1440; }
#include "buoy_site_config.h"
static inline int buoy_dark_hour(int hour) {
    const int start = BUOY_DARK_START_HOUR, end = BUOY_DARK_END_HOUR;
    return start == end ? 0 : (start < end ? (hour >= start && hour < end) : (hour >= start || hour < end));
}
static inline int buoy_axis(int raw,int first,int last,int low,int high) { return last==first ? -1 : low+(raw-first)*(high-low)/(last-first); }
static inline int buoy_in_rect(int x,int y,int x0,int y0,int x1,int y1) { return x>=x0&&x<=x1&&y>=y0&&y<=y1; }

typedef enum { BUOY_TAP, BUOY_LEFT, BUOY_RIGHT, BUOY_UP, BUOY_DOWN, BUOY_DRAG } buoy_gesture_t;
/* A drag that comes back to its starting point must never become a button tap. */
static inline buoy_gesture_t buoy_gesture(int dx, int dy, int excursion, int64_t duration_us)
{
    if (excursion < 20 && duration_us < 3000000) return BUOY_TAP;
    if (abs(dx) >= 35 && abs(dx) > abs(dy)*3/2) return dx<0?BUOY_LEFT:BUOY_RIGHT;
    if (abs(dy) >= 35 && abs(dy) > abs(dx)*3/2) return dy<0?BUOY_UP:BUOY_DOWN;
    return BUOY_DRAG;
}
static inline bool buoy_sensor_stale(int64_t now, int64_t updated) { return now-updated >= 600000000; }
static inline bool buoy_content_stale(int64_t now, int64_t updated, int ttl) { return updated<=0 || now-updated>=ttl; }
static inline void buoy_format_sensor(char *t, size_t ts, char *h, size_t hs, double temperature, double humidity, bool stale)
{
    snprintf(t,ts,"%.1f C%s",temperature,stale?" (old)":"");
    snprintf(h,hs,"%.0f%% RH",humidity);
}
/* Shared FIFO transition used by dismissal AND timeout. */
static inline bool buoy_advance_message(char *text, size_t text_size, char *sender, size_t sender_size,
    char queued_text[3][1025], char queued_sender[3][64], unsigned *count)
{
    if (!*count) return false;
    snprintf(text,text_size,"%.1024s",queued_text[0]);
    snprintf(sender,sender_size,"%.63s",queued_sender[0]);
    --*count;
    memmove(queued_text,queued_text+1,*count*sizeof(queued_text[0]));
    memmove(queued_sender,queued_sender+1,*count*sizeof(queued_sender[0]));
    return true;
}
/* Layout by measured glyph widths, not a byte-count preview. Explicit line breaks
 * give every accepted byte an accessible page, including newline-heavy UTF-8.
 * Output needs input-length + line-count + 1 bytes. No allocations. */
typedef unsigned (*buoy_glyph_width_fn)(uint32_t codepoint);
static inline unsigned buoy_text_page(const char *text, unsigned wanted, unsigned width, unsigned rows,
    buoy_glyph_width_fn glyph_width, char *out, size_t capacity)
{
    unsigned page=0,line=0,pixels=0;size_t n=0;
    for (const unsigned char *p=(const unsigned char *)text; *p;) {
        uint32_t cp=*p;unsigned bytes=1;
        if ((*p&0xe0)==0xc0 && p[1]) {bytes=2;cp=(*p&31)<<6|(p[1]&63);}
        else if ((*p&0xf0)==0xe0 && p[1] && p[2]) {bytes=3;cp=(*p&15)<<12|(p[1]&63)<<6|(p[2]&63);}
        else if ((*p&0xf8)==0xf0 && p[1] && p[2] && p[3]) {bytes=4;cp=(*p&7)<<18|(p[1]&63)<<12|(p[2]&63)<<6|(p[3]&63);}
        unsigned advance=cp=='\n'?0:glyph_width(cp);
        if (cp!='\n' && pixels && pixels+advance>width) {
            if (++line==rows) {++page;line=0;} else if(page==wanted && n+1<capacity)out[n++]='\n';
            pixels=0;
        }
        if(page==wanted && n+bytes<capacity) {memcpy(out+n,p,bytes);n+=bytes;}
        p+=bytes;pixels+=advance;
        if(cp=='\n') {pixels=0;if(++line==rows){++page;line=0;}}
    }
    if(capacity)out[n]=0;
    return page+1;
}

static inline bool buoy_agenda_expired(bool clock_valid, const char *date, const char *today)
{
    return !clock_valid || !date[0] || strcmp(date,today)!=0;
}
static inline bool buoy_valid_date(const char *date)
{
    if(strlen(date)!=10||date[4]!='-'||date[7]!='-')return false;
    for(int i=0;i<10;++i)if(i!=4&&i!=7&&(date[i]<'0'||date[i]>'9'))return false;
    int y=atoi(date),m=atoi(date+5),d=atoi(date+8);
    const int days[]={0,31,28,31,30,31,30,31,31,30,31,30,31};
    if(y<2020||y>2100||m<1||m>12||d<1)return false;
    int limit=days[m]+(m==2&&y%4==0&&(y%100!=0||y%400==0));
    return d<=limit;
}

/* Sampling owns this tracker; rendering consumes completed strokes only. */
typedef struct {
    uint16_t first_x,first_y,last_x,last_y,min_x,min_y,max_x,max_y,peak_x,peak_y,pressure;
    int64_t started_us,ended_us;
} buoy_touch_stroke_t;
typedef struct { bool down;unsigned misses;buoy_touch_stroke_t stroke; } buoy_touch_tracker_t;
static inline bool buoy_touch_track(buoy_touch_tracker_t *t,bool valid,uint16_t x,uint16_t y,uint16_t pressure,int64_t now,buoy_touch_stroke_t *out)
{
    if(valid){
        if(!t->down){
            t->stroke=(buoy_touch_stroke_t){.first_x=x,.first_y=y,.min_x=x,.max_x=x,.min_y=y,.max_y=y,.peak_x=x,.peak_y=y,.started_us=now};
            t->down=true;
        }
        t->misses=0;t->stroke.last_x=x;t->stroke.last_y=y;
        if(x<t->stroke.min_x)t->stroke.min_x=x;
        if(x>t->stroke.max_x)t->stroke.max_x=x;
        if(y<t->stroke.min_y)t->stroke.min_y=y;
        if(y>t->stroke.max_y)t->stroke.max_y=y;
        if(pressure>t->stroke.pressure){t->stroke.pressure=pressure;t->stroke.peak_x=x;t->stroke.peak_y=y;}
    }else if(t->down&&++t->misses>=3){
        t->down=false;t->stroke.ended_us=now;*out=t->stroke;return true;
    }
    return false;
}
/* One geometry source for drawing, tap dispatch and host boundary tests. */
static inline int buoy_tile_top(bool alarm,int tile) { return (alarm?30:36)+(tile/2)*(alarm?54:64); }
static inline int buoy_tile_height(bool alarm) { return alarm?48:58; }
static inline int buoy_alarm_back(int x,int y) { return buoy_in_rect(x,y,24,188,295,235); }
static inline int buoy_alarm_toggle(int x,int y) { return buoy_in_rect(x,y,30,142,289,181); }

static inline int buoy_action_target(bool alarm,int x,int y)
{
    for(int i=0;i<4;++i){int x0=30+(i%2)*132,y0=buoy_tile_top(alarm,i);
        if(buoy_in_rect(x,y,x0,y0,x0+125,y0+buoy_tile_height(alarm)-1))return i;
    }
    return -1;
}

typedef enum { BUOY_WEATHER_UNKNOWN, BUOY_WEATHER_SUNNY, BUOY_WEATHER_CLOUDY, BUOY_WEATHER_RAIN, BUOY_WEATHER_SNOW, BUOY_WEATHER_STORM, BUOY_WEATHER_FOG, BUOY_WEATHER_NIGHT, BUOY_WEATHER_MIXED } buoy_weather_t;
static inline const char *buoy_weather_name(buoy_weather_t kind)
{
    const char *names[]={"unknown","sunny","cloudy","rain","snow","storm","fog","night","mixed"};
    return kind>=BUOY_WEATHER_UNKNOWN&&kind<=BUOY_WEATHER_MIXED?names[kind]:"unknown";
}
static inline int buoy_weather_parse(const char *name)
{
    for(int i=0;i<=BUOY_WEATHER_MIXED;++i)if(!strcmp(name,buoy_weather_name(i)))return i;
    return -1;
}
