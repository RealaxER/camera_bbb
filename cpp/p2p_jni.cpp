#include <jni.h>
#include <string>
#include <android/log.h>
#include <p2p.h>

#define LOG_TAG "P2P-JNI" 
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

P2P p2p;

extern "C"
JNIEXPORT void JNICALL
Java_com_example_camera_Transport_nativePushEvent(JNIEnv *env, jobject /*thiz*/, jint typeInt, jstring dataJStr) {
    if (dataJStr == nullptr) {
        LOGE("Null data received in nativePushEvent");
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

    p2p.pushEvent(event);
    LOGD("Event pushed: type=%d, data=%s", typeInt, dataCStr);

    env->ReleaseStringUTFChars(dataJStr, dataCStr);
}


extern "C"
JNIEXPORT jobject JNICALL
Java_com_example_camera_Transport_nativePollEvent(JNIEnv *env, jobject /*thiz*/) {
    Event nativeEvent;
    bool success = p2p.popEvent(nativeEvent);

    if (!success || nativeEvent.data.empty()) {
        return nullptr;
    }

    jclass eventClass = env->FindClass("com/example/camera/Event");
    if (!eventClass) return nullptr;

    jmethodID constructor = env->GetMethodID(eventClass, "<init>", "(ILjava/lang/String;)V");
    if (!constructor) return nullptr;

    jstring dataStr = env->NewStringUTF(nativeEvent.data.c_str());

    jint typeOrdinal = static_cast<jint>(nativeEvent.type); 

    return env->NewObject(eventClass, constructor, typeOrdinal, dataStr);
}


extern "C"
JNIEXPORT void JNICALL
Java_com_example_camera_Transport_nativeSetStunServer(JNIEnv *env, jobject /*thiz*/, jstring stunServer) {
    if (stunServer == nullptr) {
        LOGE("Null STUN server received in SetStunServer");
        return;
    }

    const char *stunServerCStr = env->GetStringUTFChars(stunServer, nullptr);
    if (stunServerCStr == nullptr) {
        LOGE("Failed to convert jstring to UTF-8");
        return;
    }

    p2p.SetStunServer(std::string(stunServerCStr)); 

    env->ReleaseStringUTFChars(stunServer, stunServerCStr); 
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_camera_Transport_nativeSetTurnServer(JNIEnv *env, jobject /*thiz*/, jstring turnServer) {
    if (turnServer == nullptr) {
        LOGE("Null TURN server received in SetTurnServer");
        return;
    }

    const char *turnServerCStr = env->GetStringUTFChars(turnServer, nullptr);
    if (turnServerCStr == nullptr) {
        LOGE("Failed to convert jstring to UTF-8");
        return;
    }

    p2p.SetTurnServer(std::string(turnServerCStr)); 

    env->ReleaseStringUTFChars(turnServer, turnServerCStr);  
}


extern "C"
JNIEXPORT void JNICALL
Java_com_example_camera_Transport_nativeCreatePeerConnection(JNIEnv *env, jobject /*thiz*/) {
    p2p.CreatePeerConnection();
    LOGD("Peer connection created.");
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_camera_Transport_nativeCreateDataChannel(JNIEnv *env, jobject /*thiz*/, jstring label) {
    const char *labelStr = env->GetStringUTFChars(label, nullptr);
    if (labelStr == nullptr) {
        LOGE("Failed to get Data Channel label string");
        return;
    }

    p2p.CreateDataChannel(labelStr);
    LOGD("Data channel created with label: %s", labelStr);

    env->ReleaseStringUTFChars(label, labelStr);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_camera_Transport_nativeSetRemoteDescription(JNIEnv *env, jobject /*thiz*/, jstring description) {
    const char *desStr = env->GetStringUTFChars(description, nullptr);
    if (desStr == nullptr) {
        LOGE("Failed to get Remote Description string");
        return;
    }

    p2p.setRemoteDescription(desStr);
    LOGD("Remote description set: %s", desStr);

    env->ReleaseStringUTFChars(description, desStr);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_camera_Transport_nativeAddRemoteCandidate(JNIEnv *env, jobject /*thiz*/, jstring candidate) {
    const char *candidateStr = env->GetStringUTFChars(candidate, nullptr);
    if (candidateStr == nullptr) {
        LOGE("Failed to get Remote Candidate string");
        return;
    }

    p2p.addRemoteCandidate(candidateStr);
    LOGD("Remote candidate added: %s", candidateStr);

    env->ReleaseStringUTFChars(candidate, candidateStr);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_camera_Transport_nativeSendMessageByLabel(JNIEnv *env, jobject /*thiz*/, jstring label, jstring message) {
    const char *labelCStr = env->GetStringUTFChars(label, nullptr);
    const char *messageCStr = env->GetStringUTFChars(message, nullptr);
    
    if (labelCStr == nullptr || messageCStr == nullptr) {
        LOGE("Failed to convert jstring to UTF-8");
        return;
    }

    p2p.sendMessageByLabel(std::string(labelCStr), std::string(messageCStr));

    env->ReleaseStringUTFChars(label, labelCStr);
    env->ReleaseStringUTFChars(message, messageCStr);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_camera_Transport_nativeHandleIncomingDataChannel(JNIEnv *env, jobject /*thiz*/) {
    p2p.HandleIncomingDataChannel();
}


extern "C"
JNIEXPORT void JNICALL
Java_com_example_camera_Transport_nativeSetMaxMessageSize(JNIEnv *env, jobject /*thiz*/, jlong maxByte) {
    p2p.SetMaxMessageSize(static_cast<size_t>(maxByte));
}


extern "C"
JNIEXPORT jstring JNICALL
Java_com_example_camera_Transport_nativeGetLocalDescription(JNIEnv *env, jobject /*thiz*/) {
    std::string localDescription = p2p.GetLocalDescription();
    return env->NewStringUTF(localDescription.c_str());
}


extern "C"
JNIEXPORT jobjectArray JNICALL
Java_com_example_camera_Transport_nativeGetLocalCandidate(JNIEnv *env, jobject /*thiz*/) {
    std::vector<std::string> candidates = p2p.GetLocalCandidate();
    jobjectArray result = env->NewObjectArray(candidates.size(), env->FindClass("java/lang/String"), nullptr);
    
    for (size_t i = 0; i < candidates.size(); i++) {
        env->SetObjectArrayElement(result, i, env->NewStringUTF(candidates[i].c_str()));
    }

    return result;
}


extern "C"
JNIEXPORT jboolean JNICALL
Java_com_example_camera_Transport_nativeSendBufferByLabel(JNIEnv *env, jobject /*thiz*/, jstring label, jbyteArray data, jlong size) {
    const char *labelCStr = env->GetStringUTFChars(label, nullptr);
    jbyte *dataBytes = env->GetByteArrayElements(data, nullptr);
    
    if (labelCStr == nullptr || dataBytes == nullptr) {
        LOGE("Failed to convert jstring or jbyteArray");
        return JNI_FALSE;
    }

    bool result = p2p.sendBufferByLabel(std::string(labelCStr), reinterpret_cast<uint8_t*>(dataBytes), static_cast<size_t>(size));

    env->ReleaseStringUTFChars(label, labelCStr);
    env->ReleaseByteArrayElements(data, dataBytes, 0);

    return result ? JNI_TRUE : JNI_FALSE;
}


// extern "C"
// JNIEXPORT jobject JNICALL
// Java_com_example_camera_Transport_nativeGetDataChannelByIndex(JNIEnv *env, jobject /*thiz*/, jlong index) {
//     auto dataChannel = p2p.getDataChannelByIndex(static_cast<size_t>(index));
//     if (!dataChannel) {
//         return nullptr;
//     }

//     jclass dataChannelClass = env->FindClass("com/example/camera/DataChannel");
//     jmethodID constructor = env->GetMethodID(dataChannelClass, "<init>", "(Ljava/lang/String;)V");

//     return env->NewObject(dataChannelClass, constructor, env->NewStringUTF(dataChannel->label().c_str()));
// }


// extern "C"
// JNIEXPORT void JNICALL
// Java_com_example_camera_Transport_nativeSendMessageByChannel(JNIEnv *env, jobject /*thiz*/, jobject dataChannelObj, jstring message) {
//     jclass dataChannelClass = env->GetObjectClass(dataChannelObj);
//     jmethodID getLabelMethod = env->GetMethodID(dataChannelClass, "label", "()Ljava/lang/String;");
//     jstring label = (jstring)env->CallObjectMethod(dataChannelObj, getLabelMethod);

//     const char *messageCStr = env->GetStringUTFChars(message, nullptr);
//     if (messageCStr == nullptr) {
//         LOGE("Failed to convert jstring to UTF-8");
//         return;
//     }

//     std::string labelStr = env->GetStringUTFChars(label, nullptr);
//     p2p.sendMessageByChannel(dataChannelObj, std::string(messageCStr));

//     env->ReleaseStringUTFChars(message, messageCStr);
// }
