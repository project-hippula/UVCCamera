#include <stdlib.h>
#include <linux/time.h>
#include <unistd.h>
#include "utilbase.h"
#include "UVCStatusCallback.h"
#include "libuvc_internal.h"

#define	LOCAL_DEBUG 0

UVCStatusCallback::UVCStatusCallback(uvc_device_handle_t *devh)
:	mDeviceHandle(devh),
	mStatusCallbackObj(NULL) {

	ENTER();
	pthread_mutex_init(&status_mutex, NULL);

	uvc_set_status_callback(mDeviceHandle, uvc_status_callback, (void *)this);
	EXIT();
}

UVCStatusCallback::~UVCStatusCallback() {

	ENTER();
	// libuvc calls back under its own status_mutex, so this also waits out a callback in flight
	uvc_set_status_callback(mDeviceHandle, NULL, NULL);
	pthread_mutex_destroy(&status_mutex);
	EXIT();
}

int UVCStatusCallback::setCallback(JNIEnv *env, jobject status_callback_obj) {
	
	ENTER();
	pthread_mutex_lock(&status_mutex);
	{
		if (!env->IsSameObject(mStatusCallbackObj, status_callback_obj)) {
			istatuscallback_fields.onStatus = NULL;
			if (mStatusCallbackObj) {
				env->DeleteGlobalRef(mStatusCallbackObj);
			}
			mStatusCallbackObj = status_callback_obj;
			if (status_callback_obj) {
				// get method IDs of Java object for callback
				jclass clazz = env->GetObjectClass(status_callback_obj);
				if (LIKELY(clazz)) {
					istatuscallback_fields.onStatus = env->GetMethodID(clazz,
						"onStatus",	"(IIIILjava/nio/ByteBuffer;)V");
				} else {
					LOGW("failed to get object class");
				}
				env->ExceptionClear();
				if (!istatuscallback_fields.onStatus) {
					LOGE("Can't find IStatusCallback#onStatus");
					env->DeleteGlobalRef(status_callback_obj);
					mStatusCallbackObj = status_callback_obj = NULL;
				}
			}
		}
	}
	pthread_mutex_unlock(&status_mutex);
	RETURN(0, int);
}

void UVCStatusCallback::notifyStatusCallback(JNIEnv* env, uvc_status_class status_class, int event, int selector, uvc_status_attribute status_attribute, void *data, size_t data_len) {

	pthread_mutex_lock(&status_mutex);
	{
		if (mStatusCallbackObj) {
			jobject buf = env->NewDirectByteBuffer(data, data_len);
			env->CallVoidMethod(mStatusCallbackObj, istatuscallback_fields.onStatus, (int)status_class, event, selector, (int)status_attribute, buf);
			env->ExceptionClear();
			env->DeleteLocalRef(buf);
		}
	}
	pthread_mutex_unlock(&status_mutex);
}

void UVCStatusCallback::uvc_status_callback(uvc_status_class status_class, int event, int selector, uvc_status_attribute status_attribute, void *data, size_t data_len, void *user_ptr) {

	UVCStatusCallback *statusCallback = reinterpret_cast<UVCStatusCallback *>(user_ptr);

	if (!statusCallback->mStatusCallbackObj) {
		LOGW("status event %d/%d dropped: no Java callback", (int)status_class, event);
		return;
	}

	JavaVM *vm = getVM();
	JNIEnv *env;
	// A synchronous control transfer runs libusb's event handling on the calling Java thread,
	// so this callback can run there; detaching that thread aborts the runtime.
	bool attached = false;
	if (vm->GetEnv((void **)&env, JNI_VERSION_1_6) == JNI_EDETACHED) {
		if (vm->AttachCurrentThread(&env, NULL) != JNI_OK) {
			return;
		}
		attached = true;
	}

	statusCallback->notifyStatusCallback(env, status_class, event, selector, status_attribute, data, data_len);

	if (attached) {
		vm->DetachCurrentThread();
	}
}
