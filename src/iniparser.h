#ifndef __INI_PARSER_H
#define __INI_PARSER_H

#include <stdio.h>

typedef struct {
	char *filename;
	FILE *fd;
} Tiniparser;

#define iniparser_rewind(ip) fseek(ip->fd, 0, SEEK_SET)
#define iniparser_get_position(ip) ftell(ip->fd)
#define iniparser_set_position(ip, position) fseek(ip->fd, position, SEEK_SET)
#define iniparser_get_string(ip,section,key,buffer,buflen) iniparser_get_string_at_position(ip,section,key,0,buffer,buflen)
#define iniparser_get_int(ip,section,key) iniparser_get_int_at_position(ip,section,key,0)

Tiniparser *new_iniparser(char *filename);
void iniparser_close(Tiniparser *ip);
unsigned int iniparser_get_string_at_position(Tiniparser*ip, const char *section, const char *key, unsigned int position, char *buffer, int bufferlen);
unsigned int iniparser_get_int_at_position(Tiniparser *ip, const char *section, const char *key, unsigned int position);
unsigned int iniparser_get_octalint_at_position(Tiniparser *ip, const char *section, const char *key, unsigned int position);
char *iniparser_next_section(Tiniparser *ip, char *buf, int buflen);
unsigned short int iniparser_has_section(Tiniparser *ip, const char *section);

#endif /* __INI_PARSER_H */
