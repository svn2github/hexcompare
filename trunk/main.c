#include <stdio.h>
#include "general.h"
#include "gui.h"

char *message[] = {
	"Arguments missing.\n",
	"Usage:\n  hexcompare file1 file2\n",
	"File \"%s\" does not exist.\n"
};

int main(int argc, char *argv[]) 
{
	// Verify that we have enough input arguments.
	if (argc < 2) {
		printf("%s%s", message[0], message[1]);
		return 1;
	}

	// Load in the file names.
	struct file file_one, file_two;
	file_one.name = argv[1];
	if (argc == 2) file_two.name = argv[1];
	else file_two.name = argv[2];

	// Open the files.
	// Present the user with an error message if they cannot be opened.
	if ((file_one.pointer = fopen(file_one.name, "rb")) == NULL) {
		printf(message[2], file_one.name);
		return 1;
	}
	if ((file_two.pointer = fopen(file_two.name, "rb")) == NULL) {
		printf(message[2], file_two.name);
		fclose(file_one.pointer);
		return 1;
	}
	
	// Get the file size
	fseek(file_one.pointer, 0, SEEK_END);
	file_one.size = ftell(file_one.pointer);

	fseek(file_two.pointer, 0, SEEK_END);
	file_two.size = ftell(file_two.pointer);

	// Determine the largest file size
	unsigned long largest_file_size;
	largest_file_size = (file_one.size > file_two.size) ? file_one.size
	                    : file_two.size;
	
	// Initiate the GUI display.
	start_gui(file_one, file_two, largest_file_size);
	
	// Close the files.
	fclose(file_one.pointer);
	fclose(file_two.pointer);
		
	// Clean exit.
	return 0;
};
