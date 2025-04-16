package com.example.camera;

public class Event {
    public final Type type;
    public final String data;

    public Event(Type type, String data) {
        this.type = type;
        this.data = data;
    }

    public enum Type {
        LocalDescription,
        LocalCandidate,
        StateChange,
        GatheringStateChange,
        SetLocalDescription,
        SetLocalCandidate;

        public static Type fromInt(int i) {
            return values()[i];
        }
    }
}
