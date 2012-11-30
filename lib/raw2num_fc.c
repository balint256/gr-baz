/* Modified from http://www.swigerco.com/gnuradio/plotting.html */
/* convert raw 8-byte float/complex data to numerical two 4-byte float numbers for each complex real/imag pair */

#include <stdio.h>

FILE *input_file, *output_file;
int i;
float j,k;

int main(int argc, char *argv[]) {

if ( argc < 3 ) {
   printf("Usage: raw2num <input file> <output file> [offset] [number of numbers]\n");
   printf("Exampel: ./raw2num adc_data adc_num 65536 32768\n");
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

if (argc >= 4) 
	fseek(input_file,atoi(argv[3]),SEEK_SET);

int total = -1;
if (argc >= 5)
	total = atoi(argv[4]);

for (i=0;(total == -1) || (i < total);i++) {
    if (fread(&j,4,1,input_file) == 0) {
	if (feof(input_file))
	    printf("Input reached EOF\n");
	else
	    printf("Error with input file\n");
	break;
	}
    if (fread(&k,4,1,input_file) == 0) {
	if (feof(input_file))
	    printf("Input reached EOF\n");
	else
	    printf("Error with input file\n");
	break;
	}
    fprintf(output_file,"%d %f %f\n",i,j,k);
    }

fclose(input_file);
fclose(output_file);
return 0;
}
