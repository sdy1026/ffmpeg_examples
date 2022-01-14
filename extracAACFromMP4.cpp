
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavfilter/avfilter.h>
#include <libavutil/imgutils.h>
}

#pragma warning(disable:4996)

//全局变量
static AVFormatContext* g_formatContext = NULL;
static AVFrame* g_avFrame = NULL;
static FILE* g_destFile = NULL;

//音频数据,需要和mp4文件中的音频一致
static int audioType = 2;	//AAC LC
static int sampleIndex = 4;	//44100
/*
0: 96000 Hz
1 : 88200 Hz
2 : 64000 Hz
3 : 48000 Hz
4 : 44100 Hz
5 : 32000 Hz
6 : 24000 Hz
7 : 22050 Hz
8 : 16000 Hz
9 : 12000 Hz
10 : 11025 Hz
11 : 8000 Hz
12 : 7350 Hz
13 : Reserved
14 : Reserved
15 : frequency is written explictly
*/
static int channelConfig = 1;	//双通道

//添加ADTS(audio data transport stream)-长度为7
void addHeader(char header[], int len)
{
	len += 7;
	//0,1是固定的
	header[0] = (uint8_t)0xff;         //syncword:0xfff                          高8bits
	header[1] = (uint8_t)0xf0;         //syncword:0xfff                          低4bits
	header[1] |= (0 << 3);    //MPEG Version:0 for MPEG-4,1 for MPEG-2  1bit
	header[1] |= (0 << 1);    //Layer:0                                 2bits 
	header[1] |= 1;           //protection absent:1                     1bit
	//根据aac类型,采样率,通道数来配置
	header[2] = (audioType - 1) << 6;            //profile:audio_object_type - 1                      2bits
	header[2] |= (sampleIndex & 0x0f) << 2; //sampling frequency index:sampling_frequency_index  4bits 
	header[2] |= (0 << 1);                             //private bit:0                                      1bit
	header[2] |= (channelConfig & 0x04) >> 2;           //channel configuration:channel_config               高1bit
	//根据通道数+数据长度来配置
	header[3] = (channelConfig & 0x03) << 6;     //channel configuration:channel_config      低2bits
	header[3] |= (0 << 5);                      //original：0                               1bit
	header[3] |= (0 << 4);                      //home：0                                   1bit
	header[3] |= (0 << 3);                      //copyright id bit：0                       1bit  
	header[3] |= (0 << 2);                      //copyright id start：0                     1bit
	header[3] |= ((len & 0x1800) >> 11);           //frame length：value   高2bits
	//根据数据长度来配置
	header[4] = (uint8_t)((len & 0x7f8) >> 3);     //frame length:value    中间8bits
	header[5] = (uint8_t)((len & 0x7) << 5);       //frame length:value    低3bits
	header[5] |= (uint8_t)0x1f;                                 //buffer fullness:0x7ff 高5bits
	header[6] = (uint8_t)0xfc;
}

int extracAACFromMP4(const char* src, const char* dst)
{
	//parameter
	char errors[200] = { 0 };
	const char* srcFile = src;
	const char* destFile = dst;

	//init, register all av
	av_log_set_level(AV_LOG_INFO);
	av_register_all();
	//open src file
	int ret = avformat_open_input(&g_formatContext, srcFile, NULL, NULL);
	if (ret != 0)
	{
		av_strerror(ret, errors, 200);
		av_log(NULL, AV_LOG_WARNING, "avformat_open_input error: file=%s, ret=%d, msg=%s\n", srcFile, ret, errors);
		return -1;
	}
	//dump media info
	av_dump_format(g_formatContext, 0, srcFile, 0);
	//init stream info
	ret = avformat_find_stream_info(g_formatContext, NULL);
	if (ret != 0)
	{
		av_strerror(ret, errors, 200);
		av_log(NULL, AV_LOG_WARNING, "avformat_find_stream_info error: ret=%d, msg=%s\n", ret, errors);
		return -1;
	}
	//get audio stream index
	int audioStreamIndex = av_find_best_stream(g_formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
	if (audioStreamIndex < 0)
	{
		av_log(NULL, AV_LOG_WARNING, "audio stream error: index=%d\n", audioStreamIndex);
		return -1;
	}
	//get audio stream
	AVStream* audioStream = g_formatContext->streams[audioStreamIndex];
	if (!audioStream)
	{
		av_log(NULL, AV_LOG_WARNING, "audio stream null: index=%d\n", audioStreamIndex);
		return -1;
	}
	//open file--"wb"
	g_destFile = fopen(destFile, "wb");
	if (!g_destFile)
	{
		av_log(NULL, AV_LOG_WARNING, "open file error: file=%s\n", destFile);
	}
	//init packet
	AVPacket audioPacket;
	//get packet
	while (av_read_frame(g_formatContext, &audioPacket) >= 0)
	{
		if (audioPacket.stream_index != audioStreamIndex)
		{
			continue;
		}
		//无需编解码,直接拼接ADTS+audio data
		char adts[7] = { 0 };
		addHeader(adts, audioPacket.size);
		fwrite(adts, 1, 7, g_destFile);
		int len = fwrite(audioPacket.data, 1, audioPacket.size, g_destFile);
		if (len != audioPacket.size)
		{
			av_log(NULL, AV_LOG_WARNING, "write data error: packet size=%d, write length=%d\n", audioPacket.size, len);
		}
		av_packet_unref(&audioPacket);
	}
	return 0;
}

//释放资源
void release()
{
	if (g_formatContext)
	{
		avformat_close_input(&g_formatContext);
		g_formatContext = NULL;
	}
	if (g_destFile)
	{
		fclose(g_destFile);
		g_destFile = NULL;
	}
}

//
//int main(int argc, char* argv[])
//{
//	int ret = getAudioData();
//	if (ret != 0)
//	{
//		av_log(NULL, AV_LOG_WARNING, "get audio data error: ret=%d\n", ret);
//	}
//	release();
//	return 0;
//}