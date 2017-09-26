<!---
  category: Holographic
  samplefwlink: http://go.microsoft.com/fwlink/p/?LinkId=824113
--->

# Holographic Deaf Assitant App

An Application for assisting deaf people.


### Additional remarks

**Note** This application for Windows 10 Holographic require Visual Studio 2017 Update 3
to build, and a Windows Holographic device to execute. Windows Holographic devices include the
Microsoft HoloLens and the Microsoft HoloLens Emulator.

To obtain information about Windows 10 development, go to the [Windows Dev Center](http://go.microsoft.com/fwlink/?LinkID=532421).

To obtain information about the tools used for Windows Holographic development, including
Microsoft Visual Studio 2017 Update 3 and the Microsoft HoloLens Emulator, go to
[Install the tools](https://developer.microsoft.com/windows/holographic/install_the_tools).

## Related topics

### Samples

[BasicFaceDetection](/Samples/BasicFaceDetection)

[BasicFaceTracking](/Samples/BasicFaceTracking)

[CameraFrames](/Sample/CameraFrames)

### Reference

The following types are used in this code application:
* [Windows.Media.Capture namespace](https://msdn.microsoft.com/library/windows/apps/windows.media.capture.aspx)
  * [MediaCapture class](https://msdn.microsoft.com/library/windows/apps/windows.media.capture.mediacapture.aspx)
* [Windows.Media.Capture.Frames namespace](https://msdn.microsoft.com/library/windows/apps/windows.media.capture.frames.aspx)
  * [MediaFrameReference class](https://msdn.microsoft.com/library/windows/apps/windows.media.capture.frames.mediaframereference.aspx)
  * [MediaFrameReference.CoordinateSystem property](https://msdn.microsoft.com/library/windows/apps/windows.media.capture.frames.mediaframereference.coordinatesystem.aspx)
  * [MediaFrameSource class](https://msdn.microsoft.com/library/windows/apps/windows.media.capture.frames.mediaframesource.aspx)
  * [VideoMediaFrame class](https://msdn.microsoft.com/library/windows/apps/windows.media.capture.frames.videomediaframe.aspx)
  * [VideoMediaFrame.CameraIntrinsics](https://msdn.microsoft.com/library/windows/apps/windows.media.capture.frames.videomediaframe.cameraintrinsics.aspx)
* [Windows.Media.Devices.Core namespace](https://msdn.microsoft.com/library/windows/apps/windows.media.devices.core.aspx)
  * [CameraIntrinsics class](https://msdn.microsoft.com/library/windows/apps/windows.media.devices.core.cameraintrinsics.aspx)
  * [CameraIntrinsics.UnprojectAtUnitDepth method](https://msdn.microsoft.com/library/windows/apps/windows.media.devices.core.cameraintrinsics.unprojectatunitdepth.aspx)
* [Windows.Media.FaceAnalysis namespace](https://msdn.microsoft.com/library/windows/apps/windows.media.faceanalysis.aspx)
  * [FaceTracker class](https://msdn.microsoft.com/library/windows/apps/windows.media.faceanalysis.facetracker.aspx)
  * [FaceTracker.ProcessNextFrameAsync method](https://msdn.microsoft.com/library/windows/apps/windows.media.faceanalysis.facetracker.processnextframeasync.aspx)

## System requirements

**Client:** Windows 10 Holographic build 14393

**Phone:** Not supported

## Build the application

1. If you download the application ZIP, be sure to unzip the entire archive, not just the folder with
   the application you want to build.
2. Start Microsoft Visual Studio 2017 and select **File** \> **Open** \> **Project/Solution**.
3. Starting in the folder where you unzipped the application, go to the cpp subfolder. Double-click the Visual Studio Solution (.sln) file.
4. Press Ctrl+Shift+B, or select **Build** \> **Build Solution**.

## Run the application

The next steps depend on whether you just want to deploy the application or you want to both deploy and
run it.

### Deploying the application to the Microsoft HoloLens emulator

- Click the debug target drop-down, and select **Microsoft HoloLens Emulator**.
- Select **Build** \> **Deploy** Solution.

### Deploying the application to a Microsoft HoloLens

- Developer unlock your Microsoft HoloLens. For instructions, go to [Enable your device for development]
  (https://msdn.microsoft.com/windows/uwp/get-started/enable-your-device-for-development#enable-your-windows-10-devices).
- Find the IP address of your Microsoft HoloLens. The IP address can be found in **Settings**
  \> **Network & Internet** \> **Wi-Fi** \> **Advanced options**. Or, you can ask Cortana for this
  information by saying: "Hey Cortana, what's my IP address?"
- Right-click on your project in Visual Studio, and then select **Properties**.
- In the Debugging pane, click the drop-down and select **Remote Machine**.
- Enter the IP address of your Microsoft HoloLens into the field labelled **Machine Name**.
- Click **OK**.
- Select **Build** \> **Deploy** Solution.

### Pairing your developer-unlocked Microsoft HoloLens with Visual Studio

The first time you deploy from your development PC to your developer-unlocked Microsoft HoloLens,
you will need to use a PIN to pair your PC with the Microsoft HoloLens.
- When you select **Build** \> **Deploy Solution**, a dialog box will appear for Visual Studio to
  accept the PIN.
- On your Microsoft HoloLens, go to **Settings** \> **Update** \> **For developers**, and click on
  **Pair**.
- Type the PIN displayed by your Microsoft HoloLens into the Visual Studio dialog box and click
  **OK**.
- On your Microsoft HoloLens, select **Done** to accept the pairing.
- The solution will then start to deploy.

### Deploying and running the application

- To debug the application and then run it, follow the steps listed above to connect your
  developer-unlocked Microsoft HoloLens, then press F5 or select **Debug** \> **Start Debugging**.
  To run  the application without debugging, press Ctrl+F5 or select **Debug** \> **Start Without Debugging**.
