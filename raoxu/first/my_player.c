/** 
 * 最简单的基于FFmpeg的视频播放器 
 * Simplest FFmpeg Player 
 * 本程序实现了视频文件的解码和显示（支持HEVC，H.264，MPEG2等）。 
 * 可以播放音频和视频节目 
 * This software is a simplest video player based on FFmpeg. 
 * Suitable for beginner of FFmpeg. 
 */  


#include <stdio.h>  

#define __STDC_CONSTANT_MACROS  

#include "libavcodec/avcodec.h"  
#include "libavformat/avformat.h"  
#include "libswscale/swscale.h" 
#include "libswresample/swresample.h"
#include <SDL/SDL.h>

#define VIDEO_PICTURE_QUEUE_SIZE 5
#define SAMPLE_QUEUE_SIZE 16
#define FRAME_QUEUE_SIZE 16
#define MAX_AUDIO_FRAME_SIZE 192000

typedef struct MyAVPacketList {
    AVPacket pkt;
    struct MyAVPacketList *next;
    int serial;
} MyAVPacketList;

typedef struct PacketQueue {
    MyAVPacketList *first_pkt, *last_pkt;
    int nb_packets;
    int size;
    int abort_request;
    int serial;
    SDL_mutex *mutex;
    SDL_cond *cond;
} PacketQueue;


typedef struct Frame {
    AVFrame *frame;
    AVSubtitle sub;
    int serial;
    double pts;           /* presentation timestamp for the frame */                                                                   
	double duration;      /* estimated duration of the frame */
	int64_t pos;          /* byte position of the frame in the input file */
	SDL_Overlay *bmp;
	int allocated;
	int reallocate;      
	int width;           
	int height;          
	AVRational sar;      
} Frame;                 

typedef struct FrameQueue {
	Frame queue[FRAME_QUEUE_SIZE];
	int rindex;          
	int windex;          
	int size;            
	int max_size;        
	int keep_last;       
	int rindex_shown;
	SDL_mutex *mutex;    
	SDL_cond *cond;      
	PacketQueue *pktq;   
} FrameQueue;

typedef struct VideoState {
	int exit;
	AVFormatContext *pFormatCtx;
	int videoindex;
	int audioindex;

	PacketQueue videoq;
	PacketQueue audioq;

	FrameQueue pictq;
	FrameQueue sampq;

	SDL_Thread *read_tid;
	SDL_Thread *video_tid;
	SDL_Thread *audio_tid;

	char filename[1024];
	SDL_Overlay *bmp;   
	SDL_Rect rect; 
	struct SwsContext *img_convert_ctx;
	double vpts;
	double video_time_base;
	double video_clock;

	AVStream *video_st;
	AVCodecContext  *pCodecCtx; 

	AVStream *avdio_st;
	Uint8    *out_buffer;
	int		 out_buffer_size;
	struct SwrContext *au_convert_ctx;  
	double apts;
	double audio_time_base;
	double audio_clock;
} VideoState;

Uint8	 *audio_chunk;   
Uint32   audio_len;   
Uint8    *audio_pos;

static int packet_queue_put_private(PacketQueue *q, AVPacket *pkt)
{
	MyAVPacketList *pkt1;

	if (q->abort_request)
		return -1;

	pkt1 = av_malloc(sizeof(MyAVPacketList));

	if (!pkt1)
		return -1;
	pkt1->pkt = *pkt;
	pkt1->next = NULL;

	if (!q->last_pkt)
		q->first_pkt = pkt1;
	else
		q->last_pkt->next = pkt1;
	q->last_pkt = pkt1;
	q->nb_packets++;
	q->size += pkt1->pkt.size + sizeof(*pkt1);

	return 0;
}

static int packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
	int ret;

	if(av_dup_packet(pkt) < 0)
		return -1;

	SDL_LockMutex(q->mutex);
	ret = packet_queue_put_private(q, pkt);
	SDL_UnlockMutex(q->mutex);

	if (ret < 0)
		av_free_packet(pkt);

	return ret;
}


/* packet queue handling */
static void packet_queue_init(PacketQueue *q)
{
	memset(q, 0, sizeof(PacketQueue));
	q->mutex = SDL_CreateMutex();
	q->cond = SDL_CreateCond();
	q->abort_request = 0;
}

static void packet_queue_flush(PacketQueue *q)
{
	MyAVPacketList *pkt, *pkt1;

	SDL_LockMutex(q->mutex);
	for (pkt = q->first_pkt; pkt; pkt = pkt1) {
		pkt1 = pkt->next;
		av_free_packet(&pkt->pkt);
		av_freep(&pkt);
	}
	q->last_pkt = NULL;
	q->first_pkt = NULL;
	q->nb_packets = 0;
	q->size = 0;
	SDL_UnlockMutex(q->mutex);
}

static void packet_queue_destroy(PacketQueue *q)
{
	packet_queue_flush(q);
	SDL_DestroyMutex(q->mutex);
	SDL_DestroyCond(q->cond);
}

/* return < 0 if aborted, 0 if no packet and > 0 if packet.  */
static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block)
{
	MyAVPacketList *pkt1;
	int ret;

	SDL_LockMutex(q->mutex);

	for (;;) {
		if (q->abort_request) {
			ret = -1;
			break;
		}

		pkt1 = q->first_pkt;
		if (pkt1) {
			q->first_pkt = pkt1->next;
			if (!q->first_pkt)
				q->last_pkt = NULL;
			q->nb_packets--;
			q->size -= pkt1->pkt.size + sizeof(*pkt1);
			*pkt = pkt1->pkt;
			av_free(pkt1);
			ret = 1;
			break;
		} else if (!block) {
			ret = 0;
			break;
		} else {
			SDL_CondWait(q->cond, q->mutex);
		}
	}
	SDL_UnlockMutex(q->mutex);
	return ret;
}

static int frame_queue_init(FrameQueue *f, PacketQueue *pktq, int max_size, int keep_last)
{
	int i;
	memset(f, 0, sizeof(FrameQueue));
	if (!(f->mutex = SDL_CreateMutex()))
		return AVERROR(ENOMEM);
	if (!(f->cond = SDL_CreateCond()))
		return AVERROR(ENOMEM);
	f->pktq = pktq;
	f->max_size = max_size;
	f->keep_last = !!keep_last;
	for (i = 0; i < f->max_size; i++)//这样使用更加数组方便，也不会太占用栈的空间
		if (!(f->queue[i].frame = av_frame_alloc()))
			return AVERROR(ENOMEM);
	return 0;
}

static void frame_queue_destory(FrameQueue *f)
{
	int i;
	for (i = 0; i < f->max_size; i++) {
		Frame *vp = &f->queue[i];
//		frame_queue_unref_item(vp);
		av_frame_free(&vp->frame);
//		free_picture(vp);
	}
	SDL_DestroyMutex(f->mutex);
	SDL_DestroyCond(f->cond);
}

//表示从循环队列帧里面取出当前需要显示的一帧视频
static Frame *frame_queue_peek(FrameQueue *f)
{//rindex 还是实在当前播的这一帧，因此要加f->rindex_shown，才能拿到需要显示的下一帧
	return &f->queue[(f->rindex + f->rindex_shown) % f->max_size];//这是一个循环队列
}
//表示从循环队列帧里面取出当前需要显示的下一帧视频
static Frame *frame_queue_peek_next(FrameQueue *f)
{
	return &f->queue[(f->rindex + f->rindex_shown + 1) % f->max_size];
}

static Frame *frame_queue_peek_last(FrameQueue *f)
{
	return &f->queue[f->rindex];//即正在播的这一帧或者是可以最先拿出来的一帧
}

static Frame *frame_queue_peek_writable(FrameQueue *f)
{
	/* wait until we have space to put a new frame */
	SDL_LockMutex(f->mutex);
	while (f->size >= f->max_size &&
			!f->pktq->abort_request) {
		SDL_CondWait(f->cond, f->mutex);
	}
	SDL_UnlockMutex(f->mutex);

	if (f->pktq->abort_request)
		return NULL;

	return &f->queue[f->windex];
}

static Frame *frame_queue_peek_readable(FrameQueue *f)
{
	/* wait until we have a readable a new frame */
	SDL_LockMutex(f->mutex);
	while (f->size - f->rindex_shown <= 0 &&
			!f->pktq->abort_request) {
		SDL_CondWait(f->cond, f->mutex);
	}
	SDL_UnlockMutex(f->mutex);

	if (f->pktq->abort_request)
		return NULL;

	return &f->queue[(f->rindex + f->rindex_shown) % f->max_size];
}

static void frame_queue_push(FrameQueue *f)
{
	if (++f->windex == f->max_size)
		f->windex = 0;
	SDL_LockMutex(f->mutex);
	f->size++;
	SDL_CondSignal(f->cond);
	SDL_UnlockMutex(f->mutex);
}

static void frame_queue_next(FrameQueue *f)
{
	if (f->keep_last && !f->rindex_shown) {
		f->rindex_shown = 1;
		return;
	}
//	frame_queue_unref_item(&f->queue[f->rindex]);
	if (++f->rindex == f->max_size)//无论时push还是pop其实都是更新索引值，也就是数组的下标
		f->rindex = 0;//这是一个循环队列
	SDL_LockMutex(f->mutex);
	f->size--;
	SDL_CondSignal(f->cond);
	SDL_UnlockMutex(f->mutex);
}

double synchronize(VideoState *is, AVFrame *srcFrame, double pts)
{
	double frame_delay;

	if (pts != 0)
		is->video_clock = pts; // Get pts,then set video clock to it
	else
		pts = is->video_clock; // Don't get pts,set it to video clock

	frame_delay = is->video_time_base;
	frame_delay += srcFrame->repeat_pict * (frame_delay * 0.5);

	is->video_clock += frame_delay;

	return pts;
}

static int video_thread(void *arg) 
{
	VideoState *is = arg;
	AVCodecContext  *pCodecCtx;  
	AVCodec         *pCodec;  
	int				screen_w, screen_h;  
	SDL_Surface     *screen;   
	SDL_VideoInfo   *vi;  
	int				get_picture;
	int				ret;
	Frame			*vf;
	AVFrame			*pFrame;
	double			pts;

	pCodecCtx = is->pFormatCtx->streams[is->videoindex]->codec;  
	is->pCodecCtx = pCodecCtx;
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);

	if(pCodec == NULL){  
		printf("Codec not found\n");  
		return -1;  
	}
	
	if(avcodec_open2(pCodecCtx, pCodec,NULL) < 0) {
		printf("Could not open codec.\n");
		return -1;
	}

	is->video_time_base = av_q2d(is->pFormatCtx->streams[is->videoindex]->time_base);
	printf("is->video_time_base %f\n", is->video_time_base);

	screen_w = pCodecCtx->width;  
	screen_h = pCodecCtx->height;  
	screen = SDL_SetVideoMode(screen_w, screen_h, 0,0);  

	if(!screen) {    
		printf("SDL: could not set video mode - exiting:%s\n",SDL_GetError());    
		return -1;  
	}  

	//创建Overlay表面
	is->bmp = SDL_CreateYUVOverlay(pCodecCtx->width, pCodecCtx->height,SDL_YV12_OVERLAY, screen);   

	is->rect.x = 0;      
	is->rect.y = 0;      
	is->rect.w = screen_w;      
	is->rect.h = screen_h;    

	is->bmp = SDL_CreateYUVOverlay(pCodecCtx->width, pCodecCtx->height,SDL_YV12_OVERLAY, screen);
	pFrame = av_frame_alloc();  
	AVPacket* packet = (AVPacket*)av_malloc(sizeof(AVPacket));  

	//将从解码出来的原始数据换成本次我们将要显示的数据，pCodecCtx->pix_fmt >> PIX_FMT_YUV420P
	is->img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);   

	while(1) {
		ret = packet_queue_get(&is->videoq, packet, 0);
		if (ret > 0) {
			if (avcodec_decode_video2(pCodecCtx, pFrame, &get_picture, packet) >= 0) {
				if(get_picture) {
					if ((pts = av_frame_get_best_effort_timestamp(pFrame)) == AV_NOPTS_VALUE)
						pts = 0;
					pts *= is->video_time_base;
					pts = synchronize(is, pFrame, pts);
					pts *= 1000;
					vf = frame_queue_peek_writable(&is->pictq);
					av_frame_move_ref(vf->frame, pFrame); 
					vf->frame->pts = (int64_t)pts;
					frame_queue_push(&is->pictq);
				}
			}
		}
	}
}

void  fill_audio(void *udata, Uint8 *stream, int len){   
	if(audio_len == 0)        
		return;   
	len = (len>audio_len?audio_len:len);    

	SDL_MixAudio(stream, audio_pos, len, SDL_MIX_MAXVOLUME);  
	audio_pos += len;   
	audio_len -= len;   
}

static int audio_thread(void *arg) 
{
	VideoState *is = arg; 
	
	int             i, audioStream;  
	AVCodecContext  *pCodecCtx; 
	AVCodec         *pCodec; 
	Frame			*af; 
	AVFrame			*pFrame;

	// Get a pointer to the codec context for the audio stream  
	pCodecCtx = is->pFormatCtx->streams[is->audioindex]->codec;  
	is->audio_time_base = av_q2d(is->pFormatCtx->streams[is->audioindex]->time_base);

	// Find the decoder for the audio stream  
	pCodec=avcodec_find_decoder(pCodecCtx->codec_id);  
	if(pCodec==NULL){  
		printf("Codec not found.\n");  
		return -1;  
	}  

	// Open codec  
	if(avcodec_open2(pCodecCtx, pCodec,NULL)<0){  
		printf("Could not open codec.\n");  
		return -1;  
	}

	pFrame = av_frame_alloc();
	AVPacket *packet = (AVPacket *)malloc(sizeof(AVPacket));  
	av_init_packet(packet);  

	//Out Audio Param  
	uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;  
	//AAC:1024  MP3:1152  
	int out_nb_samples = pCodecCtx->frame_size;  
	//	AVSampleFormat out_sample_fmt=AV_SAMPLE_FMT_S16;  
	int out_sample_fmt = 1;  
	int out_sample_rate = 44100;  
	int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);  
	//Out Buffer Size  
	is->out_buffer_size = av_samples_get_buffer_size(NULL,out_channels ,out_nb_samples,out_sample_fmt, 1);	
	is->out_buffer = (uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE*2);  

	//SDL_AudioSpec  
	SDL_AudioSpec wanted_spec;  
	wanted_spec.freq = out_sample_rate;   
	wanted_spec.format = AUDIO_S16SYS;   
	wanted_spec.channels = out_channels;   
	wanted_spec.silence = 0;   
	wanted_spec.samples = out_nb_samples;   
	wanted_spec.callback = fill_audio;   
	wanted_spec.userdata = pCodecCtx;   

	if (SDL_OpenAudio(&wanted_spec, NULL)<0){   
		printf("can't open audio\n");   
		return -1;   
	}

	uint32_t ret,len = 0;  
	int get_picture;  
	//FIX:Some Codec's Context Information is missing  
	int64_t in_channel_layout=av_get_default_channel_layout(pCodecCtx->channels);  
	//Swr  
	is->au_convert_ctx = swr_alloc();  
	is->au_convert_ctx = swr_alloc_set_opts(is->au_convert_ctx,out_channel_layout, out_sample_fmt, out_sample_rate, in_channel_layout,pCodecCtx->sample_fmt , pCodecCtx->sample_rate,0, NULL);  
	swr_init(is->au_convert_ctx);  

	//Play  
	SDL_PauseAudio(0); 

	while(1){  
		ret = packet_queue_get(&is->audioq, packet, 0);
		if (ret > 0) {
			if (avcodec_decode_audio4(pCodecCtx, pFrame, &get_picture, packet) > 0) {  
				if (get_picture > 0){ 
					if (packet->pts != AV_NOPTS_VALUE){
						is->audio_clock = is->audio_time_base * packet->pts;
						is->audio_clock *= 1000;
					}
					af = frame_queue_peek_writable(&is->sampq);
					av_frame_move_ref(af->frame, pFrame);
					frame_queue_push(&is->sampq); 
				}  
			}
		}
	}
}

static int read_thread(void *arg) 
{
	VideoState *is = arg;

	is->pFormatCtx = avformat_alloc_context();
	if(avformat_open_input(&(is->pFormatCtx), is->filename, NULL, NULL) != 0){  
		printf("Couldn't open input stream\n");  
		return -1;  
	}  

	if(avformat_find_stream_info(is->pFormatCtx, NULL)<0){  
		printf("Couldn't find stream information\n");  
		return -1;  
	}

	for(int i=0; i < is->pFormatCtx->nb_streams; i++) {  
		if(is->pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){  
			is->videoindex = i; 
		}
		if(is->pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO){  
			is->audioindex = i;  
		}
	}

	//Init  
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {    
		printf( "Could not initialize SDL - %s\n", SDL_GetError());   
		return -1;  
	}

	is->video_tid = SDL_CreateThread(video_thread, is);
	is->audio_tid = SDL_CreateThread(audio_thread, is);

	AVPacket *packet = (AVPacket*)av_malloc(sizeof(AVPacket)); 
	while(1){ 
		//当音视频的帧满了这里就不再进行拆包了
		if (is->pictq.size == VIDEO_PICTURE_QUEUE_SIZE || is->sampq.size == SAMPLE_QUEUE_SIZE) {
		} else {
			if (av_read_frame(is->pFormatCtx, packet) >= 0) {
				if(packet->stream_index == is->videoindex){
					packet_queue_put(&is->videoq, packet);
				} else if(packet->stream_index == is->audioindex){
					packet_queue_put(&is->audioq, packet);
				} 
			} else {
				is->exit |= 0x1;
				break;
			}
		}
	}
}

void video_refresh(VideoState *is)
{
	Frame *vf;
	AVFrame	*pFrameYUV, frame;
	static int diffence = 0;

	pFrameYUV = &frame;
	if (is->pictq.size > 0) {
		vf = frame_queue_peek_next(&is->pictq);
		if (!vf->frame) { 
			return ;
		}

		int duration = vf->frame->pts - is->audio_clock;
		if (duration <= 45) 
			goto disp;
		else 
			return;
	} else 
		return ;

disp:
	printf("diffence %d,is->audio_clock %f, vf->frame->pts %d\n", diffence, is->audio_clock, vf->frame->pts);
	//display picture
	//取得独占权和 Overlay 表面首地址
	SDL_LockYUVOverlay(is->bmp);  

	pFrameYUV->data[0] = is->bmp->pixels[0];  
	pFrameYUV->data[1] = is->bmp->pixels[2];  
	pFrameYUV->data[2] = is->bmp->pixels[1];       
	pFrameYUV->linesize[0] = is->bmp->pitches[0];  
	pFrameYUV->linesize[1] = is->bmp->pitches[2];     
	pFrameYUV->linesize[2] = is->bmp->pitches[1];  

	//将pFrame里面的数据转换成yuv数据
	sws_scale(is->img_convert_ctx, (const uint8_t* const*)vf->frame->data, vf->frame->linesize, \
			0,is->pCodecCtx->height, pFrameYUV->data, pFrameYUV->linesize);  

	SDL_UnlockYUVOverlay(is->bmp);
	SDL_DisplayYUVOverlay(is->bmp, &is->rect);

	frame_queue_next(&is->pictq);
}

void audio_refresh(VideoState *is)
{
	Frame *af;

	if (audio_len <= 0 && is->sampq.size > 0) {
		af = frame_queue_peek_next(&is->sampq);
		if(!af->frame) {
			return;
		}

		swr_convert(is->au_convert_ctx, &is->out_buffer, MAX_AUDIO_FRAME_SIZE, (const uint8_t **)af->frame->data , af->frame->nb_samples);
		
		audio_chunk = is->out_buffer;
		audio_len   = is->out_buffer_size;
		audio_pos   = audio_chunk;

		frame_queue_next(&is->sampq);
	}
}


void do_exit(VideoState *is) 
{
	swr_free(&is->au_convert_ctx);
	sws_freeContext(is->img_convert_ctx);
	SDL_Quit();
	SDL_CloseAudio();
	avformat_network_deinit();
	frame_queue_destory(&is->pictq);
	frame_queue_destory(&is->sampq);
	packet_queue_destroy(&is->videoq);
	packet_queue_destroy(&is->audioq);
}

int event_loop(VideoState *is)
{
	while(1) {
		if (is->exit == 1) {
			printf("do exit\n");
			do_exit(is);
			return 0;
		}
		audio_refresh(is); 
		video_refresh(is);
	}
}

int main(int argc, char *argv[])
{
	VideoState *is;
	is = av_mallocz(sizeof(VideoState));
	if (!is) 
		return -1; 

	is->exit = 0;
	av_register_all();  
	avformat_network_init();

	if (frame_queue_init(&is->pictq, &is->videoq, VIDEO_PICTURE_QUEUE_SIZE, 1) < 0)
		return -1;

	if (frame_queue_init(&is->sampq, &is->audioq, SAMPLE_QUEUE_SIZE, 1) < 0)
		return -1;

	packet_queue_init(&is->videoq);
	packet_queue_init(&is->audioq);

	memcpy(is->filename, argv[1], sizeof(argv[1])); 

	is->read_tid = SDL_CreateThread(read_thread, is);

	event_loop(is);

	return 0;
}


