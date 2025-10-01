#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <time.h>
#include <unistd.h>

static AVFormatContext *format_context = NULL;
static AVStream *stream = NULL;
static AVPacket *packet = NULL;

double get_value_from_avrational(AVRational* value){
	return ((double)value->num)/value->den;
}

void clean(){
	avformat_close_input(&format_context);
	av_packet_free(&packet);
}

int main(int argc, char* argv[]){
	if(argc != 2){
		printf("Usage: <%s> <inputfile>\n", argv[0]);
		exit(0);
	}
	char* input_file = argv[1];
	if(avformat_open_input(&format_context, input_file, NULL, NULL) < 0){
		fprintf(stderr, "Could not open source file %s\n", input_file);
		clean();
		exit(1);
	}
	if(avformat_find_stream_info(format_context, NULL) < 0){
		fprintf(stderr, "Could not find stream information\n");
		clean();
		exit(1);
	}
	int stream_index = av_find_best_stream(format_context, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	if(stream_index < 0){
		fprintf(stderr, "Could not find stream in input file '%s'\n", input_file);
		clean();
		exit(1);
	}
	stream = format_context->streams[stream_index];
	int width = stream->codecpar->width;
	int height = stream->codecpar->height;
	packet = av_packet_alloc();
	if(!packet){
		fprintf(stderr, "Could not allocate packet\n");
		clean();
		exit(1);
	}

	FILE* output = fopen("video", "w+");
	struct timespec current_time, start_time;
	long long time_elapsed_u, time_to_send_u, diff;
	int n = 0;
	char name[30];
	clock_gettime(CLOCK_MONOTONIC, &start_time);
	while(av_read_frame(format_context, packet) >= 0){
		if(packet->stream_index == stream_index){
			time_to_send_u = packet->pts*av_q2d(stream->time_base)*1e6;
			clock_gettime(CLOCK_MONOTONIC, &current_time);
			time_elapsed_u = (current_time.tv_sec - start_time.tv_sec)*1e6 + (current_time.tv_nsec - start_time.tv_nsec)/1e3;
			long long diff = time_to_send_u - time_elapsed_u;
			if(diff >= 0){
				usleep(diff);
			}
			sprintf(name, "images/%d.jpeg", n++);
//			fwrite(packet->data, packet->size, 1, output);
			FILE* jpeg_file = fopen(name, "w+");
			fwrite(packet->data, packet->size, 1, jpeg_file);
			fclose(jpeg_file);
		}
	}
	clean();
	return 0;
}
