# Hippula patches

The `hippula` branch carries fixes the Hippula guardian app needs on top of upstream `main`. The app consumes the AAR from a module-local Maven repository, not from Maven Central.

- Button and status callbacks attach to the JVM only when the thread is detached. A synchronous control transfer runs libusb's event handling on the calling Java thread, and the unconditional detach there aborted the app when the camera sent an interrupt during `setPreviewSize`.
- The callbacks return early when no Java callback is registered, and their destructors unregister from libuvc, so an interrupt during release cannot reach a freed object.
- `USBMonitor.updateDeviceInfo` tolerates a null connection or short descriptors (`openDevice` returns null for a UVC device until the CAMERA permission is granted).
- 16 KB page alignment and a pinned NDK.

## Release

Bump `HIPPULA_VERSION` and push to `hippula`. The `Hippula AAR` workflow builds the release AAR, checks the arm64 page alignment and uploads the Maven repository layout as the `maven` artifact; copy it into the app's `modules/hippula-uvc-camera/android/maven/org/uvccamera/lib/`.

## Releases

| Version | Commit | AAR sha256 | Build |
| --- | --- | --- | --- |
| 0.0.13-hippula.1 | e0df72b | `e0599359b7ba117b32b6d06d4e8dcb2dcd806922c526a3cdc817cad99059ec92` | [run 43913565684](https://github.com/project-hippula/UVCCamera/actions/runs/43913565684) |
