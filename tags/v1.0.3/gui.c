#include "gui.h"

// #####################################################################
// ##              ANCILLARY MATHEMATICAL FUNCTIONS                   ##
// #####################################################################

void calculate_dimensions(int *width, int *height, int *total_blocks,
                          unsigned long *bytes_per_block,
                          unsigned long largest_file_size,
                          int *blocks_with_excess_byte)
{
	// Acquire the dimensions of window
	getmaxyx(stdscr, *height, *width);
	
	// If the window dimensions are too small, exit.
	if (*height < 16 || *width < 10) {
		endwin();
		printf("Terminal dimensions are too small to proceed. "
		       "Increase the size to a minimum of 16w x 10h.\n\n");
		exit(1);
	}
	
	// Calculate how many bytes are held in a block.
	// Each block holds a minimum of one byte. The number is 
	// biggest file size / # blocks ((width-SIDE_MARGIN) * (height-11))
	// rounded to the next number up.
	*total_blocks = (*width - SIDE_MARGIN*2) * 
	                (*height - VERTICAL_BLACK_SPACE);
	*bytes_per_block = (unsigned long) largest_file_size / 
	                   (*total_blocks);
	*blocks_with_excess_byte = (unsigned long) largest_file_size % 
	                   (*total_blocks);

	return;
}

int calculate_current_block(int total_blocks, unsigned long file_offset,
	                        unsigned long *offset_index)
{	            
	// With a given offset, calculate which element it corresponds to
	// in the offset_index.            
	int i, current_block = 0;
	for (i = 0; i < total_blocks; i++) {
		
		// Go block by block, and see if our offset is greater than
		// that of the block's.
		if (file_offset >= offset_index[i]) current_block = i;
		else break;
		
		// Break if we reach EOF.
		if (i+1 < total_blocks && offset_index[i+1] == offset_index[i])
		break;
	}
	
	return current_block;
}

int calculate_max_offset_characters(unsigned long file_offset, 
                                    int width, int rows)
{
	// Create variables.
	char offset_query[32];
	char offset_line[32];
	int offset_size = 0;
	int previous_size, current_size = 0, i;
	int redo_loop = false; 
	
	// Endless loop until we figure out an offset size that is constant
	// accross all rows.
	while (1) {
		
		// Calculate offset jump for a given offset_size
		int hex_width = width - current_size - 1 - SIDE_MARGIN * 2;
		int offset_jump = (hex_width - (hex_width % 4)) / 4;
		offset_size++;
		
		// Go row by row. If size increase, recalculate offset jump.
		// and increase x size.
		
		// Go row by row.
		redo_loop = false;
		unsigned int temp_offset = file_offset;
		for (i = 0; i < rows; i++) {
			
			sprintf(offset_query, "0x%%0%ix ", offset_size);
			sprintf(offset_line, offset_query, temp_offset);
			current_size = strlen(offset_line);
			
			if (i > 0 && current_size > previous_size) {
				redo_loop = true;
				break;
			}
			
			previous_size = current_size;
			temp_offset += offset_jump;
			
		}
		
		if (redo_loop == false)
			return current_size - 3;
	}
}

// #####################################################################
// ##                    SCREEN HANDLING FUNCTIONS                    ##
// #####################################################################

char raw_to_ascii(char input)
{
	if (input > 31 && input < 127) return input;
	else return '.';
}

// #####################################################################
// ##                      HANDLE MOUSE ACTIONS                       ##
// #####################################################################

void mouse_clicked(unsigned long *file_offset, unsigned long 
                   *offset_index, int width, int height, 
                   int total_blocks, char *mode,
                   int mouse_x, int mouse_y, int action)
{
	// In overview mode, you can single-click boxes in the top view
	// to move to the offset that they represent. Double click brings
	// you to the hex representation.
	if (*mode == OVERVIEW_MODE && (action == BUTTON1_CLICKED || 
		action == BUTTON1_DOUBLE_CLICKED)) {
			
		// If the mouse is out of bounds, return.
		if (mouse_x < SIDE_MARGIN || mouse_x > width - SIDE_MARGIN - 1
			|| mouse_y < 2 || mouse_y > height - 8)
			return;
		 
		// Calculate the box it falls in.
		int index = (width-SIDE_MARGIN*2) * (mouse_y-2) + 
					mouse_x-SIDE_MARGIN;
								
		// Set the offset to the value in the box.
		if (index < total_blocks && index >= 0)
			*file_offset = offset_index[index];
		
		// If double-clicked, set to HEX MODE.
		if (action == BUTTON1_DOUBLE_CLICKED)
			*mode = HEX_MODE;
	}
	
	return;
}
								     
								     
// #####################################################################
// ##                      GENERATE TITLE BAR                         ##
// #####################################################################
                   
void generate_titlebar(struct file file_one, struct file file_two, 
                       unsigned long file_offset, int width, int height,
                       char mode, int display)
{
	// Define and set colour for the title bar.
	init_pair(TITLE_BAR, COLOR_BLACK, COLOR_WHITE); 
	attron(COLOR_PAIR(TITLE_BAR) | A_BOLD);
	
	// Create the title bar background.
	int i; for (i = 0; i < width; i++) {
		mvprintw(0, i, " ");
		mvprintw(height-1, i, " ");
	}

	// Create the title.
	mvprintw(0, SIDE_MARGIN, "hexcompare: %s vs. %s", 
	         file_one.name, file_two.name);
	
	// Indicate file offset.
	char title_offset[32];
	sprintf(title_offset, " 0x%04x", (unsigned int) file_offset);
	mvprintw(0, width-strlen(title_offset)-SIDE_MARGIN, "%s", 
	         title_offset);
	         
	// Write bottom menu options.
	char bottom_message[128];
	strcpy(bottom_message, "Quit: q | ");
	
	if (display == HEX_VIEW) 
		strcat(bottom_message, "Hex Mode: m | ");
	else 
		strcat(bottom_message, "ASCII Mode: m | ");
	
	if (mode == OVERVIEW_MODE) 
		strcat(bottom_message, "Full View: v | Page & Arrow Keys to Move");
	else 
		strcat(bottom_message, "Mixed View: v | Arrow Keys to Move");
	
	mvprintw(height-1, SIDE_MARGIN, "%s", bottom_message);
				 
	// Set the colour scheme back to default.
	attroff(COLOR_PAIR(TITLE_BAR) | A_BOLD);

	return;
}
    
// #####################################################################
// ##            GENERATE BLOCK DATA FOR OVERVIEW MODE                ##
// #####################################################################

char *generate_blocks(struct file file_one, struct file file_two, 
                 char *block_cache, int total_blocks, 
                 unsigned long bytes_per_block,
                 int blocks_with_excess_byte)
{
	// De-allocate existing memory that holds the block data.
	if (block_cache != NULL) free(block_cache);

	// Allocate the correct amount of memory and initialize it.
	block_cache = malloc(total_blocks);
	memset(block_cache, BLOCK_EMPTY, total_blocks);
	
	// Seek to start of file.
	fseek(file_one.pointer, 0, SEEK_SET);
	fseek(file_two.pointer, 0, SEEK_SET);

	// Compare bytes of file_one with file_two. Store results in
	// a dynamically-sized block_cache.
	int i;
	unsigned char block_one[bytes_per_block + 1], block_two[bytes_per_block + 1];
	for (i = 0; i < total_blocks; i++) {
		
		// Calculate how many bytes to read for this block.
		size_t bytes_in_block;
		if (i < blocks_with_excess_byte)
			bytes_in_block = bytes_per_block + 1;
		else
			bytes_in_block = bytes_per_block;
		
		// Set the default.
		block_cache[i] = BLOCK_SAME;
		
		// Clear the block of data.
		memset(block_one, '\0', bytes_in_block);
		memset(block_two, '\0', bytes_in_block);
		
		// Read in the next block of data.
		int bytes_read_one = fread(block_one, bytes_in_block, 
		                           1, file_one.pointer);
		int bytes_read_two = fread(block_two, bytes_in_block, 
		                           1, file_two.pointer);
		                       
		// Stop here if we read 0 bytes. Both files are fully read.
		if (bytes_read_one == 0 && bytes_read_two == 0) {
			break;
		}
		
		// If the file lengths don't match up, we know the blocks are
		// different. Go to the next block.
		if (bytes_read_one != bytes_read_two) {
			block_cache[i] = BLOCK_DIFFERENT;
			continue;
		}
		
		// Both blocks have the same length. Compare them character by 
		// character. 
		int j; for (j = 0; j < bytes_in_block; j++) {
			if (block_one[j] != block_two[j]) {
				block_cache[i] = BLOCK_DIFFERENT;
				break;
			}
		}
	}
	
	return block_cache;
}

// #####################################################################
// ##            BLOCK OFFSET FUNCTIONS FOR OVERVIEW MODE             ##
// #####################################################################

unsigned long *generate_offsets(unsigned long *offset_index,
                 int total_blocks, unsigned long bytes_per_block,
                 int blocks_with_excess_byte)
{
	// De-allocate existing memory that holds the offset data.
	if (offset_index != NULL) free(offset_index);

	// Allocate the correct amount of memory and initialize it.
	offset_index = malloc(total_blocks * sizeof(unsigned long));
	memset(offset_index, 0, total_blocks);
	
	// Generate offset data.
	int i; unsigned long offset = 0;
	for (i = 0; i < total_blocks; i++) {
		offset_index[i] = offset;
		if (i < blocks_with_excess_byte) offset += bytes_per_block + 1;
		else offset += bytes_per_block;
	}

	return offset_index;
}

unsigned long calculate_offset(unsigned long file_offset, 
				              unsigned long *offset_index, int width, 
				              int total_blocks, int shift_type,
				              int mode, unsigned long largest_file_size,
				              int rows)
{
	
	// Initialize variables.
	unsigned long new_offset = file_offset;
	int blocks_in_row = width - SIDE_MARGIN*2;
	
	// Locate the current block we're in.
	int current_block = 0;
	current_block = calculate_current_block(total_blocks, file_offset,
	                                        offset_index);
	
	// Calculate parameters for the offset.
	int offset_char_size = calculate_max_offset_characters(file_offset, 
                                                           width, rows);
	int hex_width = width - offset_char_size - 3 - SIDE_MARGIN * 2;
	int offset_jump = (hex_width - (hex_width % 4)) / 4;   
	                                        
	// Return the offset of the block we want.
	switch(shift_type) {				
		case LEFT_BLOCK:
			if (current_block > 0)
				current_block--;
			break;
		case RIGHT_BLOCK:
			if (current_block < total_blocks - 1)
				current_block++;
			break;
		case UP_ROW:
			if (current_block - blocks_in_row < 0)
				current_block = 0;
			else
				current_block -= blocks_in_row;
			break;
		case DOWN_ROW:
			if (current_block + blocks_in_row >= total_blocks)
				current_block = total_blocks - 1;
			else
				current_block += blocks_in_row;
			break;
		case UP_LINE:
			if (file_offset - offset_jump > file_offset) {
				current_block = 0;
				break; 
			} else {
				return file_offset - offset_jump + 1;
			}
		case DOWN_LINE:
			if (file_offset + offset_jump >= largest_file_size)
				return file_offset;
			else
				return file_offset + offset_jump - 1;
	}

	new_offset = offset_index[current_block];
	return new_offset;
}

// #####################################################################
// ##           DRAW ROWS OF RAW DATA IN HEX/ASCII FORM               ##
// #####################################################################

void display_file_names(int row, struct file file_one, 
                        struct file file_two, int offset_char_size,
                        int offset_jump)
{
	char *filename_one, *filename_two;
	filename_one = strrchr(file_one.name, '/')+1;
	filename_two = strrchr(file_two.name, '/')+1;
	if (filename_one == NULL+1) filename_one = file_one.name;
	if (filename_two == NULL+1) filename_two = file_two.name;
	
	// Display the file names.
	attron(COLOR_PAIR(TITLE_BAR));
	mvprintw(row, SIDE_MARGIN+offset_char_size+3, " %s   ", 
	        filename_one);
	mvprintw(row, SIDE_MARGIN+offset_char_size+4+
	        offset_jump*2, " %s   ", filename_two);
	attroff(COLOR_PAIR(TITLE_BAR));
}

void display_offsets(int start_row, int finish_row, int offset_jump, 
	                 int offset_char_size, unsigned long file_offset)
{	                    
	int i;
	char offset_line[32];
	unsigned long temp_offset = file_offset;
	
	attron(COLOR_PAIR(TITLE_BAR));
    for (i = start_row; i < finish_row; i++) {
		sprintf(offset_line, "0x%%0%ix ", offset_char_size);
		mvprintw(i, SIDE_MARGIN, offset_line, temp_offset);
		temp_offset += offset_jump;
	}
	attroff(COLOR_PAIR(TITLE_BAR));
}

void draw_hex_data(int start_row, int finish_row, struct file file_one, 
                   struct file file_two, unsigned long file_offset, 
                   int offset_char_size, int offset_jump, int display)
{	              

	unsigned long temp_offset;
	temp_offset = file_offset;
	
	int i, j;
    for (i = start_row; i < finish_row; i++) {
		
		bool bold = false;
		for (j = SIDE_MARGIN+offset_char_size+3; j < 
			SIDE_MARGIN+offset_char_size+offset_jump*2+1; j += 2) {
				
			// Seek to the proper locations in the file.
			fseek(file_one.pointer, temp_offset, SEEK_SET);
			fseek(file_two.pointer, temp_offset, SEEK_SET);
			
			// Read a byte from the files.
			unsigned char byte_one, byte_two;
			int bytes_read_one = fread(&byte_one, 1, 1, file_one.pointer);
			int bytes_read_two = fread(&byte_two, 1, 1, file_two.pointer);
			
			// Convert binary to ASCII hex.
			char byte_one_hex[16], byte_two_hex[16];
			sprintf(byte_one_hex, "%02x", byte_one);
			sprintf(byte_two_hex, "%02x", byte_two);
			byte_one_hex[2] = '\0'; byte_two_hex[2] = '\0'; 
			
			// Interpret ASCII version of bytes.
			char byte_one_ascii, byte_two_ascii;
			byte_one_ascii = raw_to_ascii(byte_one);
			byte_two_ascii = raw_to_ascii(byte_two);
			
			// Make every other byte bold.
			if (bold == true) attron(A_BOLD);
			
			// Post results.
			
			// Byte 1:
			// Determine if its EMPTY/DIFFERENT/SAME.
			int colour_pair;
			if (bytes_read_one == 0) colour_pair = BLOCK_EMPTY;
			else if (bytes_read_two == 0) colour_pair = BLOCK_DIFFERENT;
			else if (strcmp(byte_one_hex,byte_two_hex) == 0) 
					colour_pair = BLOCK_SAME;
			else colour_pair = BLOCK_DIFFERENT;
			
			// Display the block.
			attron(COLOR_PAIR(colour_pair));
			if (colour_pair == BLOCK_EMPTY) {
				mvprintw(i,j, "  ");
			} else if (display == HEX_VIEW) {
				mvprintw(i,j, " %c", byte_one_ascii);
			} else {
				mvprintw(i,j, "%s", byte_one_hex);
			}			
			attroff(COLOR_PAIR(colour_pair));
			
			// Byte 2:
			// Determine if its EMPTY/DIFFERENT/SAME.
			if (bytes_read_two == 0) colour_pair = BLOCK_EMPTY;
			else if (bytes_read_one == 0) colour_pair = BLOCK_DIFFERENT;
			else if (strcmp(byte_one_hex,byte_two_hex) == 0) 
					colour_pair = BLOCK_SAME;
			else colour_pair = BLOCK_DIFFERENT;
			
			// Display the block.
			attron(COLOR_PAIR(colour_pair));
			if (colour_pair == BLOCK_EMPTY) {
				mvprintw(i,j+offset_jump*2+1, "  ");
			} else if (display == HEX_VIEW) {
				mvprintw(i,j+offset_jump*2+1, " %c", byte_two_ascii);
			} else {
				mvprintw(i,j+offset_jump*2+1, "%s", byte_two_hex);
			}			
			attroff(COLOR_PAIR(colour_pair));
	
			// Switch bold characters with non-bold characters.
			if (bold == true) {
				attroff(A_BOLD);
				bold = false;
			} else {
				bold = true;
			}	
			
			temp_offset++;
		}
	}
	
	return;
}

// #####################################################################
// ##              GENERATE SCREEN IN OVERVIEW MODE                   ##
// #####################################################################

void generate_overview(struct file file_one, struct file file_two, 
                        unsigned long *file_offset, 
                        unsigned long largest_file_size, 
                        unsigned long bytes_per_block,
                        int width, int height, char *block_cache,
                        int total_blocks, int blocks_with_excess_byte,
                        unsigned long *offset_index, int display)
{

	// In overview mode:
	// 
	// BLOCKDIAGRAM-BLOCKDIAGRAM-BLOCKDIAGRAM-BLOCKDIAGRAM
	// BLOCKDIAGRAM-BLOCKDIAGRAM-BLOCKDIAGRAM-BLOCKDIAGRAM
	// BLOCKDIAGRAM-BLOCKDIAGRAM-BLOCKDIAGRAM-BLOCKDIAGRAM
	// BLOCKDIAGRAM-BLOCKDIAGRAM-BLOCKDIAGRAM-BLOCKDIAGRAM 
	//
	//        FILENAME 1               FILENAME 2
	// OFFSET HEX1-HEX1-HEX1-HEX1-HEX1 HEX2-HEX2-HEX2-HEX2
	// OFFSET HEX1-HEX1-HEX1-HEX1-HEX1 HEX2-HEX2-HEX2-HEX2
	// OFFSET HEX1-HEX1-HEX1-HEX1-HEX1 HEX2-HEX2-HEX2-HEX2
	// 
	// Where BLOCKDIAGRAM is the blue/red squares comparing
	// hex blocks from file 1 and file 2. Size is variable.
	// SCROLLBAR is the scrollbar representing how far in
	// the file we are, HEX1 is the hex for file 1 from the
	// offset, and HEX2 is the hex for file 2 from the 
	// offset.
	
	// Create variables.
	int i, j;
	
	// Define colors block diagram.
	init_pair(BLOCK_SAME,      COLOR_WHITE, COLOR_BLUE);
	init_pair(BLOCK_DIFFERENT, COLOR_WHITE, COLOR_RED);  
	init_pair(BLOCK_EMPTY,     COLOR_BLACK, COLOR_CYAN); 
	init_pair(BLOCK_ACTIVE,    COLOR_BLACK, COLOR_YELLOW); 
	init_pair(TITLE_BAR,       COLOR_BLACK, COLOR_WHITE); 

	// Find which block in the diagram is active based off of
	// the current offset.
	
	// Generate the block diagram.
	for (i = 0; i < height - VERTICAL_BLACK_SPACE; i++) {
		for (j = 0; j < width - SIDE_MARGIN*2; j++) {
			
			// Draw the blocks that are matching/different/empty.
			int index = i*(width-SIDE_MARGIN*2)+j;
			attron(COLOR_PAIR(block_cache[index]));
			mvprintw(i+2,j+SIDE_MARGIN," ");
			attroff(COLOR_PAIR(block_cache[index]));
		}		
	}
	
	// Show the active block.
	int current_block = calculate_current_block(total_blocks, 
	                    *file_offset, offset_index);
	attron(COLOR_PAIR(BLOCK_ACTIVE));
	mvprintw(current_block / (width - SIDE_MARGIN*2) + 2,
	         current_block % (width - SIDE_MARGIN*2) + SIDE_MARGIN," ");
	attroff(COLOR_PAIR(BLOCK_ACTIVE));
	
	// Generate the offset markers.
	// Calculate parameters for the offset.
	int offset_char_size = calculate_max_offset_characters(*file_offset, 
                                                           width, 5);
	int hex_width = width - offset_char_size - 3 - SIDE_MARGIN * 2;
	int offset_jump = (hex_width - (hex_width % 4)) / 4;   
	 
	// Display the offsets.             
	// Display the hex offsets on the left.         
	display_offsets(height-7, height-2, offset_jump, offset_char_size, 
	                *file_offset);
	
	// Generate HEX characters
	// Seek to initial offset.
	draw_hex_data(height - 7, height - 2, file_one, file_two,
	              *file_offset, offset_char_size, offset_jump, display);
	
	// Write the file titles.
	display_file_names(height-8, file_one, file_two, offset_char_size,
	                   offset_jump);
	
	return;
}

// #####################################################################
// ##                 GENERATE SCREEN IN HEX MODE                     ##
// #####################################################################

void generate_hex(struct file file_one, struct file file_two, 
                        unsigned long *file_offset, 
                        unsigned long largest_file_size, 
                        unsigned long bytes_per_block,
                        int width, int height, char *block_cache,
                        int total_blocks, int blocks_with_excess_byte,
                        unsigned long *offset_index, int display)
{

	// In hex mode:
	// 
	//
	//        FILENAME 1               FILENAME 2
	// OFFSET HEX1-HEX1-HEX1-HEX1-HEX1 HEX2-HEX2-HEX2-HEX2
	// OFFSET HEX1-HEX1-HEX1-HEX1-HEX1 HEX2-HEX2-HEX2-HEX2
	// OFFSET HEX1-HEX1-HEX1-HEX1-HEX1 HEX2-HEX2-HEX2-HEX2
	// OFFSET HEX1-HEX1-HEX1-HEX1-HEX1 HEX2-HEX2-HEX2-HEX2
	// OFFSET HEX1-HEX1-HEX1-HEX1-HEX1 HEX2-HEX2-HEX2-HEX2
	// OFFSET HEX1-HEX1-HEX1-HEX1-HEX1 HEX2-HEX2-HEX2-HEX2
	// OFFSET HEX1-HEX1-HEX1-HEX1-HEX1 HEX2-HEX2-HEX2-HEX2
	// OFFSET HEX1-HEX1-HEX1-HEX1-HEX1 HEX2-HEX2-HEX2-HEX2
	// OFFSET HEX1-HEX1-HEX1-HEX1-HEX1 HEX2-HEX2-HEX2-HEX2
	// 
	// 
	
	// Define colors block diagram.
	init_pair(BLOCK_SAME,      COLOR_WHITE, COLOR_BLUE);
	init_pair(BLOCK_DIFFERENT, COLOR_WHITE, COLOR_RED);  
	init_pair(BLOCK_EMPTY,     COLOR_BLACK, COLOR_CYAN); 
	init_pair(BLOCK_ACTIVE,    COLOR_BLACK, COLOR_YELLOW); 
	init_pair(TITLE_BAR,       COLOR_BLACK, COLOR_WHITE); 
	
	// Generate the offset markers.
	// Calculate parameters for the offset.
	int offset_char_size = calculate_max_offset_characters(*file_offset, 
                                              width, height - 5);
	int hex_width = width - offset_char_size - 3 - SIDE_MARGIN * 2;
	int offset_jump = (hex_width - (hex_width % 4)) / 4;   
	
	// Display the hex offsets on the left.         
	display_offsets(3, height-2, offset_jump, offset_char_size, 
	                *file_offset);

	// Generate HEX characters
	// Seek to initial offset.
	draw_hex_data(3, height - 2, file_one, file_two,
	              *file_offset, offset_char_size, offset_jump, display);
	
	
	// Write the file titles.
	display_file_names(2, file_one, file_two, offset_char_size,
	                   offset_jump);
	                   
	return;
}

// #####################################################################
// ##                    GENERATE SCREEN VIEW                         ##
// #####################################################################

void generate_screen(struct file file_one, struct file file_two, 
                        char mode, unsigned long *file_offset,
                        int width, int height, unsigned long 
                        bytes_per_block, unsigned long
                        largest_file_size, char *block_cache,
                        int total_blocks, int blocks_with_excess_byte,
                        unsigned long *offset_index, int display) 
{		
	// Clear the window.
	clear();
	
	// Generate the title bar.
    generate_titlebar(file_one, file_two, *file_offset, width, height,
                      mode, display);
    
	// Generate the window contents according to the mode we're in.
	if (mode == OVERVIEW_MODE)
		generate_overview(file_one, file_two, file_offset, 
		                  largest_file_size, bytes_per_block,
		                  width, height, block_cache, total_blocks,
		                  blocks_with_excess_byte, offset_index, 
		                  display); 
		                  
	else if (mode == HEX_MODE)
		generate_hex(file_one, file_two, file_offset, 
		                  largest_file_size, bytes_per_block,
		                  width, height, block_cache, total_blocks,
		                  blocks_with_excess_byte, offset_index, 
		                  display); 
	return;

}

// #####################################################################
// ##                       MAIN FUNCTION                             ##
// #####################################################################

void start_gui(struct file file_one, struct file file_two,
               unsigned long largest_file_size)
{	
	// Initiate variables
	unsigned long file_offset = 0;   // File offset.
	char mode = OVERVIEW_MODE;       // Display mode.
	int key_pressed;                 // What key is pressed.
	char *block_cache;               // A quick comparison overview.
	block_cache = NULL;              // Initialize.
	unsigned long *offset_index;     // Keep track of offsets per block.
	offset_index = NULL;             // Initialize.
	int display = HEX_VIEW;          // ASCII vs. HEX mode.
	MEVENT mouse;                    // Mouse event struct.
	WINDOW *main_window;             // Pointer for main window.

	// Initiate the display. 
	main_window = initscr(); // Start curses mode.
	start_color();           // Enable the use of colours.
	raw();                   // Disable line buffering.
	noecho();                // Don't echo while we get characters.
	keypad(stdscr, TRUE);    // Enable capture of arrow keys.
	curs_set(0);             // Make the cursor invisible.
	mousemask(ALL_MOUSE_EVENTS, NULL); // Get all mouse events.

	// Calculate values based on window dimensions.
	int width, height, total_blocks, blocks_with_excess_byte;
	unsigned long bytes_per_block;
	calculate_dimensions(&width, &height, &total_blocks, 
	                     &bytes_per_block, largest_file_size,
	                     &blocks_with_excess_byte);
	                     
	// Compile the block/offset cache. The block cache contains an index
	// of what the general differences are between the two compared
	// files. It exists to avoid re-comparing the two files every time
	// the screen is regenerated. The offset cache keeps track of what
	// the offsets are for each block in the block diagram, as they
	// may be uneven.

	block_cache = generate_blocks(file_one, file_two, block_cache, 
	                              total_blocks, bytes_per_block,
	                              blocks_with_excess_byte);
	offset_index = generate_offsets(offset_index, total_blocks, 
	                          bytes_per_block, blocks_with_excess_byte);
            
	// Generate initial screen contents.
	generate_screen(file_one, file_two, mode, &file_offset,
	                width, height, bytes_per_block,
	                largest_file_size, block_cache, total_blocks,
	                blocks_with_excess_byte, offset_index, display);	
 
	// Wait for user-keypresses and react accordingly.
	while((key_pressed = wgetch(main_window)) != 'q')
	{	
		int rows; 
		if (mode == OVERVIEW_MODE) rows = 5; else rows = height - 5;
		
		switch(key_pressed)
		{				
			// Move left/right/down/up on the blog diagram in overview
			// mode.

			case KEY_LEFT:
				if (mode == OVERVIEW_MODE)
				file_offset = calculate_offset(file_offset, 
				              offset_index, width, total_blocks,
				              LEFT_BLOCK, mode, largest_file_size, 5);
				break;
			case KEY_RIGHT:
				if (mode == OVERVIEW_MODE)
				file_offset = calculate_offset(file_offset, 
				              offset_index, width, total_blocks,
				              RIGHT_BLOCK, mode, largest_file_size, 5);
				break;
			case KEY_UP:
				if (mode == OVERVIEW_MODE)
				file_offset = calculate_offset(file_offset, 
				              offset_index, width, total_blocks,
				              UP_ROW, mode, largest_file_size, 5);
				else if (mode == HEX_MODE)              
				file_offset = calculate_offset(file_offset, 
				              offset_index, width, total_blocks,
				              UP_LINE, mode, largest_file_size, rows);
				break;
			case KEY_DOWN:
				if (mode == OVERVIEW_MODE)
				file_offset = calculate_offset(file_offset, 
				              offset_index, width, total_blocks,
				              DOWN_ROW, mode, largest_file_size, 5);
				else if (mode == HEX_MODE)
				file_offset = calculate_offset(file_offset, 
				              offset_index, width, total_blocks,
				              DOWN_LINE, mode, largest_file_size, rows);
				break;	
			case KEY_NPAGE:
				file_offset = calculate_offset(file_offset, 
				              offset_index, width, total_blocks,
				              DOWN_LINE, mode, largest_file_size, rows);
				break;	
			case KEY_PPAGE:
				file_offset = calculate_offset(file_offset, 
				              offset_index, width, total_blocks,
				              UP_LINE, mode, largest_file_size, rows);
				break;	
			case 'm':
				if (display == ASCII_VIEW) display = HEX_VIEW;
				else display = ASCII_VIEW;
				break;
			case 'v':
				if (mode == OVERVIEW_MODE) mode = HEX_MODE;
				else mode = OVERVIEW_MODE;
				break;
			case KEY_MOUSE:
				if(getmouse(&mouse) == OK) {
					
					// Left single-click.
					if(mouse.bstate & BUTTON1_CLICKED)
						mouse_clicked(&file_offset, offset_index,
									 width, height, total_blocks, &mode,
									 mouse.x, mouse.y, BUTTON1_CLICKED);
					
					// Left double-click.
					if(mouse.bstate & BUTTON1_DOUBLE_CLICKED)
						mouse_clicked(&file_offset, offset_index,
								     width, height, total_blocks, &mode, 
								     mouse.x, mouse.y, 
								     BUTTON1_DOUBLE_CLICKED);
				}
				break;

				
			// Redraw the window on resize. Recaltulate dimensions,
			// and redo the block/offset cache.
			case KEY_RESIZE:
				calculate_dimensions(&width, &height, &total_blocks, 
	                               &bytes_per_block, largest_file_size,
	                               &blocks_with_excess_byte);
				block_cache = generate_blocks(file_one, file_two, 
				            block_cache, total_blocks, bytes_per_block,
				            blocks_with_excess_byte);
				offset_index = generate_offsets(offset_index, 
				               total_blocks, bytes_per_block, 
				               blocks_with_excess_byte);
				break;
		}
		
		generate_screen(file_one, file_two, mode, &file_offset,
	                    width, height, bytes_per_block,
	                    largest_file_size, block_cache, total_blocks,
	                    blocks_with_excess_byte, offset_index, display);	
	}
	
	// End curses mode and exit.
	endwin();
	return;
}
