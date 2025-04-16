#include <jni.h>
#include <string>
#include <android/log.h>
#include "EventQueue.h"

#define LOG_TAG "NativeLib"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static EventQueue gEventQueue;


struct Event {
    enum class Type {
        LocalDescription,
        LocalCandidate,
        StateChange,
        GatheringStateChange,
        SetLocalDescription,
        SetLocalCandidate,
    };

    Type type;
    std::string data;
};


extern "C"
JNIEXPORT void JNICALL
Java_com_example_camera_Transport_pushNativeEvent(JNIEnv *env, jobject /*thiz*/, jint typeInt, jstring dataJStr) {
    if (dataJStr == nullptr) {
        LOGE("Null data received in pushNativeEvent");
        return;
    }

    const char *dataCStr = env->GetStringUTFChars(dataJStr, nullptr);
    if (dataCStr == nullptr) {
        LOGE("Failed to convert jstring to UTF-8");
        return;
    }

    Event event;
    event.type = static_cast<Event::Type>(typeInt);
    event.data = std::string(dataCStr);

    gEventQueue.push(event);
    LOGD("Event pushed: type=%d, data=%s", typeInt, dataCStr);

    env->ReleaseStringUTFChars(dataJStr, dataCStr);
}


extern "C"
JNIEXPORT jobject JNICALL
Java_com_example_camera_Transport_pollNativeEvent(JNIEnv *env, jobject /*thiz*/) {
    Event nativeEvent;
    bool success = gEventQueue.pop(nativeEvent);  

    if (!success || nativeEvent.data.empty()) {
        return nullptr;
    }

    jclass eventClass = env->FindClass("com/example/camera/Event");
    if (!eventClass) return nullptr;

    // get method 
    jmethodID constructor = env->GetMethodID(eventClass, "<init>", "(ILjava/lang/String;)V");
    if (!constructor) return nullptr;

    // Tạo đối tượng data từ C++ (std::string -> jstring)
    jstring dataStr = env->NewStringUTF(nativeEvent.data.c_str());

    // Chuyển enum Type từ C++ (int) về Java
    jint typeOrdinal = static_cast<jint>(nativeEvent.type); // Type là enum trong C++

    // Trả về đối tượng Event
    return env->NewObject(eventClass, constructor, typeOrdinal, dataStr);
}
