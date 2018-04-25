#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char Base64Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void saveBufferToFile(char *filename, char *buffer, long int *bufferSize) {
	FILE *encFile;
	if ((encFile = fopen(filename, "w")) != NULL) {
		printf("Saving to file: %s\n", filename);
		fwrite(buffer, 1, *bufferSize, encFile);
		fclose(encFile);
	}
}

char * concatenateString(char *output, char *str1, char *str2) {
	output = (char *) malloc(1 + strlen(str1) + strlen(str2));
	strcpy(output, str1);
	strcat(output, str2);
	
	return output;
}

/**
 * Function sets new line character when position is proper.
 * 		(after 76 characters for Base64)
 */
void newLineChar(char *buffer, long int *pointer, int *modulo) {
	if (((*pointer + 1) % *modulo) == 0) {
		buffer[*pointer] = 10;
		*pointer = *pointer + 1;
	}
}

/**
 * Function for encoding char buffer to Base64 string format.
 *
 * \param[in] 	buffer : file data
 * \param[out] 	base64Buffer : new Base64 buffer
 * \param[in] 	fileSize : how many bytes in file
 * \param[in] 	padding : size of padding for new string
 */
void encodeToBase64(unsigned char *buffer, char *base64Buffer, long int *fileSize, int *padding) {
	unsigned char last;			/* : cutted part of previous byte */
	unsigned char newer;		/* : created base64 byte */
	
	int shiftLast, shiftR;		/* : variables for bits shifting */
	
	/* arrays with values for sequence of shift */
	int seqR[3] = { 2, 4, 6};	
	int seqL[3] = { 0, 6, 4};

	/**
	* indicates which byte from the three-bytes packet is current
	* (while loop) possible values are 0, 1 and 2
	*/ 
	int pos;				

	long int pointer = 0;		/* : indicates current position in new Base64 string */
	int modulo = 77;			/* : 77 element in string have to be a new line character */
	
	/**
	* main loop for encoding operations
	* (each bytes from file)
	*/ 
	for (int i=0; i<*fileSize; i++) {
		newLineChar(base64Buffer, &pointer, &modulo);
		
		pos = i % 3;
		
		/**
		* set shift right and last byte shift left values
		*/ 
		shiftR = seqR[(i) % 3];
		shiftLast = seqL[(i) % 3];

		/**
		* shift current buffer byte
		*/ 
		newer = buffer[i] >> shiftR;
		
		/**
		* when three-bytes packet's current position isn't 0, get part of the last byte
		* and concatenate with current buffer (shifted) byte
		*/ 
		if (pos != 0) {
			last = buffer[i-1] << shiftLast;
			last = last >> 2;
			newer = (newer | last);
		}
		
		/**
		* (we have an encoded byte in 'newer' variable)
		* write Base64 character to output buffer and
		* then increment pointer
		*/ 
		base64Buffer[pointer] = Base64Alphabet[newer];
		pointer = pointer + 1;
		
		/**
		* little differents in opperations when last byte form three-bytes 
		* packet is just encoded or when is the last byte from file data buffer
		*/ 
		if (pos == 2 || ((i+1) == *fileSize)) {
			newLineChar(base64Buffer, &pointer, &modulo);
		
			newer = buffer[i];
			
			if (pos == 1) {
				newer = newer << 2;
			} else if (pos == 0) {
				newer = newer << 4;
			}
			
			/**
			* in this case we make our operations only on current byte
			* of buffer so have to set last two bits to 0 value
			*/ 
			newer = newer&=~(1<<7);
			newer = newer&=~(1<<6);
			
			base64Buffer[pointer] = Base64Alphabet[newer];
			pointer = pointer + 1;
		}
	}
	
	/**
	* adds padding characters at the end of buffer
	*/
	for (int i=0; i<*padding; i++) {
		base64Buffer[pointer] = '=';
		pointer = pointer + 1;
	}
}

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "Error: Bad arguments!\n");
		return 1;
	}
	
	/**
	* opening file, getting size and validating
	*/ 
		FILE *fp;
	
		if ((fp = fopen(argv[1], "rb")) == NULL) {
			fprintf(stderr, "Error: Can't open file!\n");
			return 1;
		}
		printf("Opening file: %s.\n", argv[1]);
	
		fseek(fp, 0, SEEK_END);
		long int fileSize = ftell(fp);
		fseek(fp, 0, SEEK_SET);
	
		if (fileSize == 0) {
			fprintf(stderr, "Error: File is empty!\n");
			return 1;
		}
		
		
	unsigned char *buffer = NULL;	/* : pointer for data of file buffer */
	char *base64Buffer = NULL;		/* : pointer for new Base64 string buffer */
	
	/**
	* saving data of file to memory (allocating)
	*/ 
		buffer = (unsigned char*)malloc((fileSize)*sizeof(unsigned char));
	
		printf("Saving to memory: %ldB.\n", fileSize);
		fread(buffer, fileSize, 1, fp);
		fclose(fp);
		printf("File is closed.\n\n");


	/**
	* specifying the size for new Base64 characters buffer
	*/ 
	long int countedBytes = (fileSize/3) * 3;
	long int base64BufferSize = countedBytes + (1*(countedBytes/3));

	/**
	* specifying Base64 padding size if needed
	*/ 
		int padding = 0;
		if (countedBytes != fileSize) {
			int diff = fileSize - countedBytes;
	
			padding = 3 - diff;
			printf("Bytes indivisible by 3.\n");
			printf("Padding: %i.\n\n", padding);
		
			base64BufferSize = base64BufferSize + (diff + 1) + padding;
		}
	
	/**
	* specifying how many new-line characters will be set
	* (update size of Base64 string buffer)
	*/ 
	long int newLinesSize = (base64BufferSize/76);
	base64BufferSize = base64BufferSize + newLinesSize;
	
	/**
	* allocating memory for Base64 string buffer
	*/ 
	printf("Allocating %ldB memory for Base64 string.\n", base64BufferSize);
	base64Buffer = (char *)malloc((base64BufferSize)*sizeof(char));

	printf("Encoding data.\n");
	encodeToBase64(buffer, base64Buffer, &fileSize, &padding);
	free(buffer);

	char *newFilename;
	newFilename = concatenateString(newFilename, argv[1], (char *)".b64");

	saveBufferToFile(newFilename, base64Buffer, &base64BufferSize);
	
	free(newFilename);
	free(base64Buffer);
	
	return 0;
}
