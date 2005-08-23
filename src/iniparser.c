/*
Copyright (c) 2003, 2004, 2005, Olivier Sessink
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions 
are met:
  * Redistributions of source code must retain the above copyright 
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above 
    copyright notice, this list of conditions and the following 
    disclaimer in the documentation and/or other materials provided 
    with the distribution.
  * The names of its contributors may not be used to endorse or 
    promote products derived from this software without specific 
    prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN 
ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
POSSIBILITY OF SUCH DAMAGE.
*/
#include "config.h"

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
	DEBUG_MSG("iniparser_next_section, looking for next section..\n");
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
			/*DEBUG_MSG("do nothing, we're in a comment (%c)\n",ch);*/
		} else if (!sectionStart && ch=='[') {
			DEBUG_MSG("Section begins (%c)\n",ch);
			sectionStart=1;
		} else if (sectionStart && ch != ']') {
			buf[sectionNameChar] = ch;
			sectionNameChar++;
			DEBUG_MSG("added '%c' to sectionname\n",ch);
		} else if (sectionStart && sectionNameChar != 0 && ch==']') {
			buf[sectionNameChar] = '\0';
			DEBUG_MSG("iniparser_next_section, found '%c', sectionStart=%d, found [%s]\n", ch,sectionStart,buf);
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
	DEBUG_MSG("iniparser_has_section, looking for %s from position %d\n",section,0);
	while ((found = iniparser_next_section(ip, buffer, 256))) {
		DEBUG_MSG("comparing %s and %s\n",section,found);
		if (strcmp(found, section)==0) {
			DEBUG_MSG("iniparser_has_section, return 1\n");
			return 1;
		}
	}
	return 0;
}

unsigned int iniparser_get_string_at_position(Tiniparser*ip, const char *section, const char *key, long position, char *buffer, int bufferlen) {
	char ch, prevch='\0';
	unsigned int sectionNameChar=0, keyNameChar=0, bufferChar=0;
	unsigned short int inSection=0, sectionStart=0, foundKey=0, inComment=0, inWrongKey=0;
	DEBUG_MSG("iniparser_get_string_at_position, looking for key %s in section %s, starting at pos %ld\n",key,section,position);
	if (fseek(ip->fd,position,SEEK_SET) != 0) {
		DEBUG_MSG("there was an error seeking to %ld, current position=%ld, reset to zero\n",position,ftell(ip->fd));
		fseek(ip->fd, 0, SEEK_SET);
	}
	DEBUG_MSG("current position of the stream is %ld\n",ftell(ip->fd));
	while (!feof(ip->fd)){
		ch=fgetc(ip->fd);
		if (ch == '#' && (prevch == '\n' || prevch == '\0')) {
			DEBUG_MSG("inComment!\n");
			inComment = 1;
			continue;
		} else if (ch == '\n' && inComment == 1) {
			inComment = 0;
			continue;
		} else if (inComment == 1) {
			/* we do nothing if we are inside a comment */
		}

		if (!inSection) {
			if (!sectionStart && ch=='['){
				if (inSection){
					/* found nothing */
					break;
				}
				DEBUG_MSG("Section begins. Looking for [%s]\n", section);
				sectionStart=1;
			} else if (sectionStart && ch==section[sectionNameChar]){
				DEBUG_MSG("Matched section name character: %c\n", ch);
				sectionNameChar++;
			} else if (sectionStart && sectionNameChar != 0 && ch==']'){
				DEBUG_MSG("Found section name end, inSection=%d, found [%s]\n",inSection,section);
				sectionStart=0;
				inSection=1;
				sectionNameChar=0;
				DEBUG_MSG("The correct section is now found, now we continue with the key\n");
			} else if (sectionStart){
				DEBUG_MSG("Oops, wrong section, %c is not in position %d of %s\n", ch,sectionNameChar,section);
				sectionStart=0;
				sectionNameChar=0;
			}	
		} else if (inWrongKey/* && inSection is implied */) {
			if (ch == '\n') {
				DEBUG_MSG("inWrongKey, found end of line!\n");
				inWrongKey = 0;
				foundKey=0;
				keyNameChar=0;
			} else {
				/*DEBUG_MSG("inWrongKey, found %c, pass till end of line\n",ch);*/
			}
		} else if (!foundKey /* && inSection is implied */) {
			if (ch==key[keyNameChar]){
				DEBUG_MSG("Found a valid letter of the key: %c on position %d of %s, continue to test if next character is also valid\n", ch,keyNameChar,key);
				keyNameChar++;
			} else if (isspace(ch)) {
				/* *before* the key, and *after* the key, before the '=' there can be spaces */
				DEBUG_MSG("found a space, we ignore spaces when we are looking for the key\n");
			} else if (keyNameChar != 0 && ch == '='){
				DEBUG_MSG("Character %c, found the key %s, set foundKey to 1\n", ch,key);
				foundKey=1;
			} else if (ch=='\n'){
				DEBUG_MSG("End of line, start looking again for %s\n", key);
				inWrongKey=0;
				keyNameChar=0;
			} else {
				DEBUG_MSG("%c must be a character that is not on position %d of key %s, set inWrongKey\n",ch,keyNameChar,key);
				inWrongKey=1;
			}
		} else if (foundKey /* && inSection is implied */) {
			if (bufferChar < bufferlen){
				if (ch != '\n') {
					DEBUG_MSG("Insection, found the key, getting the content for the key: %c\n", ch);
					buffer[bufferChar++]=ch;
				} else {
					DEBUG_MSG("found a newline: the end of the content of the key! ");
					buffer[bufferChar]='\0';
					DEBUG_MSG("return '%s'\n",buffer);
					return bufferChar;
				}
			} else {
				DEBUG_MSG("Hit the buffer max, EOM, done w/ key %s=\n", key);
				break;
			}
		} else {
			DEBUG_MSG("unhandled character %c ?\n",ch);
		}
		prevch = ch;
	}
	buffer[bufferChar]='\0';
	DEBUG_MSG("iniparser_get_string_at_position, end-of-file, bufferChar=%d\n",bufferChar);
	return bufferChar;
}

static unsigned int iniparser_scan_int_at_position(Tiniparser *ip, const char *section, const char *key, long position, const char *scanstring) {
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
	sscanf(data, scanstring, &buffer);
	return buffer;
}

unsigned int iniparser_get_int_at_position(Tiniparser *ip, const char *section, const char *key, long position) {
	return iniparser_scan_int_at_position(ip, section, key, position, "%u");
}

unsigned int iniparser_get_octalint_at_position(Tiniparser *ip, const char *section, const char *key, long position) {
	return iniparser_scan_int_at_position(ip, section, key, position, "%o");
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
