#include <ctype.h> /* isspace() */
#include <stdio.h> /* fseek() */
#include <stdlib.h> /* malloc() */
#include <string.h> /* memset() */

/* #define DEBUG */

#ifdef DEBUG
#include <syslog.h>
#endif

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
	unsigned short int inComment = 0;
	char prevch='\0', ch;
	while (!feof(ip->fd)){
		ch=fgetc(ip->fd);
		if (ch == '#' && (prevch == '\n' || prevch=='\0')) {
			DEBUG_MSG("Comment start (%c)\n",ch);
			inComment = 1;
		} else if (ch == '\n' && inComment == 1) {
			DEBUG_MSG("Comment stop (%c)\n",ch);
			inComment = 0;
		} else if (inComment == 1) {
			/* do nothing if in comment */
			DEBUG_MSG("do nothing, we're in a comment (%c)\n",ch);
		} else if (!sectionStart && ch=='[') {
			DEBUG_MSG("Section begins (%c)\n",ch);
			sectionStart=1;
		} else if (sectionStart && ch != ']') {
			buf[sectionNameChar] = ch;
			sectionNameChar++;
			DEBUG_MSG("added '%c' to sectionname\n",ch);
		} else if (sectionStart && sectionNameChar != 0 && ch==']') {
			buf[sectionNameChar] = '\0';
			DEBUG_MSG("Found section name end, in section, found [%s]\n", buf);
			return buf;
		}
		prevch = ch;
	}
	return NULL;
}
/* test if section 'section' is available, and leaves the filepointer at the end of the section name */
unsigned short int iniparser_has_section(Tiniparser *ip, const char *section) {
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

unsigned int iniparser_get_string_at_position(Tiniparser*ip, const char *section, const char *key, unsigned int position, char *buffer, int bufferlen) {
	char ch, prevch='\0';
	unsigned int sectionNameChar=0, keyNameChar=0, bufferChar=0;
	unsigned short int inSection=0, sectionStart=0, foundKey=0, inComment=0;
	fseek(ip->fd,position,SEEK_SET);
	DEBUG_LOG("iniparser_get_string_at_position, looking for key %s in section %s, starting at pos %d\n",key,section,position);
	while (!feof(ip->fd)){
		ch=fgetc(ip->fd);
		if (ch == '#' && (prevch == '\n' || prevch == '\0')) {
			inComment = 1;
		} else if (ch == '\n' && inComment == 1) {
			inComment = 0;
		} else if (inComment == 1) {
			/* we do nothing if we are inside a comment */
		} else if (!sectionStart && ch=='['){
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
		} else if (inSection && foundKey && ch=='\n'){
			DEBUG_MSG("End of line, done with key %s=\n", key);
			foundKey=0;
			break;
		} else if (inSection && foundKey && bufferChar < bufferlen){
			DEBUG_MSG("In the section, found the key, getting the letters: %c\n", ch);
			buffer[bufferChar++]=ch;
		} else if (inSection && foundKey && bufferChar >= bufferlen){
			DEBUG_MSG("Hit the buffer max, EOM, done w/ key %s=\n", key);
			break;
		}
		prevch = ch;
	}
	buffer[bufferChar]='\0';
	return bufferChar;
}

unsigned int iniparser_get_int_at_position(Tiniparser *ip, const char *section, const char *key, unsigned int position) {
	unsigned int buffer=0;
	int i;
	char data[25];
	memset(data, 0, 25);
	if (iniparser_get_string_at_position(ip, section, key, position, data, 25)==-1){
		return -1;
	}
	for (i=0; data[i]; i++){
		if (data[i] < '0' || data[i] > '9'){
			char tmp;
			int nextValid=i+1;
			while (data[nextValid] && (data[nextValid] < '0' || data[nextValid] > '9')){
				nextValid++;
			}
			if (!data[nextValid]){
				break;
			}
			tmp=data[i];
			data[i]=data[nextValid];
			data[nextValid]=tmp;
		}
	}
	sscanf(data, "%u", &buffer);
	return buffer;
}
/*
int iniparser_value_len(Tiniparser *ip, const char *section, const char *key){
	char ch;
	unsigned int sectionNameChar=0, keyNameChar=0;
	unsigned int valueLength=0;
	unsigned short int inSection=0, sectionStart=0, foundKey=0;
	while (!feof(ip->fd)){
		ch=fgetc(ip->fd);
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
	return valueLength;
}
*/
