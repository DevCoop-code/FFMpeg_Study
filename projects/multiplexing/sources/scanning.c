#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

static AVFormatContext* fmt_ctx = NULL;

#include <stdio.h>

int main(int args, char* argv[]) {
    unsigned int index;

    // Initialize the ffmpeg library(Muxer, Demuxer, Encoder, Decoder)
    av_register_all();

    // FFmpeg 라이브러리 레벨에서 디버깅 로그를 출력하도록 함
    // av_log_set_level(AV_LOG_DEBUG);

    if (argv < 2) {
        printf("usage: %s <input>\n", argv[0]);
        return 0;
    }

    // 주어진 파일 이름으로 fmt_ctx를 가져옴, avformat_open_input : 주어진 파일을 읽어 AVFormatContext에 저장
    if (avformat_open_input(&fmt_ctx, argv[1], NULL, NULL) < 0) {
        printf("Could not open input file %s\n", argv[1]);
        return -1;
    }

    // 주어진 fmt_ctx로부터 유효한 스트림이 있는지 찾습니다
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        printf("Failed to retrieve input stream information\n");
        return -2;
    }

    // fmt_ctx->nb_streams : 컨테이너가 저장하고 있는 총 스트림 개수
    for (index = 0; index < fmt_ctx->nb_streams; index++) {
        AVCodecContext* avCodecContext = fmt_ctx->streams[index]->codec;
        if (avCodecContext->codec_type == AVMEDIA_TYPE_VIDEO) {
            printf("-----Video info-----\n");
            printf("codec id : %d\n", avCodecContext->codec_id);
            printf("bitrate : %d\n", avCodecContext->bit_rate);
            printf("width : %d / height : %d\n", avCodecContext->width, avCodecContext->height);
        }
        else if(avCodecContext->codec_type == AVMEDIA_TYPE_AUDIO) {
            printf("-----Audio info-----\n");
            printf("codec id : %d\n", avCodecContext->codec_id);
            printf("bitrate : %d\n", avCodecContext->bit_rate);
            printf("width : %d / height : %d\n", avCodecContext->width, avCodecContext->height);
        }
    }

    if (fmt_ctx != NULL) {
        avformat_close_input(&fmt_ctx);
    }
    return 0;
}