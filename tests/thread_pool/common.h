#pragma once

constexpr int MAX_COUNT = 10;
constexpr int END_COUNT = MAX_COUNT - 1;
constexpr int THR_NUM = 3;

constexpr size_t COUNT_MASK = 0x3fffff;

//#define USE_BLOCK_QUE // 切换使用阻塞版队列，不使用非阻塞版队列