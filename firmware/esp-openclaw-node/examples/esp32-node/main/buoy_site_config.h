#pragma once
/* Optional local overrides: create private_buoy_config.h (git-ignored) beside the example CMakeLists.txt. */
#if __has_include("../private_buoy_config.h")
#include "../private_buoy_config.h"
#endif
#ifndef BUOY_PING_ONE_TARGET
#define BUOY_PING_ONE_TARGET ""
#endif
#ifndef BUOY_PING_TWO_TARGET
#define BUOY_PING_TWO_TARGET ""
#endif
#ifndef BUOY_PING_ONE_MESSAGE
#define BUOY_PING_ONE_MESSAGE "Claw Buoy check-in button 1 pressed. Record receipt locally. Do not send external messages."
#endif
#ifndef BUOY_PING_TWO_MESSAGE
#define BUOY_PING_TWO_MESSAGE "Claw Buoy check-in button 2 pressed. Record receipt locally. Do not send external messages."
#endif
#ifndef BUOY_WEATHER_MESSAGE
#define BUOY_WEATHER_MESSAGE "Claw Buoy Weather button pressed. Use the configured location to obtain current weather, then update this node with display.update: weather, weatherUpdatedAt (Unix observation timestamp), weatherCondition and weatherAvailable. Include local observation time, temperature, rain likelihood and practical clothing guidance. If unavailable set weatherAvailable=false. Never invent observations. Do not send external messages."
#endif
#ifndef BUOY_AGENDA_MESSAGE
#define BUOY_AGENDA_MESSAGE "Claw Buoy Today page opened. Use the authorized configured calendar and timezone to obtain today's agenda. Update this node with display.update: agenda, agendaDate (YYYY-MM-DD), agendaUpdatedAt (Unix timestamp), agendaAvailable. If unavailable say so and set agendaAvailable=false. Never invent events. Do not send external messages."
#endif

/* POSIX TZ string; UTC is the portable default. Override locally for DST rules. */
#ifndef BUOY_TIMEZONE
#define BUOY_TIMEZONE "UTC0"
#endif
#ifndef BUOY_DARK_START_HOUR
#define BUOY_DARK_START_HOUR 22
#endif
#ifndef BUOY_DARK_END_HOUR
#define BUOY_DARK_END_HOUR 7
#endif
#ifndef BUOY_PING_ONE_LABEL
#define BUOY_PING_ONE_LABEL "Ping 1"
#endif
#ifndef BUOY_PING_TWO_LABEL
#define BUOY_PING_TWO_LABEL "Ping 2"
#endif
#ifndef BUOY_ACTION_SESSION_KEY
#define BUOY_ACTION_SESSION_KEY "agent:main:node:claw-buoy-actions"
#endif
#if BUOY_DARK_START_HOUR < 0 || BUOY_DARK_START_HOUR > 23 || BUOY_DARK_END_HOUR < 0 || BUOY_DARK_END_HOUR > 23
#error "Dark hours must be between 0 and 23; equal hours disable scheduled dimming"
#endif
