
extern "C"
{
#include <stdio.h>
#include <stdlib.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}


int AACtoPCM(const char* src, const char* dst)
{

	AVFormatContext* fmt_ctx = NULL;
	AVCodecContext* cod_ctx = NULL;
	AVCodec* cod = NULL;
	int ret = 0;
	AVPacket packet;

	//第一步创建输入文件AVFormatContext
	fmt_ctx = avformat_alloc_context();
	if (fmt_ctx == NULL)
	{
		ret = -1;
		printf("alloc fail");
		goto __ERROR;
	}
	if (avformat_open_input(&fmt_ctx, src, NULL, NULL) != 0)
	{
		ret = -1;
		printf("open fail");
		goto __ERROR;
	}

	//第二步 查找文件相关流，并初始化AVFormatContext中的流信息
	if (avformat_find_stream_info(fmt_ctx, NULL) < 0)
	{
		ret = -1;
		printf("find stream fail");
		goto __ERROR;
	}

	av_dump_format(fmt_ctx, 0, src, 0);

	//第三步查找音频流索引和解码器
	int stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &cod, -1);

	//第四步设置解码器上下文并打开解码器
	AVCodecParameters* codecpar = fmt_ctx->streams[stream_index]->codecpar;
	if (!cod)
	{
		ret = -1;
		printf("find codec fail");
		goto __ERROR;
	}
	cod_ctx = avcodec_alloc_context3(cod);
	avcodec_parameters_to_context(cod_ctx, codecpar);
	ret = avcodec_open2(cod_ctx, cod, NULL);
	if (ret < 0)
	{
		printf("can't open codec");
		goto __ERROR;
	}

	//第五步打开输出文件
	FILE* out_fb = NULL;
	out_fb = fopen(dst, "wb");
	if (!out_fb)
	{
		printf("can't open file");
		goto __ERROR;
	}

	//创建packet,用于存储解码前的数据
	av_init_packet(&packet);

	//设置转码后输出相关参数
	//采样的布局方式
	uint64_t out_channel_layout = codecpar->channel_layout;
	//采样个数
	int out_nb_samples = codecpar->frame_size;
	//采样格式
	enum AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S32;
	//采样率
	int out_sample_rate = codecpar->sample_rate;
	//通道数
	int out_channels = codecpar->channels;
	printf("%d\n", out_channels);

	//第六步创建Frame，用于存储解码后的数据
	AVFrame* frame = av_frame_alloc();
	frame->channels = out_channels;
	frame->format = out_sample_fmt;
	frame->nb_samples = out_nb_samples;
	av_frame_get_buffer(frame, 0);



	//第七步重采样初始化与设置参数
	struct SwrContext* convert_ctx = swr_alloc();
	convert_ctx = swr_alloc_set_opts(convert_ctx,
		out_channel_layout,
		out_sample_fmt,
		out_sample_rate,
		codecpar->channel_layout,
		(AVSampleFormat)codecpar->format,
		codecpar->sample_rate,
		0,
		NULL);
	swr_init(convert_ctx);
	int buffer_size = av_samples_get_buffer_size(NULL, out_channels, out_nb_samples, out_sample_fmt, 0);
	// uint8_t **data = (uint8_t **)av_calloc((size_t)out_channels, sizeof(*data));
	uint8_t** data = (uint8_t**)av_calloc(1, sizeof(*data));
	int alloc_size = av_samples_alloc(data,
		NULL,
		out_channels,
		out_nb_samples,
		out_sample_fmt,
		0);


	//while循环，每次读取一帧，并转码
	//第八步 读取数据并解码,重采样进行保存
	while (av_read_frame(fmt_ctx, &packet) >= 0)
	{

		if (packet.stream_index != stream_index)
		{
			continue;
		}


		ret = avcodec_send_packet(cod_ctx, &packet);
		if (ret < 0)
		{
			ret = -1;
			printf("decode error");
			goto __ERROR;
		}

		while (avcodec_receive_frame(cod_ctx, frame) >= 0)
		{
			swr_convert(convert_ctx, data, alloc_size, (const uint8_t**)frame->data, frame->nb_samples);
			fwrite(data[0], 1, buffer_size, out_fb);
		}

		av_packet_unref(&packet);
	}

__ERROR:
	if (fmt_ctx)
	{
		avformat_close_input(&fmt_ctx);
		avformat_free_context(fmt_ctx);
	}

	if (cod_ctx)
	{
		avcodec_close(cod_ctx);
		avcodec_free_context(&cod_ctx);
	}

	if (out_fb)
	{
		fclose(out_fb);
	}

	if (frame)
	{
		av_frame_free(&frame);
	}

	if (convert_ctx)
	{
		swr_free(&convert_ctx);
	}

	return 0;
}