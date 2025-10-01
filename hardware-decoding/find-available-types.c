#include <stdio.h>
#include <libavutil/hwcontext.h>

int main() {
    // Initialize FFmpeg libraries

    // Iterate through available hardware device types
    enum AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;
    printf("Available Hardware Device Types:\n");
    while ((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE) {
        const char* name = av_hwdevice_get_type_name(type);
	AVBufferRef *hw_device = NULL;
	int ret = av_hwdevice_ctx_create(&hw_device, type, NULL, NULL, 0);
	if (ret == 0) {
            printf("Hardware device %s is available %d.\n", name, type);
        } else {
            printf("Hardware device %s is not available %d.\n", name, type);
        }
        av_buffer_unref(&hw_device);  
    }

    return 0;
}

