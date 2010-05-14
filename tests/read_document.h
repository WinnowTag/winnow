/* 
 * File:   read_document.h
 * Author: seangeo
 *
 * Created on June 13, 2008, 10:05 AM
 */

#ifndef _READ_DOCUMENT_H
#define	_READ_DOCUMENT_H

static char * read_document(const char * filename) {
  char *out = NULL;
  FILE *file;

  if (NULL != (file = fopen(filename, "r"))) {
    fseek(file, 0, SEEK_END);
    int size = ftell(file) + 1;
    out = calloc(size, sizeof(char));
    fseek(file, 0, SEEK_SET);
    fread(out, sizeof(char), size, file);
    out[size] = 0;
    fclose(file);
  }

  return out;
}

#endif	/* _READ_DOCUMENT_H */

