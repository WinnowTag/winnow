
#ifndef _XML_ERROR_FUNCTIONS_H
#define	_XML_ERROR_FUNCTIONS_H

#include "logging.h"

static void xmlErrorFunction		(void * userData,  xmlErrorPtr error) {
  if (error->message) {
    error("XML error: %s", error->message);
  }  
}

#define SET_XML_ERROR_HANDLERS \
  xmlSetStructuredErrorFunc(NULL, &xmlErrorFunction);

#endif	/* _XML_ERROR_FUNCTIONS_H */

