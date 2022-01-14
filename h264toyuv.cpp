#include <iostream>
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavfilter/avfilter.h>
#include <libavutil/imgutils.h>
};
using namespace std;
#define  YUVFORMAT_YUV420P AV_PIX_FMT_YUV420P

void h264toyuv(const char* src, const char* dst)
{
	int nRet = 0;
	const char* pInFileName = src;
	const char* pOutFileName = dst;
	AVDictionary* pDic = nullptr;
	AVFormatContext* pInFmtCtx = nullptr;
	nRet = avformat_open_input(&pInFmtCtx, pInFileName, nullptr, &pDic);
	if (nRet < 0)
	{
		printf("Could not open input file.");
		return;
	}
	avformat_find_stream_info(pInFmtCtx, nullptr);
	printf("===========Input Information==========\n");
	av_dump_format(pInFmtCtx, 0, pInFileName, 0);
	printf("======================================\n");

	int vudioStreamIndex = -1;
	for (int i = 0; i < pInFmtCtx->nb_streams; ++i)
	{
		if (AVMEDIA_TYPE_VIDEO == pInFmtCtx->streams[i]->codecpar->codec_type)
		{
			vudioStreamIndex = i;
			break;
		}
	}
	AVStream* in_stream = pInFmtCtx->streams[vudioStreamIndex];
	AVCodec* pInCodec = avcodec_find_decoder(in_stream->codecpar->codec_id);
	if (nullptr == pInCodec)
	{
		printf("avcodec_find_decoder fail.");
		return;
	}
	AVCodecContext* pInCodecCtx = avcodec_alloc_context3(pInCodec); //??????????????
	nRet = avcodec_parameters_to_context(pInCodecCtx, in_stream->codecpar);
	if (nRet < 0)
	{
		printf("avcodec_parameters_to_context fail.");
		return;
	}
	//打开解码器
	if (avcodec_open2(pInCodecCtx, pInCodec, nullptr) < 0)
	{
		printf("Error: Can't open codec!\n");
		return;
	}
	printf("width = %d\n", pInCodecCtx->width);
	printf("height = %d\n", pInCodecCtx->height);
	AVPacket* packet = av_packet_alloc();
	av_init_packet(packet);

	FILE* fp = fopen(pOutFileName, "wb+");
	size_t y_size = 0;
	int got_picture = 0;
	int nCount = 0;
	AVFrame* pFrame = av_frame_alloc();
	while (av_read_frame(pInFmtCtx, packet) >= 0)
	{
		if (vudioStreamIndex == packet->stream_index)
		{
			//avcodec_send_packet送原始数据给编码器进行编码
			//avcodec_receive_frame
			if (avcodec_send_packet(pInCodecCtx, packet) < 0 || (got_picture = avcodec_receive_frame(pInCodecCtx, pFrame)) < 0)
			{
				continue;
			}
			if (!got_picture)//
			{
				int picSize = pFrame->height * pFrame->width;
				int newSize = picSize * 1.5;
				//申请内存
				unsigned char* buf = new unsigned char[newSize];
				int a = 0, i;
				for (i = 0; i < pFrame->height; i++)
				{
					memcpy(buf + a, pFrame->data[0] + i * pFrame->linesize[0], pFrame->width);
					a += pFrame->width;
				}
				for (i = 0; i < pFrame->height / 2; i++)
				{
					memcpy(buf + a, pFrame->data[1] + i * pFrame->linesize[1], pFrame->width / 2);
					a += pFrame->width / 2;
				}
				for (i = 0; i < pFrame->height / 2; i++)
				{
					memcpy(buf + a, pFrame->data[2] + i * pFrame->linesize[2], pFrame->width / 2);
					a += pFrame->width / 2;
				}
				fwrite(buf, 1, newSize, fp);

				printf("Succeed to decode %d Frame!\n", nCount);
				nCount++;
			}
			av_packet_unref(packet);
		}
	}
	fflush(fp);
	//flush decoder
	//当av_read_frame 退出循环的时候，实际上解码器中可能还包含
	//剩余的几帧数据。直接调用avcodec_decode_video2获得AVFrame ,
	//而不再向解码器传递AVPacket
	while (1)
	{
		if (avcodec_send_packet(pInCodecCtx, packet) < 0 || (got_picture = avcodec_receive_frame(pInCodecCtx, pFrame)) < 0)
		{
			std::cout << "h264toyuv420p end";
			goto __end;
		}
		if (!got_picture)
		{
			break;
		}
		int picSize = pFrame->height * pFrame->width;
		int newSize = picSize * 1.5;
		//申请内存
		unsigned char* buf = new unsigned char[newSize];
		int a = 0, i;
		for (i = 0; i < pFrame->height; i++)
		{
			memcpy(buf + a, pFrame->data[0] + i * pFrame->linesize[0], pFrame->width);
			a += pFrame->width;
		}
		for (i = 0; i < pFrame->height / 2; i++)
		{
			memcpy(buf + a, pFrame->data[1] + i * pFrame->linesize[1], pFrame->width / 2);
			a += pFrame->width / 2;
		}
		for (i = 0; i < pFrame->height / 2; i++)
		{
			memcpy(buf + a, pFrame->data[2] + i * pFrame->linesize[2], pFrame->width / 2);
			a += pFrame->width / 2;
		}
		fwrite(buf, 1, newSize, fp);
		printf("flush Succeed to decode %d Frame!\n", nCount);
		nCount++;
	}
__end:
	fflush(fp);
	fclose(fp);
	av_frame_free(&pFrame);
	avcodec_close(pInCodecCtx);
	avformat_close_input(&pInFmtCtx);
	std::cout << "h264toyuv420p end";
}

