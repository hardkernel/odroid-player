/*
 * Copyright (C) 2021 Amlogic Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __AGMPLAYER_ES_TYPES_COMMONS_H__
#define __AGMPLAYER_ES_TYPES_COMMONS_H__

#include <stdio.h>
#include <float.h>
#include<stdint.h>

#define AGMP_ES_HANDLE void *

#define BOOL int
#define true 1
#define false 0

typedef struct AgmpWindow
{
    int x;
    int y;
    int w;
    int h;
} AgmpWindow;

#endif /* __AGMPLAYER_ES_TYPES_COMMONS_H__ */
