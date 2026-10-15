#pragma once

void menu_init(void);
void menu_status(const char* fmt, ...);

void graphic_init(void);
void graphic_draw_fast(void);
void graphic_biosdrain_fadein(void);

extern int graphic_vsync_sema;


#define C_WHTB "\0330F"
#define C_REDB "\0330C"
#define C_GRNB "\0330A"
#define C_YELB "\0330E"