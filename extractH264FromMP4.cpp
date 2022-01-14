/*
 * ��ȡ��Ƶ�е���Ƶ����: https://www.jianshu.com/p/11cdf48ec248
 *
 */

#include <stdio.h>
extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/log.h>
#include <libavformat/avio.h>
}

#pragma warning(disable:4996)


/*
 ��֡ǰ�����������(һ��SPS/PPS��֡����������4�ֽڱ�ʾ��Ϊ0X00000001��������֡��������3���ֽڱ�ʾ��Ϊ0X000001��Ҳ�ж���4�ֽڱ�ʾ�ģ������������ǰ��ķ�ʽ)
 out��Ҫ�����AVPaket
 sps_pps��SPS��PPS���ݵ�ָ�룬���ڷǹؼ�֡�ʹ�NULL
 sps_pps_size��SPS/PPS���ݵĴ�С�����ڷǹؼ�֡��0
 in��ָ��ǰҪ�����֡��ͷ��Ϣ��ָ��
 in_size�ǵ�ǰҪ�����֡��С(nal_size)
*/
static int alloc_and_copy(AVPacket* out, const uint8_t* sps_pps, uint32_t sps_pps_size, const uint8_t* in, uint32_t in_size)
{
	uint32_t offset = out->size; // ƫ����������out�������ݵĴ�С��������д�����ݾ�Ҫ��ƫ��������ʼ����
	// ������Ĵ�С��SPS/PPSռ4�ֽڣ�����ռ3�ֽ�
	uint8_t nal_header_size = sps_pps == NULL ? 3 : 4;
	int err;

	// ÿ�δ���ǰ��Ҫ��out�������ݣ����ݵĴ�С���Ǵ˴�Ҫд������ݵĴ�С��Ҳ�����������С����sps/pps��С���ϼ��ϱ�֡���ݴ�С
	if ((err = av_grow_packet(out, sps_pps_size + in_size + nal_header_size)) < 0)
		return err;

	// 1.�����sps_pps���Ƚ�sps_pps������out��memcpy()���������ڴ濽������һ������Ϊ����Ҫ�洢�ĵط����ڶ���������Ҫ���������ݣ������������ǿ������ݵĴ�С��
	if (sps_pps)
	{
		memcpy(out->data + offset, sps_pps, sps_pps_size);
	}

	// 2.�����������루sps/pps������4λ0x00000001��������������3λ0x000001��
	for (int i = 0; i < nal_header_size; i++)
	{
		(out->data + offset + sps_pps_size)[i] = i == nal_header_size - 1 ? 1 : 0;
	}

	// 3.����ٿ���NALU����(��ǰ�����֡����)
	memcpy(out->data + sps_pps_size + nal_header_size + offset, in, in_size);

	return 0;
}

/*
��ȡ������sps/pps����
codec_extradata��codecpar����չ���ݣ�sps/pps���ݾ��������չ��������
codec_extradata_size����չ���ݴ�С
out_extradata�����sps/pps���ݵ�AVPacket��
padding:���Ǻ�AV_INPUT_BUFFER_PADDING_SIZE��ֵ(64)�������ڽ������������ĩβ��Ҫ�Ķ����ֽڸ�������Ҫ����Ҫ����ΪһЩ�Ż�������ȡ��һ�ζ�ȡ32����64���أ����ܻ��ȡ����size��С�ڴ��ĩβ��
*/
int h264_extradata_to_annexb(const uint8_t* codec_extradata, const int codec_extradata_size, AVPacket* out_extradata, int padding)
{
	uint16_t unit_size; // sps/pps���ݳ���
	uint64_t total_size = 0; // ����sps/pps���ݳ��ȼ����������볤�Ⱥ���ܳ���
	printf("aaaa12 h264\n");

	//for (int i = 0; i < codec_extradata_size; ++i)
	//{
	//	printf("%02x ", *(codec_extradata + i));
	//}

	/*
		out:��һ��ָ��һ���ڴ��ָ�룬����ڴ����ڴ�����п�����sps/pps���ݺ�������������
		unit_nb:sps/pps����
		sps_done��sps�����Ƿ��Ѿ��������
		sps_seen���Ƿ���sps����
		pps_seen���Ƿ���pps����
		sps_offset��sps���ݵ�ƫ�ƣ�Ϊ0
		pps_offset��pps���ݵ�ƫ�ƣ���Ϊpps������sps���棬������ƫ�ƾ�������sps���ݳ���+sps����������ռ�ֽ���
	*/
	uint8_t* out = NULL, unit_nb, sps_done = 0,
		sps_seen = 0, pps_seen = 0, sps_offset = 0, pps_offset = 0;
	const uint8_t* extradata = codec_extradata + 4; // ��չ���ݵ�ǰ4λ�����õ����ݣ�ֱ�������õ���������չ����
	static const uint8_t nalu_header[4] = { 0, 0, 0, 1 }; // sps/pps����ǰ���4bit��������

	// extradata��һ���ֽڵ����2λ����ָʾ����ÿ��sps/pps������ռ�ֽ�����(*extradata��ʾextradata��һ���ֽڵ����ݣ�֮������1ָ����һ���ֽ�)
	int length_size = (*extradata++ & 0x3) + 1;

	sps_offset = pps_offset = -1;

	// extradata�ڶ����ֽ����5λ����ָʾsps�ĸ���,һ�������һ����չֻ��һ��sps��pps��֮��ָ��ָ����һλ
	unit_nb = *extradata++ & 0x1f;
	if (!unit_nb) { // unit_nbΪ0��ʾû��sps���ݣ�ֱ����ת������pps�ĵط�
		goto pps;
	}
	else { // unit_nb��Ϊ0����sps���ݣ�����sps_seen��ֵ1��sps_offset��ֵ0
		sps_offset = 0;
		sps_seen = 1;
	}

	while (unit_nb--) { // ����ÿ��sps��pps(�ȱ���sps��Ȼ���ٱ���pps)
		int err;

		// �ٽ���2���ֽڱ�ʾsps/pps���ݵĳ���
		unit_size = (extradata[0] << 8) | extradata[1];
		total_size += unit_size + 4; // 4��ʾsps/pps�����볤��
		if (total_size > INT_MAX - padding) { // total_size̫�������������������Ҫ���ж�
			av_log(NULL, AV_LOG_ERROR,
				"Too big extradata size, corrupted stream or invalid MP4/AVCC bitstream\n");
			av_free(out);
			return AVERROR(EINVAL);
		}

		// extradata + 2 + unit_size��������չ���ݶ����˱����������쳣��
		if (extradata + 2 + unit_size > codec_extradata + codec_extradata_size) {
			av_log(NULL, AV_LOG_ERROR, "Packet header is not contained in global extradata, "
				"corrupted stream or invalid MP4/AVCC bitstream\n");
			av_free(out);
			return AVERROR(EINVAL);
		}

		// av_reallocp()���������ڴ���չ����out��չ�ܳ���padding�ĳ���
		if ((err = av_reallocp(&out, total_size + padding)) < 0)
			return err;

		// �Ƚ�4�ֽڵ������뿽����out
		memcpy(out + total_size - unit_size - 4, nalu_header, 4);
		// �ٽ�sps/pps���ݿ�����out,extradata + 2����Ϊ��2�ֽ��Ǳ�ʾsps/pps���ȵģ�����Ҫ����
		memcpy(out + total_size - unit_size, extradata + 2, unit_size);
		// ����sps/pps���ݴ������ָ��extradata��������sps/pps����
		extradata += 2 + unit_size;
	pps:
		if (!unit_nb && !sps_done++) { // ִ�е��������sps�Ѿ��������ˣ�����������pps����
			// pps�ĸ���
			unit_nb = *extradata++;
			if (unit_nb) { // ���pps��������0���pps_seen��ֵ1������������pps
				pps_offset = total_size;
				pps_seen = 1;
			}
		}
	}

	if (out) // ���out�����ݣ���ô��out + total_size����padding(��64)���ֽ���0���
		memset(out + total_size, 0, padding);

	// ���������û��sps��pps�������ʾ
	if (!sps_seen)
		av_log(NULL, AV_LOG_WARNING,
			"Warning: SPS NALU missing or invalid. "
			"The resulting stream may not play.\n");

	if (!pps_seen)
		av_log(NULL, AV_LOG_WARNING,
			"Warning: PPS NALU missing or invalid. "
			"The resulting stream may not play.\n");

	// ����������sps/pps��AVPaket��ֵ
	out_extradata->data = out;
	out_extradata->size = total_size;

	return length_size;
}

/*
	Ϊ�����������ʼ�롢SPS/PPS����Ϣ��д���ļ���
	AVPacket���ݰ����ܰ���һ֡��֡���ݣ�������Ƶ��˵ֻ��1֡������Ƶ��˵�Ͱ�����֡
	inΪҪ��������ݰ�
	fileΪ����ļ���ָ��
*/
int h264_mp4toannexb(AVFormatContext* fmt_ctx, AVPacket* in, FILE* file)
{
	AVPacket* out = NULL; // ����İ�
	AVPacket spspps_pkt; // sps/pps���ݵ�AVPaket

	int len; // fwrite()����д���ļ�ʱ�ķ���ֵ
	uint8_t unit_type; // NALUͷ��nal_unit_type��Ҳ����NALU���ͣ�5��ʾ��I֡��7��ʾSPS��8��ʾPPS
	int32_t nal_size; // һ��NALU(Ҳ����һ֡�����һ���ֽ���ͷ��Ϣ)�Ĵ�С���������NALU��ǰ���4���ֽ���
	uint8_t nal_size_len = 4; // ���nal_size���ֽ���
	uint32_t cumul_size = 0; // �Ѿ�������ֽ�������cumul_size==buf_sizeʱ��ʾ�����������ݶ���������
	const uint8_t* buf; // ������������ָ��
	const uint8_t* buf_end; // ������������ĩβָ��
	int buf_size; // �����������ݴ�С
	int ret = 0, i;

	out = av_packet_alloc();

	buf = in->data;
	buf_size = in->size;
	buf_end = in->data + in->size; // �����׵�ַ�������ݴ�С��������β��ַ

	do {
		ret = AVERROR(EINVAL);
		if (buf + nal_size_len > buf_end) // ˵��������������û�����ݣ����������
		{
			goto fail;
		}

		// ȡ��NALUǰ���4���ֽڵõ���һ֡�����ݴ�С
		for (nal_size = 0, i = 0; i < nal_size_len; i++)
		{
			nal_size = (nal_size << 8) | buf[i];
		}

		buf += nal_size_len; // buf����4λָ��NALU��ͷ��Ϣ(1���ֽ�)
		unit_type = *buf & 0x1f; // ȡ��NALUͷ��Ϣ�ĺ���5��bit����5bit��¼NALU������

		// ������������˳�
		if (nal_size > buf_end - buf || nal_size < 0)
		{
			goto fail;
		}

		// unit_type��5��ʾ�ǹؼ�֡�����ڹؼ�֡Ҫ����ǰ�����SPS��PPS��Ϣ
		if (unit_type == 5) {

			printf("aaaa11 type=%d\n", unit_type);
			// ���SPS��PPS��Ϣ����FFmpeg��SPS��PPS��Ϣ�����codecpar->extradata��
			h264_extradata_to_annexb(fmt_ctx->streams[in->stream_index]->codecpar->extradata,
				fmt_ctx->streams[in->stream_index]->codecpar->extradata_size,
				&spspps_pkt,
				AV_INPUT_BUFFER_PADDING_SIZE);

			// Ϊ�������������(��ʼ�룬���ڷָ�һ֡һ֡������)
			if ((ret = alloc_and_copy(out,
				spspps_pkt.data, spspps_pkt.size,
				buf, nal_size)) < 0)
				goto fail;
		}
		else {
			// �ǹؼ�ֻ֡��Ҫ���������
			if ((ret = alloc_and_copy(out, NULL, 0, buf, nal_size)) < 0)
				goto fail;
		}


		buf += nal_size; // һ֡�������ָ���Ƶ���һ֡
		cumul_size += nal_size + nal_size_len;// �ۼ��Ѿ�����õ����ݳ���
	} while (cumul_size < buf_size);

	// SPS��PPS�������붼��Ӻ���д���ļ�
	len = fwrite(out->data, 1, out->size, file);
	if (len != out->size) {
		av_log(NULL, AV_LOG_DEBUG, "warning, length of writed data isn't equal pkt.size(%d, %d)\n", len, out->size);
	}
	// fwrite()ֻ�ǽ�����д�뻺�棬fflush()�Ž���������д���ļ�
	fflush(file);

fail:
	av_packet_free(&out); // ������;����ʧ���˳�֮ǰ��Ҫ����ʹ��out�ͷţ����������ڴ�й¶

	return ret;

}


int extractH264FromMP4(const char* src, const char* dst)
{
	int flag;

	AVFormatContext* fmt_ctx = NULL;
	av_log_set_level(AV_LOG_DEBUG);

	// �������ļ�
	flag = avformat_open_input(&fmt_ctx, src, NULL, NULL);
	if (flag < 0) {
		av_log(NULL, AV_LOG_ERROR, "���ļ�ʧ�ܣ�\n");
		// �˳�����֮ǰ�ǵùر�֮ǰ�򿪵��ļ��ͷ��ڴ�
		avformat_close_input(&fmt_ctx);
		return -1;
	}

	// ��Ҫд��h264���ļ���û�оͻᴴ��һ��
	FILE* file = fopen(dst, "wb");
	if (file == NULL) {
		// ��ʧ��
		av_log(NULL, AV_LOG_ERROR, "���ܴ򿪻򴴽��洢�ļ���");
		// �˳�֮ǰ�ǵùر�֮ǰ�򿪵������ļ����ͷ��ڴ�
		avformat_close_input(&fmt_ctx);
		return -1;
	}

	// �����Ϣ
	av_dump_format(fmt_ctx, 0, src, 0);

	/*
	 * ��ȡ��õ�һ·������Դ
	 *�ڶ�����������������
	 *���������������������ţ���֪����д-1
	 *���ĸ���������صĶ�Ӧ�����������ţ�������Ƶ��Ӧ����Ƶ���������ţ����Բ��ع�����-1
	 *��������������õı�������������þ�дNULL
	 *������������һЩ��׼����ʱ��������0
	 *����ֵ�����ҵ�����������ֵ
	 * */
	int video_idx; // ��Ƶ����ֵ
	video_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	if (video_idx < 0) {
		av_log(NULL, AV_LOG_ERROR, "��ȡ��ʧ�ܣ�");
		// �˳�ǰ�ر�����ļ��ͷ��ڴ�
		avformat_close_input(&fmt_ctx);
		fclose(file);
		return -1;
	}

	// ��ȡ���еİ�����
	AVPacket pkt;
	av_init_packet(&pkt); // ��ʼ��pkt
	//pkt.data = NULL;
	//pkt.size = 0;
	// ѭ����ȡ�������еİ�(����ע�⴫��ȥ����pkt�ĵ�ַ)
	while (av_read_frame(fmt_ctx, &pkt) >= 0) {
		// �ж϶�ȡ�İ����������Ƿ��������ҵ�����
		if (pkt.stream_index == video_idx) {

			// Ϊ�����������ʼ�롢SPS/PPS����Ϣ
			h264_mp4toannexb(fmt_ctx, &pkt, file);
		}
		// ��Ϊÿ��ѭ����ҪΪpkt�����ڴ棬����һ��ѭ������ʱҪ�ͷ��ڴ�
		av_packet_unref(&pkt);
	}

	avformat_close_input(&fmt_ctx);
	if (file) {
		fclose(file);
	}

	return 0;
}