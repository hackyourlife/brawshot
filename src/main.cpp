#include "BlackmagicRawAPI.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <iostream>
#include <chrono>
#include <atomic>
#include <thread>

#include <turbojpeg.h>

#include "brawshot.h"

#ifdef DEBUG
	#include <cassert>
	#define VERIFY(condition) assert(SUCCEEDED(condition))
#else
	#define VERIFY(condition) condition
#endif

#define	JPEG_QUALITY	95
#define	JPEG_SUBSAMP	TJSAMP_444
#define	JPEG_FLAGS	(TJFLAG_FASTDCT)

static const BlackmagicRawResourceFormat s_resourceFormat = blackmagicRawResourceFormatRGBAU16;
static const char* outputFileName = "output";
static const char* ref_filename = nullptr;

static const int s_maxJobsInFlight = 1;
static std::atomic<int> jobsInFlight = {0};

static tjhandle tjinst = nullptr;

static bool single = false;
static bool raw_dump = false;
static bool ref_after_lut = false;

void output_image(unsigned int width, unsigned int height, uint8_t* image,
		unsigned long index)
{
	char filename[256];
	if(single) {
		strcpy(filename, outputFileName);
	} else {
		sprintf(filename, "%s-%04lu.jpg", outputFileName, index);
	}

	unsigned char* jpeg_buf = NULL;
	unsigned long jpeg_size = 0;

	if(tjCompress2(tjinst, image, width, 0, height, TJPF_BGRX, &jpeg_buf,
				&jpeg_size, JPEG_SUBSAMP, JPEG_QUALITY,
				JPEG_FLAGS) < 0) {
		printf("compression error\n");
		exit(1);
	} else {
		FILE* f = fopen(filename, "wb");
		fwrite(jpeg_buf, jpeg_size, 1, f);
		fclose(f);
		tjFree(jpeg_buf);
	}
}

void output_raw(unsigned int width, unsigned int height, uint16_t* image,
		unsigned long index)
{
	char filename[256];
	if(single) {
		strcpy(filename, outputFileName);
	} else {
		sprintf(filename, "%s-%04lu.raw", outputFileName, index);
	}

	FILE* f = fopen(filename, "wb");
	fwrite(image, width * height * sizeof(uint16_t), 4, f);
	fclose(f);
}

struct UserData {
	VideoProcessor*	processor;
	unsigned long	index;
	bool		add;
	bool		output;

	UserData(VideoProcessor* processor, bool add, bool output)
			: processor(processor), add(add), output(output) {}
	~UserData() {}
};

class CameraCodecCallback : public IBlackmagicRawCallback
{
public:
	explicit CameraCodecCallback() = default;
	virtual ~CameraCodecCallback() = default;

	virtual void ReadComplete(IBlackmagicRawJob* readJob, HRESULT result, IBlackmagicRawFrame* frame) {
		IBlackmagicRawJob* decodeAndProcessJob = nullptr;

		UserData* userData = nullptr;
		VERIFY(readJob->GetUserData((void**)&userData));

		if(result == S_OK) {
			VERIFY(frame->SetResourceFormat(s_resourceFormat));
		}

		if(result == S_OK) {
			result = frame->CreateJobDecodeAndProcessFrame(nullptr, nullptr, &decodeAndProcessJob);
		}

		if(result == S_OK) {
			result = decodeAndProcessJob->SetUserData(userData);
		}

		if(result == S_OK) {
			result = decodeAndProcessJob->Submit();
		}

		if(result != S_OK) {
			if(decodeAndProcessJob) {
				decodeAndProcessJob->Release();
			}
		}

		readJob->Release();
	}

	virtual void ProcessComplete(IBlackmagicRawJob* job, HRESULT result, IBlackmagicRawProcessedImage* processedImage) {
		unsigned int width = 0;
		unsigned int height = 0;
		void* imageData = nullptr;

		if(result == S_OK) {
			result = processedImage->GetWidth(&width);
		}

		if(result == S_OK) {
			result = processedImage->GetHeight(&height);
		}

		if(result == S_OK) {
			result = processedImage->GetResource(&imageData);
		}

		UserData* userData = nullptr;
		VERIFY(job->GetUserData((void**)&userData));

		if(result == S_OK) {
			if(userData->add) {
				userData->processor->add((uint16_t*) imageData);
			} else {
				userData->processor->subtract((uint16_t*) imageData);
			}

			if(userData->output) {
				if(raw_dump) {
					uint16_t* output = new uint16_t[width * height * 4];
					userData->processor->output_raw(output);

					--jobsInFlight;
					unsigned long index = userData->index;
					output_raw(width, height, output, index);
					delete[] output;
				} else {
					uint8_t* output = new uint8_t[width * height * 4];
					userData->processor->output(output);

					--jobsInFlight;
					unsigned long index = userData->index;
					output_image(width, height, output, index);
					delete[] output;
				}
			} else {
				--jobsInFlight;
			}
		}

		delete userData;

		job->Release();
	}

	virtual void DecodeComplete(IBlackmagicRawJob*, HRESULT) {}
	virtual void TrimProgress(IBlackmagicRawJob*, float) {}
	virtual void TrimComplete(IBlackmagicRawJob*, HRESULT) {}
	virtual void SidecarMetadataParseWarning(IBlackmagicRawClip*, const char*, uint32_t, const char*) {}
	virtual void SidecarMetadataParseError(IBlackmagicRawClip*, const char*, uint32_t, const char*) {}
	virtual void PreparePipelineComplete(void*, HRESULT) {}

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, LPVOID*) {
		return E_NOTIMPL;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef(void) {
		return 0;
	}

	virtual ULONG STDMETHODCALLTYPE Release(void) {
		return 0;
	}
};

HRESULT ProcessClip(IBlackmagicRawClip* clip, const char* lut_filename,
		unsigned int window_size, float gain)
{
	HRESULT result;

	long unsigned int frameCount = 0;
	long unsigned int frameIndex = 0;

	unsigned int width = 0;
	unsigned int height = 0;

	result = clip->GetWidth(&width);
	result = clip->GetHeight(&height);

	VideoProcessor processor(width, height, gain, lut_filename);

	if(ref_filename != nullptr) {
		FILE* f = fopen(ref_filename, "rb");
		uint16_t* image = new uint16_t[width * height * 4];
		fread(image, width * height * sizeof(uint16_t), 4, f);
		fclose(f);
		processor.load_reference(image, ref_after_lut);
		delete[] image;
	}

	result = clip->GetFrameCount(&frameCount);

	unsigned long output_delay = window_size;
	if(single) {
		output_delay = frameCount;
	}
	if(output_delay > frameCount) {
		output_delay = frameCount;
	}

	while(frameIndex < frameCount) {
		if(jobsInFlight >= s_maxJobsInFlight) {
			std::this_thread::sleep_for(std::chrono::microseconds(100));
			continue;
		}

		float percent = frameIndex * 100.0 / (frameCount - 1);
		printf("\r\x1b[KProcessing frame %lu [%5.1f%%]", frameIndex, percent);
		fflush(stdout);

		bool output = frameIndex >= output_delay - 1;
		if(frameIndex >= output_delay) {
			unsigned long idx = frameIndex - output_delay;
			IBlackmagicRawJob* jobRead = nullptr;
			if(result == S_OK) {
				result = clip->CreateJobReadFrame(idx, &jobRead);
			}

			UserData* userData = nullptr;
			if(result == S_OK) {
				userData = new UserData(&processor, false, false);
				VERIFY(jobRead->SetUserData(userData));
			}

			if(result == S_OK) {
				result = jobRead->Submit();
			}

			if(result != S_OK) {
				if(jobRead != nullptr) {
					jobRead->Release();
				}

				break;
			}

			++jobsInFlight;

			while(jobsInFlight >= s_maxJobsInFlight) {
				std::this_thread::sleep_for(std::chrono::microseconds(100));
			}
		}

		IBlackmagicRawJob* jobRead = nullptr;
		if(result == S_OK) {
			result = clip->CreateJobReadFrame(frameIndex, &jobRead);
		}

		UserData* userData = nullptr;
		if(result == S_OK) {
			userData = new UserData(&processor, true, output);
			if(output) {
				userData->index = frameIndex - output_delay + 1;
			}
			VERIFY(jobRead->SetUserData(userData));
		}

		if(result == S_OK) {
			result = jobRead->Submit();
		}

		if(result != S_OK) {
			if(jobRead != nullptr) {
				jobRead->Release();
			}

			break;
		}

		++jobsInFlight;

		frameIndex++;
	}

	printf("\n");

	printf("Waiting for jobs to finish...\n");

	while(jobsInFlight > 0) {
		std::this_thread::sleep_for(std::chrono::microseconds(50));
	}

	return result;
}

int main(int argc, const char** argv)
{
	const char* self = *argv;
	const char* lut_filename = nullptr;
	const char* clipName = nullptr;
	unsigned int window_size = 100;
	float gain = 1.0f;

	argc--;
	argv++;

	for(; argc; argc--, argv++) {
		if(!strcmp(*argv, "-l") && argc > 1) {
			lut_filename = argv[1];
			argc--;
			argv++;
		} else if(!strcmp(*argv, "-o") && argc > 1) {
			outputFileName = argv[1];
			argc--;
			argv++;
		} else if(!strcmp(*argv, "-i") && argc > 1) {
			clipName = argv[1];
			argc--;
			argv++;
		} else if(!strcmp(*argv, "-g") && argc > 1) {
			gain = (float) atof(argv[1]);
			argc--;
			argv++;
		} else if(!strcmp(*argv, "-r") && argc > 1) {
			ref_filename = argv[1];
			argc--;
			argv++;
		} else if(!strcmp(*argv, "-L")) {
			ref_after_lut = true;
		} else if(!strcmp(*argv, "-s")) {
			single = true;
		} else if(!strcmp(*argv, "-R")) {
			raw_dump = true;
			single = true;
		} else if(!strcmp(*argv, "-w") && argc > 1) {
			int win = atoi(argv[1]);
			if(win < 0) {
				std::cerr << "Invalid window size" << std::endl;
				return 1;
			}
			window_size = (unsigned int) win;
			argc--;
			argv++;
		} else {
			std::cerr << "Usage: " << self << " -i clip.braw [-o image.bmp]" << std::endl;
			return 1;
		}
	}

	if(clipName == nullptr) {
		std::cerr << "Missing clip name" << std::endl;
		return 1;
	}

	tjinst = tjInitCompress();

	HRESULT result = S_OK;

	IBlackmagicRawFactory* factory = nullptr;
	IBlackmagicRaw* codec = nullptr;
	IBlackmagicRawClip* clip = nullptr;

	CameraCodecCallback callback;

	unsigned int width = 0;
	unsigned int height = 0;

	factory = CreateBlackmagicRawFactoryInstanceFromPath(BRAWSDK_ROOT "/Libraries/");
	if(factory == nullptr) {
		std::cerr << "Failed to create IBlackmagicRawFactory!" << std::endl;
		goto end;
	}

	result = factory->CreateCodec(&codec);
	if(result != S_OK) {
		std::cerr << "Failed to create IBlackmagicRaw!" << std::endl;
		goto end;
	}

	result = codec->OpenClip(clipName, &clip);
	if(result != S_OK) {
		std::cerr << "Failed to open IBlackmagicRawClip!" << std::endl;
		goto end;
	}

	result = codec->SetCallback(&callback);
	if(result != S_OK) {
		std::cerr << "Failed to set IBlackmagicRawCallback!" << std::endl;
		goto end;
	}

	result = clip->GetWidth(&width);
	if(result != S_OK) {
		std::cerr << "Failed to get image width!" << std::endl;
		goto end;
	}

	result = clip->GetHeight(&height);
	if(result != S_OK) {
		std::cerr << "Failed to get image height!" << std::endl;
		goto end;
	}

	ProcessClip(clip, lut_filename, window_size, gain);

	codec->FlushJobs();

end:
	if(clip != nullptr) {
		clip->Release();
	}

	if(codec != nullptr) {
		codec->Release();
	}

	if(factory != nullptr) {
		factory->Release();
	}

	return result;
}
