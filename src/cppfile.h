#ifndef CPPFILE_H
#define CPPFILE_H

struct cppfile;

struct cppfile *cppfile_new(const char *filename);
void cppfile_delete(struct cppfile *file);

#endif
