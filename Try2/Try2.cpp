#include <gst/gst.h>
#include <signal.h>

GstElement* pipeline;

//TODO: ��������� ������ ��������� �����

static void sigint_handler(int signum) {
    g_print("Received SIGINT, stopping pipeline.\n");
    gst_element_send_event(pipeline, gst_event_new_eos());
}

static void on_pad_added(GstElement* element, GstPad* pad, gpointer data) {
    gchar* name = gst_pad_get_name(pad);  // �������� ��� ����� ������������ ����.

    GstElement* other_element = NULL;  // �������������� ��������� �� ������ �������, ������� ����� �������� ������ � ����� ����.

    // ���������, ����� �� ��� ���� �������, ����������� �� ��, ��� ��� �����-���.
    if (g_str_has_prefix(name, "audio")) {
        // �������� ������� 'audio_queue' �� ���������.
        other_element = gst_bin_get_by_name(GST_BIN(pipeline), "audio_queue");

        // �������� ��� ��������� �������� 'audio_queue'.
        GstPad* sinkpad = gst_element_get_static_pad(other_element, "sink");

        // ��������� ����, ����� ���������� ����� ����� �����������������/��������� � ��������� 'audio_queue'.
        if (gst_pad_link(pad, sinkpad) != GST_PAD_LINK_OK) {
            g_printerr("�� ������� ���������� ����� ����� �����������������/��������� � �����\n");
        }
        gst_object_unref(sinkpad);  // ����������� ������ �� ��� ���������.
    }
    // ���������, ����� �� ��� ���� �������, ����������� �� ��, ��� ��� �����-���.
    else if (g_str_has_prefix(name, "video")) {
        // �������� ������� 'video_queue' �� ���������.
        other_element = gst_bin_get_by_name(GST_BIN(pipeline), "video_queue");

        // �������� ��� ��������� �������� 'video_queue'.
        GstPad* sinkpad = gst_element_get_static_pad(other_element, "sink");

        // ��������� ����, ����� ���������� ����� ����� �����������������/��������� � ��������� 'video_queue'.
        if (gst_pad_link(pad, sinkpad) != GST_PAD_LINK_OK) {
            g_printerr("�� ������� ���������� ����� ����� �����������������/��������� � �����\n");
        }

        gst_object_unref(sinkpad);  // ����������� ������ �� ��� ���������.
    }

    GstCaps* caps = gst_pad_query_caps(pad, NULL);  // ����������� �������������� ����.
    g_print("�������������� ���� %s: %s\n", name, gst_caps_to_string(caps));  // ������� �������������� ���� �� ������.
    gst_caps_unref(caps);  // ����������� �������, ���������� ��� �������������� ����.

    g_free(name);  // ����������� ������, ���������� ��� ��� ����.
}



int main(int argc, char* argv[]) {
    gst_init(&argc, &argv);

    pipeline = gst_pipeline_new("my_pipeline");

    GstElement* source = gst_element_factory_make("filesrc", "source");
    GstElement* demuxer = gst_element_factory_make("matroskademux", "demuxer");
    GstElement* video_queue = gst_element_factory_make("queue", "video_queue");
    GstElement* audio_queue = gst_element_factory_make("queue", "audio_queue");
    GstElement* queue_for_display = gst_element_factory_make("queue", "queue_for_display");
    GstElement* queue_for_file = gst_element_factory_make("queue", "queue_for_file");
    GstElement* video_encoder = gst_element_factory_make("jpegenc", "video_encoder"); // video encoder for avi
    GstElement* video_decoder = gst_element_factory_make("vp9dec", "video_decoder");
    GstElement* audio_decoder = gst_element_factory_make("opusdec", "audio_decoder");
    GstElement* video_sink = gst_element_factory_make("autovideosink", "video_sink");  // New video sink
    //GstElement* audio_sink = gst_element_factory_make("autoaudiosink", "audio_sink");  // ����� webm �� �������������
    GstElement* tee = gst_element_factory_make("tee", "tee"); 
    GstElement* avi_mux = gst_element_factory_make("avimux", "avi_mux");
    GstElement* file_sink = gst_element_factory_make("filesink", "file_sink");

    if (!pipeline || !source || !demuxer || !video_queue || !audio_queue ||
        !video_decoder || !audio_decoder || !video_sink || !tee || !avi_mux || !file_sink) {
        g_printerr("One or more elements could not be created.\n");
        return -1;
    }
    else {
		g_printerr("All elements created successfully.\n");
	}

    if (!video_encoder) {
        g_printerr("Video encoder element could not be created.\n");
        return -1;
    }
    else {
        g_object_set(video_encoder, "quality", 100, NULL);  // ������������� �������� �� ��������
        g_printerr("Video encoder element created successfully.\n");
    }

    g_object_set(source, "location", argv[1], NULL); //x64/Debug/temp/vid.webm
    g_object_set(file_sink, "location", argv[2], NULL); //x64/Debug/temp/output.avi


    gst_bin_add_many(GST_BIN(pipeline), source, demuxer, video_queue, audio_queue, video_encoder,
        video_decoder, audio_decoder, video_sink, tee, queue_for_display, queue_for_file, avi_mux, file_sink, NULL);


    gst_element_link(source, demuxer);
    gst_element_link_many(video_queue, video_decoder, tee, NULL);
    gst_element_link_many(tee, queue_for_display, video_sink, NULL);
    gst_element_link_many(tee, queue_for_file, video_encoder, avi_mux, NULL);
    gst_element_link_many(audio_queue, audio_decoder, avi_mux, NULL);
    gst_element_link(avi_mux, file_sink);



    g_signal_connect(demuxer, "pad-added", G_CALLBACK(on_pad_added), NULL);
    g_signal_connect(avi_mux, "pad-added", G_CALLBACK(on_pad_added), NULL);


    signal(SIGINT, sigint_handler);

    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    GstBus* bus = gst_element_get_bus(pipeline);
    GstMessage* msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
        (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    if (msg != NULL) {
        GError* err;
        gchar* debug_info;

        switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR:
            gst_message_parse_error(msg, &err, &debug_info);
            g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
            g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
            g_clear_error(&err);
            g_free(debug_info);
            break;
        case GST_MESSAGE_EOS:
            g_printerr("End-Of-Stream reached.\n");
            break;
        default:
            g_printerr("Unexpected message received.\n");
            break;
        }
        gst_message_unref(msg);
    }

    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    return 0;
}
