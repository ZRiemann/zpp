#pragma once

#include <zpp/types.h>
#include <zpp/core/inspection/mission.h>
#include <zpp/core/inspection/namespace.h>
NSB_INSPECTION

/**
 * @brief 任务检查点数据结构
 * @note 该结构用于记录单个任务在某个检查点的耗时信息
 * 
 */
struct task{
    uint64_t mission_info;
    tsc_t timepoint_start; // timestamp counter at the start of the checkpoint
    tsc_t timepoint_stop; // timestamp counter at the end of the checkpoint
};
NSE_INSPECTION