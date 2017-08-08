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

#pragma once

#include "Common\StepTimer.h"
#include "Common\DeviceResources.h"

#include "Audio\OmnidirectionalSound.h"


// Updates, renders, and presents holographic content using Direct3D.
namespace HolographicFaceTracker
{
    class VideoFrameProcessor;
    class FaceTrackerProcessor;

    class QuadRenderer;
    class SpinningCubeRenderer;
    class TextRenderer;
    class NV12VideoTexture;

    class HolographicFaceTrackerMain : public DX::IDeviceNotify
    {
    public:
        HolographicFaceTrackerMain(const std::shared_ptr<DX::DeviceResources>& deviceResources);
        ~HolographicFaceTrackerMain();

        // Sets the holographic space. This is our closest analogue to setting a new window
        // for the app.
        void SetHolographicSpace(Windows::Graphics::Holographic::HolographicSpace^ holographicSpace);

        // Starts the holographic frame and updates the content.
        Windows::Graphics::Holographic::HolographicFrame^ Update();

        // Renders holograms, including world-locked content.
        bool Render(Windows::Graphics::Holographic::HolographicFrame^ holographicFrame);

        bool IsReadyToRender(void) const { return m_isReadyToRender; }

        // Handle saving and loading of app state owned by AppMain.
        void SaveAppState();
        void LoadAppState();

        // IDeviceNotify
        virtual void OnDeviceLost();
        virtual void OnDeviceRestored();

    private:
		// Process continuous speech recognition results.
		void OnResultGenerated(
			Windows::Media::SpeechRecognition::SpeechContinuousRecognitionSession ^sender,
			Windows::Media::SpeechRecognition::SpeechContinuousRecognitionResultGeneratedEventArgs ^args
		);

		void OnRecognitionCompleted(
			Windows::Media::SpeechRecognition::SpeechContinuousRecognitionSession  ^sender,
			Windows::Media::SpeechRecognition::SpeechContinuousRecognitionCompletedEventArgs  ^args
		);

		void OnHypothesisGenerated(
			Windows::Media::SpeechRecognition::SpeechRecognizer ^sender,
			Windows::Media::SpeechRecognition::SpeechRecognitionHypothesisGeneratedEventArgs ^args
		);




		// Recognize when conditions might impact speech recognition quality.
		void OnSpeechQualityDegraded(
			Windows::Media::SpeechRecognition::SpeechRecognizer^ recognizer,
			Windows::Media::SpeechRecognition::SpeechRecognitionQualityDegradingEventArgs^ args
		);

		//// Initializes the speech command list.
		//void InitializeSpeechCommandList();

		// Initializes a speech recognizer.
		bool InitializeSpeechRecognizer();

		// Provides a voice prompt, before starting the scenario.
		void BeginVoiceUIPrompt();
		void PlayRecognitionBeginSound();
		void PlayRecognitionSound();

		// Creates a speech command recognizer, and starts listening.
		Concurrency::task<bool> StartRecognizeSpeechCommands();

		// Resets the speech recognizer, if one exists.
		Concurrency::task<void> StopCurrentRecognizerIfExists();
		// Sets up the texture for the hologram.
		void RenderOffscreenTexture();






        // Asynchronously creates resources for new holographic cameras.
        void OnCameraAdded(
            Windows::Graphics::Holographic::HolographicSpace^ sender,
            Windows::Graphics::Holographic::HolographicSpaceCameraAddedEventArgs^ args);

        // Synchronously releases resources for holographic cameras that are no longer
        // attached to the system.
        void OnCameraRemoved(
            Windows::Graphics::Holographic::HolographicSpace^ sender,
            Windows::Graphics::Holographic::HolographicSpaceCameraRemovedEventArgs^ args);

        // Used to notify the app when the positional tracking state changes.
        void OnLocatabilityChanged(
            Windows::Perception::Spatial::SpatialLocator^ sender,
            Platform::Object^ args);

        // Clears event registration state. Used when changing to a new HolographicSpace
        // and when tearing down AppMain.
        void UnregisterHolographicEventHandlers();

        void ProcessFaces(
            std::vector<Windows::Graphics::Imaging::BitmapBounds> const& faces,
            Windows::Media::Capture::Frames::MediaFrameReference^ frame,
            Windows::Perception::Spatial::SpatialCoordinateSystem^ worldCoordSystem);

        // Cached pointer to device resources.
        std::shared_ptr<DX::DeviceResources>                            m_deviceResources;

        // Render loop timer.
        DX::StepTimer                                                   m_timer;

        // Represents the holographic space around the user.
        Windows::Graphics::Holographic::HolographicSpace^               m_holographicSpace;

        // SpatialLocator that is attached to the primary camera.
        Windows::Perception::Spatial::SpatialLocator^                   m_locator;

        // A reference frame attached to the holographic camera.
        Windows::Perception::Spatial::SpatialLocatorAttachedFrameOfReference^ m_referenceFrame;


        // Video and face tracking processors
        std::shared_ptr<VideoFrameProcessor>                            m_videoFrameProcessor;
        std::shared_ptr<FaceTrackerProcessor>                           m_faceTrackerProcessor;

        // Objects related to rendering/3D models
        std::shared_ptr<QuadRenderer>                                   m_quadRenderer;
		std::shared_ptr<QuadRenderer>                                   m_quadRenderer_details;
        std::shared_ptr<SpinningCubeRenderer>                           m_spinningCubeRenderer;
        std::shared_ptr<TextRenderer>                                   m_textRenderer;
		std::shared_ptr<TextRenderer>                                   m_textRenderer_details;
        std::shared_ptr<NV12VideoTexture>                               m_videoTexture;
		//std::shared_ptr<string>                                         pre_sentence = make_shared<string>("Value 1");
		
		
        // Used to avoid redundant copying of frames to our DirectX texture.
        int64_t                                                         m_previousFrameTimestamp = 0;

        // Indicates whether any faces being tracked at the moment.
        bool                                                            m_trackingFaces = false;

        // Indicates whether all resources necessary for rendering are ready.
        bool                                                            m_isReadyToRender = false;

        // Event registration tokens.
        Windows::Foundation::EventRegistrationToken                     m_cameraAddedToken;
        Windows::Foundation::EventRegistrationToken                     m_cameraRemovedToken;
        Windows::Foundation::EventRegistrationToken                     m_locatabilityChangedToken;

		Windows::Foundation::EventRegistrationToken                     m_speechRecognizerResultEventToken;
		Windows::Foundation::EventRegistrationToken                     m_speechRecognitionQualityDegradedToken;
		Windows::Foundation::EventRegistrationToken						m_speechRecognitionHypothesis;
		Windows::Foundation::EventRegistrationToken						m_speechRecognitionCompletedToken;

		bool                                                            m_listening;

		// Speech recognizer.
		Windows::Media::SpeechRecognition::SpeechRecognizer^            m_speechRecognizer;

		// Maps commands to color data.
		// We will create a Vector of the key values in this map for use as speech commands.
		Platform::Collections::Map<Platform::String^, Windows::Foundation::Numerics::float4>^ m_speechCommandData;
		// The most recent speech command received.
		Platform::String^                                               m_lastCommand;

		// Handles playback of speech synthesis audio.
		OmnidirectionalSound                                            m_speechSynthesisSound;
		OmnidirectionalSound                                            m_recognitionSound;
		OmnidirectionalSound                                            m_startRecognitionSound;

		bool                                                            m_waitingForSpeechPrompt = false;
		bool                                                            m_waitingForSpeechCue = false;
		float                                                           m_secondsUntilSoundIsComplete = 0.f;
		std::wstring													m_lastSentence = L"Welcome";
		bool                                                            searching = false;
    };
}
