/*
 * Copyright (c) 2001 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

 /**
  * @file
  * video decoding with libavcodec API example
  *
  * @example decode_video.c
  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavfilter/avfilter.h>
#include <libavutil/imgutils.h>
}

extern int extractH264FromMP4(const char* src, const char* dst);
extern void h264toyuv(const char* src, const char* dst);
extern int extracAACFromMP4(const char* src, const char* dst);
extern int AACtoPCM(const char* src, const char* dst);
extern void getAllDevNames();
extern int writeCameraVideoStream();

int main(int argc, char** argv)
{
	const char* srcName = "C:/Users/wangzhi/Desktop/shenhua.mp4";

	const char* h264Name = "C:/Users/wangzhi/Desktop/h264.h264";
	const char* yuvName = "C:/Users/wangzhi/Desktop/yuv.yuv";

	const char* aacName = "C:/Users/wangzhi/Desktop/aac.aac";
	const char* pcmName = "C:/Users/wangzhi/Desktop/pcm.pcm";

 //   extractH264FromMP4(srcName, h264Name);
 //   h264toyuv(h264Name, yuvName);
	//extracAACFromMP4(srcName, aacName);
	//AACtoPCM(aacName, pcmName);
	//enumAllDevice();
	getAllDevNames();
    return 0;
}




