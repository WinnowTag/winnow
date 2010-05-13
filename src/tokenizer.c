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
#include <libxml/uri.h>
#include "tokenizer.h"
#include "logging.h"
#include "buffer.h"
#include <string.h>
#include <ctype.h>
#include <Judy.h>
#include <regex.h>

// From http://www.daniweb.com/code/snippet216955.html
int rreplace (char *buf, int size, regex_t *re, char *rp) {
    char *pos;
    int sub, so, n;
    regmatch_t pmatch [10]; /* regoff_t is int so size is int */

    if (regexec (re, buf, 10, pmatch, 0)) return 0;
    for (pos = rp; *pos; pos++)
        if (*pos == '\\' && *(pos + 1) > '0' && *(pos + 1) <= '9') {
            so = pmatch [*(pos + 1) - 48].rm_so;
            n = pmatch [*(pos + 1) - 48].rm_eo - so;
            if (so < 0 || strlen (rp) + n - 1 > size) return 1;
            memmove (pos + n, pos + 2, strlen (pos) - 1);
            memmove (pos, buf + so, n);
            pos = pos + n - 2;
        }
    sub = pmatch [1].rm_so; /* no repeated replace when sub >= 0 */
    for (pos = buf; !regexec (re, pos, 1, pmatch, 0); ) {
        n = pmatch [0].rm_eo - pmatch [0].rm_so;
        pos += pmatch [0].rm_so;
        if (strlen (buf) - n + strlen (rp) > size) return 1;
        memmove (pos + strlen (rp), pos + n, strlen (pos) - n + 1);
        memmove (pos, rp, strlen (rp));
        pos += strlen (rp);
        if (sub >= 0) break;
    }
    return 0;
}

static int replace(char *buf, int size, const char * pattern, char *rp) {
	regex_t regex;
	int regex_error;

	if ((regex_error = regcomp(&regex, pattern, REG_EXTENDED)) != 0) {
		char buffer[1024];
	    regerror(regex_error, &regex, buffer, sizeof(buffer));
	    fatal("Error compiling REGEX: %s", buffer);
	} else if (rreplace(buf, size, &regex, rp)) {
		fatal("Error doing regex replace");
	}

	return 0;
}

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

		free(s);
	}
}

static void foldcase(char * txt) {
	int i;

	for (i = 0; txt[i]; i++) {
		txt[i] = tolower(txt[i]);
	}
}

static Pvoid_t tokenize_text(char * txt, int length) {
	Pvoid_t features = (Pvoid_t) NULL;
	char *token;

	// Remove HTML entities
	replace(txt, length, "&[^;]+;", " ");
	// Remove all non-alphanums
	replace(txt, length, "[^a-zA-Z0-9\\-]", " ");
	// Remove leading and trailing dashes
	replace(txt, length, "[[:space:]]+[\\-]+", " ");
	replace(txt, length, "\\-+[[:space:]]+", " ");
	// Normalize whitespace
	replace(txt, length, "[[:space:]]+", " ");
	foldcase(txt);

	for (; (token = strsep(&txt, "\t\n ")) != NULL; ) {
		if (token != '\0') {
			int toklen = strlen(token) + 1; // +1 for \0
			if (toklen > 2) {
				Word_t *PValue;
				char *feature = malloc(toklen * sizeof(char) + 2);
				strncpy(feature, "t:", 3);
				strncat(feature, token, toklen);
				JSLI(PValue, features, (unsigned char*) feature);
				(*PValue)++;
			}
		}
	}

	return features;
}

static Buffer *extractText(htmlDocPtr doc) {
    Buffer *buf = new_buffer(256);
    xmlTextReaderPtr reader = xmlReaderWalker(doc);
    while(xmlTextReaderRead(reader)){
        processNode(reader, buf);
    }
    xmlFreeTextReader(reader);
    return buf;
}

static Pvoid_t add_url_component(char * uri, Pvoid_t features) {
	if (uri) {
		Word_t *PValue;
		int size = strlen(uri) + 8;
		char * token = malloc(sizeof(char) * size);
		strncpy(token, "URLSeg:", size);
		strncat(token, uri, size);
		JSLI(PValue, features, (unsigned char*) token);
		(*PValue)++;
	}

	return features;
}

static Pvoid_t tokenize_uri(const char * uristr, Pvoid_t features) {
	xmlURIPtr uri = xmlParseURI(uristr);
	if (uri) {
		features = add_url_component(uri->path, features);

		if (uri->server) {
			replace(uri->server, strlen(uri->server), "www\\.", "");
			features = add_url_component(uri->server, features);
		}

		xmlFreeURI(uri);
	}

	return features;
}

static Pvoid_t tokenize_uris(htmlDocPtr doc, Pvoid_t features) {
	xmlTextReaderPtr reader = xmlReaderWalker(doc);

	while (xmlTextReaderRead(reader)) {
		int type = xmlTextReaderNodeType(reader);
		if (type == XML_ELEMENT_NODE) {
			char *uri = (char *) xmlTextReaderGetAttribute(reader, BAD_CAST "href");
			if (uri) {
				features = tokenize_uri(uri, features);
			}

			uri = (char*) xmlTextReaderGetAttribute(reader, BAD_CAST "src");
			if (uri) {
				features = tokenize_uri(uri, features);
			}
		}
	}

	return features;
}

/** Tokenize a string of HTML.
 *
 * @param html The HTML string.
 * @return an Array of Features.
 */
Pvoid_t html_tokenize(const char * html) {
	Pvoid_t features;
	xmlSubstituteEntitiesDefault(0);
	htmlDocPtr doc = htmlParseDoc(BAD_CAST html, "UTF-8");

	if (doc) {
		Buffer *buf = extractText(doc);
		features = tokenize_text(buf->buf, buf->length);
		features = tokenize_uris(doc, features);
		free_buffer(buf);
	}

	xmlFreeDoc(doc);

	return features;
}
