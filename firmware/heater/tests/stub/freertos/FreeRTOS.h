// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#pragma once
#include <deque>
#include <vector>
#include <cstring>
typedef void* QueueHandle_t; typedef void* TaskHandle_t; typedef int BaseType_t;
#define pdTRUE 1
#define pdPASS 1
#define configMAX_PRIORITIES 25
#define pdMS_TO_TICKS(x) (x)
struct FakeQ{size_t sz; std::deque<std::vector<char>> q;};
inline QueueHandle_t xQueueCreate(int, size_t s){auto*q=new FakeQ(); q->sz=s; return q;}
inline int xQueueSend(QueueHandle_t h,const void*d,int){auto*q=(FakeQ*)h;q->q.emplace_back((const char*)d,(const char*)d+q->sz);return 1;}
inline int xQueueReceive(QueueHandle_t h,void*d,int){auto*q=(FakeQ*)h; if(!q||q->q.empty())return 0; memcpy(d,q->q.front().data(),q->sz); q->q.pop_front(); return 1;}
inline int xQueueReset(QueueHandle_t){return 1;}
inline int xTaskCreatePinnedToCore(...){return 1;}
