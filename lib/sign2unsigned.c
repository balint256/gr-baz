/* Modified from http://www.swigerco.com/gnuradio/plotting.html */
/* convert raw 2-byte data to numerical */

#include <stdio.h>

FILE *input_file, *output_file;
int i;
int j;

int main(int argc, char *argv[]) {

if ( argc < 3 ) {
   printf("Usage: raw2num <input file> <output file> [bias] [file offset] [number of numbers]\n");
   printf("Exampel: ./raw2num adc_data adc_num 0 50000 100000\n");
   return 1;
   }

if ( (input_file = fopen(argv[1],"r")) == NULL ) {
        perror("Input file");
        return 1;
        }
if ( (output_file = fopen(argv[2],"w")) == NULL ) {
        perror("Output file");
        return 1;
        }

int bias = 0;
if (argc >= 4)
	bias = atoi(argv[3]);

if (argc >= 5) 
	fseek(input_file,atoi(argv[4]),SEEK_SET);

int total = -1;
if (argc >= 6)
	total = atoi(argv[5]);

for (i=0;(total == -1) || (i < total);i++) {
    fread(&j,2,1,input_file);
    if (j>32767)
       j-=32768;
    else
       j+=32768;
    j+=bias;
    fwrite(&j,2,1,output_file);
    }

fclose(input_file);
fclose(output_file);
return 0;
}

