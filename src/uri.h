/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#ifndef _URI_H_
#define _URI_H_

static int uri_fragment_id(const char *uri) {
  int id = 0;
  xmlURIPtr id_uri = xmlParseURI(uri);
          
  if (id_uri && id_uri->fragment) {
    id = strtol(id_uri->fragment, NULL, 10);
  }
  
  xmlFreeURI(id_uri);
  return id;
}

#endif /* _URI_H_ */
