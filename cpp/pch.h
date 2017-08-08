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


#include <stdlib.h>
// Windows
#include <agile.h>
#include <collection.h>
#include <concrt.h>
#include <ppltasks.h>
#include <wincodec.h>
#include <wrl.h>
#include <WindowsNumerics.h>

// DirectX
#include <d2d1_2.h>
#include <d2d1effects_1.h>
#include <d3d11_4.h>
#include <DirectXColors.h>
#include <DirectXMath.h>
#include <dwrite_2.h>
#include <dxgi1_5.h>

// STL
#include <memory>
#include <map>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <string>


#include <array>
#include <Mfidl.h>
#include <Mfapi.h>
#include <Mfreadwrite.h>
#include <xaudio2.h>
#include <xapo.h>
#include <hrtfapoapi.h>
#include "Common\PrintWstringToDebugConsole.h"
#include "LuxandFaceSDK.h"

struct TFaceRecord {
	char filename[1024];
	FSDK_FaceTemplate FaceTemplate;
};