// compressROM - simple ZX Spectrum ROM compression
// Copyright (c) 2023, Tom Dalby
// 
// compressROM is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// compressROM is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with compressROM. If not, see <http://www.gnu.org/licenses/>. 
//
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "rom_descriptor.h"

void error(int errorcode);
uint16_t simplelz(uint8_t* fload,uint8_t* store,uint16_t filesize);
void printOut(FILE *fp, uint8_t *buffer, uint32_t filesize, char *name);
int replaceChar(char *str, char orig, char rep);
void toUpperStr(char *dst, const char *src);
void toLowerStr(char *dst, const char *src);

// convert binary ROM file to compressed const uint8_t array, pads 8kB ROMs with zeros if needed
int main(int argc, char* argv[]) {
	if (argc < 2) {
        fprintf(stdout,"Usage compressrom <options> infile\n");
		fprintf(stdout,"  Options:\n");
		fprintf(stdout,"    -p pad space to 16kB, for 8kB ROMs only\n");
		fprintf(stdout,"    -b produce binary file\n");
		fprintf(stdout,"    -c check compression\n");
		fprintf(stdout,"    -d do not compress just create header, also ignores size check\n");
		fprintf(stdout,"    -t <ROM title>\n");
        exit(0);
    }
	// check for options
	bool padSpace=false,binaryOn=false,testCompression=false,noCompression=false;
    char *romTitle = NULL;
    uint32_t argNum = 1;
    while (argv[argNum][0] == '-') {
        if (argv[argNum][1] == 'p') {
            padSpace = true;
        } else if (argv[argNum][1] == 'b') {
            binaryOn = true;
        } else if (argv[argNum][1] == 'c') {
            testCompression = true;
        } else if (argv[argNum][1] == 'd') {
            noCompression = true;
        } else if (argv[argNum][1] == 't') {
            argNum++;
            romTitle = argv[argNum];
        } else {
            error(0);
        }
        argNum++;
    }

    // open files
    FILE *fp_in,*fp_out;
    if ((fp_in = fopen(argv[argNum], "rb")) == NULL) {
        fprintf(stdout, "'%s'\n", argv[argNum]);
        error(1);
    }
    fseek(fp_in,0,SEEK_END); // jump to the end of the file to get the length
	uint16_t filesize=ftell(fp_in); // get the file size
    rewind(fp_in);
    //
	if(padSpace==true&&filesize!=8192) error(2); // only pad 8kB ROMs
    uint8_t *readin;
	uint32_t i,j;
	if(testCompression==false) {
		if(noCompression==false) {
    		if(filesize%8192!=0) error(2); // doesn't seem to be a ROM so error	
		}
		if(filesize>16384) {
			if((readin=malloc(filesize))==NULL) error(3); // cannot allocate memory
			for(i=0;i<filesize;i++) readin[i]=0x00; // clear readin buffer
		} else {
			if((readin=malloc(16384))==NULL) error(3); // cannot allocate memory
			for(i=0;i<16384;i++) readin[i]=0x00; // clear readin buffer, has the affect of padding 8kB ROMs			
		}
	} else {
		if((readin=malloc(filesize))==NULL) error(3); // cannot allocate memory
	}
    if(fread(readin,sizeof(uint8_t),filesize,fp_in)<filesize) error(4); // cannot read enough bytes in
    fclose(fp_in);
    //
	if(testCompression==false) {
		uint8_t *comp;	
		if((comp=malloc(filesize+(filesize/32)))==NULL) error(3); // cannot allocate memory for compression
		uint16_t compsize;
		if(padSpace==true) filesize=16384;
		if(noCompression==false) {
			compsize=simplelz(readin,comp,filesize);
		} else {
			compsize=filesize;
			for(i=0;i<filesize;i++) comp[i]=readin[i];
		}
		free(readin);    
		//
		char outName[256],headerName[256];
		i=0;
		uint32_t j=0;
		if(argv[argNum][j]>='0'&&argv[argNum][j]<='9') headerName[j++]='_';
		do {
			outName[i]=argv[argNum][i];
			if(argv[argNum][i]>='A'&&argv[argNum][i]<='Z') {
				headerName[j++]=argv[argNum][i]+32;
			} else if((argv[argNum][i]>='0'&&argv[argNum][i]<='9')||
					(argv[argNum][i]>='a'&&argv[argNum][i]<='z')||
					argv[argNum][i]=='_') {
				headerName[j++]=argv[argNum][i];
			}
			i++;
		} while(i<252&&argv[argNum][i]!='.');
		outName[i]=headerName[j]='\0';
		if(binaryOn) {
			strcat(outName,".bin");
			if ((fp_out=fopen(outName,"wb"))==NULL) error(5);
			fwrite(comp,sizeof(uint8_t),compsize,fp_out);
			fclose(fp_out);
		} else {
            char description[33] = "";
            strncpy(description, outName, 33);
            replaceChar(description, '-', ' ');
            replaceChar(description, '_', ' ');

            char filename[256] = "";
            toLowerStr(filename, outName);
            strcat(filename,".h");
            replaceChar(filename, '-', '_');
            replaceChar(filename, ' ', '_');
            replaceChar(filename, '/', '_');
            replaceChar(filename, '\'', '_');

            if ((fp_out = fopen(filename, "wb")) == NULL)
                error(5);

            outName[strlen(outName) - 2] = '\0';
            replaceChar(outName, '.', '_');
            filename[strlen(filename) - 2] = '\0';
            replaceChar(filename, '.', '_');

            char outHeaderGuard[256] = "";
            toUpperStr(outHeaderGuard, filename);

            fprintf(fp_out, "#ifndef %s_H\n", outHeaderGuard);
            fprintf(fp_out, "#define %s_H\n\n", outHeaderGuard);
            fprintf(fp_out, "#include \"../rom_descriptor.h\"\n\n");

            fprintf(fp_out, "#pragma startup register%s\n", headerName);
            fprintf(fp_out, "void register%s() __attribute__((constructor));\n\n", headerName);

            printOut(fp_out, comp, compsize, headerName);

            fprintf(fp_out, "RomDescriptor %sRomDescriptor = {\n", headerName);
            fprintf(fp_out, "\t\"%s\",\n", (romTitle != NULL) ? romTitle : description);
            fprintf(fp_out, "\t%d,\n", compsize);
            fprintf(fp_out, "\tBUILT_IN_ROM,\n");
            fprintf(fp_out, "\t%s,\n", headerName);
            fprintf(fp_out, "\t-1,\n");
            fprintf(fp_out, "\tNULL\n");
            fprintf(fp_out, "};\n\n");

            fprintf(fp_out, "void register%s() {\n", headerName);
            fprintf(fp_out, "\tregisterRom(&%sRomDescriptor);\n", headerName);
            fprintf(fp_out, "}\n\n");

            fprintf(fp_out, "#endif // %s_H\n", outHeaderGuard);

 			fclose(fp_out);
		}
    	free(comp);		
	} else {
    	i=j=0;
    	uint8_t c;
		do {
			c=readin[j++];
			if(c<128) {
				i+=(c+1);
				j+=(c+1);
			}
			else if(c>128) {
				j++;
				i+=c-126;
			}
		} while(c!=128);	
		if(i==16384||i==8192) {
			fprintf(stdout,"pass (%d)\n",i);
		} else {
			fprintf(stdout,"fail (%d)\n",i);
		}
		free(readin);    
	}
    return 0;
}

/**
 * Print out the binary in a standard header format
 *
 * @param fp target file
 * @param buffer source buffer
 * @param filesize file size
 * @param name name of the file
 */
void printOut(FILE *fp, uint8_t *buffer, uint32_t filesize, char *name) {
    fprintf(fp, "const uint8_t %s[] = {\n\t", name);
    for (uint32_t i = 0; i < filesize; i++) {
        fprintf(fp, "0x%02x", buffer[i]);
        if (i < filesize - 1) {
            fprintf(fp, ",");
        }
        if (((i + 1) % 16) == 0 && i != 0) {
            fprintf(fp, "\n\t");
        } else {
            fprintf(fp, " ");
        }
    }
    fprintf(fp, "\n}; // %d bytes\n\n", filesize);
}

//
// very simple lz with 256byte backward look
// 
// x=128+ then copy sequence from x-offset from next byte offset 
// x=0-127 then copy literal x+1 times
// minimum sequence size 2
uint16_t simplelz(uint8_t* fload,uint8_t* store,uint16_t filesize)
{
	uint16_t i;
	uint8_t * store_p, * store_c;
	uint8_t litsize = 1;
	uint16_t repsize, offset, repmax, offmax;
	store_c = store;
	store_p = store_c + 1;
	//
	i = 0;
	*store_p++ = fload[i++];
	do {
		// scan for sequence
		repmax = 2;
		if (i > 255) offset = i - 256; else offset = 0;
		do {
			repsize = 0;
			while (fload[offset + repsize] == fload[i + repsize] && i + repsize < filesize && repsize < 129) {
				repsize++;
			}
			if (repsize > repmax) {
				repmax = repsize;
				offmax = i - offset;
			}
			offset++;
		} while (offset < i && repmax < 129);
		if (repmax > 2) {
			if (litsize > 0) {
				*store_c = litsize - 1;
				store_c = store_p++;
				litsize = 0;
			}
			*store_p++ = offmax - 1; //1-256 -> 0-255
			*store_c = repmax + 126;
			store_c = store_p++;
			i += repmax;
		}
		else {
			litsize++;
			*store_p++ = fload[i++];
			if (litsize > 127) {
				*store_c = litsize - 1;
				store_c = store_p++;
				litsize = 0;
			}
		}
	} while (i < filesize);
	if (litsize > 0) {
		*store_c = litsize - 1;
		store_c = store_p++;
	}
	*store_c = 128;	// end marker
	return store_p - store;
}

// E00 - bad option
// E01 - cannot open input file
// E02 - incorrect ROM file
// E03 - cannot allocate memory
// E04 - problem reading ROM file
// E05 - cannot open output file
void error(int errorcode) {
	fprintf(stdout, "[E%02d]\n", errorcode);
	exit(errorcode);
}

int replaceChar(char *str, char orig, char rep) {
    char *ix = str;
    int n = 0;
    while ((ix = strchr(ix, orig)) != NULL) {
        *ix++ = rep;
        n++;
    }
    return n;
}

void toUpperStr(char *dst, const char *src) {
    while (*src != 0) {
        (*(dst++) = toupper((unsigned char) *(src++)));
    }
}

void toLowerStr(char *dst, const char *src) {
    while (*src != 0) {
        (*(dst++) = tolower((unsigned char) *(src++)));
    }
}
