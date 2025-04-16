package com.example.camera;

import android.os.Handler;
import android.os.Looper;
import com.example.camera.Event;

public class Transport {
    static {
        System.loadLibrary("nativeeventlib");
    }

    public interface EventCallback {
        void onEventReceived(String message);
        void onErrorOccurred(String errorMessage);
        void onStatusUpdate(String status);
    }

    // Biến để lưu callback
    private static EventCallback callback;

    // Set callback từ Java
    public static void setCallback(EventCallback eventCallback) {
        callback = eventCallback;
    }

    public static void notifyEventReceived(Event message) {
        if (callback != null) {
            new Handler(Looper.getMainLooper()).post(() -> {
                callback.onEventReceived(message);
            });
        }
    }

    public static void notifyErrorOccurred(String errorMessage) {
        if (callback != null) {
            new Handler(Looper.getMainLooper()).post(() -> {
                callback.onErrorOccurred(errorMessage);
            });
        }
    }

    public static void notifyStatusUpdate(String status) {
        if (callback != null) {
            new Handler(Looper.getMainLooper()).post(() -> {
                callback.onStatusUpdate(status);
            });
        }
    }

    public static void startListeningForEvents() {
        new Thread(new Runnable() {
            @Override
            public void run() {
                while (true) {
                    Event event = Transport.pollNativeEvent();
                    if (event != null) {
                        notifyEventReceived(event);
                    } else {
                        notifyErrorOccurred("No event data available");
                    }
                }
            }
        }).start();
    }

    public void pushEvent(Event event) { 
        pushNativeEvent(event.type, event.data);
    }

    private static native boolean pollNativeEvent();
    private static native void pushNativeEvent(int, String);

}