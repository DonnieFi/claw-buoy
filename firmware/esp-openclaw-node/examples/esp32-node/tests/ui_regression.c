#include "buoy_ui_rules.h"
#include <assert.h>
#include <stdio.h>
static unsigned width(uint32_t cp) { return cp=='W'?15:cp>127?12:7; }
static void check_text(const char *input) {
    char out[1100],joined[1200];unsigned n=buoy_text_page(input,0,260,6,width,out,sizeof(out));size_t used=0;
    for(unsigned p=0;p<n;++p){
        assert(buoy_text_page(input,p,260,6,width,out,sizeof(out))==n);
        unsigned pixels=0,lines=1;
        for(const unsigned char *q=(unsigned char*)out;*q;++q){
            if(*q=='\n'){pixels=0;++lines;continue;}
            if((*q&0xc0)!=0x80)pixels+=width(*q<128?*q:0x100);
            assert(pixels<=260);assert(used<sizeof(joined)-1);joined[used++]=*q;
        }
        assert(lines<=7); /* A terminal newline may add an empty final line. */
    }
    joined[used]=0;char expected[1100];size_t e=0;
    for(const char *q=input;*q;++q)if(*q!='\n')expected[e++]=*q;
    expected[e]=0;assert(!strcmp(joined,expected));
}
static void affine_and_press_tests(void)
{
    buoy_touch_tracker_t t={0};buoy_touch_stroke_t e;
    assert(buoy_touch_event(&t,true,2900,3600,50,0,&e)&&e.press);
    assert(!buoy_touch_event(&t,true,2800,3500,60,10000,&e));
    assert(!buoy_touch_event(&t,false,0,0,0,20000,&e));
    assert(!buoy_touch_event(&t,true,2700,3400,60,30000,&e));
    assert(!buoy_touch_event(&t,false,0,0,0,40000,&e));
    assert(!buoy_touch_event(&t,false,0,0,0,50000,&e));
    assert(buoy_touch_event(&t,false,0,0,0,60000,&e)&&!e.press);
    assert(!buoy_touch_event(&t,false,0,0,0,70000,&e));
    assert(buoy_touch_event(&t,true,2900,3600,50,80000,&e)&&e.press);
    buoy_affine_t a;
    assert(!buoy_affine_fit(&a,0,0,1000,1000,2000,2000));
    /* Skewed panel, reversed axes, and an independent midpoint. */
    assert(buoy_affine_fit(&a,2900,3600,800,800,3000,900));
    assert(abs(buoy_affine_axis(a.x,2900,3600)-40)<=1);
    assert(abs(buoy_affine_axis(a.y,2900,3600)-40)<=1);
    assert(abs(buoy_affine_axis(a.x,800,800)-280)<=1);
    assert(abs(buoy_affine_axis(a.y,800,800)-190)<=1);
    assert(abs(buoy_affine_axis(a.x,3000,900)-280)<=1);
    assert(abs(buoy_affine_axis(a.y,3000,900)-40)<=1);
    assert(abs(buoy_affine_axis(a.x,1850,2200)-160)<=1);
    assert(abs(buoy_affine_axis(a.y,1850,2200)-115)<=1);
    assert(buoy_press_target(2,0,60,60));
    assert(!buoy_press_target(0,0,60,60));
    assert(!buoy_press_target(2,1,60,60));
    assert(!buoy_press_target(2,0,10,60));
    assert(buoy_press_target(2,2,60,141));
    for(int x=0;x<320;++x)for(int y=0;y<240;++y)
        if(buoy_alarm_toggle(x,y))assert(buoy_action_target(true,x,y)<0&&!buoy_alarm_back(x,y));
    puts("PASS: affine skew/reversed axes/midpoint/degenerate fit; press-down target boundaries; 44px toggle");
}
int main(void){
    affine_and_press_tests();
    char t[40],h[24];buoy_format_sensor(t,sizeof(t),h,sizeof(h),21.3,57.4,false);
    assert(!strcmp(t,"21.3 C")&&!strcmp(h,"57% RH"));
    buoy_format_sensor(t,sizeof(t),h,sizeof(h),-12.5,100,true);
    assert(!strcmp(t,"-12.5 C (old)")&&!strcmp(h,"100% RH"));
    assert(!buoy_sensor_stale(599999999,0));assert(buoy_sensor_stale(600000000,0));
    assert(buoy_content_stale(10000,0,10800));assert(!buoy_content_stale(10000,1,10800));
    assert(buoy_content_stale(10801,1,10800));
    assert(!buoy_agenda_expired(true,"2026-10-14","2026-10-14"));
    assert(buoy_agenda_expired(true,"2026-10-14","2026-10-15"));
    assert(buoy_agenda_expired(true,"2026-12-31","2027-01-01"));
    assert(buoy_agenda_expired(false,"2026-10-14","2026-10-14"));
    assert(buoy_valid_date("2028-02-29"));assert(!buoy_valid_date("2026-02-29"));
    assert(!buoy_valid_date("2026-13-02"));assert(!buoy_valid_date("2026-10-00"));
    assert(buoy_gesture(2,2,3,200000)==BUOY_TAP);
    assert(buoy_gesture(-70,5,70,400000)==BUOY_LEFT);
    assert(buoy_gesture(80,3,80,400000)==BUOY_RIGHT);
    assert(buoy_gesture(3,-80,80,400000)==BUOY_UP);
    assert(buoy_gesture(3,80,80,400000)==BUOY_DOWN);
    assert(buoy_gesture(2,2,70,400000)==BUOY_DRAG); /* Drag ending on ping tile */
    assert(buoy_gesture(0,0,0,4000000)==BUOY_DRAG); /* Long hold is not a tap */
    assert(buoy_axis(3661,3661,829,40,280)==40);
    assert(buoy_axis(829,3661,829,40,280)==280);
    assert(buoy_axis(3661,829,3661,40,280)==280);
    assert(buoy_axis(1,1,1,40,280)==-1);
    assert(buoy_in_rect(210,65,162,36,288,94));
    assert(!buoy_in_rect(210,65,30,174,289,213));
    assert(buoy_alarm_step(1435,5)==0);assert(buoy_alarm_step(0,-60)==1380);
    assert(buoy_dark_hour(22)&&buoy_dark_hour(6)&&!buoy_dark_hour(7));
    char q[3][1025]={"B","C","D"},f[3][64]={"b","c","d"},text[1025]="A",sender[64]="a";unsigned count=3;
    assert(buoy_advance_message(text,sizeof(text),sender,sizeof(sender),q,f,&count));assert(!strcmp(text,"B")&&count==2);
    assert(buoy_advance_message(text,sizeof(text),sender,sizeof(sender),q,f,&count));assert(!strcmp(text,"C")&&count==1);
    assert(buoy_advance_message(text,sizeof(text),sender,sizeof(sender),q,f,&count));assert(!strcmp(text,"D")&&count==0);
    assert(!buoy_advance_message(text,sizeof(text),sender,sizeof(sender),q,f,&count));
    char long_text[1025];memset(long_text,'W',1024);long_text[1024]=0;check_text(long_text);
    memset(long_text,'\n',1024);long_text[1024]=0;check_text(long_text);
    check_text("Short\ntext");check_text("Rain: 10% — bring a coat.\nFamily café at 16:00. ☔");check_text("");
    for(int i=0;i<1024;++i){long_text[i]=i%3?'i':'\n';}
    long_text[1024]=0;check_text(long_text);
    assert(buoy_alarm_back(160,210)&&buoy_alarm_back(24,188)&&buoy_alarm_back(295,235));
    assert(!buoy_alarm_back(160,170));assert(buoy_alarm_toggle(160,160));
    assert(!buoy_alarm_toggle(160,210));
    assert(buoy_action_target(true,210,50)==1);assert(buoy_action_target(false,210,65)==1);
    for(int y=0;y<240;++y)for(int x=0;x<320;++x){
        if(buoy_alarm_back(x,y)||buoy_alarm_toggle(x,y))assert(buoy_action_target(true,x,y)==-1);
        assert(!(buoy_alarm_back(x,y)&&buoy_alarm_toggle(x,y)));
    }
    buoy_touch_tracker_t tracker={0};buoy_touch_stroke_t stroke;
    assert(!buoy_touch_track(&tracker,false,0,0,0,0,&stroke));
    assert(!buoy_touch_track(&tracker,true,2100,3000,120,10000,&stroke));
    assert(!buoy_touch_track(&tracker,true,2102,3001,400,20000,&stroke));
    assert(!buoy_touch_track(&tracker,true,2110,3010,110,30000,&stroke));
    assert(!buoy_touch_track(&tracker,false,0,0,0,40000,&stroke));
    assert(!buoy_touch_track(&tracker,false,0,0,0,50000,&stroke));
    assert(buoy_touch_track(&tracker,false,0,0,0,60000,&stroke));
    assert(stroke.peak_x==2102&&stroke.peak_y==3001&&stroke.last_x==2110&&stroke.first_x==2100);
    assert(stroke.min_x==2100&&stroke.max_x==2110&&stroke.ended_us-stroke.started_us==50000);
    /* A single-sample short tap survives rendering delays; two noisy misses
       during a held stroke do not manufacture a second press. */
    assert(!buoy_touch_track(&tracker,true,1000,2000,200,70000,&stroke));
    assert(!buoy_touch_track(&tracker,false,0,0,0,80000,&stroke));
    assert(!buoy_touch_track(&tracker,false,0,0,0,90000,&stroke));
    assert(!buoy_touch_track(&tracker,true,1001,2001,250,100000,&stroke));
    assert(!buoy_touch_track(&tracker,false,0,0,0,110000,&stroke));
    assert(!buoy_touch_track(&tracker,false,0,0,0,120000,&stroke));
    assert(buoy_touch_track(&tracker,false,0,0,0,130000,&stroke));
    assert(stroke.started_us==70000&&stroke.peak_x==1001);
    char label[16];
    buoy_test_label(label,sizeof(label),"Ping Dad");assert(!strcmp(label,"Test Dad"));
    buoy_test_label(label,sizeof(label),"Ping 1");assert(!strcmp(label,"Test 1"));
    buoy_test_label(label,sizeof(label),"Nap");assert(!strcmp(label,"Test Nap"));
    buoy_test_label(label,sizeof(label),"Ping Extremely Long Label");assert(strlen(label)==15);
    buoy_test_label(label,sizeof(label),BUOY_PING_ONE_LABEL);assert(!strncmp(label,"Test ",5));
    for(int kind=0;kind<=BUOY_WEATHER_MIXED;++kind)assert(buoy_weather_parse(buoy_weather_name(kind))==kind);
    assert(buoy_weather_parse("invalid")==-1);
    puts("PASS: alarm Back/toggle/tile separation over all screen pixels; 10ms stroke tracker, peak-pressure coordinates and dropout debounce");
    puts("PASS: numeric formatting, freshness/rollover, gestures, mapping/hit regions, alarm, notification FIFO, max/newline/UTF-8 pagination");
}
