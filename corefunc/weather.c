/*
 *  This file is part of the havokmud package
 *  Copyright (C) 2007 Gavin Hurlbut
 *
 *  havokmud is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*HEADER---------------------------------------------------
 * $Id$
 *
 * Copyright 2007 Gavin Hurlbut
 * All rights reserved
 */

/**
 * @file
 * @brief Handles the weather in the MUD
 */

#include "config.h"
#include "environment.h"
#include <stdio.h>
#include <string.h>
#include "logging.h"
#include "interthread.h"
#include "protos.h"

static char ident[] _UNUSED_ =
    "$Id$";


/*
 * what stage is moon in? (1 - 32) 
 */
unsigned char   moontype;

int             gSeason;        /* global variable --- the season */
int             gMoonSet = 3;
int             gSunRise = 5;
int             gSunSet = 18;
int             gMoonRise = 22;

int             gLightLevel = 4;        /* defaults to sunlight */

void ChangeSeason(int month);


void weather_and_time(int mode)
{
    another_hour(mode);
    if (mode) {
        weather_change();
    }
}

void another_hour(int mode)
{
    char            moon[20];
#if 0
    char            buf[100];
#endif
    int             tmp;

    time_info.hours++;

    tmp = time_info.hours;

    if (mode) {
        /*
         * as a test, save a piece of the world every mud hour 
         */
#if 0
        SaveTheWorld();
#endif
        if (tmp == gMoonRise) {
            if (moontype < 4) {
                strcpy(moon, "new");
            } else if (moontype < 12) {
                strcpy(moon, "waxing");
            } else if (moontype < 20) {
                strcpy(moon, "full");
            } else if (moontype < 28) {
                strcpy(moon, "waning");
            } else {
                strcpy(moon, "new");
            }
#if 0
            switch_light(MOON_RISE);
            sprintf(buf, "The %s moon begins to rise from the western "
                         "horizon.\n\r", moon);
            send_to_outdoor(buf);
#endif
            if (moontype > 16 && moontype < 22) {
                /* 
                 * brighter during these moons 
                 */
                gLightLevel++;
            }
        }

        if (tmp == gSunRise && !IS_SET(SystemFlags, SYS_ECLIPS)) {
            weather_info.sunlight = SUN_RISE;
#if 0
            send_to_outdoor("The sun begins to rise from the western "
                            "horizon.\n\r");
#endif
        }

        if (tmp == gSunRise + 1 && !IS_SET(SystemFlags, SYS_ECLIPS)) {
            weather_info.sunlight = SUN_LIGHT;
#if 0
            switch_light(SUN_LIGHT);
            send_to_outdoor("The day has begun spreading light throughout the"
                            " land.\n\r");
#endif
        }

        if (tmp == gSunSet && !IS_SET(SystemFlags, SYS_ECLIPS)) {
            weather_info.sunlight = SUN_SET;
#if 0
            send_to_outdoor("The sun slowly disappears into the eastern "
                            "horizon.\n\r");
#endif
        }

        if (tmp == gSunSet + 1) {
            weather_info.sunlight = SUN_DARK;
#if 0
            switch_light(SUN_DARK);
            send_to_outdoor("The night has begun, dropping a blanket of "
                            "darkness.\n\r");
#endif
        }

        if (tmp == gMoonSet) {
#if 0
            switch_light(MOON_SET);
            if (moontype > 15 && moontype < 25) {
                send_to_outdoor("Darkness once again fills the realm as the "
                                "moon sets.\n\r");
            } else {
                send_to_outdoor("The moon slowly sets.\n\r");
            }
#endif
        }

        if (tmp == 12) {
#if 0
            send_to_outdoor("The sun is directly above, it must be noon.\n\r");
#endif
        }

        if (time_info.hours > 23) {
            time_info.hours -= 24;
            time_info.day++;
            switch (time_info.day) {
            case 0:
            case 6:
            case 13:
            case 20:
            case 27:
            case 34:
#if 0
                PulseMobiles(EVENT_WEEK);
#endif
                break;
            }

            /*
             * check the season 
             */
            ChangeSeason(time_info.month);

            moontype++;
            if (moontype > 32)
                moontype = 1;

            if (time_info.day > 34) {
                time_info.day = 0;
                time_info.month++;
                GetMonth(time_info.month);
#if 0
                PulseMobiles(EVENT_MONTH);
#endif

                if (time_info.month > 16) {
                    time_info.month = 0;
                    time_info.year++;
                }
            }

            ChangeSeason(time_info.month);
        }
    }
}

void ChangeSeason(int month)
{
    extern int      gSeason;
    switch (month) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 16:
    default:
        gSunRise = 9;           /* very late */
        gSunSet = 16;           /* very early */
        gSeason = SEASON_WINTER;
        break;
    case 4:
    case 5:
    case 6:
    case 7:
        gSunRise = 7;           /* late */
        gSunSet = 18;           /* early */
        gSeason = SEASON_SPRING;
        break;
    case 8:
    case 9:
    case 10:
    case 11:
        gSunRise = 5;           /* early */
        gSunSet = 20;           /* late */
        gSeason = SEASON_SUMMER;
        break;
    case 12:
    case 13:
    case 14:
    case 15:
        gSunRise = 7;           /* late */
        gSunSet = 18;           /* early */
        gSeason = SEASON_FALL;
        break;
    }
}

void weather_change()
{
    int             diff,
                    change;

    if ((time_info.month >= 9) && (time_info.month <= 16)) {
        diff = (weather_info.pressure > 985 ? -2 : 2);
    } else {
        diff = (weather_info.pressure > 1015 ? -2 : 2);
    }
    weather_info.change += (dice(NULL, 1, 4, 1) * diff + dice(NULL, 2, 6, 2) - 
                            dice(NULL, 2, 6, 2));

    weather_info.change = MIN(weather_info.change, 12);
    weather_info.change = MAX(weather_info.change, -12);

    weather_info.pressure += weather_info.change;

    weather_info.pressure = MIN(weather_info.pressure, 1040);
    weather_info.pressure = MAX(weather_info.pressure, 960);

    change = 0;

    switch (weather_info.sky) {
    case SKY_CLOUDLESS:
        if (weather_info.pressure < 990) {
            change = 1;
        } else if (weather_info.pressure < 1010 && dice(NULL, 1, 4, 1) == 1) {
            change = 1;
        }
        break;
    case SKY_CLOUDY:
        if (weather_info.pressure < 970) {
            change = 2;
        } else if (weather_info.pressure < 990) {
            if (dice(NULL, 1, 4, 1) == 1) {
                change = 2;
            } else {
                change = 0;
            }
        } else if (weather_info.pressure > 1030 && dice(NULL, 1, 4, 1) == 1) {
                change = 3;
        }
        break;
    case SKY_RAINING:
        if (weather_info.pressure < 970) {
            if (dice(NULL, 1, 4, 1) == 1) {
                change = 4;
            } else {
                change = 0;
            }
        } else if (weather_info.pressure > 1030) {
            change = 5;
        } else if (weather_info.pressure > 1010 && dice(NULL, 1, 4, 1) == 1) {
            change = 5;
        }
        break;
    case SKY_LIGHTNING:
        if (weather_info.pressure > 1010) {
            change = 6;
        } else if (weather_info.pressure > 990 && dice(NULL, 1, 4, 1) == 1) {
            change = 6;
        }
        break;
    default:
        change = 0;
        weather_info.sky = SKY_CLOUDLESS;
        break;
    }

#if 0
    ChangeWeather(change);
#endif
}


void GetMonth(int month)
{
    if (month < 0) {
        return;
    }
#if 0
    if (month <= 1) {
        send_to_outdoor(" It is bitterly cold outside\n\r");
    } else if (month <= 2) {
        send_to_outdoor(" It is very cold \n\r");
    } else if (month <= 3) {
        send_to_outdoor(" It is chilly outside \n\r");
    } else if (month == 4) {
        send_to_outdoor(" The flowers start to bloom \n\r");
#if 0
        PulseMobiles(EVENT_SPRING);
#endif
    } else if (month == 8) {
        send_to_outdoor(" It is warm and humid. \n\r");
#if 0
        PulseMobiles(EVENT_SUMMER);
#endif
    } else if (month == 12) {
        send_to_outdoor(" It starts to get a little windy \n\r");
#if 0
        PulseMobiles(EVENT_FALL);
#endif
    } else if (month == 13) {
        send_to_outdoor(" The air is getting chilly \n\r");
    } else if (month == 14) {
        send_to_outdoor(" The leaves start to change colors. \n\r");
    } else if (month == 15) {
        send_to_outdoor(" It starts to get cold \n\r");
    } else if (month == 16) {
        send_to_outdoor(" It is bitterly cold outside \n\r");
#if 0
        PulseMobiles(EVENT_WINTER);
#endif
    }
#endif
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
