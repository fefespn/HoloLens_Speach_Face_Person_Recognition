//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "pch.h"
#include "WinBase.h"
#include "Windows.h"
#include "HolographicFaceTrackerMain.h"
#include "Common\DirectXHelper.h"

#include "Content\VideoFrameProcessor.h"
#include "Content\FaceTrackerProcessor.h"

#include "Content\SpinningCubeRenderer.h"
#include "Content\QuadRenderer.h"
#include "Content\TextRenderer.h"
#include "Content\NV12VideoTexture.h"

#include <windows.graphics.directx.direct3d11.interop.h>
#include <Collection.h>

#include "Audio/OmnidirectionalSound.h"
#include <iostream>

#include <iostream>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <shellapi.h>

//#include <msclr\marshal_cppstd.h>
using namespace std;

using namespace HolographicFaceTracker;

using namespace Concurrency;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Numerics;
using namespace Windows::Graphics::Holographic;
using namespace Windows::Graphics::Imaging;
using namespace Windows::Media::Capture;
using namespace Windows::Media::Capture::Frames;
using namespace Windows::Media::Devices::Core;
using namespace Windows::Perception::Spatial;
using namespace Windows::UI::Input::Spatial;
using namespace Windows::Media::SpeechRecognition;
using namespace Windows::Storage::Streams;
using namespace Windows::Graphics::Imaging;
using namespace Windows::UI::Xaml::Media::Imaging;

using namespace Windows::UI::Xaml::Media;
using namespace concurrency;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::Web::Http;
using namespace Windows::Web::Http::Headers;

using namespace Windows::Storage;
using namespace Windows::Storage::Pickers;
using namespace Windows::Storage::Streams;

using namespace Windows::UI::Xaml::Controls;

using namespace Windows::Security::Cryptography;



using namespace std::placeholders;

using namespace Microsoft::WRL::Wrappers;
using namespace Microsoft::WRL;

using namespace DirectX;

// Loads and initializes application assets when the application is loaded.
HolographicFaceTrackerMain::HolographicFaceTrackerMain(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	m_deviceResources(deviceResources)
{
	// Register to be notified if the device is lost or recreated.
	m_deviceResources->RegisterDeviceNotify(this);

}

void HolographicFaceTrackerMain::BeginVoiceUIPrompt()
{
	// RecognizeWithUIAsync provides speech recognition begin and end prompts, but it does not provide
	// synthesized speech prompts. Instead, you should provide your own speech prompts when requesting
	// phrase input.
	// Here is an example of how to do that with a speech synthesizer. You could also use a pre-recorded 
	// voice clip, a visual UI, or other indicator of what to say.
	auto speechSynthesizer = ref new Windows::Media::SpeechSynthesis::SpeechSynthesizer();

	StringReference voicePrompt;

	// A command list is used to continuously look for one-word commands.
	// You need some way for the user to know what commands they can say. In this example, we provide
	// verbal instructions; you could also use graphical UI, etc.
	voicePrompt = L"Welcome, you can speak anything, and the subtitles will be shown";

	// Kick off speech synthesis.
	create_task(speechSynthesizer->SynthesizeTextToStreamAsync(voicePrompt), task_continuation_context::use_current())
		.then([this, speechSynthesizer](task<Windows::Media::SpeechSynthesis::SpeechSynthesisStream^> synthesisStreamTask)
	{
		try
		{
			// The speech synthesis is sent as a byte stream.
			Windows::Media::SpeechSynthesis::SpeechSynthesisStream^ stream = synthesisStreamTask.get();

			// We can initialize an XAudio2 voice using that byte stream.
			// Here, we use it to play an HRTF audio effect.
			auto hr = m_speechSynthesisSound.Initialize(stream, 0);
			if (SUCCEEDED(hr))
			{
				m_speechSynthesisSound.SetEnvironment(HrtfEnvironment::Small);
				m_speechSynthesisSound.Start();

				// Amount of time to pause after the audio prompt is complete, before listening 
				// for speech input.
				static const float bufferTime = 0.15f;

				// Wait until the prompt is done before listening.
				m_secondsUntilSoundIsComplete = m_speechSynthesisSound.GetDuration() + bufferTime;
				m_waitingForSpeechPrompt = true;
			}
		}
		catch (Exception^ exception)
		{
			PrintWstringToDebugConsole(
				std::wstring(L"Exception while trying to synthesize speech: ") +
				exception->Message->Data() +
				L"\n"
			);

			// Handle exceptions here.
		}
	});
}

void HolographicFaceTrackerMain::PlayRecognitionBeginSound()
{
	// The user needs a cue to begin speaking. We will play this sound effect just before starting 
	// the recognizer.
	auto hr = m_startRecognitionSound.GetInitializationStatus();
	if (SUCCEEDED(hr))
	{
		m_startRecognitionSound.SetEnvironment(HrtfEnvironment::Small);
		m_startRecognitionSound.Start();

		// Wait until the audible cue is done before starting to listen.
		m_secondsUntilSoundIsComplete = m_startRecognitionSound.GetDuration();
		m_waitingForSpeechCue = true;
	}
}

void HolographicFaceTrackerMain::PlayRecognitionSound()
{
	// The user should be given a cue when recognition is complete. 
	auto hr = m_recognitionSound.GetInitializationStatus();
	if (SUCCEEDED(hr))
	{
		// re-initialize the sound so it can be replayed.
		m_recognitionSound.Initialize(L"BasicResultsEarcon.wav", 0);
		m_recognitionSound.SetEnvironment(HrtfEnvironment::Small);

		m_recognitionSound.Start();
	}
}

Concurrency::task<void> HolographicFaceTrackerMain::StopCurrentRecognizerIfExists()
{
	if (m_speechRecognizer != nullptr)
	{
		return create_task(m_speechRecognizer->StopRecognitionAsync()).then([this]()
		{
			m_speechRecognizer->RecognitionQualityDegrading -= m_speechRecognitionQualityDegradedToken;
			//m_speechRecognizer->StateChanged -= m_stateChangedToken;
			m_speechRecognizer->HypothesisGenerated -= m_speechRecognitionHypothesis;

			if (m_speechRecognizer->ContinuousRecognitionSession != nullptr)
			{
				m_speechRecognizer->ContinuousRecognitionSession->ResultGenerated -= m_speechRecognizerResultEventToken;
				m_speechRecognizer->ContinuousRecognitionSession->Completed -= m_speechRecognitionCompletedToken;
			}
		});
	}
	else
	{
		return create_task([this]() {});
	}
}

bool HolographicFaceTrackerMain::InitializeSpeechRecognizer()
{
	m_speechRecognizer = ref new SpeechRecognizer();
	//m_speechRecognizer = ref new SpeechRecognizer(SpeechRecognizer::SystemSpeechLanguage);

	if (!m_speechRecognizer)
	{
		return false;
	}

	m_speechRecognitionQualityDegradedToken = m_speechRecognizer->RecognitionQualityDegrading +=
		ref new TypedEventHandler<SpeechRecognizer^, SpeechRecognitionQualityDegradingEventArgs^>(
			std::bind(&HolographicFaceTrackerMain::OnSpeechQualityDegraded, this, _1, _2)
			);

	m_speechRecognizerResultEventToken = m_speechRecognizer->ContinuousRecognitionSession->ResultGenerated +=
		ref new TypedEventHandler<SpeechContinuousRecognitionSession^, SpeechContinuousRecognitionResultGeneratedEventArgs^>(
			std::bind(&HolographicFaceTrackerMain::OnResultGenerated, this, _1, _2)
			);
	m_speechRecognitionCompletedToken = m_speechRecognizer->ContinuousRecognitionSession->Completed +=
		ref new TypedEventHandler<SpeechContinuousRecognitionSession^, SpeechContinuousRecognitionCompletedEventArgs^>(
			std::bind(&HolographicFaceTrackerMain::OnRecognitionCompleted, this, _1, _2)
			);
	//m_speechRecognizer->ContinuousRecognitionSession->com

	m_speechRecognitionHypothesis = m_speechRecognizer->HypothesisGenerated +=
		ref new TypedEventHandler<SpeechRecognizer^, SpeechRecognitionHypothesisGeneratedEventArgs^>(
			std::bind(&HolographicFaceTrackerMain::OnHypothesisGenerated, this, _1, _2)
			);




	return true;
}

task<bool> HolographicFaceTrackerMain::StartRecognizeSpeechCommands()
{
	return StopCurrentRecognizerIfExists().then([this]()
	{
		if (!InitializeSpeechRecognizer())
		{
			return task_from_result<bool>(false);
		}




		SpeechRecognitionTopicConstraint^ topicConstraint = ref new
			SpeechRecognitionTopicConstraint(SpeechRecognitionScenario::Dictation, L"Dictation");

		m_speechRecognizer->UIOptions->AudiblePrompt = "Say what you want to search for...";
		m_speechRecognizer->UIOptions->ExampleText = "Ex. 'weather for London'";
		m_speechRecognizer->Constraints->Clear();
		m_speechRecognizer->Constraints->Append(topicConstraint);

		//return create_task(m_speechRecognizer->CompileConstraintsAsync())
		return create_task(m_speechRecognizer->CompileConstraintsAsync())
			.then([this](task<SpeechRecognitionCompilationResult^> previousTask)
		{
			try
			{
				SpeechRecognitionCompilationResult^ compilationResult = previousTask.get();


				if (compilationResult->Status == SpeechRecognitionResultStatus::Success)
				{

					// If compilation succeeds, we can start listening for results.
					return create_task(m_speechRecognizer->ContinuousRecognitionSession->StartAsync()).then([this](task<void> startAsyncTask) {
						//return create_task(m_speechRecognizer->RecognizeAsync()).then([this](task<void> startAsyncTask) {

						try
						{
							// StartAsync may throw an exception if your app doesn't have Microphone permissions. 
							// Make sure they're caught and handled appropriately (otherwise the app may silently not work as expected)
							//	startAsyncTask.get();
							startAsyncTask.get();


							return true;
						}
						catch (Exception^ exception)
						{
							PrintWstringToDebugConsole(
								std::wstring(L"Exception while trying to start speech Recognition: ") +
								exception->Message->Data() +
								L"\n"
							);

							return false;
						}
					});
				}
				else
				{
					OutputDebugStringW(L"Could not initialize constraint-based speech engine!\n");

					// Handle errors here.
					return create_task([this] {return false; });
				}
			}
			catch (Exception^ exception)
			{
				// Note that if you get an "Access is denied" exception, you might need to enable the microphone 
				// privacy setting on the device and/or add the microphone capability to your app manifest.

				PrintWstringToDebugConsole(
					std::wstring(L"Exception while trying to initialize speech command list:") +
					exception->Message->Data() +
					L"\n"
				);

				// Handle exceptions here.
				return create_task([this] {return false; });
			}
		});
	});
}

void HolographicFaceTrackerMain::SetHolographicSpace(HolographicSpace^ holographicSpace)
{
	UnregisterHolographicEventHandlers();

	m_holographicSpace = holographicSpace;

	// Use the default SpatialLocator to track the motion of the device.
	m_locator = SpatialLocator::GetDefault();

	// Be able to respond to changes in the positional tracking state.
	m_locatabilityChangedToken =
		m_locator->LocatabilityChanged +=
		ref new Windows::Foundation::TypedEventHandler<SpatialLocator^, Object^>(
			std::bind(&HolographicFaceTrackerMain::OnLocatabilityChanged, this, _1, _2)
			);

	// Respond to camera added events by creating any resources that are specific
	// to that camera, such as the back buffer render target view.
	// When we add an event handler for CameraAdded, the API layer will avoid putting
	// the new camera in new HolographicFrames until we complete the deferral we created
	// for that handler, or return from the handler without creating a deferral. This
	// allows the app to take more than one frame to finish creating resources and
	// loading assets for the new holographic camera.
	// This function should be registered before the app creates any HolographicFrames.
	m_cameraAddedToken =
		m_holographicSpace->CameraAdded +=
		ref new Windows::Foundation::TypedEventHandler<HolographicSpace^, HolographicSpaceCameraAddedEventArgs^>(
			std::bind(&HolographicFaceTrackerMain::OnCameraAdded, this, _1, _2)
			);

	// Respond to camera removed events by releasing resources that were created for that
	// camera.
	// When the app receives a CameraRemoved event, it releases all references to the back
	// buffer right away. This includes render target views, Direct2D target bitmaps, and so on.
	// The app must also ensure that the back buffer is not attached as a render target, as
	// shown in DeviceResources::ReleaseResourcesForBackBuffer.
	m_cameraRemovedToken =
		m_holographicSpace->CameraRemoved +=
		ref new Windows::Foundation::TypedEventHandler<HolographicSpace^, HolographicSpaceCameraRemovedEventArgs^>(
			std::bind(&HolographicFaceTrackerMain::OnCameraRemoved, this, _1, _2)
			);

	// The simplest way to render world-locked holograms is to create a stationary reference frame
	// when the app is launched. This is roughly analogous to creating a "world" coordinate system
	// with the origin placed at the device's position as the app is launched.
	m_referenceFrame = m_locator->CreateAttachedFrameOfReferenceAtCurrentHeading();

	// First we initialize our MediaCapture and FaceTracking objects
	task<void> videoInitTask = VideoFrameProcessor::CreateAsync()
		.then([this](std::shared_ptr<VideoFrameProcessor> videoProcessor)
	{
		m_videoFrameProcessor = std::move(videoProcessor);

		return FaceTrackerProcessor::CreateAsync(m_videoFrameProcessor)
			.then([this](std::shared_ptr<FaceTrackerProcessor> faceProcessor)
		{
			m_faceTrackerProcessor = std::move(faceProcessor);
		});
	});

	// Then we can initialize our device dependent resources, which depend on the VideoFrameProcessor.
	videoInitTask.then([this]
	{
		std::vector<task<void>> deviceInitTasks = {
			DX::CreateAndInitializeAsync(m_quadRenderer, m_deviceResources),
			DX::CreateAndInitializeAsync(m_spinningCubeRenderer, m_deviceResources),
			DX::CreateAndInitializeAsync(m_textRenderer, m_deviceResources, 512u, 512u),
			DX::CreateAndInitializeAsync(m_textRenderer_details, m_deviceResources, 512u, 512u),
		};

		if (m_videoFrameProcessor)
		{
			VideoMediaFrameFormat^ videoFormat = m_videoFrameProcessor->GetCurrentFormat();
			deviceInitTasks.push_back(DX::CreateAndInitializeAsync(m_videoTexture, m_deviceResources, videoFormat->Width, videoFormat->Height));
		}

		when_all(deviceInitTasks.begin(), deviceInitTasks.end()).then([this]
		{
			// If we weren't able to create the VideoFrameProcessor, then we don't have any cameras we can use.
			// Set our status message to inform the user. Typically this should only happen on the emulator.
			if (m_videoFrameProcessor == nullptr)
			{
				m_textRenderer->RenderTextOffscreen(L"No camera available");
			}
			// Everything is good to go, so we can set our status message to inform the user when we don't detect
			// any faces.
			else
			{
				m_textRenderer->RenderTextOffscreen(L"No faces detected");
				m_textRenderer_details->RenderTextOffscreen(L"No faces details ");
			}

			m_isReadyToRender = true;
		});
	});

	// Notes on spatial tracking APIs:
	// * Stationary reference frames are designed to provide a best-fit position relative to the
	//   overall space. Individual positions within that reference frame are allowed to drift slightly
	//   as the device learns more about the environment.
	// * When precise placement of individual holograms is required, a SpatialAnchor should be used to
	//   anchor the individual hologram to a position in the real world - for example, a point the user
	//   indicates to be of special interest. Anchor positions do not drift, but can be corrected; the
	//   anchor will use the corrected position starting in the next frame after the correction has
	//   occurred.

	// Preload audio assets for audio cues.
	//m_startRecognitionSound.Initialize(L"Audio//BasicListeningEarcon.wav", 0);
	m_startRecognitionSound.Initialize(L"BasicListeningEarcon.wav", 0);
	m_recognitionSound.Initialize(L"BasicResultsEarcon.wav", 0);

	// Begin the code sample scenario.
	BeginVoiceUIPrompt();


}

void HolographicFaceTrackerMain::UnregisterHolographicEventHandlers()
{
	if (m_holographicSpace != nullptr)
	{
		// Clear previous event registrations.

		if (m_cameraAddedToken.Value != 0)
		{
			m_holographicSpace->CameraAdded -= m_cameraAddedToken;
			m_cameraAddedToken.Value = 0;
		}

		if (m_cameraRemovedToken.Value != 0)
		{
			m_holographicSpace->CameraRemoved -= m_cameraRemovedToken;
			m_cameraRemovedToken.Value = 0;
		}
	}

	if (m_locator != nullptr)
	{
		m_locator->LocatabilityChanged -= m_locatabilityChangedToken;
	}
}

HolographicFaceTrackerMain::~HolographicFaceTrackerMain()
{
	// Deregister device notification.
	m_deviceResources->RegisterDeviceNotify(nullptr);

	UnregisterHolographicEventHandlers();
}

void HolographicFaceTrackerMain::ProcessFaces(std::vector<BitmapBounds> const& faces, Windows::Media::Capture::Frames::MediaFrameReference^ frame, SpatialCoordinateSystem^ worldCoordSystem)
{
	VideoMediaFrameFormat^ videoFormat = frame->VideoMediaFrame->VideoFormat;
	SpatialCoordinateSystem^ cameraCoordinateSystem = frame->CoordinateSystem;
	CameraIntrinsics^ cameraIntrinsics = frame->VideoMediaFrame->CameraIntrinsics;

	IBox<float4x4>^ cameraToWorld = cameraCoordinateSystem->TryGetTransformTo(worldCoordSystem);

	// If we can't locate the world, this transform will be null.
	if (cameraToWorld == nullptr)
	{
		return;
	}

	float const textureWidthInv = 1.0f / static_cast<float>(videoFormat->Width);
	float const textureHeightInv = 1.0f / static_cast<float>(videoFormat->Height);

	// The face analysis returns very "tight fitting" rectangles.
	// We add some padding to make the visuals more appealing.
	constexpr uint32_t paddingForFaceRect = 24u;
	constexpr float averageFaceWidthInMeters = 0.15f;

	float const pixelsPerMeterAlongX = cameraIntrinsics->FocalLength.x;
	float const averagePixelsForFaceAt1Meter = pixelsPerMeterAlongX * averageFaceWidthInMeters;

	// Place the cube 25cm above the center of the face.
	//float3 const cubeOffsetInWorldSpace = float3{ 0.0f, 0.25f, 0.0f };
	float3 const cubeOffsetInWorldSpace = float3{ 0.0f, 0.05f, 0.0f };

	BitmapBounds bestRect = {};
	float3 bestRectPositionInCameraSpace = float3::zero();
	float bestDotProduct = -1.0f;

	for (BitmapBounds const& faceRect : faces)
	{
		Point const faceRectCenterPoint = {
			static_cast<float>(faceRect.X + faceRect.Width / 2u),
			static_cast<float>(faceRect.Y + faceRect.Height / 2u),
		};

		// Calculate the vector towards the face at 1 meter.
		float2 const centerOfFace = cameraIntrinsics->UnprojectAtUnitDepth(faceRectCenterPoint);

		// Add the Z component and normalize.
		float3 const vectorTowardsFace = normalize(float3{ centerOfFace.x, centerOfFace.y, -1.0f });

		// Estimate depth using the ratio of the current faceRect width with the average faceRect width at 1 meter.
		float const estimatedFaceDepth = averagePixelsForFaceAt1Meter / static_cast<float>(faceRect.Width);

		// Get the dot product between the vector towards the face and the gaze vector.
		// The closer the dot product is to 1.0, the closer the face is to the middle of the video image.
		float const dotFaceWithGaze = dot(vectorTowardsFace, -float3::unit_z());

		// Scale the vector towards the face by the depth, and add an offset for the cube.
		float3 const targetPositionInCameraSpace = vectorTowardsFace * estimatedFaceDepth;

		// Pick the faceRect that best matches the users gaze.
		if (dotFaceWithGaze > bestDotProduct)
		{
			bestDotProduct = dotFaceWithGaze;
			bestRect = faceRect;
			bestRectPositionInCameraSpace = targetPositionInCameraSpace;
		}
	}

	// Transform the cube from Camera space to World space.
	float3 const bestRectPositionInWorldspace = transform(bestRectPositionInCameraSpace, cameraToWorld->Value);

	m_spinningCubeRenderer->SetTargetPosition(bestRectPositionInWorldspace + cubeOffsetInWorldSpace);

	// Texture Coordinates are [0,1], but our FaceRect is [0,Width] and [0,Height], so we need to normalize these coordinates
	// We also add padding for the faceRects to make it more visually appealing.
	float const normalizedWidth = static_cast<float>(bestRect.Width + paddingForFaceRect * 2u) * textureWidthInv;
	float const normalizedHeight = static_cast<float>(bestRect.Height + paddingForFaceRect * 2u) * textureHeightInv;
	float const normalizedX = static_cast<float>(bestRect.X - paddingForFaceRect) * textureWidthInv;
	float const normalizedY = static_cast<float>(bestRect.Y - paddingForFaceRect) * textureHeightInv;

	//m_quadRenderer->SetTexCoordScaleAndOffset({ normalizedWidth, normalizedHeight }, { normalizedX, normalizedY });
}

// Updates the application state once per frame.
HolographicFrame^ HolographicFaceTrackerMain::Update()
{
	if (!m_isReadyToRender)
	{
		return nullptr;
	}

	// The HolographicFrame has information that the app needs in order
	// to update and render the current frame. The app begins each new
	// frame by calling CreateNextFrame.
	HolographicFrame^ holographicFrame = m_holographicSpace->CreateNextFrame();

	// Get a prediction of where holographic cameras will be when this frame
	// is presented.
	HolographicFramePrediction^ prediction = holographicFrame->CurrentPrediction;

	// Back buffers can change from frame to frame. Validate each buffer, and recreate
	// resource views and depth buffers as needed.
	m_deviceResources->EnsureCameraResources(holographicFrame, prediction);

	SpatialCoordinateSystem^ currentCoordinateSystem = m_referenceFrame->GetStationaryCoordinateSystemAtTimestamp(prediction->Timestamp);

	auto m_pre_sentence = make_shared<wstring>(L"Value 1");
	//m_pre_sentence = pre_sentence;
	if (m_videoFrameProcessor && m_faceTrackerProcessor)
	{
		m_trackingFaces = m_faceTrackerProcessor->IsTrackingFaces();

		if (m_trackingFaces)
		{
			if (MediaFrameReference^ frame = m_videoFrameProcessor->GetLatestFrame())
			{
				ProcessFaces(m_faceTrackerProcessor->GetLatestFaces(), frame, currentCoordinateSystem);

				int64_t const currentTimeStamp = frame->SystemRelativeTime->Value.Duration;

				// Copy only new frames to our DirectX texture.
				if (currentTimeStamp > m_previousFrameTimestamp)
				{
					m_videoTexture->CopyFromVideoMediaFrame(frame->VideoMediaFrame);
					class MyClass
					{
					public:
						MyClass();
						~MyClass();

						int s = 1;
						
					};

					
					m_previousFrameTimestamp = currentTimeStamp;
					// 2 weeks code.
					if (searching == false){
					
						// Encode the buffer back into a Base64 string.

						SoftwareBitmap^ sftBitmap_c = frame->VideoMediaFrame->SoftwareBitmap;
						SoftwareBitmap^ sftBitmap = SoftwareBitmap::Convert(sftBitmap_c, BitmapPixelFormat::Bgra8);
						InMemoryRandomAccessStream^ mss = ref new InMemoryRandomAccessStream();
						//SpinningCubeRenderer  m_spinningcube = *m_spinningCubeRenderer;
						//Windows::Foundation::Numerics::float4 a = Windows::Foundation::Numerics::float4(0.0f, 1.0f, 0.0f, 0.0f);
						//  m_spinningCubeRenderer->SetColor(a);
						//  m_spinningcube.SetColor(a);
						 // std::shared_ptr<TextRenderer>  m_textRenderer_r = m_textRenderer;
						  
						  //MyClass m_textRenderer_r;

						 // auto s = make_shared<wstring>(L"Value 1");
						 // m_textRenderer
						 
						  auto m_textRenderer_r = m_textRenderer;
						create_task(BitmapEncoder::CreateAsync(BitmapEncoder::PngEncoderId, mss)
						).then([this, sftBitmap, mss, m_pre_sentence, m_textRenderer_r](Windows::Graphics::Imaging::BitmapEncoder^ encoder) // initialize the encoder for PNG
						{
							encoder->SetSoftwareBitmap(sftBitmap);  // set the bitmap for the encoder
							//encoder->FlushAsync()
							create_task(encoder->FlushAsync()).then([this, sftBitmap, mss, m_pre_sentence, m_textRenderer_r]() // flush all the data
							{
								//if (bobo) {
								IBuffer^ bufferr = ref new Buffer(mss->Size);
								create_task(mss->ReadAsync(bufferr, mss->Size, InputStreamOptions::None)).then([this, mss, bufferr, m_pre_sentence, m_textRenderer_r](IBuffer^ bufferr2) // return the byte reader
								{
									String^ strBase64New_new = CryptographicBuffer::EncodeToBase64String(bufferr);

									{

										HttpClient^ httpClient = ref new HttpClient();
										//Uri^ uri = ref new Uri("https://api.kairos.com/enroll");
										Uri^ uri = ref new Uri("https://api.kairos.com/recognize");
										httpClient->DefaultRequestHeaders->TryAppendWithoutValidation("app_id", "98a9ce6b");
										httpClient->DefaultRequestHeaders->TryAppendWithoutValidation("app_key", "314e18bf9c959790db7be4e05e520b68");

										//Platform::String^ s = "{  \"image\": \"" + strBase64New_new + "\",  \"subject_id\": \"Elizabeth\",  \"gallery_name\": \"MyGallery\"}";
										Platform::String^ s = "{  \"image\": \"" + strBase64New_new + "\", \"gallery_name\": \"MyGallery\"}";
										IAsyncOperationWithProgress<HttpResponseMessage^, HttpProgress> ^accessSQLOp = httpClient->PostAsync(uri, ref new HttpStringContent(s, Windows::Storage::Streams::UnicodeEncoding::Utf8, "application/json"));
										auto operationTask = create_task(accessSQLOp);
										operationTask.then([this, m_pre_sentence, m_textRenderer_r](HttpResponseMessage^ response) {

											int a = (int)response->StatusCode;
											if (response->StatusCode == HttpStatusCode::Ok)
											{
												try
												{
													auto asyncOperationWithProgress = response->Content->ReadAsStringAsync();
													create_task(asyncOperationWithProgress).then([this, m_pre_sentence, m_textRenderer_r](Platform::String^ responJsonText)
													{
														Platform::String^  ss = (responJsonText);
														//string &s = ss->Data;
														std::wstring s(ss->Data());		
														int first_index = s.find(L"subject_id");
														if (first_index > 0) {
															int second_index = s.find(L",", first_index + 13);
															std::wstring name = s.substr(first_index + 13, second_index - first_index-13 - 1);
															m_textRenderer_r->pre_sentence_pre = name+L": ";
														}
														else {
															m_textRenderer_r->pre_sentence_pre = L"unknown: ";
														}
														//Windows::Foundation::Numerics::float4 a = Windows::Foundation::Numerics::float4(0.0f, 1.0f, 0.0f, 0.0f);
														// m_spinningcube.SetColor(Windows::Foundation::Numerics::float4((1.0f, 1.0f, 1.0f, 1.0f)));
														//m_textRenderer_r->RenderTextOffscreen(L"Found API");
														//m_textRenderer_r->pre_sentence_pre = L"FOFO";
														//*m_pre_sentence = (L"Hello:");
														
														
													});
												}
												catch (Exception^ ex)
												{
												}
											}
										});

									}
								});



							});
						});
						

				}
			}
		}

		searching = true;
	}
	else 
	{
		
		//m_textRenderer_r->pre_sentence_pre = L"";
		m_textRenderer->pre_sentence_pre = L"";
		searching = false; 
	}
	}


	//std::wstring fefe = L"firas";

	// Check for new speech input since the last frame.
	if (m_lastCommand != nullptr)
	{
		auto command = m_lastCommand;
		m_lastCommand = nullptr;
		std::wstring lastCommandString = command->Data();
		m_lastSentence = lastCommandString;
		m_textRenderer->RenderTextOffscreen(L'"' +lastCommandString + L'"');
		m_textRenderer_details->RenderTextOffscreen(m_textRenderer->pre_sentence_pre);
		
	}

	SpatialPointerPose^ pointerPose = SpatialPointerPose::TryGetAtTimestamp(currentCoordinateSystem, prediction->Timestamp);
	SpatialPointerPose^ pointerPose_details = SpatialPointerPose::TryGetAtTimestamp(currentCoordinateSystem, prediction->Timestamp);

	m_timer.Tick([&] {
		m_spinningCubeRenderer->Update(m_timer);

		// If we're tracking faces, then put the quad to the left side of the viewport, 2 meters out.
		if (m_trackingFaces)
		{
			/*m_quadRenderer->Update(pointerPose, float3{ 0.0f, -0.15f, -2.0f }, m_timer);*/
			m_quadRenderer->Update(pointerPose, float3{ -0.0f, 0.0f, -2.0f }, m_timer);
			//m_quadRenderer_details->Update(pointerPose_details, float3{ -0.0f, 1.0f, -1.0f }, m_timer);
		}
		// Otherwise, put the quad centered in the viewport, 2 meters out.
		else
		{
			m_quadRenderer->ResetTexCoordScaleAndOffset();
			//m_quadRenderer_details->ResetTexCoordScaleAndOffset();
			/*m_quadRenderer->Update(pointerPose, float3{ 0.0f, -0.15f, -2.0f }, m_timer);*/
			m_quadRenderer->Update(pointerPose, float3{ -0.0f, 0.0f, -2.0f }, m_timer);
		//	m_quadRenderer_details->Update(pointerPose_details, float3{ -0.0f, 1.0f, -1.0f }, m_timer);
		}

		// Wait to listen for speech input until the audible UI prompts are complete.
		if ((m_waitingForSpeechPrompt == true) &&
			((m_secondsUntilSoundIsComplete -= static_cast<float>(m_timer.GetElapsedSeconds())) <= 0.f))
		{
			m_waitingForSpeechPrompt = false;
			PlayRecognitionBeginSound();
		}
		else if ((m_waitingForSpeechCue == true) &&
			((m_secondsUntilSoundIsComplete -= static_cast<float>(m_timer.GetElapsedSeconds())) <= 0.f))
		{
			m_waitingForSpeechCue = false;
			m_secondsUntilSoundIsComplete = 0.f;
			StartRecognizeSpeechCommands();
		}


	});

	// We complete the frame update by using information about our content positioning
	// to set the focus point.
	// Next, we get a coordinate system from the attached frame of reference that is
	// associated with the current frame. Later, this coordinate system is used for
	// for creating the stereo view matrices when rendering the sample content.

	for (auto cameraPose : prediction->CameraPoses)
	{
		// The HolographicCameraRenderingParameters class provides access to set
		// the image stabilization parameters.
		HolographicCameraRenderingParameters^ renderingParameters = holographicFrame->GetRenderingParameters(cameraPose);

		// If we're tracking faces, then put the focus point on the cube
		if (m_trackingFaces)
		{
			renderingParameters->SetFocusPoint(
				currentCoordinateSystem,
				m_spinningCubeRenderer->GetPosition()
			);
		}
		// Otherwise put the focus point on status message quad.
		else
		{
			renderingParameters->SetFocusPoint(
				currentCoordinateSystem,
				m_quadRenderer->GetPosition(),
				m_quadRenderer->GetNormal(),
				m_quadRenderer->GetVelocity()
			);
			
		}
	}

	// The holographic frame will be used to get up-to-date view and projection matrices and
	// to present the swap chain.
	return holographicFrame;
}

// Renders the current frame to each holographic camera, according to the
// current application and spatial positioning state. Returns true if the
// frame was rendered to at least one camera.
bool HolographicFaceTrackerMain::Render(Windows::Graphics::Holographic::HolographicFrame^ holographicFrame)
{
	if (!m_isReadyToRender)
	{
		return false;
	}

	SpatialCoordinateSystem^ currentCoordinateSystem = m_referenceFrame->GetStationaryCoordinateSystemAtTimestamp(holographicFrame->CurrentPrediction->Timestamp);

	// Lock the set of holographic camera resources, then draw to each camera
	// in this frame.
	return m_deviceResources->UseHolographicCameraResources<bool>(
		[this, holographicFrame, currentCoordinateSystem](std::map<UINT32, std::unique_ptr<DX::CameraResources>>& cameraResourceMap)
	{
		// Up-to-date frame predictions enhance the effectiveness of image stablization and
		// allow more accurate positioning of holograms.
		holographicFrame->UpdateCurrentPrediction();
		HolographicFramePrediction^ prediction = holographicFrame->CurrentPrediction;

		bool atLeastOneCameraRendered = false;
		for (auto cameraPose : prediction->CameraPoses)
		{
			// This represents the device-based resources for a HolographicCamera.
			DX::CameraResources* pCameraResources = cameraResourceMap[cameraPose->HolographicCamera->Id].get();

			// Get the device context.
			const auto context = m_deviceResources->GetD3DDeviceContext();
			const auto depthStencilView = pCameraResources->GetDepthStencilView();

			// Set render targets to the current holographic camera.
			ID3D11RenderTargetView *const targets[1] = { pCameraResources->GetBackBufferRenderTargetView() };
			context->OMSetRenderTargets(1, targets, depthStencilView);

			// Clear the back buffer and depth stencil view.
			context->ClearRenderTargetView(targets[0], DirectX::Colors::Transparent);
			context->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

			// The view and projection matrices for each holographic camera will change
			// every frame. This function refreshes the data in the constant buffer for
			// the holographic camera indicated by cameraPose.
			pCameraResources->UpdateViewProjectionBuffer(m_deviceResources, cameraPose, currentCoordinateSystem);

			// Attach the view/projection constant buffer for this camera to the graphics pipeline.
			bool cameraActive = pCameraResources->AttachViewProjectionBuffer(m_deviceResources);

			// Render world-locked content only when positional tracking is active.
			if (cameraActive)
			{
				// If we are tracking any faces, then we render the cube over their head, and the video image on the quad.
				if (m_trackingFaces)
				{
					m_spinningCubeRenderer->Render();
					// m_quadRenderer->RenderNV12(m_videoTexture->GetLuminanceTexture(), m_videoTexture->GetChrominanceTexture());
					m_quadRenderer->RenderRGB(m_textRenderer_details->GetTexture());
					//m_quadRenderer_details->RenderRGB(m_textRenderer_details->GetTexture());
				}
				// Otherwise we render the status message on the quad.
				else
				{
					m_quadRenderer->RenderRGB(m_textRenderer->GetTexture());
					//m_quadRenderer_details->RenderRGB(m_textRenderer_details->GetTexture());
				}
			}

			atLeastOneCameraRendered = true;
		}

		return atLeastOneCameraRendered;
	});
}

void HolographicFaceTrackerMain::SaveAppState()
{
	// This sample does not save any state on suspend.
}

void HolographicFaceTrackerMain::LoadAppState()
{
	// This sample has no state to restore on suspend.
}

void HolographicFaceTrackerMain::RenderOffscreenTexture()
{
	m_textRenderer->RenderTextOffscreen(L"Hello, Hologram!\n");
}

// Notifies classes that use Direct3D device resources that the device resources
// need to be released before this method returns.
void HolographicFaceTrackerMain::OnDeviceLost()
{
	m_isReadyToRender = false;

	m_quadRenderer->ReleaseDeviceDependentResources();
	//m_quadRenderer_details->ReleaseDeviceDependentResources();
	m_spinningCubeRenderer->ReleaseDeviceDependentResources();
	m_textRenderer->ReleaseDeviceDependentResources();
	m_textRenderer_details->ReleaseDeviceDependentResources();
	m_videoTexture->ReleaseDeviceDependentResources();
}

// Notifies classes that use Direct3D device resources that the device resources
// may now be recreated.
void HolographicFaceTrackerMain::OnDeviceRestored()
{
	auto initTasks = {
		m_quadRenderer->CreateDeviceDependentResourcesAsync(),
		m_spinningCubeRenderer->CreateDeviceDependentResourcesAsync(),
		m_textRenderer->CreateDeviceDependentResourcesAsync(),
		m_textRenderer_details->CreateDeviceDependentResourcesAsync(),
		m_videoTexture->CreateDeviceDependentResourcesAsync(),
	};

	when_all(initTasks.begin(), initTasks.end()).then([this] {
		m_isReadyToRender = true;
	});
	RenderOffscreenTexture();
}

void HolographicFaceTrackerMain::OnLocatabilityChanged(SpatialLocator^ sender, Object^ args)
{
	switch (sender->Locatability)
	{
	case SpatialLocatability::Unavailable:
		// Holograms cannot be rendered.
	{
		String^ message = L"Warning! Positional tracking is " +
			sender->Locatability.ToString() + L".\n";
		OutputDebugStringW(message->Data());
	}
	break;

	// In the following three cases, it is still possible to place holograms using a
	// SpatialLocatorAttachedFrameOfReference.
	case SpatialLocatability::PositionalTrackingActivating:
		// The system is preparing to use positional tracking.

	case SpatialLocatability::OrientationOnly:
		// Positional tracking has not been activated.
		break;

	case SpatialLocatability::PositionalTrackingInhibited:
		// Positional tracking is temporarily inhibited. User action may be required
		// in order to restore positional tracking.
		break;

	case SpatialLocatability::PositionalTrackingActive:
		// Positional tracking is active. World-locked content can be rendered.
		break;
	}
}

void HolographicFaceTrackerMain::OnCameraAdded(
	HolographicSpace^ sender,
	HolographicSpaceCameraAddedEventArgs^ args
)
{
	Deferral^ deferral = args->GetDeferral();
	HolographicCamera^ holographicCamera = args->Camera;
	create_task([this, deferral, holographicCamera]()
	{
		// Create device-based resources for the holographic camera and add it to the list of
		// cameras used for updates and rendering. Notes:
		//   * Since this function may be called at any time, the AddHolographicCamera function
		//     waits until it can get a lock on the set of holographic camera resources before
		//     adding the new camera. At 60 frames per second this wait should not take long.
		//   * A subsequent Update will take the back buffer from the RenderingParameters of this
		//     camera's CameraPose and use it to create the ID3D11RenderTargetView for this camera.
		//     Content can then be rendered for the HolographicCamera.
		m_deviceResources->AddHolographicCamera(holographicCamera);

		// Holographic frame predictions will not include any information about this camera until
		// the deferral is completed.
		deferral->Complete();
	});
}

void HolographicFaceTrackerMain::OnCameraRemoved(
	HolographicSpace^ sender,
	HolographicSpaceCameraRemovedEventArgs^ args
)
{
	// Before letting this callback return, ensure that all references to the back buffer
	// are released.
	// Since this function may be called at any time, the RemoveHolographicCamera function
	// waits until it can get a lock on the set of holographic camera resources before
	// deallocating resources for this camera. At 60 frames per second this wait should
	// not take long.
	m_deviceResources->RemoveHolographicCamera(args->Camera);
}


void HolographicFaceTrackerMain::OnHypothesisGenerated(SpeechRecognizer ^sender, SpeechRecognitionHypothesisGeneratedEventArgs ^args)
{

	m_lastCommand = args->Hypothesis->Text;

	// When the debugger is attached, we can print information to the debug console.
	PrintWstringToDebugConsole(
		std::wstring(L"Last command was: ") +
		m_lastCommand->Data() +
		L"\n"
	);
}


void HolographicFaceTrackerMain::OnRecognitionCompleted(SpeechContinuousRecognitionSession ^sender, SpeechContinuousRecognitionCompletedEventArgs ^args)
{

	if (args->Status != SpeechRecognitionResultStatus::Success)
	{
		// If TimeoutExceeded occurs, the user has been silent for too long. We can use this to 
		// cancel recognition if the user in dictation mode and walks away from their device, etc.
		// In a global-command type scenario, this timeout won't apply automatically.
		// With dictation (no grammar in place) modes, the default timeout is 20 seconds.
		if (args->Status == SpeechRecognitionResultStatus::TimeoutExceeded)
		{
			m_lastCommand = "";
		}
		else
		{
			m_lastCommand = args->Status.ToString();

			// When the debugger is attached, we can print information to the debug console.
			PrintWstringToDebugConsole(
				std::wstring(L"Last command was: ") +
				m_lastCommand->Data() +
				L"\n"
			);

		}
	}

}
// For speech example.
// Change the cube color, if we get a valid result.
void HolographicFaceTrackerMain::OnResultGenerated(SpeechContinuousRecognitionSession ^sender, SpeechContinuousRecognitionResultGeneratedEventArgs ^args)
{

	m_lastCommand = args->Result->Text;

	// When the debugger is attached, we can print information to the debug console.
	PrintWstringToDebugConsole(
		std::wstring(L"Last command was: ") +
		m_lastCommand->Data() +
		L"\n"
	);
}


void HolographicFaceTrackerMain::OnSpeechQualityDegraded(Windows::Media::SpeechRecognition::SpeechRecognizer^ recognizer, Windows::Media::SpeechRecognition::SpeechRecognitionQualityDegradingEventArgs^ args)
{
	switch (args->Problem)
	{
	case SpeechRecognitionAudioProblem::TooFast:
		OutputDebugStringW(L"The user spoke too quickly.\n");
		break;

	case SpeechRecognitionAudioProblem::TooSlow:
		OutputDebugStringW(L"The user spoke too slowly.\n");
		break;

	case SpeechRecognitionAudioProblem::TooQuiet:
		OutputDebugStringW(L"The user spoke too softly.\n");
		break;

	case SpeechRecognitionAudioProblem::TooLoud:
		OutputDebugStringW(L"The user spoke too loudly.\n");
		break;

	case SpeechRecognitionAudioProblem::TooNoisy:
		OutputDebugStringW(L"There is too much noise in the signal.\n");
		break;

	case SpeechRecognitionAudioProblem::NoSignal:
		OutputDebugStringW(L"There is no signal.\n");
		break;

	case SpeechRecognitionAudioProblem::None:
	default:
		OutputDebugStringW(L"An error was reported with no information.\n");
		break;
	}
}





//	string str_strBase64New = msclr::interop::marshal_as<std::string>(strBase64New);
/*
DataReader^ dataReader = DataReader::FromBuffer(buffer);
Platform::Array<unsigned char, 1>^ pixels = ref new Platform::Array<unsigned char, 1>(buffer->Length);
dataReader->ReadBytes(pixels);
char a1 = pixels[0];
char a2 = pixels[1];
char a3 = pixels[2];
char a4 = pixels[3];
char a5 = pixels[4];
char a6 = pixels[5];
cout << "hello: " << a1 << " " << a2 << endl;
*/
/*	BitmapPixelFormat forr = sftBitmap->BitmapPixelFormat;
int h = (sftBitmap->PixelHeight);
int w = sftBitmap->PixelWidth;
Windows::Storage::Streams::IBuffer^ buffer = ref new Buffer(h*w * 4);
sftBitmap->CopyToBuffer(buffer);
String^ strBase64New = CryptographicBuffer::EncodeToBase64String(buffer);
int l = strBase64New->Length();

const wchar_t* begin = strBase64New->Data();
char a0 = begin[0];
char a1 = begin[1];
char a2 = begin[2];
char a3 = begin[3];
char a4 = begin[4];
*/
//	bytep[] array = new byte[10] ;


/*
WriteableBitmap^ writeableBitmap = ref new WriteableBitmap(300, 300);
create_task(StorageFile::GetFileFromApplicationUriAsync(ref new Uri("ms-appx:///Assets/Logo.scale-100.png"))).then([this](StorageFile^ file)
{
if (file)
{
create_task(file->OpenAsync(FileAccessMode::Read)).then([this](IRandomAccessStream^ fileStream)
{

create_task(writeableBitmap->SetSourceAsync(fileStream)).then([this]()
{
FileSavePicker^ picker = ref new FileSavePicker();
auto imgExtensions = ref new Platform::Collections::Vector<String^>();
imgExtensions->Append(".jpg");
picker->FileTypeChoices->Insert("JPG File", imgExtensions);
create_task(picker->PickSaveFileAsync()).then([this](StorageFile^ saveFile)
{
if (saveFile == nullptr)
{
return;
}
create_task(saveFile->OpenAsync(FileAccessMode::ReadWrite)).then([this](IRandomAccessStream^ stream)
{
create_task(BitmapEncoder::CreateAsync(BitmapEncoder::JpegEncoderId, stream)).then([this](BitmapEncoder^ encoder)
{
IBuffer^ buffer = writeableBitmap->PixelBuffer;
DataReader^ dataReader = DataReader::FromBuffer(buffer);
Platform::Array<unsigned char, 1>^ pixels = ref new Platform::Array<unsigned char, 1>(buffer->Length);
dataReader->ReadBytes(pixels);
encoder->SetPixelData(BitmapPixelFormat::Bgra8, BitmapAlphaMode::Ignore, writeableBitmap->PixelWidth, writeableBitmap->PixelHeight, 96.0, 86.0, pixels);
create_task(encoder->FlushAsync());
});
});
});
});
});
}
});
*/


// Define a Base64 encoded string.
//	String strBase64 = "uiwyeroiugfyqcajkds897945234==";

// Decoded the string from Base64 to binary.
//	IBuffer^ bufferr = CryptographicBuffer.DecodeFromBase64String(strBase64);






//Windows::Storage::Streams::IBuffer^ buffer = wrBitmap->PixelBuffer;
/*	BitmapPixelFormat pf = sftBitmap->BitmapPixelFormat;
Boolean b = sftBitmap->IsReadOnly;
WriteableBitmap^ wrBitmap =ref new WriteableBitmap(sftBitmap->PixelWidth, sftBitmap->PixelHeight);
sftBitmap->CopyToBuffer(wrBitmap->PixelBuffer);
sftBitmap->CopyToBuffer()*/
//Stream pixelStream = writeableBitmap.PixelBuffer.AsStream();
//byte[] pixels = new byte[buffer.Length];













// Create a WritableBitmap for our visualization display; copy the original bitmap pixels to wb's buffer.
// Note that WriteableBitmap doesn't support NV12 and we have to convert it to 32-bit BGRA.
/*	SoftwareBitmap^ convertedSource = SoftwareBitmap::Convert(previewFrame->SoftwareBitmap, BitmapPixelFormat::Bgra8);

WriteableBitmap^ displaySource = ref new WriteableBitmap(convertedSource->PixelWidth, convertedSource->PixelHeight);
convertedSource->CopyToBuffer(displaySource->PixelBuffer);
Image
// Create our display using the available image and face results.
SetupVisualization(displaySource, faces);
*/

//BitmapImage
//ImageSource^  img = (ImageSource)wrBitmap;

/*
HttpClient^ client = ref new  HttpClient();


Uri^ uri = ref new Uri("https://api-us.faceplusplus.com/facepp/v3/compare");

Windows::Data::Json::JsonObject^ json =ref new Windows::Data::Json::JsonObject();
//json->SetNamedValue("api_key", Windows::Data::Json::JsonValue::CreateStringValue("api_key='LHK_bvFQXz-RwuhD7alTCeXfBftIrlXg'"));
//	json->SetNamedValue("api_secret", Windows::Data::Json::JsonValue::CreateStringValue("api_secret='fIT0J-oCu31yAUDCVayuqGHumVzvGdco'"));
//json->SetNamedValue("image_url1", Windows::Data::Json::JsonValue::CreateStringValue("http://i66.tinypic.com/wkr5lt.png"));
//json->SetNamedValue("image_url2", Windows::Data::Json::JsonValue::CreateStringValue("http://i66.tinypic.com/11j2a2g.png"));
//	json->SetNamedValue("Name", Windows::Data::Json::JsonValue::CreateStringValue(p->Name));



HttpStringContent^	content =ref new HttpStringContent(json->ToString(), Windows::Storage::Streams::UnicodeEncoding::Utf8, "application/json");

HttpRequestMessage^	request =ref new HttpRequestMessage(HttpMethod::Post, uri);
request->Content = content;
client->DefaultRequestHeaders->Accept->Append(ref new  HttpMediaTypeWithQualityHeaderValue("application/json"));
//HttpResponseMessage^ response;
//client->SendRequestAsync(request,HttpCompletionOption)
//HttpResponseMessage^ response = client->SendRequestAsync(request);
//HttpResponseMessage^ response =  client->SendRequestAsync(request, HttpCompletionOption::ResponseContentRead);
//	 HttpResponseMessage response = client->SendRequestAsync(request, HttpCompletionOption::ResponseContentRead).AsTask().Result;
Platform::String^  s;
//string s = "asdasd";
create_task(client->SendRequestAsync(request, HttpCompletionOption::ResponseContentRead)).then([=](HttpResponseMessage^ response)
{
//	response->EnsureSuccessStatusCode();
if (response->StatusCode == HttpStatusCode::Ok) {
Platform::String^  ss = response->StatusCode.ToString() + " " + (response->ReasonPhrase);

}

return create_task(response->Content->ReadAsStringAsync());
}).then([=](String^ responseBodyAsText) {

// Format the HTTP response to display better
// responseBodyAsText = responseBodyAsText->Replace("<br>", Environment->NewLine); // Insert new lines
Platform::String^  sss = responseBodyAsText;
}).then([=](task<void> prevTask)
{
try
{
prevTask.get();
}
catch (Exception ^ex)
{
Platform::String^  errorr_str = "Error = " + ex->HResult + "  Message: " + ex->Message;
return;
}
});


*/






/*

auto encoderId = Windows::Graphics::Imaging::BitmapEncoder::JpegEncoderId;

IRandomAccessStream^ stream;
stream->Size = 0;
create_task(Windows::Graphics::Imaging::BitmapEncoder::CreateAsync(
encoderId,
stream
)).then([this](Windows::Graphics::Imaging::BitmapEncoder^ encoder)
{
// An array representing 2x2 red, opaque pixel data
Array<byte>^ pixels = {
255, 0, 0, 255,
255, 0, 0, 255,
255, 0, 0, 255,
255, 0, 0, 255
};

encoder->SetPixelData(
Windows::Graphics::Imaging::BitmapPixelFormat::Rgba8,
Windows::Graphics::Imaging::BitmapAlphaMode::Straight,
2, // pixel width
2, // pixel height
96, // horizontal DPI
96, // vertical DPI
pixels
);
encoder->FlushAsync();
encoder->b

});
*/
/*

FileSavePicker^ savePicker = ref new FileSavePicker();
auto plainTextExtensions = ref new Platform::Collections::Vector<String^>();
plainTextExtensions->Append(".jpg");
savePicker->FileTypeChoices->Insert("JPEG Image", plainTextExtensions);
savePicker->SuggestedFileName = "MyImage";
savePicker->SuggestedStartLocation = PickerLocationId::PicturesLibrary;

create_task(savePicker->PickSaveFileAsync()).then([this](StorageFile^ file)
{
if (file)
{
create_task(file->OpenAsync(Windows::Storage::FileAccessMode::ReadWrite)).then([this](IRandomAccessStream^ stream)
{
auto encoderId = Windows::Graphics::Imaging::BitmapEncoder::JpegEncoderId;
stream->Size = 0;

create_task(Windows::Graphics::Imaging::BitmapEncoder::CreateAsync(
encoderId,
stream
)).then([this](Windows::Graphics::Imaging::BitmapEncoder^ encoder)
{
// An array representing 2x2 red, opaque pixel data
Array<byte>^ pixels = {
255, 0, 0, 255,
255, 0, 0, 255,
255, 0, 0, 255,
255, 0, 0, 255
};

encoder->SetPixelData(
Windows::Graphics::Imaging::BitmapPixelFormat::Rgba8,
Windows::Graphics::Imaging::BitmapAlphaMode::Straight,
2, // pixel width
2, // pixel height
96, // horizontal DPI
96, // vertical DPI
pixels
);
encoder->FlushAsync();
});
});
}
});

*/







//	std::wstring w_str = std::wstring(ss.begin(), ss.end());
//	const wchar_t* w_chars = w_str.c_str();
//	String^ str2 = ref new Platform::String(w_chars);

//Windows::Data::Json::JsonObject^ postData = ref new Windows::Data::Json::JsonObject();
//postData->SetNamedValue("Name", Windows::Data::Json::JsonValue::CreateStringValue(p->Name));
//postData->SetNamedValue("Age", Windows::Data::Json::JsonValue::CreateStringValue(p->Age.ToString()));

// async send "get" request to get response string form service 
//Platform::String^ s = "api_key='LHK_bvFQXz-RwuhD7alTCeXfBftIrlXg' api_secret='fIT0J-oCu31yAUDCVayuqGHumVzvGdco' image_url1='http://i66.tinypic.com/wkr5lt.png' image_url2='http://i66.tinypic.com/11j2a2g.png'";
//	Platform::String^ s1 = "{  \"image\": \"http://media.kairos.com/kairos-elizabeth.jpg\",  \"subject_id\": \"Elizabeth\",  \"gallery_name\": \"MyGallery\"}";




//IAsyncOperationWithProgress<HttpResponseMessage^, HttpProgress>^ response  = client->SendRequestAsync(request, HttpCompletionOption::ResponseContentRead);

//HttpResponseMessage^ message = response->GetResults();

//IAsyncOperationWithProgress<Platform::String^,unsigned long long>^ message2 =  message->Content->ReadAsStringAsync();

//Platform::String ^s = message2->GetResults();

///HttpContent^ content = message->Content;
//Type(message->Content->ReadAsStringAsync());
/////////////////////////////SCRIPT|
/********Luxand SDK******/
/*
TFaceRecord face1, face2;
strncpy_s(face1.filename, "firas1.png", 1024);
strncpy_s(face2.filename, "firas2.png", 1024);

if (FSDK_ActivateLibrary("lA7F3HdSJJuPE3p8Jx+ZA6GkElY+SBLmkY0HNw+srhEqOpNmlKo1MkCcu0vh6wsdrvrkcODmFz6vp1Y4qrf9K14UGx/H16jnsfEjRucwIQS+l2l1nTvBLh7+RugdQjTJS08G44LkQeU/cJ6sRmw+pJXZbz9Tm/mAAKPrFhoGOtI=") != FSDKE_OK)
{
//	cerr << "Error activating FaceSDK" << endl;
//	cerr << "Please run the License Key Wizard (Start - Luxand - FaceSDK - License Key Wizard)" << endl;
//	return -1;
}
FSDK_Initialize("");

HImage ImageHandle1, ImageHandle2;
if (FSDK_LoadImageFromFile(&ImageHandle1, "C:\\Users\\Public\\imgs\\firas1.png") != FSDKE_OK ||
FSDK_LoadImageFromFile(&ImageHandle2, "C:\\Users\\Public\\imgs\\firas2.png") != FSDKE_OK)
{
//	cerr << "Error loading file" << endl;
///	return;
}

//Assuming that faces are vertical (HandleArbitraryRotations = false) to speed up face detection
FSDK_SetFaceDetectionParameters(true, true, 512);
FSDK_SetFaceDetectionThreshold(3);
int r1 = FSDK_GetFaceTemplate(ImageHandle1, &face1.FaceTemplate);
int r2 = FSDK_GetFaceTemplate(ImageHandle2, &face2.FaceTemplate);
FSDK_FreeImage(ImageHandle1);
FSDK_FreeImage(ImageHandle2);

if (r1 != FSDKE_OK || r2 != FSDKE_OK)
{
//	cerr << "Error detecting face" << endl;
//	return;
}
float Similarity;
FSDK_MatchFaces(&face1.FaceTemplate, &face2.FaceTemplate, &Similarity);

std::string  Similarity_string = std::to_string(Similarity);
PrintWstringToDebugConsole(
std::wstring(L"Similarity= "+std::wstring(Similarity_string.begin(), Similarity_string.end()))

);



*/

/////////////////////////////SCRIPT|