extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavfilter/avfilter.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libavutil/avutil.h>
#include <libavdevice/avdevice.h>
}

#include <string>
#include <memory>
#include <thread>
#include <iostream>
#include <string.h>
#include <tchar.h>


using namespace std;

class SwsScaleContext
{
public:
	SwsScaleContext()
	{

	}
	void SetSrcResolution(int width, int height)
	{
		srcWidth = width;
		srcHeight = height;
	}

	void SetDstResolution(int width, int height)
	{
		dstWidth = width;
		dstHeight = height;
	}
	void SetFormat(AVPixelFormat iformat, AVPixelFormat oformat)
	{
		this->iformat = iformat;
		this->oformat = oformat;
	}
public:
	int srcWidth;
	int srcHeight;
	int dstWidth;
	int dstHeight;
	AVPixelFormat iformat;
	AVPixelFormat oformat;
};

AVFormatContext* inputContext = nullptr;
AVCodecContext* encodeContext = nullptr;
AVFormatContext* outputContext;
int64_t lastReadPacktTime;
int64_t packetCount = 0;
struct SwsContext* pSwsContext = nullptr;
uint8_t* pSwpBuffer = nullptr;
static int interrupt_cb(void* ctx)
{
	int  timeout = 3;
	if (av_gettime() - lastReadPacktTime > timeout * 1000 * 1000)
	{
		return -1;
	}
	return 0;
}

int OpenInput(string inputUrl)
{
	inputContext = avformat_alloc_context();
	lastReadPacktTime = av_gettime();
	// 设置超时操作
	inputContext->interrupt_callback.callback = interrupt_cb;
	// 使用libavdevice的时候 用av_find_input_format() 寻找输入设备，这里是寻找摄像头设备 
	//具体可参考 https://blog.csdn.net/leixiaohua1020/article/details/39702113
	AVInputFormat* ifmt = av_find_input_format("dshow");
	// 利用AVDictionary 来配置参数 key =rtbufsize , value = 18432000
	AVDictionary* format_opts = nullptr;
	av_dict_set_int(&format_opts, "rtbufsize", 18432000, 0);

	int ret = avformat_open_input(&inputContext, inputUrl.c_str(), ifmt, &format_opts);
	if (ret < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Input file open input failed\n");
		return  ret;
	}
	ret = avformat_find_stream_info(inputContext, nullptr);
	if (ret < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Find input file stream inform failed\n");
	}
	else
	{
		av_log(NULL, AV_LOG_FATAL, "Open input file  %s success\n", inputUrl.c_str());
	}
	return ret;
}


shared_ptr<AVPacket> ReadPacketFromSource()
{
	// 初始化一个 数据packet share_ptr 利用get 来获得包数据 指针
	shared_ptr<AVPacket> packet(static_cast<AVPacket*>(av_malloc(sizeof(AVPacket))), [&](AVPacket* p) { av_packet_free(&p); av_freep(&p); });
	av_init_packet(packet.get());
	lastReadPacktTime = av_gettime();
	int ret = av_read_frame(inputContext, packet.get());
	if (ret >= 0)
	{
		return packet;
	}
	else
	{
		return nullptr;
	}
}

// 输出操作
int OpenOutput(string outUrl, AVCodecContext* encodeCodec)
{

	int ret = avformat_alloc_output_context2(&outputContext, nullptr, "mp4", outUrl.c_str());
	if (ret < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "open output context failed\n");
		goto Error;
	}

	ret = avio_open2(&outputContext->pb, outUrl.c_str(), AVIO_FLAG_WRITE, nullptr, nullptr);
	if (ret < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "open avio failed");
		goto Error;
	}

	for (int i = 0; i < inputContext->nb_streams; i++) // 如果有音频 和 视频 nputContext->nb_streams则为2 
	{
		if (inputContext->streams[i]->codec->codec_type == AVMediaType::AVMEDIA_TYPE_AUDIO)// 如果有stream 为音频 则不处理
		{
			continue;
		}
		AVStream* stream = avformat_new_stream(outputContext, encodeCodec->codec);
		ret = avcodec_copy_context(stream->codec, encodeCodec);
		if (ret < 0)
		{
			av_log(NULL, AV_LOG_ERROR, "copy coddec context failed");
			goto Error;
		}
	}

	ret = avformat_write_header(outputContext, nullptr);
	if (ret < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "format write header failed");
		goto Error;
	}

	av_log(NULL, AV_LOG_FATAL, " Open output file success %s\n", outUrl.c_str());
	return ret;
Error:
	if (outputContext)
	{
		for (int i = 0; i < outputContext->nb_streams; i++)
		{
			avcodec_close(outputContext->streams[i]->codec);
		}
		avformat_close_input(&outputContext);
	}
	return ret;
}

void Init()
{
	av_register_all();
	avfilter_register_all();
	avformat_network_init();
	avdevice_register_all();
	// 设置 log 为error级别
	av_log_set_level(AV_LOG_ERROR);
}

void CloseInput()
{
	if (inputContext != nullptr)
	{
		avformat_close_input(&inputContext);
	}

	if (pSwsContext)
	{
		sws_freeContext(pSwsContext);
	}
}

void CloseOutput()
{
	if (outputContext != nullptr)
	{
		//（1）循环调用interleave_packet()以及write_packet()，将还未输出的AVPacket输出出来。
		//（2）调用AVOutputFormat的write_trailer()，输出文件尾。
		// 具体看雷神博客 ： https://blog.csdn.net/leixiaohua1020/article/details/44201645
		int ret = av_write_trailer(outputContext);
		avformat_close_input(&outputContext);
	}
}

int WritePacket(shared_ptr<AVPacket> packet)
{
	auto inputStream = inputContext->streams[packet->stream_index];
	auto outputStream = outputContext->streams[packet->stream_index];

	// time_base 表示包在视频中的流 的时间基数，跟编码格式有关系
	// den 是 一个base stream 时间（分母）
	// num 是 分子，表示 一个 base stream时间中 分多少份儿，
	// 这里除30 ，表示 一个 packet 占用的视频流为 base/num/30 （自己的理解，仅供参考）
	//这里 需要先设置好 packet的 pts，否则视频播放速度会乱
	packet->pts = packet->dts = packetCount * (outputContext->streams[0]->time_base.den) /
		outputContext->streams[0]->time_base.num / 30;
	//cout <<"pts:"<<packet->pts<<endl;
	packetCount++;
	return av_interleaved_write_frame(outputContext, packet.get());
}

// 初始化解码器，根据输入流编码 找到 并打开 解码器
int InitDecodeContext(AVStream* inputStream)
{
	auto codecId = inputStream->codec->codec_id;
	auto codec = avcodec_find_decoder(codecId);
	if (!codec)
	{
		return -1;
	}

	int ret = avcodec_open2(inputStream->codec, codec, NULL);
	return ret;

}

// 初始化编码器 找到 h264编码，初始化，并且打开
int initEncoderCodec(AVStream* inputStream, AVCodecContext** encodeContext)
{
	AVCodec* picCodec;

	picCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
	(*encodeContext) = avcodec_alloc_context3(picCodec);

	(*encodeContext)->codec_id = picCodec->id;
	(*encodeContext)->has_b_frames = 0;
	(*encodeContext)->time_base.num = inputStream->codec->time_base.num;
	(*encodeContext)->time_base.den = inputStream->codec->time_base.den;
	(*encodeContext)->pix_fmt = *picCodec->pix_fmts;
	(*encodeContext)->width = inputStream->codec->width;
	(*encodeContext)->height = inputStream->codec->height;
	(*encodeContext)->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	int ret = avcodec_open2((*encodeContext), picCodec, nullptr);
	if (ret < 0)
	{
		std::cout << "open video codec failed" << endl;
		return  ret;
	}
	return 1;
}

// 解码 ，将 packet 解码到 frame中
bool Decode(AVStream* inputStream, AVPacket* packet, AVFrame* frame)
{
	int gotFrame = 0;
	auto hr = avcodec_decode_video2(inputStream->codec, frame, &gotFrame, packet);
	if (hr >= 0 && gotFrame != 0)
	{
		return true;
	}
	return false;
}

// 对 解码packet后的数据fram 进行编码，并且 将编码数据存在 创建的packet
std::shared_ptr<AVPacket> Encode(AVCodecContext* encodeContext, AVFrame* frame)
{
	int gotOutput = 0;
	std::shared_ptr<AVPacket> pkt(static_cast<AVPacket*>(av_malloc(sizeof(AVPacket))), [&](AVPacket* p) { av_packet_free(&p); av_freep(&p); });
	av_init_packet(pkt.get());
	pkt->data = NULL;
	pkt->size = 0;
	int ret = avcodec_encode_video2(encodeContext, pkt.get(), frame, &gotOutput);
	if (ret >= 0 && gotOutput)
	{
		return pkt;
	}
	else
	{
		return nullptr;
	}
}


int initSwsContext(struct SwsContext** pSwsContext, SwsScaleContext* swsScaleContext)
{
	*pSwsContext = sws_getContext(swsScaleContext->srcWidth, swsScaleContext->srcHeight, swsScaleContext->iformat,
		swsScaleContext->dstWidth, swsScaleContext->dstHeight, swsScaleContext->oformat,
		SWS_BICUBIC,
		NULL, NULL, NULL);
	if (pSwsContext == NULL)
	{
		return -1;
	}
	return 0;
}

int initSwsFrame(AVFrame* pSwsFrame, int iWidth, int iHeight)
{
	// 计算一帧的大小
	int numBytes = av_image_get_buffer_size(encodeContext->pix_fmt, iWidth, iHeight, 1);
	/*if(pSwpBuffer)
	{
		av_free(pSwpBuffer);
	}*/
	pSwpBuffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
	// 将数据存入 uint8_t 中
	av_image_fill_arrays(pSwsFrame->data, pSwsFrame->linesize, pSwpBuffer, encodeContext->pix_fmt, iWidth, iHeight, 1);
	pSwsFrame->width = iWidth;
	pSwsFrame->height = iHeight;
	pSwsFrame->format = encodeContext->pix_fmt;
	return 1;
}

int writeCameraVideoStream()
{

	SwsScaleContext swsScaleContext;
	AVFrame* videoFrame = av_frame_alloc();
	AVFrame* pSwsVideoFrame = av_frame_alloc();

	Init();
	int ret = OpenInput(std::string("video=Integrated Camera"));

	if (ret < 0) goto Error;

	InitDecodeContext(inputContext->streams[0]);


	ret = initEncoderCodec(inputContext->streams[0], &encodeContext);

	if (ret >= 0)
	{
		ret = OpenOutput("D:\\Camera.mp4", encodeContext);
	}
	if (ret < 0) goto Error;


	swsScaleContext.SetSrcResolution(inputContext->streams[0]->codec->width, inputContext->streams[0]->codec->height);
	swsScaleContext.SetDstResolution(encodeContext->width, encodeContext->height);
	swsScaleContext.SetFormat(inputContext->streams[0]->codec->pix_fmt, encodeContext->pix_fmt);
	initSwsContext(&pSwsContext, &swsScaleContext);
	initSwsFrame(pSwsVideoFrame, encodeContext->width, encodeContext->height);
	int64_t startTime = av_gettime();
	while (true)
	{
		auto packet = ReadPacketFromSource();
		if (av_gettime() - startTime > 10 * 1000 * 1000)
		{
			break;
		}
		if (packet && packet->stream_index == 0)
		{
			if (Decode(inputContext->streams[0], packet.get(), videoFrame))
			{
				sws_scale(pSwsContext, (const uint8_t* const*)videoFrame->data,
					videoFrame->linesize, 0, inputContext->streams[0]->codec->height, (uint8_t* const*)pSwsVideoFrame->data, pSwsVideoFrame->linesize);
				auto packetEncode = Encode(encodeContext, pSwsVideoFrame);
				if (packetEncode)
				{
					ret = WritePacket(packetEncode);
					//cout <<"ret:" << ret<<endl;
				}

			}

		}

	}
	cout << "Get Picture End " << endl;
	av_frame_free(&videoFrame);
	avcodec_close(encodeContext);
	av_frame_free(&pSwsVideoFrame);

Error:
	CloseInput();
	CloseOutput();

	while (true)
	{
		this_thread::sleep_for(chrono::seconds(100));
	}
	return 0;
}

