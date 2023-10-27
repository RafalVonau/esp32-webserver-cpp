#ifndef __WWW_FILESYSTEM_H
#define __WWW_FILESYSTEM_H

struct www_file_t {
 const char *name;
 int size;
 const char *data;
 int gz;
 const char* mime_type;
};
extern struct www_file_t www_filesystem[];


#endif

