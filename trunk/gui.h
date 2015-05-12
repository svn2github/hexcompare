#ifndef HEX_GUI
#define HEX_GUI

#include <math.h>
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include "general.h"

#define OVERVIEW_MODE 0
#define HEX_MODE 1

#define HEX_VIEW 0
#define ASCII_VIEW 1

#define BLOCK_SAME 1            /* Blue Box */
#define BLOCK_DIFFERENT 2       /* Red Box */
#define BLOCK_EMPTY 3           /* Grey Box */
#define BLOCK_ACTIVE 4          /* Green Box */
#define TITLE_BAR 5             /* Black text on White Background */

#define SIDE_MARGIN 2           /* Width of the side margins in chars */
#define VERTICAL_BLACK_SPACE 11 /* Sum of padding from top to bottom */

#define UP_ROW 2
#define DOWN_ROW -2
#define LEFT_BLOCK -1
#define RIGHT_BLOCK 1
#define UP_LINE 3
#define DOWN_LINE -3

void start_gui(struct file file_one, struct file file_two,
               unsigned long largest_file_size);

#endif
