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

//v1.0 initial release
//v1.1 added header to compressed ROM, limit names to 32chars

void error(int errorcode);
uint16_t simplelz(uint8_t* fload,uint8_t* store,uint16_t filesize);
void printOut(FILE *fp,uint8_t *buffer,uint16_t filesize,char *name,uint8_t cm,char *oname,bool noCompression);

// convert binary ROM file to compressed const uint8_t array, pads 8kB ROMs with zeros if needed
int main(int argc, char* argv[]) {
	if (argc < 2) {
        fprintf(stdout,"Usage compressrom <options> infile <displayname>\n"); 
		fprintf(stdout,"  Options:\n");		
		fprintf(stdout,"    -z create zxc2 compatible ROM\n");
		fprintf(stdout,"    -p pad space to 16kB, for 8kB ROMs only\n");
		fprintf(stdout,"    -b produce binary file instead of header\n");
		fprintf(stdout,"    -c check compression of binary file\n");
		fprintf(stdout,"    -d do not compress just create header, also ignores size check\n");
		fprintf(stdout,"  if no displayname given infile filename will be used\n");
        exit(0);
    }
	// check for options
	bool padSpace=false,binaryOn=false,testCompression=false,noCompression=false;
	uint argNum=1;
	uint8_t whichROM=0;
	while(argv[argNum][0]=='-') {
		if(argv[argNum][1]=='p') {
			padSpace=true;
		} else if(argv[argNum][1]=='b') {
			binaryOn=true;
		} else if(argv[argNum][1]=='c') {
			testCompression=true;
		} else if(argv[argNum][1]=='d') {
			noCompression=true;
		} else if(argv[argNum][1]=='z') {
			whichROM=1;
		} else {
			error(0);
		}
		argNum++;
	}
    // open files
    FILE *fp_in,*fp_out;
	if ((fp_in=fopen(argv[argNum],"rb"))==NULL) error(1);
    fseek(fp_in,0,SEEK_END); // jump to the end of the file to get the length
	uint16_t filesize=ftell(fp_in); // get the file size
    rewind(fp_in);
    //
	if(padSpace==true&&filesize!=8192) error(2); // only pad 8kB ROMs
    uint8_t *readin;
	uint i,j;
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
		char outName[33],headerName[33],fName[256];
		i=0;
		do {
			fName[i]=argv[argNum][i];
			i++;
		} while(i<251&&(argv[argNum][i]!='.'||i<strlen(argv[argNum])-4));
		fName[i] = '\0';
		i=0;
		uint j=0;
		if((argv[argNum][j]>='0'&&argv[argNum][j]<='9')) {
			headerName[j]='_';	// starts with a number
			j++;
		}
		do {
			outName[i]=argv[argNum][i];
			if(j<32) {
				if(argv[argNum][i]>='A'&&argv[argNum][i]<='Z') {
					headerName[j++]=argv[argNum][i]+32;
				} else if((argv[argNum][i]>='0'&&argv[argNum][i]<='9')||
						(argv[argNum][i]>='a'&&argv[argNum][i]<='z')) {
					headerName[j++]=argv[argNum][i];
				} else {
					headerName[j++]='_';
				}
			}
			i++;
		} while(i<32&&(argv[argNum][i]!='.'||i<strlen(argv[argNum])-4));
		outName[i]=headerName[j]='\0';
		if(binaryOn) {
			strcat(fName,".bin");
			if(strcmp(fName,argv[argNum])==0) strcat(fName,"1"); // just in case the same as the input file
			if ((fp_out=fopen(fName,"wb"))==NULL) error(1); 
			fwrite(comp,sizeof(uint8_t),compsize,fp_out);
			fclose(fp_out);
		} else {
			strcat(fName,".h");		
			if ((fp_out=fopen(fName,"wb"))==NULL) error(1); 
			if(noCompression==false) {
				fprintf(fp_out,"// ,%s",headerName);
				for(i=strlen(headerName);i<35;i++) fprintf(fp_out," ");
				fprintf(fp_out,"// xx - %dbytes\n",compsize+34);
			}
			if(argNum<argc-1) {
				printOut(fp_out,comp,compsize,headerName,whichROM,argv[argc-1],noCompression);
			} else {
				printOut(fp_out,comp,compsize,headerName,whichROM,outName,noCompression);
			}
			fclose(fp_out);
		}
    	free(comp);		
	} else {
    	i=0;
		j=34;
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
//
void printOut(FILE *fp,uint8_t *buffer,uint16_t filesize,char *name,uint8_t cm,char *oname,bool noCompression) {
    uint i,j;
    fprintf(fp,"    const uint8_t %s[]={ ",name);
	if(noCompression==false) {
		fprintf(fp,"0x%02x,",cm);	// compatibility mode
		fprintf(fp,"0x00,");	// spare
		j=0;
		do {
			if(j<strlen(oname)) fprintf(fp,"0x%02x,",oname[j]); 
			else fprintf(fp,"0x00,"); 
		} while(++j<32);
		fprintf(fp,"\n");
		for(j=0;j<23+strlen(name);j++) fprintf(fp," ");   
	}
    for(i=0;i<filesize;i++) {
        if((i%32)==0&&i!=0) {
            fprintf(fp,"\n");
            for(j=0;j<23+strlen(name);j++) fprintf(fp," ");   
        }
        fprintf(fp,"0x%02x",buffer[i]);
        if(i<filesize-1) {
            fprintf(fp,",");
        }
    }
	if(noCompression==false) fprintf(fp," };\n");
	else fprintf(fp," };\n // %dbytes",filesize);
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
// E01 - cannot open input/output file
// E02 - incorrect ROM file
// E03 - cannot allocate memory
// E04 - problem reading ROM file
void error(int errorcode) {
	fprintf(stdout, "[E%02d]\n", errorcode);
	exit(errorcode);
}