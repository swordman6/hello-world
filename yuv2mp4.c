#include <stdio.h>
#include <string.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>

#define TRUE (1)

simple_yuvtomp4(const char *inpath, const char *outpath)
{
	int width = 352;
	int height = 288;
	int fps = 25;

	//查找编码器
	const AVCodec * codec = avcodec_find_encoder(AV_CODEC_ID_H264);
	
	//给编码器分配内存
	AVCodecContext *avcodec_context = avcodec_alloc_context3(codec);
	//时间基,pts,dts的时间单位  pts(解码后帧被显示的时间), dts(视频帧送入解码器的时间)的时间单位,是两帧之间的时间间隔
	avcodec_context->time_base.den = fps;//pts
	avcodec_context->time_base.num = 1;//1秒
	avcodec_context->codec_id = AV_CODEC_ID_H264;

	avcodec_context->codec_type = AVMEDIA_TYPE_VIDEO;//表示视频类型
	avcodec_context->pix_fmt = AV_PIX_FMT_YUV420P;//视频数据像素格式

	avcodec_context->coded_width  = width;//视频宽高
	avcodec_context->coded_height = height;

	avcodec_context->bit_rate  = 1*1024*1024;
	avcodec_context->gop_size  = 50; // 画面组大小，关键帧
	avcodec_context->codec_tag = 0;
	
	//初始化编解码器
	int ret = avcodec_open2(avcodec_context, codec, NULL);
	if (ret) {
		return -1;
	}	
	
	//初始化封装格式上下文
	AVFormatContext* avformat_context = NULL;
	avformat_alloc_output_context2(&avformat_context, NULL, NULL, outpath);

	//添加视频流
	AVStream* avvideo_stream = avformat_new_stream(avformat_context, codec);//创建一个流
	avvideo_stream->id 					 = 0;	
	avvideo_stream->time_base.den        = avcodec_context->time_base.den;
	avvideo_stream->time_base.num		 = avcodec_context->time_base.num;
	
	// 将AVCodecContext信息拷贝到AVCodecParameterst结构体中
    avcodec_parameters_from_context(avvideo_stream->codecpar, avcodec_context);

	int buffer_size = av_image_get_buffer_size(avcodec_context->pix_fmt,
                                                   avcodec_context->width,
                                                   avcodec_context->height,
                                                   1);
	unsigned char *out_buffer = (unsigned char *)av_malloc(buffer_size);

	AVFrame *frame = av_frame_alloc();
	av_image_fill_arrays(frame->data,
	                             frame->linesize,
	                             out_buffer,
	                             avcodec_context->pix_fmt,
	                             avcodec_context->width,
	                             avcodec_context->height,
	                             1);
	frame->format = AV_PIX_FMT_YUV420P;
	frame->width  = width;
	frame->height = height;

	//打开输出文件
	if (avio_open(&avformat_context->pb, outpath, AVIO_FLAG_WRITE) < 0) {
    	return -1;
	}
	
	int av_wh_result = avformat_write_header(avformat_context, NULL);
	if (av_wh_result != AVSTREAM_INIT_IN_WRITE_HEADER) {
		return -1;
	}

	unsigned char * file_buffer = (unsigned char *)av_malloc(width * height * 3 / 2);

	FILE *in_file = fopen(inpath, "rb");
	int i= 0;

	AVPacket *av_packet = av_packet_alloc();
	av_init_packet(av_packet);

	int readonce = width * height * 3 / 2;

	while(TRUE) {

	    //读取yuv帧数据  注意yuv420p的长度  width * height * 3 / 2
	    if (fread(file_buffer, 1, readonce, in_file) <= 0) {
	        break;
	    } else if (feof(in_file)) {
	        break;
	    }		

	    //封装yuv帧数据
	    frame->data[0] = file_buffer;//y数据的起始位置在数组中的索引
	    frame->data[1] = file_buffer + width * height;//u数据的起始位置在数组中的索引
	    frame->data[2] = file_buffer + width * height * 5 / 4;//v数据的起始位置在数组中的索引
	    frame->linesize[0] = width    ;//y数据的行宽
	    frame->linesize[1] = width / 2;//u数据的行宽
	    frame->linesize[2] = width / 2;//v数据的行宽
	    frame->pts = i;
	    i++;

	    avcodec_send_frame(avcodec_context, frame);//将yuv帧数据送入编码器

    	while(TRUE) {
	        int ret = avcodec_receive_packet(avcodec_context, av_packet);//从编码器中取出h264帧
	        if (ret) {
	            av_packet_unref(av_packet);
	            break;
	        }
	        av_packet_rescale_ts(av_packet, avcodec_context->time_base, avvideo_stream->time_base);
	        av_packet->stream_index = avvideo_stream->index;
			printf("av_packet->dts = %d,av_packet->pts = %d\n",av_packet->dts,av_packet->pts);

	        //将帧写入视频文件中，与av_write_frame的区别是,将对 packet 进行缓存和 pts 检查。
	        av_interleaved_write_frame(avformat_context, av_packet);
    	}
	}

	//读取剩余的帧
	avcodec_send_frame(avcodec_context, frame);
	while(TRUE) {
	    int ret = avcodec_receive_packet(avcodec_context, av_packet);
	    if (ret) {
	        av_packet_unref(av_packet);
	        break;
	    }
	    av_packet_rescale_ts(av_packet, avcodec_context->time_base, avvideo_stream->time_base);
	    av_packet->stream_index = avvideo_stream->index;

	    //将对 packet 进行缓存和 pts 检查，这是区别于 av_write_frame 的地方。
	    av_write_frame(avformat_context, av_packet);
	}
	
	fclose(in_file);
	avcodec_close(avcodec_context);
	av_free(frame);
	av_free(out_buffer);
	av_packet_free(&av_packet);
	av_write_trailer(avformat_context);
	avio_close(avformat_context->pb);
	avformat_free_context(avformat_context);
		
	return 0;

}