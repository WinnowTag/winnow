// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#ifndef RANDOMBACKGROUND_H
#define RANDOMBACKGROUND_H 1

#include "pool.h"
#include "item.h"
#include "cls_config.h"

extern Pool * create_random_background_from_file (const ItemSource * is, const char * file);
extern Pool * create_random_background_from_db   (const ItemSource * is, const DBConfig * db);
#endif
