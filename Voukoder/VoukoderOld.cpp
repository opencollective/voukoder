#include "VoukoderOld.h"
#include <wx/wx.h>
#include "../Core/wxVoukoderDialog.h"
#include "../Core/EncoderEngine.h"

static EncoderEngine* encoderEngine = NULL;

class actctx_activator
{
protected:
	ULONG_PTR m_cookie; // Cookie for context deactivation

public:
	// Construct the activator and activates the given activation context
	actctx_activator(_In_ HANDLE hActCtx)
	{
		if (!ActivateActCtx(hActCtx, &m_cookie))
			m_cookie = 0;
	}

	// Deactivates activation context and destructs the activator
	virtual ~actctx_activator()
	{
		if (m_cookie)
			DeactivateActCtx(0, m_cookie);
	}
};

HINSTANCE g_instance = NULL;
HANDLE g_act_ctx = NULL;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		g_instance = hModule;
		GetCurrentActCtx(&g_act_ctx);
	}
	else if (ul_reason_for_call == DLL_PROCESS_DETACH)
	{
		if (g_act_ctx)
			ReleaseActCtx(g_act_ctx);
	}

	return TRUE;
}

void init(const char* appname)
{
	Voukoder::Config::Get();
}

bool open_config_dialog(char* settings, HWND hwnd)
{
	// Default window is the active window
	if (hwnd == NULL)
		hwnd = GetActiveWindow();

	// Restore plugin's activation context.
	actctx_activator actctx(g_act_ctx);

	// Initialize application.
	new wxApp();
	wxEntryStart(g_instance);

	// Create and launch configuration dialog.
	ExportInfo exportInfo;
	exportInfo.Deserialize(settings);

	int result;
	
	{
		// Create wxWidget-approved parent window.
		wxWindow parent;
		parent.SetHWND((WXHWND)hwnd);
		parent.AdoptAttributesFromHWND();
		wxTopLevelWindows.Append(&parent);

		// Show the dialog
		wxVoukoderDialog dialog(&parent, exportInfo);
		result = dialog.ShowModal();

		wxTopLevelWindows.DeleteObject(&parent);
		parent.SetHWND((WXHWND)NULL);
	}

	wxEntryCleanup();

	if (result == (int)wxID_OK)
	{
		strcpy(settings, exportInfo.Serialize().c_str());

		return true;
	}

	return false;
}

bool encoder_open(const char* settings)
{
	// Create export info
	ExportInfo exportInfo;
	exportInfo.Deserialize(settings);
	exportInfo.application = VKDR_APPNAME;

	// TODO
	exportInfo.video.width = 320;
	exportInfo.video.height = 240;
	exportInfo.video.sampleAspectRatio = { 1, 1 };
	exportInfo.video.timebase = { 1,25 };
	exportInfo.video.pixelFormat = AV_PIX_FMT_YUV420P;
	exportInfo.video.colorRange = AVCOL_RANGE_MPEG;
	exportInfo.video.colorSpace = AVCOL_SPC_BT709;
	exportInfo.video.colorPrimaries = AVCOL_PRI_BT709;
	exportInfo.video.colorTransferCharacteristics = AVCOL_TRC_BT709;
	exportInfo.audio.timebase = { 1, 44100 };
	exportInfo.audio.sampleFormat = AVSampleFormat::AV_SAMPLE_FMT_FLTP;
	exportInfo.audio.channelLayout = av_get_channel_layout("stereo");

	int ret;

	// Create encoder engine instance
	encoderEngine = new EncoderEngine(exportInfo);
	if ((ret = encoderEngine->open()) < 0)
	{
		vkLogErrorVA("Unable to open the encoder engine with the supplied export info. (Code: %d)", ret);
		return false;
	}

	return true;
}

void encoder_close()
{
	assert(encoderEngine != NULL);

	// Finalize and close
	encoderEngine->finalize();
	encoderEngine->close();

	encoderEngine = NULL;
}

bool encoder_write_video_frame(int width, int height, const char* pixfmt, int idx, uint8_t** data, int* linesize)
{
	assert(encoderEngine != NULL);
	assert(sizeof(data) / sizeof(data[0]) == sizeof(linesize) / sizeof(linesize[0]));

	// Create a frame
	AVFrame* frame = av_frame_alloc();
	frame->width = width;
	frame->height = height;
	frame->format = av_get_pix_fmt(pixfmt);
	frame->pts = idx;

	// Fill frame buffer
	for (int i = 0; i < sizeof(data) / sizeof(data[0]); i++)
	{
		frame->data[i] = data[i];
		frame->linesize[i] = linesize[i];
	}

	// Encode & write video frame
	if (encoderEngine->writeVideoFrame(frame) < 0)
	{
		vkLogErrorVA("Unable to write video frame #%d.", frame->pts);

		av_frame_free(&frame);

		return false;
	}

	av_frame_free(&frame);

	return true;
}

bool encoder_write_audio_frame(const char* smpfmt, uint8_t** data, int size)
{
	//// Create a frame
	//AVFrame* frame = av_frame_alloc();
	//frame->sample_rate = sampleRate;


	//frame->format = av_get_sample_fmt(smpfmt);
	//frame->channel_layout = av_get_channel_layout();
	//frame->nb_samples = chunkSize;

	return true;
}
