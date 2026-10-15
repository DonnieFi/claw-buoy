#pragma once
/* Optional private deployment overrides live outside the public source allowlist. */
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
#define BUOY_PING_ONE_MESSAGE "Office Buoy check-in button 1 pressed. Record receipt locally. Do not send external messages."
#endif
#ifndef BUOY_PING_TWO_MESSAGE
#define BUOY_PING_TWO_MESSAGE "Office Buoy check-in button 2 pressed. Record receipt locally. Do not send external messages."
#endif
#ifndef BUOY_WEATHER_MESSAGE
#define BUOY_WEATHER_MESSAGE "Office Buoy Weather button pressed. Use the configured location to obtain current weather, then update this node with display.update: weather, weatherUpdatedAt (Unix observation timestamp), weatherCondition and weatherAvailable. Include local observation time, temperature, rain likelihood and practical clothing guidance. If unavailable set weatherAvailable=false. Never invent observations. Do not send external messages."
#endif
#ifndef BUOY_AGENDA_MESSAGE
#define BUOY_AGENDA_MESSAGE "Office Buoy Today page opened. Use the authorized configured calendar and timezone to obtain today's agenda. Update this node with display.update: agenda, agendaDate (YYYY-MM-DD), agendaUpdatedAt (Unix timestamp), agendaAvailable. If unavailable say so and set agendaAvailable=false. Never invent events. Do not send external messages."
#endif
