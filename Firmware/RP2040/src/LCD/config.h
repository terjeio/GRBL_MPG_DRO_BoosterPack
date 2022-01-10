/*
 * config.h
 *
 *  Created on: Jul 12, 2013
 *      Author: RobG
 */

#pragma once

#define ILI9340 // 2.2" 320x240

#ifndef JPG_ENABLE
#define JPG_ENABLE 0
#endif

#if JPG_ENABLE && !defined(TJPGD_BUFSIZE)
#define TJPGD_BUFSIZE 3200
#endif

// set to 1 to add touch support (requires compatible driver!)
#ifndef TOUCH_ENABLE
#define TOUCH_ENABLE 0
#endif

#if TOUCH_ENABLE && !defined(TOUCH_MAXSAMPLES)
#define TOUCH_MAXSAMPLES 50
#endif
