#include <ctype.h> /* isspace() */
#include <stdio.h> /* fseek() */
#include <stdlib.h> /* malloc() */
#include <string.h> /* memset() */

/* #define DEBUG */

#include "jk_lib.h"
#include "iniparser.h"

Tiniparser *new_iniparser(char *filename) {
	FILE *tmp;
	tmp = fopen(filename, "r");
	if (tmp) {
		Tiniparser *ip = malloc(sizeof(Tiniparser));
		ip->filename = strdup(filename);
		ip->fd = tmp;
		DEBUG_MSG("new_iniparser, ip=%p for filename %s\n",ip,filename);
		return ip;
	}
	return NULL;
}

void iniparser_close(Tiniparser *ip) {
	fclose(ip->fd);
	free(ip->filename);
	free(ip);
}

char *iniparser_next_section(Tiniparser *ip, char *buf, int buflen) {
	int sectionNameChar=0, sectionStart=0;
	while (!feof(ip->fd)){
		char ch=fgetc(ip->fd);
		if (!sectionStart && ch=='[') {
			DEBUG_MSG("Section begins.\n");
			sectionStart=1;
		} else if (sectionStart && ch != ']') {
			buf[sectionNameChar] = ch;
			sectionNameChar++;
/*			DEBUG_MSG("added '%c' to sectionname\n",ch);*/
		} else if (sectionStart && sectionNameChar != 0 && ch==']') {
			buf[sectionNameChar] = '\0';
			DEBUG_MSG("Found section name end, in section, found [%s]\n", buf);
			return buf;
		}
	}
	return NULL;
}
/* test if section 'section' is available, and leaves the filepointer at the end of the section name */
int iniparser_has_section(Tiniparser *ip, const char *section) {
	char buffer[256], *found;
	fseek(ip->fd,0,SEEK_SET);
	while (found = iniparser_next_section(ip, buffer, 256)) {
		DEBUG_MSG("comparing %s and %s\n",section,found);
		if (strcmp(found, section)==0) {
			return 1;
		}
	}
	return 0;
}

int iniparser_get_string_at_position(Tiniparser*ip, const char *section, const char *key, unsigned int position, char *buffer, int bufferlen) {
	char ch;
	int sectionNameChar=0, keyNameChar=0, bufferChar=0;
	int inSection=0, sectionStart=0, foundKey=0;
	fseek(ip->fd,position,SEEK_SET);
	DEBUG_LOG("iniparser_get_string_at_position, looking for key %s in section %s, starting at pos %d\n",key,section,position);
	while (!feof(ip->fd)){
		ch=fgetc(ip->fd);
		if (!sectionStart && ch=='['){
			if (inSection){
				break;
			}
			DEBUG_MSG("Section begins. Looking for [%s]\n", section);
			sectionStart=1;
		} else if (sectionStart && ch==section[sectionNameChar]){
			DEBUG_MSG("Matched section name character: %c\n", ch);
			sectionNameChar++;
		} else if (sectionStart && sectionNameChar != 0 && ch==']'){
			DEBUG_MSG("Found section name end, in section, found [%s]\n", section);
			sectionStart=0;
			inSection=1;
			sectionNameChar=0;
		} else if (sectionStart){
			DEBUG_MSG("Oops, wrong section guess it's not [%s]\n", section);
			sectionStart=0;
			sectionNameChar=0;
		}
		if (inSection && !foundKey && ch==key[keyNameChar]){
			DEBUG_MSG("Found a letter of the key: %c\n", ch);
			keyNameChar++;
		} else if (inSection && !foundKey && keyNameChar != 0 && isspace(ch)) {
			DEBUG_MSG("found a space, we ignore spaces when we are looking for the key\n");
		} else if (inSection && !foundKey && keyNameChar != 0 && ch == '='){
			DEBUG_MSG("Found the end of the key, found the key %s=\n", key);
			foundKey=1;
		} else if (inSection && keyNameChar != 0 && !foundKey){
			DEBUG_MSG("Oops, wrong key, guess this key isn't %s= (ch=%c)\n", key,ch);
			foundKey=0;
			keyNameChar=0;
		} else if (inSection && foundKey && (ch==13 || ch==10 || ch==';')){
			DEBUG_MSG("Invalid character or comment start, done with key %s=\n", key);
			foundKey=0;
			break;
		} else if (inSection && foundKey && bufferChar < bufferlen){
			DEBUG_MSG("In the section, found the key, getting the letters: %c\n", ch);
			buffer[bufferChar++]=ch;
		} else if (inSection && foundKey && bufferChar >= bufferlen){
			DEBUG_MSG("Hit the buffer max, EOM, done w/ key %s=\n", key);
			break;
		}
	}
	buffer[bufferChar]='\0';
	return bufferChar;
}

int iniparser_get_int_at_position(Tiniparser *ip, const char *section, const char *key, unsigned int position) {
	unsigned int buffer=0;
	int i;
	char data[25];
	memset(data, 0, 25);
	if (iniparser_get_string_at_position(ip, section, key, position, data, 25)==-1){
		return -1;
	}
	for (i=0; data[i]; i++){
		if (data[i] < '0' || data[i] > '9'){
			int nextValid=i+1;
			while (data[nextValid] && (data[nextValid] < '0' || data[nextValid] > '9')){
				nextValid++;
			}
			if (!data[nextValid]){
				break;
			}
			char tmp=data[i];
			data[i]=data[nextValid];
			data[nextValid]=tmp;
		}
	}
	sscanf(data, "%u", &buffer);
	return buffer;
}

/*
FILE *ftruncate1(FILE *fp, const char *filename, const char *mode, unsigned int newLen){
	FILE *beginning=tmpfile();
	int amountRead=0;
	int chunkSize=1024;
	int amountWrote=0;
	fseek(fp, 0, SEEK_SET);
	fseek(beginning, 0, SEEK_SET);
	while (amountRead != newLen){
		char *data=NULL;
		if (amountRead+chunkSize > newLen){
			chunkSize=newLen-amountRead;
		}
		data= malloc(chunkSize);
		amountRead+=fread(data, sizeof(char), chunkSize, fp);
		fwrite(data, sizeof(char), chunkSize, beginning);
		free(data);
	}
	fclose(fp);
	fp=fopen(filename, "w");
	chunkSize=1024;
	fseek(beginning, 0, SEEK_SET);
	fseek(fp, 0, SEEK_SET);
	while (amountWrote != newLen){
		char *data=0;
		if (amountWrote+chunkSize > newLen){
			chunkSize=newLen-amountWrote;
		}
		data=malloc(chunkSize);
		amountWrote+=fread(data, sizeof(char), chunkSize, beginning);
		fwrite(data, sizeof(char), chunkSize, fp);
		free(data);
	}
	fclose(beginning);
	fclose(fp);
	fp=fopen(filename, mode);
	fseek(fp, 0, SEEK_END);
	return fp;
}*/

int readKeyValueLen(const char *section, const char *key, const char *file){
	FILE *fp;
	char ch;
	int sectionNameChar=0, keyNameChar=0;
	int valueLength=0;
	int inSection=0, sectionStart=0, foundKey=0;
	fp=fopen(file, "r");
	if (!fp){
		return 0;
	}
	while (!feof(fp)){
		ch=fgetc(fp);
		if (!sectionStart && ch=='['){
			if (inSection){
				break;
			}
			sectionStart=1;
		} else if (sectionStart && ch==section[sectionNameChar]){
			sectionNameChar++;
		} else if (sectionStart && sectionNameChar != 0 && ch==']'){
			sectionStart=0;
			inSection=1;
			sectionNameChar=0;
		} else if (sectionStart){
			sectionStart=0;
			sectionNameChar=0;
		}
		
		if (inSection && !foundKey && ch==key[keyNameChar]){
			keyNameChar++;
		} else if (inSection && !foundKey && keyNameChar != 0 && ch == '='){
			foundKey=1;
		} else if (inSection && keyNameChar != 0 && !foundKey){
			foundKey=0;
			keyNameChar=0;
		} else if (inSection && foundKey && (ch==13 || ch==10 || ch==';')){
			foundKey=0;
			break;
		} else if (inSection && foundKey){
			valueLength++;
		}
	}
	fclose(fp);
	return valueLength;
}


/*
void writeKey1(const char *section, const char *key, int value, const char *file){
	char *data=malloc(10);
	snDEBUG_MSG(data, 10, "%d%c", value, 0);
	writeKey2(section, key, data, file);
}

void writeKey2(const char *section, const char *key, const char *value, const char *file){
	FILE *fp;
	char ch;
	int sectionNameChar=0, keyNameChar=0;
	int currentValueLen=readKeyValueLen(section, key, file);
	int inSection=0, sectionStart=0, foundKey=0;
	fp=fopen(file, "r+");
	if (!fp){
		/File doesn't exist?/
		fp=fopen(file, "w+");
		if (!fp){
			DEBUG_MSG("Unable to open file %s\n", file);
			return;
		}
	}
	while (!feof(fp)){
		ch=fgetc(fp);
		if (!sectionStart && ch=='['){
			if (inSection){
				break;
			}
			sectionStart=1;
		}
		else if (sectionStart && ch==section[sectionNameChar]){
			sectionNameChar++;
		}
		else if (sectionStart && sectionNameChar != 0 &&  ch==']'){
			inSection=1;
			sectionStart=0;
		}
		else if (sectionStart){
			sectionStart=0;
			sectionNameChar=0;
		}

		if (inSection && ch==key[keyNameChar]){
			keyNameChar++;
		}
		else if (inSection && keyNameChar != 0 && ch == '='){
			foundKey=1;
		}
		else if (inSection && keyNameChar != 0){
			foundKey=0;
			keyNameChar=0;
		}

		if (inSection && foundKey){
			UINT curPos=0;
			long fileSize=0;
			char *data=0;
			/Found the section name and key name, get the number of bytes left in the file, read them all, write the new stuff
			 *in, then re-write the old stuff.
			 *DEBUG_MSG("Found the section and the key.  Changing the value. [%s]\n%s=%s\n", section, key, value);
			 /
			curPos=ftell(fp);
			fseek(fp, 0, SEEK_END);
			fileSize=ftell(fp);
			fseek(fp, curPos, SEEK_SET);
			/DEBUG_MSG("curpos(%d); filesize(%d); currentValueLen(%d); nextchar(%c);\n", curPos, fileSize, currentValueLen, fgetc(fp));/
			fileSize-=currentValueLen+curPos;
			fseek(fp, -fileSize, SEEK_END);
			data=malloc(fileSize+1);
			memset(data, 0, fileSize+1);
			/DEBUG_MSG("Reading %d bytes of data from the file\n", fileSize);/
			fileSize=fread(data, sizeof(char), fileSize, fp);
			/DEBUG_MSG("Read a total of %d bytes.  Data gathered:\n\n|%s|\n\n", fileSize, data);/
			ftruncate1(fp, file, "r+", curPos);
			/DEBUG_MSG("Reset file pointer\nWriting new value '%s'\n", value);/
			fwrite(value, sizeof(char), strlen(value), fp);
			/DEBUG_MSG("Wrote new value\nWriting rest of file\n");/
			fwrite(data, sizeof(char), fileSize, fp);
			free(data);
			break;
		}
	}
	if the section was never found, make it at the end of the file and add the key under it.
	if (!inSection && !foundKey){
		/Probably didn't find the key, so we will add it.
		 *DEBUG_MSG("Didn't find the section or the key.  Adding [%s]\n%s=%s\n", section, key, value);
		 /
		fseek(fp, 0, SEEK_END);
		fwrite("\n\n[", sizeof(char), 3, fp);
		fwrite(section, sizeof(char), strlen(section), fp);
		fwrite("]\n", sizeof(char), 2, fp);
		fwrite(key, sizeof(char), strlen(key), fp);
		fwrite("=", sizeof(char), 1, fp);
		fwrite(value, sizeof(char), strlen(value), fp);
		fwrite("\n", sizeof(char), 1, fp);
	}
	if (inSection && !foundKey){
		UINT curPos=0;
		long fileSize=0;
		int eof=0;
		char *data=0;
		/Probably didn't find the key, and hit eof or eos, so we will add it.
		 *DEBUG_MSG("Found the section, but not the key.  Adding the key and value [%s]\n%s=%s\n", section, key, value);
		 /
		curPos=ftell(fp);
		eof=feof(fp);
		if (ch=='['){
			curPos--;
		}

		fseek(fp, 0, SEEK_END);
		fileSize=ftell(fp)-curPos;
		data=malloc(fileSize+1);
		memset(data, 0, fileSize+1);
		fseek(fp, curPos, SEEK_SET);
		fileSize=fread(data, sizeof(char), fileSize, fp);
		data[fileSize]=0;
		fseek(fp, curPos, SEEK_SET);

		fwrite(key, sizeof(char), strlen(key), fp);
		fwrite("=", sizeof(char), 1, fp);
		fwrite(value, sizeof(char), strlen(value), fp);
		fwrite("\n", sizeof(char), 1, fp);

		/DEBUG_MSG("Writting back data(len=%d;wlen=%ld):\n'%s'\n", strlen(data), fileSize, data);/
		fwrite(data, sizeof(char), fileSize, fp);
		free(data);
	}

	fclose(fp);
}
*/
