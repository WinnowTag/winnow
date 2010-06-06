/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <string.h>
#include <stdlib.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <time.h>
#include <regex.h>
#include "logging.h"
#include "hmac_internal.h"
#include "hmac_credentials.h"
#include "buffer.h"

struct auth_data {
  char *access_id;
  char *signature;
};

static int prefix_of(const char * prefix, const char *string, int prefix_size) {
  int length = strlen(string);
  int i;
  int is_prefix = 0;
  
  if (prefix_size <= length) {
    is_prefix = 1;
    
    for (i = 0; i < length && i < prefix_size; i++) {
      if (string[i] != prefix[i]) {
        is_prefix = 0;
        break;
      }
    }
  }
  
  return is_prefix;
}

char * get_header(const struct curl_slist *header, const char * prefix, int prefix_size) {  
  char * return_header = NULL;
  
  for (; header; header = header->next) {
    debug("Trying: %s", header->data);
    if (prefix_of(prefix, header->data, prefix_size)) {
      return_header = header->data;
      break;
    }
  }
  
  return return_header;
}

static void append_header(Buffer *canon_s, const struct curl_slist *headers, const char * header) {
  int header_key_length = strlen(header);
  char *found_header = get_header(headers, header, header_key_length);
  
  if (found_header) {
    char *header_value = &found_header[header_key_length + 1];
    buffer_in(canon_s, header_value, strlen(header_value));
  }
  buffer_in(canon_s, "\n", 1);
}

static void http_date(char * s, int size) {
  time_t now = time(NULL);
  struct tm now_gm;
  gmtime_r(&now, &now_gm);
  strftime(s, size, "%a, %d %b %Y %H:%M:%S GMT", &now_gm);
}

static struct curl_slist * add_date_header_if_missing(struct curl_slist * headers) {
  if (!get_header(headers, "Date:", 5)) {
    char dateheader[36];
    memset(dateheader, 0, sizeof(dateheader));
    strcpy(dateheader, "Date: ");
    http_date(&dateheader[6], 30);
    headers = curl_slist_append(headers, dateheader);
  }
  
  return headers;
}

/* Encode some data into Base64 using OpenSSL.
 *
 * Sample code from http://www.ioncannon.net/cc/34/howto-base64-encode-with-cc-and-openssl/
 *
 */
static char * base64(const char * s, int length) {
  BIO *bmem, *b64;
  BUF_MEM *bptr;
  
  b64 = BIO_new(BIO_f_base64());
  bmem = BIO_new(BIO_s_mem());
  b64 = BIO_push(b64, bmem);
  BIO_write(b64, s, length);
  BIO_flush(b64);
  BIO_get_mem_ptr(b64, &bptr);
  
  char *buff = malloc(bptr->length);
  memcpy(buff, bptr->data, bptr->length - 1); // trim the new line
  buff[bptr->length - 1] = 0;
  
  BIO_free_all(b64);
  
  return buff;
}

static char * hmac(const char * secret, const char * data, int * length) {
  char *signature = calloc(EVP_MAX_MD_SIZE + 1, sizeof(char));  
  HMAC(EVP_sha1(), secret, strlen(secret), data, strlen(data), signature, length);
  
  return signature;
}

static char * build_auth_header(char * access_id, char * signature) {
  Buffer *auth_header = new_buffer(64);
  buffer_in(auth_header, "Authorization: AuthHMAC ", strlen("Authorization: AuthHMAC "));
  buffer_in(auth_header, access_id, strlen(access_id));
  buffer_in(auth_header, ":", 1);
  buffer_in(auth_header, signature, strlen(signature));
  buffer_in(auth_header, "\0", 1);
  
  char * return_header = auth_header->buf;
  free(auth_header);
  
  return return_header;
}

static char * extract_match(const regmatch_t * match, char * source) {
  char *s = calloc(match->rm_eo - match->rm_so + 1, sizeof(char));
  strncpy(s, &source[match->rm_so], match->rm_eo - match->rm_so);
  return s;
}

static int extract_authentication_data(struct curl_slist *headers, struct auth_data * data) {
  int found_data = 0;
  char *authorization_header = NULL;
  int keysize = strlen("Authorization:");
  
  if ((authorization_header = get_header(headers, "Authorization:", keysize))) {
    regex_t regex;
    regmatch_t matches[3];

    if (regcomp(&regex, "AuthHMAC ([^:]+):([A-Za-z0-9+/=]+)$", REG_EXTENDED)) {
      fatal("Error compiling regex");
      return -1;
    }

    if (0 == regexec(&regex, authorization_header, 3, matches, 0)) {
      char * access_id = extract_match(&matches[1], authorization_header);
      char * signature = extract_match(&matches[2], authorization_header);
      
      if (access_id && signature) {
        data->access_id = access_id;
        data->signature = signature;
        found_data = 1;
      }
    }

    regfree(&regex);
  }
  
  return found_data;
}

char * canonical_string(const char * method, const char * path, const struct curl_slist *headers) {
  // TOOD handle this
  Buffer *canon_s = new_buffer(256);
  buffer_in(canon_s, method, strlen(method));
  buffer_in(canon_s, "\n", 1);
  
  append_header(canon_s, headers, "Content-Type:");
  append_header(canon_s, headers, "Content-MD5:");
  append_header(canon_s, headers, "Date:");
  
  buffer_in(canon_s, path, strlen(path));
  
  buffer_in(canon_s, "\0", 1);
  char * return_string = canon_s->buf;
  free(canon_s);
  return return_string;
}

char * build_signature(const char * method, const char * path, const struct curl_slist *headers, const char * secret) {
  int signature_length = 0;
  char * data = canonical_string(method, path, headers);
  debug("cs = '%s'", data);
  char *signature = hmac(secret, data, &signature_length);
  char *base64_sig = base64(signature, signature_length);
  
  free(data);
  free(signature);
  
  return base64_sig;
}

struct curl_slist * hmac_sign(const char * method, const char * path, struct curl_slist *headers, const Credentials * credentials) {
  if (valid_credentials(credentials)) {
    headers = add_date_header_if_missing(headers);
    char * signature = build_signature(method, path, headers, credentials->secret_key);
    char * auth_header = build_auth_header(credentials->access_id, signature);  

    curl_slist_append(headers, auth_header);

    free(signature);
    free(auth_header);
  }
  
  return headers;
}

int hmac_auth(const char * method, const char * path, struct curl_slist *headers, const Credentials * credentials) {
  int authenticated = 0;
  struct auth_data auth = {NULL, NULL};
  memset(&auth, 0, sizeof(auth));
    
  if (extract_authentication_data(headers, &auth)) {
    char * computed_signature = build_signature(method, path, headers, credentials->secret_key);
    debug("computed = '%s'", computed_signature);
    authenticated = strcmp(auth.access_id, credentials->access_id) == 0 && strcmp(auth.signature, computed_signature) == 0;    
    free(computed_signature);
  } else {
    debug("No authentication data");
  }

  if (auth.access_id) free(auth.access_id);
  if (auth.signature) free(auth.signature);
  
  return authenticated;
}
