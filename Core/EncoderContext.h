#pragma once

#include "lavf.h"
#include "FrameFilter.h"

struct EncoderContext
{
	AVCodecContext* codecContext = NULL;
	AVStream* stream = NULL;
	FrameFilter* frameFilter = NULL;
	int64_t next_pts = 0;
	char *stats_info = NULL;
};