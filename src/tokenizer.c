/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <libxml/HTMLparser.h>
#include <libxml/xmlreader.h>
#include <libxml/tree.h>
#include "tokenizer.h"
#include "logging.h"
#include "buffer.h"
#include <string.h>

static void processNode(xmlTextReaderPtr reader, Buffer * buf) {
	int type = xmlTextReaderNodeType(reader);

	if (type == XML_TEXT_NODE) {
		unsigned char *s = xmlTextReaderReadString(reader);
		int slen = strlen(s);
		char enc[512];
		int enclen = 512;
		if (!htmlEncodeEntities(enc, &enclen, s, &slen, 0)) {
			buffer_in(buf, enc, enclen);

			if (enc[enclen-1] != ' ') {
				buffer_in(buf, " ", 1);
			}
		}
	}
}

/** Tokenize a string of HTML.
 *
 * @param html The HTML string.
 * @return an Array of Features.
 */
Array * html_tokenize(const char * html) {
	xmlSubstituteEntitiesDefault(0);
	htmlDocPtr doc = htmlParseDoc(BAD_CAST html, "UTF-8");

	if (doc) {
		Buffer *buf = new_buffer(256);
		xmlTextReaderPtr reader = xmlReaderWalker(doc);
		while(xmlTextReaderRead(reader)) {
			processNode(reader, buf);

		}

		xmlFreeTextReader(reader);
		buffer_in(buf, "\0", 1);
		printf("%s\n", buf->buf);
		free_buffer(buf);
	}

	xmlFreeDoc(doc);

	return create_array(0);
}
