// General info: http://doc.winnowtag.org/open-source
// Source code repository: http://github.com/winnowtag
// Questions and feedback: contact@winnowtag.org
//
// Copyright (c) 2007-2011 The Kaphan Foundation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// contact@winnowtag.org

#ifndef _LOGGING_H_
#define _LOGGING_H_

#include <config.h>

#define MALLOC_ERR() {error("malloc error at %s:%i", __FILE__, __LINE__); goto malloc_error;} 

#define fatal(fmt, ...) _fatal(__FILE__, __LINE__, fmt, ## __VA_ARGS__)
#define error(fmt, ...) _error(__FILE__, __LINE__, fmt, ## __VA_ARGS__)
#define info(fmt, ...)  _info (__FILE__, __LINE__, fmt, ## __VA_ARGS__)

#ifdef _DEBUG
#  define debug(fmt, ...) _debug(__FILE__, __LINE__, fmt, ## __VA_ARGS__)
#  define trace(fmt, ...)
#else
#  define debug(fmt, ...)
#  define trace(fmt, ...)
#endif

extern void initialize_logging(const char *logfile);
extern void _fatal (const char *file, int line, const char *fmt, ...);
extern void _error (const char *file, int line, const char *fmt, ...);
extern void _info  (const char *file, int line, const char *fmt, ...);
extern void _debug (const char *file, int line, const char *fmt, ...);
extern void _trace (const char *file, int line, const char *fmt, ...);

#endif
